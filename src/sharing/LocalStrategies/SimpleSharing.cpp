#include "SimpleSharing.hpp"

#include "painless.hpp"
#include "solvers/SolverFactory.hpp"
#include "utils/Logger.hpp"
#include "utils/Parameters.hpp"

SimpleSharing::SimpleSharing(const std::shared_ptr<ClauseDatabase>& clauseDB,
							 unsigned int sizeLimitAtImport,
							 unsigned long literalsPerRoundPerProducer,
							 const std::vector<std::shared_ptr<SharingEntity>>& producers,
							 const std::vector<std::shared_ptr<SharingEntity>>& consumers)
	: SharingStrategy(producers, consumers, clauseDB)
	, sizeLimit(sizeLimitAtImport)
	, literalPerRound(literalsPerRoundPerProducer)
{
	LOGSTAT("[Simple] Producers: %d, Consumers: %d.", m_producers.size(), getClientCount());
}

SimpleSharing::~SimpleSharing() {}

bool
SimpleSharing::importClause(const ClauseExchangePtr& clause)
{
	assert(clause->size > 0 && clause->from != -1);

	int id = clause->from;

	LOGDEBUG3("Solver %d: Clause with size %d is tested against limit %d", id, clause->size, this->sizeLimit);

	if (clause->size <= this->sizeLimit) {
		this->stats.receivedClauses++;
		return m_clauseDB->addClause(clause);
	} else {
		this->stats.filteredAtImport++;
		return false;
	}
}

bool
SimpleSharing::doSharing()
{
	if (globalEnding)
		return true;

	// 1- Get selection
	// consumer receives the same amount as in the original
	this->m_clauseDB->giveSelection(selection, literalPerRound * m_producers.size());

	stats.sharedClauses += selection.size();

	// 2-Send the best clauses (all producers included) to all consumers
	this->exportClauses(selection);

	LOGDEBUG2("TotalSize: %ld => selectedClauses: %ld, DB size: %ld",
			  literalPerRound * m_producers.size(),
			  selection.size(),
			  this->m_clauseDB->getSize());

	selection.clear();

	/* empty database to limit growth */
	this->m_clauseDB->clearDatabase();

	LOG1("[SimpleShr] received cls %ld, shared cls %ld", stats.receivedClauses.load(), stats.sharedClauses);

	if (globalEnding)
		return true;
	return false;
}