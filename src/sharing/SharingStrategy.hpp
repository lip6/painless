#pragma once

#include "SharingEntity.hpp"
#include "containers/ClauseDatabase.hpp"
#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <set>

/**
 * @brief SharingStrategy class, inheriting from SharingEntity. It defines how a
 sharing strategy should behave.
 *
 * @details SharingStrategy serves as a central manager that receives clauses
 from its subscriptions (where this
 * SharingEntity is added as a client), filters them, then send the filtered
 ones to this SharingEntity clients. The
 * exportClauseToClient virtual method is overrided with an implementation that
 makes sure to not export a clause to its
 * producer. Since this SharingEntity can be a client to one it clients
 (Biderectionnal subscription This <-> Other).

 *
 * @ingroup sharing
 */
class SharingStrategy : public SharingEntity, public Configurable
{
public:
  /// @brief Local Sharing statistics.
  /// \ingroup sharing
  /// Sharing statistics.
  struct Statistics
  {
    /**
     * @brief Stringify the different statistics.
     * @return std::string containing the stats
     */
    virtual std::string toString() const
    {
      std::ostringstream oss;
      oss << "Strategy Basic Stats:" << std::endl;
      oss << " -receivedCls: " << receivedClauses.load() << std::endl;
      oss << " -sharedCls: " << sharedClauses << std::endl;
      oss << " -filteredAtImport: " << filteredAtImport.load() << std::endl;

      return oss.str();
    }

    /// Number of shared clauses that have been shared.
    unsigned long sharedClauses{ 0 };

    /// Number of shared clauses received from subscriptions (producers).
    std::atomic<unsigned long> receivedClauses{ 0 };

    /// Number of clause filtered at import
    std::atomic<unsigned long> filteredAtImport{ 0 };
  };

  /**
   * @brief Constructor for SharingStrategy.
   * @param clients A vector of shared pointers to SharingEntity objects that
   * can consume clauses.
   */
  SharingStrategy(const std::vector<std::shared_ptr<SharingEntity>>& clients)
    : SharingEntity(clients)
  {
  }

  /**
   * @brief Virtual destructor.
   */
  virtual ~SharingStrategy() {}

  /**
   * @brief Executes the sharing process by filtering imported clauses before
   * exporting them to this SharingEntity cliens.
   * @return True if the sharing didn't encounter an error, false otherwise.
   */
  virtual bool doSharing() = 0;

  /**
   * @brief Determines the sleeping time for the sharer.
   * @return Microseconds to sleep.
   */
  virtual std::chrono::microseconds getSleepingTime() = 0;

  /**
   * @brief Return the statistics of this SharingStrategy via a reference to
   * support different subclasses.
   * @return A constant reference to the statistics maintained by this
   * SharingStrategy
   */
  virtual const Statistics& getStatistics() const = 0;

protected:
  /**
   * @brief A SharingStrategy doesn't send a clause to the source client (->from
   * must store the sharingId of its producer)
   */
  bool exportClauseToClient(const ClauseExchangePtr& clause,
                            std::shared_ptr<SharingEntity> client) override
  {
    if (clause->from != client->getSharingId())
      return client->importClause(clause);
    else
      return false;
  }
};