#pragma once

#include "config/Configurable.hpp"
#include "containers/ClauseExchange.hpp"

#include <atomic>
#include <memory>
#include <numeric>
#include <vector>

/**
 * @defgroup pl_containers_db Clause Databases
 * @brief Different ClauseDatabase implementations
 * @ingroup pl_containers
 * @{
 */

/**
 * @class ClauseDatabase
 * @brief Abstract base class defining the interface for clause storage and
 * management.
 *
 * This class provides a common interface for different implementations of
 * clause databases. It allows for adding, retrieving, and managing clauses
 * using a specific logic.
 *
 * @warning **Thread-safety contract:** All implementations of this interface
 * MUST be thread-safe. The clause database is accessed concurrently by
 * multiple producer threads (importing clauses via @ref addClause) and may be
 * accessed by one or more consumer threads (draining via @ref giveSelection,
 * @ref getOneClause, @ref getClauses, @ref shrinkDatabase, or
 * @ref clearDatabase). Implementations are responsible for providing the
 * necessary internal synchronization (locks, lock-free structures, etc.) to
 * guarantee correctness under concurrent access.
 *
 * @warning **Atomicity of @ref getSize :** @ref getSize must return a value
 * that is consistent at the moment of the call. Callers should be aware that
 * the returned size is a snapshot and may become stale immediately after the
 * call returns. Composite check-then-act patterns built on @ref getSize (for
 * example, "if size < N then addClause") are NOT atomic at the interface
 * level and may exceed bounds under concurrent producers. Implementations
 * that need to enforce a hard capacity must do so internally within
 * @ref addClause (e.g., by rejecting clauses once the bound is reached).
 *
 * @note Implementations should document any additional ordering or
 * progress guarantees they provide beyond basic thread-safety (e.g.,
 * lock-free progress, FIFO ordering, fairness between producers).
 */
class ClauseDatabase : public Configurable
{
public:
  /// @brief Default Constructor.
  ClauseDatabase() {}

  /// @brief Virtual destructor to ensure proper cleanup of derived classes.
  virtual ~ClauseDatabase() {}

  /**
   * @brief Add a clause to the database.
   * @param clause Shared pointer to the clause to be added.
   * @return true if the clause was successfully added, false otherwise
   * (e.g., the database is at capacity or the implementation chose to
   * reject the clause for any other reason).
   *
   * @warning Implementations MUST be safe to call concurrently from multiple
   * producer threads, and concurrently with any consumer-side method
   * (@ref giveSelection, @ref getOneClause, @ref getClauses,
   * @ref shrinkDatabase, @ref clearDatabase). Any capacity enforcement must
   * be performed atomically inside this call; callers cannot reliably
   * pre-check capacity via @ref getSize due to TOCTOU.
   */
  virtual bool addClause(ClauseExchangePtr clause) = 0;

  /**
   * @brief Fill the given buffer with a selection of clauses.
   * @param selectedCls Vector to be filled with selected clauses.
   * @param literalCountLimit The maximum number of literals to be selected.
   * @return The number of literals in the selected clauses.
   *
   * @warning Implementations MUST be safe to call concurrently with
   * @ref addClause from producer threads. Concurrent calls to this method
   * itself (multiple consumers) can be possible if supported by the
   * implementation. It should be explicitly documented; otherwise this method
   * assumes a single consumer.
   */
  virtual size_t giveSelection(std::vector<ClauseExchangePtr>& selectedCls,
                               unsigned int literalCountLimit) = 0;

  /**
   * @brief Retrieve all clauses from the database.
   * @param v_cls Vector to be filled with all clauses in the database.
   *
   * @warning Implementations MUST be safe to call concurrently with
   * @ref addClause. The returned set is a snapshot; clauses added during
   * or after the call may or may not be included. There shouldn't be a data
   * race between multiple consumers. The first one should drain the full
   * database.
   */
  virtual void getClauses(std::vector<ClauseExchangePtr>& v_cls) = 0;

  /**
   * @brief Retrieve a single clause from the database.
   * @param cls Reference to a shared pointer where the selected clause will
   * be stored.
   * @return true if a clause was retrieved, false if the database is empty.
   *
   * @warning Implementations MUST be safe to call concurrently with
   * @ref addClause from producer threads. Concurrent calls to this method
   * itself (multiple consumers) can be possible if supported by the
   * implementation. It should be explicitly documented; otherwise this method
   * assumes a single consumer.
   */
  virtual bool getOneClause(ClauseExchangePtr& cls) = 0;

  /**
   * @brief Get the current number of clauses in the database.
   * @return The number of clauses currently stored in the database.
   *
   * @warning The returned value is a point-in-time snapshot and may be
   * stale by the time the caller observes it. Do NOT use this value to
   * gate calls to @ref addClause or @ref getOneClause as a correctness
   * mechanism — such check-then-act patterns are racy. Use it only for
   * statistics, heuristics, or advisory throttling.
   *
   * @warning Implementations MUST be safe to call concurrently with all
   * other methods on this interface.
   */
  virtual size_t getSize() const = 0;

  /**
   * @brief Reduce the size of the database by removing some clauses.
   * @return The number of literals removed from the database.
   *
   * @warning Implementations MUST be safe to call concurrently with
   * @ref addClause from producer threads. Concurrent invocation with other
   * consumer-side methods (@ref giveSelection, @ref getOneClause,
   * @ref getClauses, @ref clearDatabase) is only supported if the
   * implementation explicitly documents it.
   */
  virtual size_t shrinkDatabase() = 0;

  /**
   * @brief Remove all clauses from the database.
   *
   * @warning Implementations MUST be safe to call concurrently with
   * @ref addClause from producer threads. Clauses added concurrently with
   * this call may or may not be removed, depending on the interleaving.
   * Concurrent invocation with other consumer-side methods is only
   * supported if the implementation explicitly documents it.
   */
  virtual void clearDatabase() = 0;
};

/**
 * @}
 */