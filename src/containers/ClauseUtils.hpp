#pragma once

#include "containers/ClauseExchange.hpp"
#include <cstdint>
#include <memory>
#include <vector>

/**
 * @brief Helping Functions for Clause Management (Hash and Equality)
 * @ingroup utils
 */
namespace ClauseUtils {

/**
 * @brief Jenkins lookup3 hash function.
 * @details Implementation based on https://burtleburtle.net/bob/c/lookup3.c
 */
#define _jenkins_rot(x, k) (((x) << (k)) | ((x) >> (32 - (k))))

/**
 * @brief Computes a hash value for a single key using the Jenkins lookup3 algorithm.
 * @param key The key to hash.
 * @return The computed hash value.
 */
inline size_t
lookup3_hash(size_t key)
{
	size_t s1, s2;
	s1 = s2 = 0xdeadbeef;
	s2 ^= s1;
	s2 -= _jenkins_rot(s1, 14);
	key ^= s2;
	key -= _jenkins_rot(s2, 11);
	s1 ^= key;
	s1 -= _jenkins_rot(key, 25);
	s2 ^= s1;
	s2 -= _jenkins_rot(s1, 16);
	key ^= s2;
	key -= _jenkins_rot(s2, 4);
	s1 ^= key;
	s1 -= _jenkins_rot(key, 14);
	s2 ^= s1;
	s2 -= _jenkins_rot(s1, 24);

	return s2;
}

/**
 * @brief Computes a hash value for a clause using the Jenkins lookup3 algorithm.
 * @param clause Pointer to the array of literals in the clause.
 * @param size Number of literals in the clause.
 * @return The computed hash value for the clause.
 */
inline size_t
lookup3_hash_clause(const int* clause, unsigned int size)
{
	if (size == 0)
		return 0;
	size_t hash = lookup3_hash(clause[0]);
	for (unsigned int i = 1; i < size; i++) {
		hash ^= lookup3_hash(clause[i]);
	}
	return hash;
}

/**
 * @brief Computes a hash value for a simple clause.
 * @param clause The simple clause to hash.
 * @return The computed hash value.
 */
std::size_t
hashClause(const simpleClause& clause);

/**
 * @brief Equality operator for simple clauses.
 * @param lhs The left-hand side simple clause.
 * @param rhs The right-hand side simple clause.
 * @return True if the clauses are equal, false otherwise.
 */
bool
operator==(const simpleClause& lhs, const simpleClause& rhs);

/**
 * @brief Calculates the total number of literals in a vector of clauses.
 * @param clauses Vector of shared pointers to ClauseExchange objects.
 * @return The total number of literals across all clauses.
 */
int
getLiteralsCount(const std::vector<ClauseExchangePtr>& clauses);

/**
 * @brief Hash functor for simple clauses.
 */
struct ClauseHash
{
	std::size_t operator()(const simpleClause& clause) const { return hashClause(clause); }
};

/**
 * @brief Hash functor for ClauseExchange objects.
 */
struct ClauseExchangeHash
{
	std::size_t operator()(const ClauseExchange& clause) const { return lookup3_hash_clause(clause.lits, clause.size); }
};

/**
 * @brief Equality functor for ClauseExchange objects.
 * @details Implements a commutative equality check based on the Mallob ProducedClauseEqualsCommutative.
 */
struct ClauseExchangeEqual
{
	bool operator()(const ClauseExchange& left, const ClauseExchange& right) const
	{
		if (left.size != right.size)
			return false;

		// Same size: check if all literals in left are in right
		uint ri = 0; // right index

		for (int lit : left) {

			// Same elements (if sorted and equal will only continue => linear complexity)
			if (lit == right[ri]) {
				ri++;
				continue;
			}

			// A different element: check if it is present in remaining element of right
			if (std::find(right.begin() + ri + 1, right.end(), lit) == right.end())
				return false;
		}

		// Same size and all literals match
		return true;
	}
};

/**
 * @brief Hash functor for ClauseExchangePtr objects.
 */
struct ClauseExchangePtrHash
{
	std::size_t operator()(const ClauseExchangePtr& clause) const { return ClauseExchangeHash()(*clause); }
};

/**
 * @brief Equality functor for ClauseExchangePtr objects.
 */
struct ClauseExchangePtrEqual
{
	bool operator()(const ClauseExchangePtr& left, const ClauseExchangePtr& right) const
	{
		return ClauseExchangeEqual()(*left, *right);
	}
};

} // namespace ClauseUtils
