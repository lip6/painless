/**
 * @file types.hpp
 * @brief Core type definitions for the Painless SAT solver framework
 *
 * This file contains fundamental type definitions, enumerations, and data structures
 * used throughout the Painless framework for parallel and distributed SAT solving.
 * @ingroup painlessapi
 */

#ifndef __painless_types_hpp_INCLUDED
#define __painless_types_hpp_INCLUDED

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

// ==================================
// 				  Types
// ==================================

/**
 * @brief Enumeration for SAT solver results.
 *
 * Represents the possible outcomes of a SAT solving attempt.
 * These values follow the standard SAT competition return codes.
 * The TIMEOUT value is an extension.
 */
enum class SatAnswer
{
	SAT = 10,	  /**< Formula is satisfiable (a satisfying assignment exists) */
	UNSAT = 20,	  /**< Formula is unsatisfiable (no satisfying assignment exists) */
	TIMEOUT = 30, /**< Solver timed out before determining satisfiability */
	UNKNOWN = 0	  /**< Result is unknown or indeterminate */
};

// ==================================
// 		  Primitive Type Aliases
// ==================================

using uchar = unsigned char;
using ushort = unsigned short;
using uint = unsigned int;
using ulong = uint64_t;

/**
 * @brief Type for clause size representation
 *
 * Used to represent the number of literals in a clause.
 * Maximum value: 2^32 - 1
 */
using csize_t = uint32_t;

/**
 * @brief Type for literal representation
 *
 * A literal is a Boolean variable or its negation.
 * - Positive values represent positive literals (variable x)
 * - Negative values represent negative literals (¬x)
 * - Zero is used as a clause terminator in DIMACS format
 *
 * @note Valid literal range: [MIN_LIT, MAX_LIT]
 */
using lit_t = int32_t;

/**
 * @brief Type for variable representation
 *
 * Represents a Boolean variable index in the SAT problem.
 * Variables are numbered starting from 1 (following DIMACS convention).
 *
 * @note Variable 0 is invalid; valid range: [1, MAX_LIT]
 */
using var_t = uint32_t;

/**
 * @brief Type for Painless object identifiers
 *
 * Used to uniquely identify various objects within the Painless framework
 * (solvers, workers, sharers, etc.)
 */
using plid_t = int32_t;

/**
 * @brief Type for Literal Block Distance (LBD/glue) values
 *
 * LBD (also known as glue) is a quality metric for learned clauses,
 * representing the number of distinct decision levels in the clause.
 * Lower values indicate higher quality clauses.
 * @see https://www.ijcai.org/Proceedings/09/Papers/074.pdf
 *
 */
using lbd_t = uint32_t;

/**
 * @brief Type for reference counting
 *
 * Used for reference counting in intrusive pointers.
 */
using rcount_t = uint32_t;

/**
 * @brief Type for clause hash values
 *
 * Used to compute and store hash values for clauses, enabling efficient
 * duplicate detection and clause management.
 */
using hash_t = int64_t;

// ==================================
// 		       View Types
// ==================================

/**
 * @brief Immutable view of a clause
 *
 * Provides read-only access to a sequence of literals representing a clause.
 * @warning Does not own the underlying data.
 */
using clause_view_t = std::span<const lit_t>;

/**
 * @brief Mutable view of a clause
 *
 * Provides read-write access to a sequence of literals representing a clause.
 * @warning Does not own the underlying data.
 */
using clause_view_mut_t = std::span<lit_t>;

/**
 * @brief Immutable view of a cube
 *
 * A cube is a conjunction of literals (used in CDCL and other algorithms).
 * Provides read-only access to the cube's literals.
 * @warning Does not own the underlying data.
 */
using cube_view_t = std::span<const lit_t>;

/**
 * @brief Mutable view of a cube
 *
 * Provides read-write access to a sequence of literals representing a cube.
 * @warning Does not own the underlying data.
 */
using cube_view_mut_t = std::span<lit_t>;

/**
 * @brief Immutable view of a model (satisfying assignment)
 *
 * Provides read-only access to a satisfying assignment.
 * Each element represents the truth value of a variable.
 * @warning Does not own the underlying data.
 */
using model_view_t = std::span<const lit_t>;

