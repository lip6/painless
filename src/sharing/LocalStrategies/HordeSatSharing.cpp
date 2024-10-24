#include "HordeSatSharing.hpp"
#include "painless.hpp"
#include "sharing/Filters/BloomFilter.hpp"
#include "solvers/SolverFactory.hpp"
#include "utils/Logger.hpp"
#include "utils/Parameters.hpp"

#include <chrono>

HordeSatSharing::HordeSatSharing(const std::shared_ptr<ClauseDatabase>& clauseDB,
								 unsigned long literalsPerProducerPerRound,
								 unsigned int initialLbdLimit,
								 unsigned int roundsBeforeLbdIncrease,
								 const std::vector<std::shared_ptr<SharingEntity>>& producers,
								 const std::vector<std::shared_ptr<SharingEntity>>& consumers)
	: SharingStrategy(producers, consumers, clauseDB)
	, literalPerRound(literalsPerProducerPerRound)
	, initialLbdLimit(initialLbdLimit)
	, roundBeforeIncrease(roundsBeforeLbdIncrease)
{
	this->round = 0;

	for (auto& weakProducer : m_producers) {
		if (auto producer = weakProducer.lock()) {
			this->lbdLimitPerProducer.emplace(producer->getSharingId(), initialLbdLimit);
			this->literalsPerProducer.emplace(producer->getSharingId(), 0);
		}
	}

	LOGSTAT("[HordeSat] Producers: %d, Consumers: %d, Initial Lbd limit: %u, round "
			"before increase: %d, literals per round: %d",
			m_producers.size(),
			this->getClientCount(),
			initialLbdLimit,
			this->roundBeforeIncrease,
			literalPerRound);
	if (literalPerRound <= 0) {
		LOGERROR(
			"LiteralPerRound is to be positive in order to correctly compute the percentage used in lbd limit update.");
		std::abort();
	}
}

HordeSatSharing::~HordeSatSharing() {}

bool
HordeSatSharing::importClause(const ClauseExchangePtr& clause)
{
	assert(clause->size > 0 && clause->from != -1);

	int id = clause->from;

	LOGDEBUG3("Solver %d: Clause with lbd %d is tested against limit %d",
			  id,
			  clause->lbd,
			  this->lbdLimitPerProducer[id].load());

	if (clause->lbd <= this->lbdLimitPerProducer[id]) {
		this->stats.receivedClauses++;
		if (m_clauseDB->addClause(clause)) {
			this->literalsPerProducer.at(clause->from) += clause->size;
			return true;
		} else
			return false;
	} else {
		this->stats.filteredAtImport++;
		return false;
	}
}

bool
HordeSatSharing::doSharing()
{

	// Check for global termination condition
	if (globalEnding)
		return true;

	// Step 1: Get new clause selection
	this->m_clauseDB->giveSelection(selection, literalPerRound * m_producers.size());

	std::shared_lock<std::shared_mutex> lock(m_producersMutex);

	// Step 2: Process producers
	for (auto& weakProducer : m_producers) {
		if (auto producer = weakProducer.lock()) {
			int produced, producedPercent;
			int id = producer->getSharingId();
			produced = this->literalsPerProducer.at(id);
			producedPercent = (100 * produced) / literalPerRound;
			LOG3("[HordeSat] Production rate of %d = %d", id, producedPercent);

			// Adjust production based on utilization
			if (producedPercent < HordeSatSharing::UNDER_UTILIZATION_THRESHOLD) {
				// Increase clause production
				this->lbdLimitPerProducer.at(id).fetch_add(1);
				LOG3("[HordeSat] production increase for entity %d.", id);
			} else if (producedPercent > HordeSatSharing::OVER_UTILIZATION_THRESHOLD) {
				// Decrease clause production (one writer, one reader scenario)
				unsigned int currentLimit = this->lbdLimitPerProducer.at(id).load();
				if (currentLimit > 2) {
					this->lbdLimitPerProducer.at(id).store(currentLimit - 1);
					LOG3("[HordeSat] production decrease for entity %d.", id);
				}
			}

			// Reset production for this round
			this->literalsPerProducer.at(id) = 0;
		}
	}
	lock.unlock();

	stats.sharedClauses += selection.size();
	LOGDEBUG3("TotalSize: %ld => selectedClauses: %ld", literalPerRound * m_producers.size(), selection.size());

	// Step 3: Export clauses to clients
	this->exportClauses(selection);

	// Step 4: Clear selection vector
	selection.clear();

	round++;
	LOG2("[HordeSat] received cls %ld, shared cls %ld", stats.receivedClauses.load(), stats.sharedClauses);

	// Check for global termination condition again
	if (globalEnding)
		return true;

	return false;
}