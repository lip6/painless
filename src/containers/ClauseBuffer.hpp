#pragma once

#include "containers/ClauseExchange.hpp"
#include "utils/Logger.hpp"
#include "utils/Parameters.hpp"
#include <atomic>
#include <boost/lockfree/policies.hpp>
#include <boost/lockfree/queue.hpp>
#include <vector>
/**
 * @defgroup pl_containers Painless Containers Classes
 * @brief Different clause related data structures
 * @{
*/

/**
 * @class ClauseBuffer
 * @brief Wrapper for boost::lockfree::queue to manage ClauseExchange objects.
 *
 * This class utilizes a non-fixed size boost::lockfree::queue for efficient, lock-free operations between multiple
 * producers and consumers. It uses raw pointers internally and provides a thread-safe interface for ClauseExchangePtr.
 * @warning This class is currently non-copyable.
 * @todo An optimal and safe move/copy mechanism
 */
class ClauseBuffer
{
  private:
	boost::lockfree::queue<ClauseExchange*, boost::lockfree::fixed_sized<false>> queue;
	std::atomic<size_t> m_size; ///< Tracks the number of elements in the queue

  public:
	/**
	 * @brief Default Constructor deleted to enforce parameterized constructor.
	 */
	ClauseBuffer() = delete;

	/**
	 * @brief Constructs a ClauseBuffer with the specified size.
	 * @param size The initial capacity of the queue.
	 */
	explicit ClauseBuffer(size_t size)
		: queue(size)
		, m_size(0)
	{
	}

	/**
	 * @brief Copy constructor (deleted).
	 */
	ClauseBuffer(const ClauseBuffer&) = delete;

	/**
	 * @brief Move constructor (deleted).
	 */
	ClauseBuffer(const ClauseBuffer&&) = delete;

	/**
	 * @brief Assignment operator (deleted).
	 */
	ClauseBuffer& operator=(const ClauseBuffer&) = delete;

	/**
	 * @brief Move constructor.
	 */
	ClauseBuffer(ClauseBuffer&&) = default;

	/**
	 * @brief Move assignment operator.
	 */
	ClauseBuffer& operator=(ClauseBuffer&&) = default;

	/**
	 * @brief Destructor.
	 */
	~ClauseBuffer() { clear(); }

	/**
	 * @brief Adds a single clause to the buffer.
	 * @param clause The clause to add.
	 * @return true if the clause was successfully added, false otherwise.
	 */
	bool addClause(ClauseExchangePtr clause)
	{
		ClauseExchange* raw = clause->toRawPtr();
		if (queue.push(raw)) {
			m_size.fetch_add(1, std::memory_order_release);
			return true;
		} else {
			// Reference count is decremented, since we do not store the returned ClauseExchangePtr, thus it goes out of scope
			ClauseExchange::fromRawPtr(raw);
			return false;
		}
	}

	/**
	 * @brief Adds multiple clauses to the buffer.
	 * @param clauses A vector of clauses to add.
	 * @return The number of clauses successfully added.
	 */
	size_t addClauses(const std::vector<ClauseExchangePtr>& clauses)
	{
		size_t old_size = m_size.load(std::memory_order_relaxed);

		for (const auto& clause : clauses) {
			addClause(clause);
		}

		return m_size.load(std::memory_order_acquire) - old_size;
	}

	/**
	 * @brief Attempts to add a single clause to the buffer using bounded push.
	 *
	 * This method tries to add a clause to the buffer using Boost's bounded_push operation.
	 * If the internal memory pool of the queue is exhausted, the operation will fail.
	 *
	 * @param clause The clause to add to the buffer.
	 * @return true if the clause was successfully added, false if the buffer is full or push failed.
	 */
	bool tryAddClauseBounded(ClauseExchangePtr clause)
	{
		ClauseExchange* raw = clause->toRawPtr();
		if (queue.bounded_push(raw)) {
			m_size.fetch_add(1, std::memory_order_release);
			return true;
		} else {
			ClauseExchange::fromRawPtr(raw);
			return false;
		}
	}

	/**
	 * @brief Attempts to add multiple clauses to the buffer using bounded push.
	 *
	 * This method tries to add clauses to the buffer until either all clauses are added
	 * or a push operation fails (indicating the buffer is full).
	 *
	 * @param clauses A vector of clauses to add to the buffer.
	 * @return The number of clauses successfully added to the buffer.
	 */
	size_t tryAddClausesBounded(const std::vector<ClauseExchangePtr>& clauses)
	{
		size_t old_size = m_size.load(std::memory_order_relaxed);
		for (const auto& clause : clauses) {
			if (!tryAddClauseBounded(clause)) {
				// If a push fails, we've reached the capacity, so we break
				break;
			}
		}
		return m_size.load(std::memory_order_acquire) - old_size;
	}

	/**
	 * @brief Retrieves a single clause from the buffer.
	 * @param[out] clause The retrieved clause.
	 * @return true if a clause was retrieved, false if the buffer was empty.
	 */
	bool getClause(ClauseExchangePtr& clause)
	{
		LOGDEBUG3("Size before pop %ld", this->size());
		ClauseExchange* raw;
		if (queue.pop(raw)) {
			clause = ClauseExchange::fromRawPtr(raw);
			m_size.fetch_sub(1, std::memory_order_release);
			return true;
		}
		return false;
	}

	/**
	 * @brief Retrieves all available clauses from the buffer.
	 * @param[out] clauses A vector to store the retrieved clauses.
	 */
	void getClauses(std::vector<ClauseExchangePtr>& clauses)
	{
		ClauseExchange* raw;
		while (queue.pop(raw)) {
			clauses.push_back(ClauseExchange::fromRawPtr(raw));
			m_size.fetch_sub(1, std::memory_order_release);
		}
	}

	/**
	 * @brief Returns the current number of clauses in the buffer.
	 * @return The number of clauses in the buffer.
	 */
	size_t size() const { return m_size.load(std::memory_order_acquire); }

	/**
	 * @brief Clears all clauses from the buffer.
	 */
	void clear()
	{
		ClauseExchange* raw;
		while (queue.pop(raw)) {
			ClauseExchange::fromRawPtr(raw);
		}
		m_size.store(0, std::memory_order_release);
	}

	/**
	 * @brief Checks if the buffer is empty.
	 * @return true if the buffer is empty, false otherwise.
	 */
	bool empty() const { return m_size.load(std::memory_order_acquire) == 0; }
};

/**
 * @} // end of pl_containers group
 */