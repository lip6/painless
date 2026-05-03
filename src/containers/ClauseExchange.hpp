#pragma once

#include <algorithm>
#include <atomic>
#include <boost/intrusive_ptr.hpp>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "painless/types.hpp"

// Forward declaration of ClauseExchange
class ClauseExchange;

/**
 * @typedef ClauseExchangePtr
 * @brief Type alias for the smart pointer used to manage ClauseExchange
 * objects.
 *
 * This alias allows for easy switching between different smart pointer
 * implementations or raw pointers in the future, if needed.
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
 * @warning If the size of the clause is greater than 1 the lbd is forced to at
 * least 2
 */
class ClauseExchange
{
public:
  lbd_t lbd;    ///< Literal Block Distance (LBD) of the clause
  plid_t from;  ///< Source identifier of the clause
  csize_t size; ///< Size of the clause
  std::atomic<rcount_t> refCounter; ///< Counter for intrusive_ptr copies and
                                    ///< raw pointer conversions
  lit_t lits[0]; ///< Flexible array member for storing clause literals (must be
                 ///< last)

  static constexpr plid_t ORIGINAL = -1;
  static constexpr plid_t EXTERNAL = -2;

  /**
   * @brief Create a new ClauseExchange object.
   * @param lbd Literal Block Distance of the clause.
   * @param from Source identifier of the clause
   * @return ClauseExchangePtr Smart pointer to the created object.
   * @throw std::bad_alloc If memory allocation fails.
   */
  static ClauseExchangePtr create(const csize_t size,
                                  const lbd_t lbd,
                                  const plid_t from);

  /**
   * @brief Create a new ClauseExchange object from a vector of literals.
   * @param v_cls Vector containing the clause literals.
   * @param lbd Literal Block Distance of the clause.
   * @param from Source identifier of the clause
   * @return ClauseExchangePtr Smart pointer to the created object.
   * @throw std::bad_alloc If memory allocation fails.
   */
  static ClauseExchangePtr create(clause_view_t v_cls,
                                  const lbd_t lbd,
                                  const plid_t from);

  /**
   * @brief Create a new ClauseExchange object using pointers.
   * @param begin Start of integer data.
   * @param end End of integer data.
   * @param lbd Literal Block Distance of the clause.
   * @param from Source identifier of the clause
   * @return ClauseExchangePtr Smart pointer to the created object.
   * @throw std::bad_alloc If memory allocation fails.
   */
  template<class Iterator>
  static ClauseExchangePtr create(const Iterator begin,
                                  const Iterator end,
                                  const lbd_t lbd,
                                  const plid_t from)
  {
    // Create a new ClauseExchange object
    auto ce = create(end - begin, lbd, from);
    csize_t i = 0;
    for (Iterator it = begin; it != end; it++, i++) {
      assert(*it);
      ce->lits[i] = *it;
    }

    return ce;
  }

  /**
   * @brief Destructor.
   */
  ~ClauseExchange() = default;

  /**
   * @brief Access clause literal by index.
   * @param index Index of the literal to access.
   * @return Reference to the literal at the specified index.
   */
  lit_t operator[](csize_t index)
  {
    assert(index < size && "Index out of bounds");
    return lits[index];
  }

  /**
   * @brief Access clause literal by index (const version).
   * @param index Index of the literal to access.
   * @return Const reference to the literal at the specified index.
   */
  const lit_t operator[](csize_t index) const
  {
    assert(index < size && "Index out of bounds");
    return lits[index];
  }

  /**
   * @brief Get iterator to the beginning of the clause.
   * @return Pointer to the first element of the clause.
   */
  lit_t* begin() { return lits; }

  /**
   * @brief Get iterator to the end of the clause.
   * @return Pointer to one past the last element of the clause.
   */
  lit_t* end() { return lits + size; }

  /**
   * @brief Get const iterator to the beginning of the clause.
   * @return Const pointer to the first element of the clause.
   */
  const lit_t* begin() const { return lits; }

  /**
   * @brief Get const iterator to the end of the clause.
   * @return Const pointer to one past the last element of the clause.
   */
  const lit_t* end() const { return lits + size; }

  /**
   * @brief Check if this clause should be kept permanently or not in the solver
   * receiving it
   * @return True if the clause is redundant, False if it is irredundant
   */
  bool isRedundant() const { return lbd > 0; }

  /**
   * @brief Check if this clause is unit
   * @return True if this clause is unit, False otherwise
   */
  bool isUnit() const { return size == 1; }

  /**
   * @brief Check if this clause was learnt locally
   * @return True if this clause was learnt locally, False otherwise (other
   * process, original formula, external helpers)
   */
  bool isLLocal() const { return from >= 0; }

  /**
   * @brief Check if this clause was from the original formula
   * @return True if this clause was from the original formula, False otherwise
   * (externally or locally learnt)
   */
  bool isOriginal() const { return from == -1; }

  /**
   * @brief Check if this clause was learnt externally
   * @return True if this clause was learnt externally, False otherwise
   * (original formula, locally learnt)
   */
  bool isLExternal() const { return from == -2; }

  /**
   * @brief Sort the literals in ascending order
   */
  void sortLiterals() { std::sort(begin(), end()); }

  /**
   * @brief Sort the literals in descending order
   */
  void sortLiteralsDescending()
  {
    std::sort(begin(), end(), std::greater<lit_t>());
  }

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
  static ClauseExchangePtr fromRawPtr(ClauseExchange* ptr)
  {
    // False to not increment the ref counter at construction, thus no need to
    // cancel the increment done at toRawPtr
    return ClauseExchangePtr(ptr, false);
  }

private:
  /**
   * @brief Private constructor. Forces LBD to at least 2 for non units
   * @param size Size of the clause.
   * @param lbd Literal Block Distance of the clause.
   * @param from Source identifier of the clause.
   */
  ClauseExchange(const csize_t size, const lbd_t lbd, const plid_t from);
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
 * @brief Decrement the reference count of a ClauseExchange object and delete if
 * it reaches zero.
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