/**
 * @brief Mutable view of a model
 *
 * Provides read-write access to a satisfying assignment.
 * @warning Does not own the underlying data.
 */
using model_view_mut_t = std::span<lit_t>;

// ==================================
// 		  Owning Container Types
// ==================================

/**
 * @brief Owning container for a clause
 *
 * Stores a disjunction of literals. Owns the underlying data.
 */
using clause_t = std::vector<lit_t>;

/**
 * @brief Owning container for a cube
 *
 * Stores a conjunction of literals. Owns the underlying data.
 */
using cube_t = std::vector<lit_t>;

/**
 * @brief Owning container for a satisfying assignment
 *
 * Stores a complete model (satisfying assignment) where each element
 * represents a literal in the assignment.
 */
using model_t = std::vector<lit_t>;

// ==================================
// 		  Callback Types
// ==================================

// ==================================
// 		  Result Structure
// ==================================

/**
 * @brief Structure representing a SAT solving result
 *
 * Encapsulates both the satisfiability answer and the corresponding model
 * (if the formula is satisfiable).
 */
struct result_t
{
	SatAnswer answer; /**< The satisfiability result (SAT, UNSAT, TIMEOUT, UNKNOWN) */
	model_t model;	  /**< The satisfying assignment (empty if UNSAT or UNKNOWN) */

	/**
	 * @brief Default constructor
	 *
	 * Creates a result with UNKNOWN answer and empty model.
	 */
	result_t()
		: answer(SatAnswer::UNKNOWN)
		, model()
	{
	}

	/**
	 * @brief Constructor with answer and model (copy)
	 *
	 * @param ans The satisfiability answer
	 * @param mod The satisfying assignment
	 */
	result_t(SatAnswer ans, const model_t& mod)
		: answer(ans)
		, model(mod)
	{
	}

	/**
	 * @brief Constructor with answer and model (move)
	 *
	 * @param ans The satisfiability answer
	 * @param mod The satisfying assignment (moved)
	 */
	result_t(SatAnswer ans, model_t&& mod)
		: answer(ans)
		, model(std::move(mod))
	{
	}

	// Default copy/move constructors and assignment operators
	result_t(const result_t&) = default;			/**< Copy constructor */
	result_t(result_t&&) = default;					/**< Move constructor */
	result_t& operator=(const result_t&) = default; /**< Copy assignment operator */
	result_t& operator=(result_t&&) = default;		/**< Move assignment operator */

	/**
	 * @brief Equality comparison operator
	 *
	 * Two results are equal if both their answers and models match.
	 * Checks answer first for early exit optimization.
	 *
	 * @param other The result to compare with
	 * @return true if results are equal, false otherwise
	 *
	 * @warning The models needs to be sorted by the variable value (abs(model[i])).
	 */
	bool operator==(const result_t& other) const
	{
		if (answer != other.answer)
			return false;
		else {
			csize_t size = model.size();
			if (other.model.size() != size)
				return false;
			for (uint i = 0; i < size; i++) {
				if (model[i] != other.model[i])
					return false;
			}
			return true;
		}
	}

	/**
	 * @brief Inequality comparison operator
	 *
	 * @param other The result to compare with
	 * @return true if results are not equal, false otherwise
	 */
	bool operator!=(const result_t& other) const { return !(*this == other); }
};

// ==================================
// 		  Constants
// ==================================

/**
 * @brief Maximum acceptable literal value
 *
 * Also represents the maximum variable index.
 * Computed as 2^31 - 1 for 32-bit signed integers.
 *
 * @note This is also used as the maximum variable count
 */
#define MAX_LIT ((1U << (sizeof(lit_t) * 8 - 1)) - 1)

/**
 * @brief Minimum acceptable literal value
 *
 * Computed as the negation of MAX_LIT.
 *
 * @note Valid literal range: [MIN_LIT, MAX_LIT]
 */
#define MIN_LIT (-MAX_LIT)

/**
 * @brief Maximum clause size supported by the type system
 *
 * Represents the maximum number of literals that can be stored in a clause
 * using the csize_t type (2^32 - 1).
 */
#define MAX_CSIZE ((csize_t) - 1)

#endif