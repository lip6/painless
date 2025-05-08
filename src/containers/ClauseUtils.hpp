#pragma once

#include <vector>
#include <memory>
#include "containers/ClauseExchange.hpp"

namespace ClauseUtils {

/**
 * @brief Jenkins lookup3 hash function.
 * @details Implementation based on https://burtleburtle.net/bob/c/lookup3.c
 */
#define _jenkins_rot(x, k) (((x) << (k)) | ((x) >> (32 - (k))))

/**
 * @brief Computes a hash value for a clause using the Jenkins lookup3 algorithm.
 * @param clause Pointer to the array of literals in the clause.
 * @param size Number of literals in the clause.
 * @return The computed hash value for the clause.
 */
hash_t
lookup3_hash_clause(const lit_t* clause, const csize_t size);

/**
 * @brief Calculates the total number of literals in a vector of clauses.
 * @param clauses Vector of shared pointers to ClauseExchange objects.
 * @return The total number of literals across all clauses.
 */
hash_t
getLiteralsCount(const std::vector<ClauseExchangePtr>& clauses);

/**
 * @brief Equality functor for simple clauses.
 * @details Implements a commutative equality check based on the Mallob ProducedClauseEqualsCommutative.
 */
struct ClauseEqual
{
    bool operator()(const simpleClause& left, const simpleClause& right) const;
};

/**
 * @brief Equality functor for simple clauses.
 * @details Implements a commutative equality check based on the Mallob ProducedClauseEqualsCommutative.
 */
struct CLikeClauseEqual
{
    bool operator()(const ClikeClause& left, const ClikeClause& right) const;
};

/**
 * @brief Equality functor for ClauseExchange objects.
 * @details Implements a commutative equality check based on the Mallob ProducedClauseEqualsCommutative.
 */
struct ClauseExchangeEqual
{
    bool operator()(const ClauseExchange& left, const ClauseExchange& right) const;
};

/**
 * @brief Equality functor for ClauseExchangePtr objects.
 * @details Implements a commutative equality check based on the Mallob ProducedClauseEqualsCommutative.
 */
struct ClauseExchangePtrEqual
{
    bool operator()(const ClauseExchangePtr& left, const ClauseExchangePtr& right) const;
};

/**
 * @brief Hash functor for simple clauses.
 */
struct ClauseHash
{
    hash_t operator()(const simpleClause& clause) const;
};

/**
 * @brief Hash functor for simple c like clauses.
 */
struct ClikeClauseHash
{
    hash_t operator()(const ClikeClause& clause) const;
};

/**
 * @brief Hash functor for ClauseExchange objects.
 */
struct ClauseExchangeHash
{
    hash_t operator()(const ClauseExchange& clause) const;
};

/**
 * @brief Hash functor for ClauseExchangePtr objects.
 */
struct ClauseExchangePtrHash
{
    hash_t operator()(const ClauseExchangePtr& clause) const;
};

} // namespace ClauseUtils