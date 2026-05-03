#ifndef NDIST

#pragma once

#include "sharing/Filters/BloomFilter.hpp"
#include "sharing/SharingStrategy.hpp"
#include "solvers/SolverInterface.hpp"
#include "utils/MpiUtils.hpp"

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
  /// @brief Statistics of a global sharing strategy
  struct Statistics : public SharingStrategy::Statistics
  {
    /// @brief duplicates detected by bloom filter for toSend database
    unsigned long sharedDuplicasAvoided{ 0 };

    /// @brief dpublicates detected by bloom filter for received database
    unsigned long receivedDuplicas{ 0 };

    /// @brief Number of sent messages
    unsigned long messagesSent{ 0 };

    std::string toString() const override
    {
      std::ostringstream oss;
      oss << SharingStrategy::Statistics::toString();
      oss << " -sharedDuplicasAvoided: " << sharedDuplicasAvoided << std::endl;
      oss << " -receivedDuplicas: " << receivedDuplicas << std::endl;
      oss << " -messagesSent: " << messagesSent << std::endl;

      return oss.str();
    }
  };

  /**
   * @brief Constructor for GlobalSharingStrategy.
   * @param clauseDB Shared pointer to the clause database.
   * @param clients Vector of sharing entities that consume clauses.
   * @param sleepTime The number of microseconds to sleep between two
   * doSharing()'s.
   */
  GlobalSharingStrategy(
    const std::chrono::microseconds sleepTime,
    const std::shared_ptr<ClauseDatabase>& clauseDB,
    const std::vector<std::shared_ptr<SharingEntity>>& clients = {})
    : SharingStrategy(clients)
    , m_clauseDB(clauseDB)
    , m_sleepTime(sleepTime)
    , m_gstats(std::make_unique<GlobalSharingStrategy::Statistics>())
  {
  }

  ///  Destructor
  virtual ~GlobalSharingStrategy() {}

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
    LOGD4(
      "Global Strategy %d importing a cls %p", this->getSharingId(), cls.get());
    return m_clauseDB->addClause(cls);
  };

  /**
   * @brief Exports a clause to a specific client.
   * @param clause Pointer to the clause to be exported.
   * @param client Shared pointer to the client receiving the clause.
   * @return True if the clause was successfully exported, false otherwise.
   */
  bool exportClauseToClient(const ClauseExchangePtr& clause,
                            std::shared_ptr<SharingEntity> client)
  {
    LOGD4("Global Strategy %d exports a cls %p to %d",
          this->getSharingId(),
          clause.get(),
          client->getSharingId());
    return client->importClause(clause);
  }

  //===================================================================================
  // SharingStrategy interface
  //===================================================================================

  /// Return the sleepTime attribute of this sharing strategy
  std::chrono::microseconds getSleepingTime() override { return m_sleepTime; }

  /**
   * @brief Return the statistics of this GlobalSharingStrategy via a reference.
   * @return A constant reference to the statistics maintained by this
   * GlobalSharingStrategy
   */
  virtual const Statistics& getStatistics() const override { return *m_gstats; }

protected:
  /// Time in microseconds to wait between two consicutive doSharing calls
  std::chrono::microseconds m_sleepTime;

  /// Global Sharing statistics.
  std::unique_ptr<Statistics> m_gstats;

  /// Clause database where imported clauses are stored.
  std::shared_ptr<ClauseDatabase> m_clauseDB;
};

/**
 * @} // end of global_sharing group
 */

#endif
