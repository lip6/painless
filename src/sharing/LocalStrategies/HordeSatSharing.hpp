#pragma once

#include "sharing/Filters/BloomFilter.hpp"
#include "sharing/SharingStrategy.hpp"

#include <atomic>
#include <unordered_map>
#include <vector>

/**
 * @defgroup local_sharing Intra-Process Sharing Strategies
 * @ingroup sharing
 * @brief Different Classes for Sharing clauses between different threads in the same process
 * @{
 */

/**
 * @brief This strategy is a HordeSat-like sharing strategy.
 * @todo Strengthening strategy + keep units for future ones in derived classes?
 */
class HordeSatSharing : public SharingStrategy
{
  public:
	/**
	 * @brief Constructor for HordeSatSharing.
	 * @param clauseDB Shared pointer to the clause database.
	 * @param producers Vector of shared pointers to producer entities.
	 * @param consumers Vector of shared pointers to consumer entities.
	 * @param literalsPerProducerPerRound Number of literals a producer should export to this strategy per round
	 * @param initialLbdLimit The initial value of the maximum allowed lbd value for a given producer
	 * @param roundsBeforeLbdIncrease The number of rounds to wait before updating the lbd limit of the producers
	 */
	HordeSatSharing(const std::shared_ptr<ClauseDatabase>& clauseDB,
					unsigned long literalsPerProducerPerRound,
					unsigned int  initialLbdLimit,
					unsigned int  roundsBeforeLbdIncrease,
					const std::vector<std::shared_ptr<SharingEntity>>& producers = {},
					const std::vector<std::shared_ptr<SharingEntity>>& consumers = {});

	/**
	 * @brief Destructor for HordeSatSharing.
	 */
	~HordeSatSharing();

	// SharingEntity Interface
	// =======================
	/**
	 * @brief Imports a single clause if the lbd limit is respected. And updates the amount of literals exprted by the
	 * producer for the coming round.
	 * @param clause Pointer to the clause to be imported.
	 * @return True if the clause was successfully imported, false otherwise.
	 */
	bool importClause(const ClauseExchangePtr& clause) override;

	/**
	 * @brief Imports multiple clauses.
	 * @param v_clauses Vector of clause pointers to be imported.
	 */
	void importClauses(const std::vector<ClauseExchangePtr>& v_clauses) override
	{
		for (auto clause : v_clauses)
			importClause(clause);
	}

	// SharingStrategy Interface
	// =========================

	/**
	 * @brief Performs the sharing operation. It checks the literal production to decide if the lbd limit should be
	 * increased, decreased or unchanged. The selection of clauses from the database is exported to all consumers
	 * @return True if sharing is complete or should be terminated, false otherwise.
	 */
	bool doSharing() override;

  protected:
	/**
	 * @brief Adds a producer to the sharing strategy. It initializes the lbd limit and the per round literal production
	 * for the producer.
	 * @param producer Shared pointer to the producer entity to be added.
	 */
	void addProducer(std::shared_ptr<SharingEntity> producer) override
	{
		SharingStrategy::addProducer(producer);
		/* lock m_producerMutex is released */

		this->lbdLimitPerProducer.emplace(producer->getSharingId(), initialLbdLimit);
		this->literalsPerProducer.emplace(producer->getSharingId(), 0);
	}

	/**
	 * @brief Removes a producer from the sharing strategy.
	 * @param producer Shared pointer to the producer entity to be removed.
	 */
	void removeProducer(std::shared_ptr<SharingEntity> producer) override
	{
		SharingStrategy::removeProducer(producer);
		/* lock m_producer is released */
		this->lbdLimitPerProducer.erase(producer->getSharingId());
		this->literalsPerProducer.erase(producer->getSharingId());
	}

  protected:
	/// Number of shared literals per round.
	unsigned long literalPerRound;

	/// @brief Initial lbd value for per producer filter
	unsigned int initialLbdLimit;

	/// Number of rounds before forcing an increase in production
	unsigned int roundBeforeIncrease;

	/// @brief Round Number
	int round;

	/// Used to manipulate clauses (as a member to reduce number of allocations).
	std::vector<ClauseExchangePtr> selection;

	// Static Constants
	// ----------------

	static constexpr int UNDER_UTILIZATION_THRESHOLD = 75;
	static constexpr int OVER_UTILIZATION_THRESHOLD = 98;

	// Data accessible from other threads via importClause
	// ---------------------------------------------------

	/// SharingEntity id to lbdLimit
	std::unordered_map<unsigned int, std::atomic<unsigned int>> lbdLimitPerProducer;

	/// Metadata for clause database
	std::unordered_map<unsigned int, std::atomic<unsigned long>> literalsPerProducer;
};

/**
 * @} // end of local_sharing group
 */