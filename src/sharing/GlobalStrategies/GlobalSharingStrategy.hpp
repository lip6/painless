#pragma once

#include "sharing/Filters/BloomFilter.hpp"
#include "sharing/SharingStatistics.hpp"
#include "sharing/SharingStrategy.hpp"

#include "solvers/SolverInterface.hpp"

#include <mpi.h>

/**
 * @defgroup global_sharing Inter-Process Sharing Strategies
 * @ingroup sharing
 * @brief Different Classes for Sharing clauses between different processes
 * @{
 */


/**
 * @class GlobalSharingStrategy
 * @brief Base class for global clause sharing strategies across MPI processes.
 *
 * This class provides the interface and common functionality for implementing
 * various inter-MPI process clause sharing strategies.
 */
class GlobalSharingStrategy : public SharingStrategy
{

  public:
	/**
	 * @brief Constructor for GlobalSharingStrategy.
	 * @param clauseDB Shared pointer to the clause database.
	 * @param producers Vector of sharing entities that produce clauses.
	 * @param consumers Vector of sharing entities that consume clauses.
	 */
	GlobalSharingStrategy(const std::shared_ptr<ClauseDatabase>& clauseDB,
						  const std::vector<std::shared_ptr<SharingEntity>>& producers = {},
						  const std::vector<std::shared_ptr<SharingEntity>>& consumers = {});

	///  Destructor
	virtual ~GlobalSharingStrategy();

	//===================================================================================
	// SharingEntity interface (save in m_clauseDB)
	//===================================================================================
	/**
	 * @brief Imports a clause into the clause database.
	 * @param cls Pointer to the clause to be imported.
	 * @return True if the clause was successfully imported, false otherwise.
	 */
	bool importClause(const ClauseExchangePtr& cls) override
	{
		LOGDEBUG3("Global Strategy %d importing a cls %p", this->getSharingId(), cls.get());
		return m_clauseDB->addClause(cls);
	};

	/**
	 * @brief Imports multiple clauses into the clause database.
	 * @param v_cls Vector of clauses to be imported.
	 */
	void importClauses(const std::vector<ClauseExchangePtr>& v_cls) override
	{
		for (auto& cls : v_cls)
			importClause(cls);
	}

	/**
	 * @brief Exports a clause to a specific client.
	 * @param clause Pointer to the clause to be exported.
	 * @param client Shared pointer to the client receiving the clause.
	 * @return True if the clause was successfully exported, false otherwise.
	 */
	bool exportClauseToClient(const ClauseExchangePtr& clause, std::shared_ptr<SharingEntity> client)
	{
		LOGDEBUG3(
			"Global Strategy %d exports a cls %p to %d", this->getSharingId(), clause.get(), client->getSharingId());
		return client->importClause(clause);
	}

	//===================================================================================
	// GlobalSharingStrategy interface
	//===================================================================================

	/**
	 * @brief Gets the sleeping time for the sharing strategy.
	 * @return The sleeping time in microseconds.
	 */
	std::chrono::microseconds getSleepingTime() override;

	/**
	 * @brief Handles the process of joining when a solution is found.
	 * @param winnerRank The rank of the process that found the solution.
	 * @param res The result of the SAT solving process.
	 * @param model The satisfying assignment, if any.
	 */
	virtual void joinProcess(int winnerRank, SatResult res, const std::vector<int>& model);

	/**
	 * @brief Prints the statistics of the sharing strategy.
	 */
	void printStats() override;

	/**
	 * @brief Initializes MPI-related variables.
	 * @return True if initialization was successful, false otherwise.
	 * @warning Be careful to call this only once per instance.
	 */
	virtual bool initMpiVariables();

	/**
	 * @brief Performs the clause sharing operation.
	 * @return True if sharing is complete and the process can terminate, false otherwise.
	 * @warning Winner's rank must be smaller 2^16 - 1. See the end of doSharing
	 */
	virtual bool doSharing() override;

  protected:
	GlobalSharingStatistics gstats; ///< Statistics for global sharing
	bool requests_sent; ///< Flag indicating if requests to end were sent to the root
	std::vector<MPI_Request> recv_end_requests; ///< MPI requests for non-blocking receive of end signals
	MPI_Request send_end_request;				///< MPI request for non-root Isend for final synchronization with root
	std::vector<int> receivedFinalResultRoot; ///< Buffer for storing results received from other mpi processes
};

/**
 * @} // end of global_sharing group
 */
