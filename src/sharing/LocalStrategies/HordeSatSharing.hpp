#pragma once

#include "sharing/Filters/BloomFilter.hpp"
#include "sharing/SharingStrategy.hpp"

#include <atomic>
#include <unordered_map>
#include <vector>

/**
 * @defgroup local_sharing Intra-Process Sharing Strategies
 * @ingroup sharing
 * @brief Different Classes for Sharing clauses between different threads in the
 * same process
 * @{
 */

/**
 * @brief This strategy is a HordeSat-like sharing strategy.
 * @details This implementation needs to maintain statistics per producer, thus
 * the list in the constructor and the addProducer method. Only the maps are
 * initialized. In general, the strategy tries to maintain a certain literal
 * exportation rate.
 * @warning This strategy needs to initialize the producers related data before
 * adding this SharingEntity as client to the producers.
 * @todo Strengthening strategy + keep units for future ones in derived classes?
 */
class HordeSatSharing : public SharingStrategy
{
public:
  /**
   * @brief Constructor for HordeSatSharing.
   * @param literalsPerProducerPerRound Number of literals a producer should
   * export to this strategy per round.
   * @param initialLbdLimit The initial value of the maximum allowed lbd value
   * for a given producer.
   * @param roundsBeforeLbdIncrease The number of rounds to wait before updating
   * the lbd limit of the producers.
   * @param sleepTime The number of microseconds to sleep between two
   * doSharing()'s.
   * @param clauseDB Shared pointer to the clause database.
   * @param clients Vector of shared pointers to client entities.
   */
  HordeSatSharing(
    const uint producerCount,
    const ulong literalsPerProducerPerRound,
    const lbd_t initialLbdLimit,
    const uint roundsBeforeLbdIncrease,
    const std::chrono::microseconds sleepTime,
    const std::shared_ptr<ClauseDatabase>& clauseDB,
    const std::vector<std::shared_ptr<SharingEntity>>& clients = {});

  HordeSatSharing(
    const std::shared_ptr<ClauseDatabase>& clauseDB,
    const std::vector<std::shared_ptr<SharingEntity>>& clients = {});

  /**
   * @brief Destructor for HordeSatSharing.
   */
  ~HordeSatSharing();

  // SharingEntity Interface
  // =======================
  /**
   * @brief Imports a single clause if the lbd limit is respected. And updates
   * the amount of literals exprted by the producer for the coming round.
   * @param clause Pointer to the clause to be imported.
   * @return True if the clause was successfully imported, false otherwise.
   */
  bool importClause(const ClauseExchangePtr& clause) override;

  // SharingStrategy Interface
  // =========================

  /**
   * @brief Performs the sharing operation. It checks the literal production to
   * decide if the lbd limit should be increased, decreased or unchanged. The
   * selection of clauses from the database is exported to all clients
   * @return True if the sharing didn't encounter an error, false otherwise.
   */
  bool doSharing() override;

  /**
   * @brief Return the sleepTime attribute of this sharing strategy
   */
  std::chrono::microseconds getSleepingTime() override { return m_sleepTime; }

  const Statistics& getStatistics() const { return *m_stats; }

protected:
  void setOption(const std::string& key, int value) override;
  void setOption(const std::string& key, double value) override;
  void setOption(const std::string& key, const std::string& value) override;
  bool onConfigured() override;

  /// Time in microseconds to wait between two consicutive doSharing calls
  std::chrono::microseconds m_sleepTime;

  /// Clause database where imported clauses are stored.
  std::shared_ptr<ClauseDatabase> m_clauseDB;

  /// Used to manipulate clauses (as a member to reduce number of allocations).
  std::vector<ClauseExchangePtr> m_selection;

  /// Number of shared literals per producer per round.
  ulong m_literalsPerProducerPerRound;

  /// Sharing statistics.
  std::unique_ptr<Statistics> m_stats;

  /// Initial lbd value for per producer filter
  lbd_t m_initialLbdLimit;

  /// Number of rounds before forcing an increase in production
  uint m_roundsBeforeIncrease;

  /// Round Number
  uint m_round;

  std::vector<double> m_producerMeanLbd;

  // Static Constants
  // ----------------

  static constexpr int UNDER_UTILIZATION_THRESHOLD = 75;
  static constexpr int OVER_UTILIZATION_THRESHOLD = 98;

  uint m_producerCount;

  std::string m_producersList;

  // Data accessible from other threads via importClause
  // ---------------------------------------------------

  /// Producers' lbdLimit
  std::unique_ptr<std::atomic<uint>[]> m_lbdLimitPerProducer;

  /// Producers' production
  std::unique_ptr<std::atomic<ulong>[]> m_literalsPerProducer;
};

/**
 * @} // end of local_sharing group
 */