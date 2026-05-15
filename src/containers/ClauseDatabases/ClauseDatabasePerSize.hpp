#pragma once

#include "containers/ClauseBuffer.hpp"
#include "containers/ClauseDatabase.hpp"

#include <atomic>
#include <vector>

/**
 * @class ClauseDatabasePerSize
 * @brief A clause database that organizes clauses based on their size.
 *
 * This class implements the ClauseDatabase interface, storing clauses in
 * separate buffers based on their size.
 * 
 * Cannot change the number of buckets (clause buffers) concurrently.
 * ClauseBuffers are boost MPMC queue buffers.
 * 
 *
 * @ingroup pl_containers_db
 * @todo resize by changing maxClauseSize for dynamically managing the maximum
 * size
 */
class ClauseDatabasePerSize
  : public ClauseDatabase
{
public:
  /**
   * @brief Default constructor deleted to enforce use of parameterized
   * constructor.
   */
  ClauseDatabasePerSize();

  /**
   * @brief Constructor with a specified maximum clause size.
   * @param maxClauseSize The maximum size of clauses to be stored.
   */
  explicit ClauseDatabasePerSize(int maxClauseSize);

  /**
   * @brief Virtual destructor.
   */
  ~ClauseDatabasePerSize();

  /**
   * @brief Adds a clause to the appropriate size-based buffer. The number of buffers is static. The buffers are lock free
   * @param clause The clause to be added.
   * @return true if the clause was successfully added, false otherwise.
   */
  bool addClause(ClauseExchangePtr clause) override;

  /**
   * @brief Selects clauses up to a specified total size.
   * @param selectedCls Vector to store the selected clauses.
   * @param literalCountLimit The maximum literals count to select.
   * @return The number of literals in the selected clauses.
   */
  size_t giveSelection(std::vector<ClauseExchangePtr>& selectedCls,
                       unsigned int literalCountLimit) override;

  /**
   * @brief Retrieves all clauses from all size-based buffers.
   * @param v_cls Vector to store the retrieved clauses.
   */
  void getClauses(std::vector<ClauseExchangePtr>& v_cls) override;

  /**
   * @brief Retrieves the smallest clause from the database.
   * @param cls Reference to store the retrieved clause.
   * @return true if a clause was retrieved, false if all buffers are empty.
   */
  bool getOneClause(ClauseExchangePtr& cls) override;

  /**
   * @brief Gets the total number of clauses across all size-based buffers.
   * @return The total number of clauses.
   */
  size_t getSize() const override;

  /**
   * @brief Does nothing in this implementation.
   * @return The 0 value.
   */
  size_t shrinkDatabase() override
  {
    LOGD4("This does nothing!");
    return 0;
  };

  /**
   * @brief Clears all size-based buffers.
   */
  void clearDatabase() override;

private:
  void setOption(const std::string& key, int value) override;
  bool onConfigured() override;

private:
  /**
   * @brief Vector of clause buffers, one for each possible clause size.
   */
  std::vector<std::unique_ptr<ClauseBuffer>> m_clausesPerSize;
  std::mutex m_consumeMX;
  std::atomic<size_t> m_size;
  /**
   * @brief The maximum clause size accepted in this clause database.
   */
  int m_maxClauseSize;
};