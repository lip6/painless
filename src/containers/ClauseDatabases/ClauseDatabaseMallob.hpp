#pragma once

#include "containers/ClauseBuffer.hpp"
#include "containers/ClauseDatabase.hpp"
#include <atomic>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#define MIN_LBD 2
#define UNIT_SIZE 1

/**
 * @class ClauseDatabaseMallob
 * @brief A clause database implementation attempting to naively mimic the Simplified Adaptive Database from Mallob
 * (ref:https://doi.org/10.1613/jair.1.15827).
 *
 * This class implements a clause database that organizes clauses based on their size and LBD (Literal Block Distance).
 * It uses a vector of ClauseBuffers to store clauses, with each buffer corresponding to a specific size and LBD range.
 * The implementation aims to provide efficient clause storage, retrieval, and management while maintaining a
 * balance between clause quality and database size.
 *
 * Key features:
 * - Clauses are partitioned based on size and LBD.
 * - Supports concurrent additions with lock-free mechanisms for unit clauses.
 * - Implements a shrinking mechanism to maintain the database size within capacity.
 *
 * @ingroup pl_containers_db
 *
 * @todo fix the lbd partitioning to not have empty vectors, worth it ?
 */
class ClauseDatabaseMallob : public ClauseDatabase
{
  public:
	ClauseDatabaseMallob() = delete;

	/**
	 * @brief Constructs a new ClauseDatabaseMallob object.
	 *
	 * @param maxClauseSize Maximum size of clauses to be stored.
	 * @param maxPartitioningLbd Maximum LBD value for separate partitioning.
	 * @param maxCapacity Maximum total literal capacity of the database.
	 * @param maxFreeSize Maximum size for which giveSelection does not decrement the literal count while filling the
	 * export buffer.
	 * @throw std::invalid_argument if any parameter is invalid.
	 */
	explicit ClauseDatabaseMallob(int maxClauseSize, int maxPartitioningLbd, size_t maxCapacity, int maxFreeSize);

	/**
	 * @brief Virtual destructor.
	 */
	~ClauseDatabaseMallob() override;

	/**
	 * @brief Adds a clause to the database.
	 *
	 * This method attempts to add a clause to the appropriate buffer based on its size and LBD.
	 * It uses shared locks for synchronizing the m_currentLiteralCounts and the other class atributes with
	 * shrinkDatabase. Units are always added, capacity is ignored for them.
	 *
	 * @param clause pointer to the clause to be added.
	 * @return true if the clause was successfully added, false otherwise.
	 */
	bool addClause(ClauseExchangePtr clause) override;

	/**
	 * @brief Fills a buffer with selected clauses up to a given size limit.
	 *
	 * This method selects clauses from the database, prioritizing unit clauses and then
	 * iterating through the clause buffers to fill the selection up to the specified limit.
	 *
	 * @param selectedCls Vector to be filled with selected clauses.
	 * @param literalCountLimit Maximum number of literals to select.
	 * @return Number of literals in the selected clauses.
	 */
	size_t giveSelection(std::vector<ClauseExchangePtr>& selectedCls, unsigned int literalCountLimit) override;

	/**
	 * @brief Fills a buffer with all clauses in the database.
	 *
	 * @param v_cls Vector to be filled with all clauses.
	 */
	void getClauses(std::vector<ClauseExchangePtr>& v_cls) override;

	/**
	 * @brief Retrieves a single clause from the best (smallest index) buffer possible.
	 *
	 * This method attempts to retrieve a clause from any non-empty buffer in the database.
	 *
	 * @param cls Reference to store the retrieved clause.
	 * @return true if a clause was found, false if the database is empty.
	 */
	bool getOneClause(ClauseExchangePtr& cls) override;

	/**
	 * @brief Gets the total number of clauses in the database.
	 *
	 * @return Number of clauses in the database.
	 */
	size_t getSize() const override;

	/**
	 * @brief Shrinks the database by removing clauses to maintain the size within capacity.
	 *
	 * This method removes clauses from the worst (highest index) buffers until the
	 * database size is within the specified capacity. However it starts by adding
	 * missed clauses addition due to the previous shrink unique_lock. Unit clauses are never removed
	 *
	 * @return Number of clauses removed during shrinking.
	 */
	size_t shrinkDatabase() override;

	/**
	 * @brief Clears all clauses from the database and resets internal counters.
	 */
	void clearDatabase() override;

  protected:
	/**
	 * @brief Calculates the index for a clause based on its size and LBD.
	 *
	 * This function computes an index in the clauses vector based on the clause's size and LBD.
	 * It uses m_maxPartitioningLbd to determine the partitioning of LBD values.
	 *
	 * @param size Size of the clause (must be >= UNIT_SIZE).
	 * @param lbd LBD (Literal Block Distance) of the clause (must be >= MIN_LBD).
	 * @return Index in the clauses vector.
	 * @throw std::invalid_argument if size is less than UNIT_SIZE or lbd is less than MIN_LBD.
	 *
	 * Index calculation:
	 * - For each size, there are m_maxPartitioningLbd possible indices.
	 * - LBD values >= (MIN_LBD + m_maxPartitioningLbd) are grouped together in the last partition.
	 *
	 * Example (assuming m_maxPartitioningLbd == 2):
	 * - idx 0: size = 1, lbd = 2
	 * - idx 1: size = 1, lbd >= 3 (never used)
	 * - idx 2: size = 2, lbd = 2
	 * - idx 3: size = 2, lbd >= 3 (never used)
	 * - idx 4: size = 3, lbd = 2
	 * - idx 5: size = 3, lbd >= 3
	 * ...
	 */
	inline unsigned getIndex(int size, int lbd) const
	{
		if (size < UNIT_SIZE || lbd < MIN_LBD)
			throw std::invalid_argument("size is less than 1 or lbd is less than 2");
		return (size - 1) * m_maxPartitioningLbd + std::min(lbd - MIN_LBD, m_maxPartitioningLbd - 1);
	}

	/**
	 * @brief Calculates the size of a clause from its index.
	 *
	 * @param index Index in the clauses vector.
	 * @return Size of the clause.
	 */
	inline int getSizeFromIndex(unsigned index) const { return (index == 0) ? 1 : (index / m_maxPartitioningLbd) + 1; }

	/**
	 * @brief Calculates the LBD partition from an index.
	 *
	 * @param index Index in the clauses vector.
	 * @return LBD partition.
	 */
	inline int getLbdPartitionFromIndex(unsigned index) const { return (index % m_maxPartitioningLbd) + MIN_LBD; }

	const size_t m_totalLiteralCapacity; ///< Maximum total literal capacity of the database.
	const int m_maxPartitioningLbd;		 ///< Maximum LBD value for separate partitioning.
	const int m_maxClauseSize;			 ///< Maximum size of clauses to be stored.
	const int m_freeMaxSize; ///< Maximum size for which giveSelection does not count in while filling exportBuffer.

	std::vector<std::unique_ptr<ClauseBuffer>> m_clauses; ///< Vector of clause buffers, indexed by size and LBD.
	std::atomic<long> m_currentLiteralSize;	 ///< Current number of literals in the database (excluding unit clauses).
	std::atomic<int> m_currentWorstIndex;	 ///< Index of the current worst clause in the database.
	mutable std::shared_mutex m_shrinkMutex; ///< Mutex used to coordinate shrinking and clause addition.
	ClauseBuffer m_missedAdditionsBfr;
};