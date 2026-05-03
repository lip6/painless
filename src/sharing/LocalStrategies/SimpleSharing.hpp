#pragma once

#include "sharing/Filters/BloomFilter.hpp"
#include "sharing/SharingStrategy.hpp"

#include <vector>

/**
 * @brief This strategy is a simple sharing strategy using size to filter out
 * long clauses.
 * @ingroup local_sharing
 */
class SimpleSharing
  : public SharingStrategy
{
public:
  /**
   * @brief Constructor for SimpleSharing.
   * @param sizeLimitAtImport The maximum size of allowed clauses at import.
   * @param literalsPerRound The number of literals to export at each round.
   * @param sleepTime The number of microseconds to sleep between two
   * doSharing()'s.
   * @param clauseDB Shared pointer to the clause database.
   * @param clients Vector of shared pointers to client entities.
   */
  SimpleSharing(
    const csize_t sizeLimitAtImport,
    const ulong literalsPerRound,
    const std::chrono::microseconds sleepTime,
    const std::shared_ptr<ClauseDatabase>& clauseDB,
    const std::vector<std::shared_ptr<SharingEntity>>& clients = {});

  SimpleSharing(
    const std::shared_ptr<ClauseDatabase>& clauseDB,
    const std::vector<std::shared_ptr<SharingEntity>>& clients = {});

  /**
   * @brief Destructor for SimpleSharing.
   */
  ~SimpleSharing();

  // SharingEntity Interface
  // =======================
  /**
   * @brief Imports a single clause. Clauses are tested against sizeLimit before
   * @param clause Pointer to the clause to be imported.
   * @return True if the clause was successfully imported, false otherwise.
   */
  bool importClause(const ClauseExchangePtr& clause) override;

  // SharingStrategy Interface
  // =========================

  /**
   * @brief Performs the sharing operation.
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

  /// Clause database where imported clauses are stored.
  std::shared_ptr<ClauseDatabase> m_clauseDB;

  /// Used to manipulate clauses (as a member to reduce number of allocations).
  std::vector<ClauseExchangePtr> m_selection;

  /// Time in microseconds to wait between two consicutive doSharing calls
  std::chrono::microseconds m_sleepTime;

  /// Sharing statistics.
  std::unique_ptr<Statistics> m_stats;

  /// Number of shared literals per round.
  ulong m_literalsPerRound;

  /// Maximum clause size
  csize_t m_sizeLimit;
};