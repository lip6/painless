#pragma once

#include <algorithm>
#include <atomic>
#include <boost/intrusive_ptr.hpp>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <vector>

/// Simple clause definition
using simpleClause = std::vector<int>;

// Forward declaration of ClauseExchange
class ClauseExchange;

/**
 * @typedef ClauseExchangePtr
 * @brief Type alias for the smart pointer used to manage ClauseExchange objects.
 *
 * This alias allows for easy switching between different smart pointer implementations
 * or raw pointers in the future, if needed.
 */
using ClauseExchangePtr = boost::intrusive_ptr<ClauseExchange>;

/**
 * @class ClauseExchange
 * @brief Represents an exchangeable clause with flexible array member.
 *
 *
 * This class provides a memory-efficient way to store and manage clauses
 * of varying sizes. It uses a flexible array member for storing the actual
 * clause data and supports reference counting through boost::intrusive_ptr.
 * 
 * @ingroup pl_containers
 *
 * @todo Template for metadata for better memory footprint.
 * @warning If the size of the clause is greater than 1 the lbd is forced to at least 2
 */
class ClauseExchange
{
  public:
	unsigned int lbd;					  ///< Literal Block Distance (LBD) of the clause
	int from;							  ///< Source identifier of the clause
	unsigned int size;					  ///< Size of the clause
	std::atomic<unsigned int> refCounter; ///< Counter for intrusive_ptr copies and raw pointer conversions
	int lits[0];						  ///< Flexible array member for storing clause literals (must be last)

	/**
	 * @brief Create a new ClauseExchange object.
	 * @param lbd Literal Block Distance of the clause (default: 0).
	 * @param from Source identifier of the clause (default: -1).
	 * @return ClauseExchangePtr Smart pointer to the created object.
	 * @throw std::bad_alloc If memory allocation fails.
	 */
	static ClauseExchangePtr create(const unsigned int size, const unsigned int lbd = 0, const int from = -1);

	/**
	 * @brief Create a new ClauseExchange object using pointers.
	 * @param begin Start of integer data.
	 * @param end End of integer data.
	 * @param lbd Literal Block Distance of the clause (default: 0).
	 * @param from Source identifier of the clause (default: -1).
	 * @return ClauseExchangePtr Smart pointer to the created object.
	 * @throw std::bad_alloc If memory allocation fails.
	 */
	static ClauseExchangePtr create(const int* begin, const int* end, const unsigned int lbd = 0, const int from = -1);

	/**
	 * @brief Create a new ClauseExchange object from a vector of literals.
	 * @param v_cls Vector containing the clause literals.
	 * @param lbd Literal Block Distance of the clause (default: 0).
	 * @param from Source identifier of the clause (default: -1).
	 * @return ClauseExchangePtr Smart pointer to the created object.
	 * @throw std::bad_alloc If memory allocation fails.
	 */
	static ClauseExchangePtr create(const std::vector<int>& v_cls, const unsigned int lbd = 0, const int from = -1);

	/**
	 * @brief Destructor.
	 */
	~ClauseExchange() = default;

	/**
	 * @brief Access clause literal by index.
	 * @param index Index of the literal to access.
	 * @return Reference to the literal at the specified index.
	 */
	int& operator[](unsigned int index)
	{
		assert(index < size && "Index out of bounds");
		return lits[index];
	}

	/**
	 * @brief Access clause literal by index (const version).
	 * @param index Index of the literal to access.
	 * @return Const reference to the literal at the specified index.
	 */
	const int& operator[](unsigned int index) const
	{
		assert(index < size && "Index out of bounds");
		return lits[index];
	}

	/**
	 * @brief Get iterator to the beginning of the clause.
	 * @return Pointer to the first element of the clause.
	 */
	int* begin() { return lits; }

	/**
	 * @brief Get iterator to the end of the clause.
	 * @return Pointer to one past the last element of the clause.
	 */
	int* end() { return lits + size; }

	/**
	 * @brief Get const iterator to the beginning of the clause.
	 * @return Const pointer to the first element of the clause.
	 */
	const int* begin() const { return lits; }

	/**
	 * @brief Get const iterator to the end of the clause.
	 * @return Const pointer to one past the last element of the clause.
	 */
	const int* end() const { return lits + size; }

	/**
	 * @brief Sort the literals in ascending order
	 */
	void sortLiterals() { std::sort(begin(), end()); }

	/**
	 * @brief Sort the literals in descending order
	 */
	void sortLiteralsDescending() { std::sort(begin(), end(), std::greater<int>()); }

	/**
	 * @brief Convert the clause to a string representation.
	 * @return String representation of the clause.
	 */
	std::string toString() const;

	/**
	 * @brief Convert to a raw pointer and increment the reference count.
	 * @return Raw pointer to this object.
	 */
	ClauseExchange* toRawPtr()
	{
		refCounter.fetch_add(1, std::memory_order_relaxed);
		return this;
	}

	/**
	 * @brief Create an intrusive_ptr from a raw pointer.
	 * @param ptr Raw pointer to a ClauseExchange object.
	 * @return ClauseExchangePtr Smart pointer to the object.
	 */
	static ClauseExchangePtr fromRawPtr(ClauseExchange* ptr) { return ClauseExchangePtr(ptr, false); }

  private:
	/**
	 * @brief Private constructor. Forces LBD to at least 2 for non units
	 * @param size Size of the clause.
	 * @param lbd Literal Block Distance of the clause.
	 * @param from Source identifier of the clause.
	 */
	ClauseExchange(unsigned int size, unsigned int lbd, int from);
};

/**
 * @brief Increment the reference count of a ClauseExchange object.
 * @param ce Pointer to the ClauseExchange object.
 */
inline void
intrusive_ptr_add_ref(ClauseExchange* ce)
{
	ce->refCounter.fetch_add(1, std::memory_order_relaxed);
}

/**
 * @brief Decrement the reference count of a ClauseExchange object and delete if it reaches zero.
 * @param ce Pointer to the ClauseExchange object.
 */
inline void
intrusive_ptr_release(ClauseExchange* ce)
{
	if (ce->refCounter.fetch_sub(1, std::memory_order_acq_rel) == 1) {
		ce->~ClauseExchange();
		std::free(ce);
	}
}