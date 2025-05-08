#include "containers/ClauseUtils.hpp"
#include "sharing/Filters/BloomFilter.hpp"
#include "utils/Logger.hpp"
#include <algorithm>
#include <numeric>

namespace ClauseUtils {

static inline hash_t
lookup3_hash(hash_t key)
{
	hash_t s1, s2;
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
 * @brief Helper function to check if two clauses are equal.
 * @details Used internally by the equality functors.
 */
static inline bool
areClausesEqual(const lit_t* left, const csize_t leftSize, const lit_t* right, const csize_t rightSize)
{
	if (leftSize != rightSize)
		return false;

	// Same size: check if all literals in left are in right
	uint ri = 0; // right index
	const lit_t* const rightEnd = right + rightSize;
	for (uint li = 0; li < leftSize; li++) {

		// Same elements (if sorted and equal will only continue => linear complexity)
		if (left[li] == right[ri]) {
			ri++;
			continue;
		}

		// A different element: check if it is present in remaining element of right
		if (std::find(right + ri + 1, rightEnd, left[li]) == rightEnd)
			return false;
	}

	// Same size and all literals match
	return true;
}


hash_t
lookup3_hash_clause(const lit_t* clause, const csize_t size)
{
	if (size == 0)
		return 0;
	hash_t hash = lookup3_hash(clause[0]);
	for (unsigned int i = 1; i < size; i++) {
		hash ^= lookup3_hash(clause[i]);
	}
	return hash;
}

bool
ClauseEqual::operator()(const simpleClause& left, const simpleClause& right) const
{
	return areClausesEqual(left.data(), left.size(), right.data(), right.size());
}

bool
CLikeClauseEqual::operator()(const ClikeClause& left, const ClikeClause& right) const
{
	return areClausesEqual(left.lits, left.size, right.lits, right.size);
}

bool
ClauseExchangeEqual::operator()(const ClauseExchange& left, const ClauseExchange& right) const
{
	return areClausesEqual(left.lits, left.size, right.lits, right.size);
}

bool
ClauseExchangePtrEqual::operator()(const ClauseExchangePtr& left, const ClauseExchangePtr& right) const
{
	return areClausesEqual(left->lits, left->size, right->lits, right->size);
}

hash_t
ClauseHash::operator()(const simpleClause& clause) const
{
	return lookup3_hash_clause(clause.data(), clause.size());
}

hash_t
ClikeClauseHash::operator()(const ClikeClause& clause) const
{
	return lookup3_hash_clause(clause.lits, clause.size);
}

hash_t
ClauseExchangeHash::operator()(const ClauseExchange& clause) const
{
	return lookup3_hash_clause(clause.lits, clause.size);
}

hash_t
ClauseExchangePtrHash::operator()(const ClauseExchangePtr& clause) const
{
	return lookup3_hash_clause(clause->lits, clause->size);
}


hash_t
getLiteralsCount(const std::vector<ClauseExchangePtr>& clauses)
{
	return std::accumulate(
		clauses.begin(), clauses.end(), 0, [](int sum, const auto& clause) { return sum + clause->size; });
}

} // namespace ClauseUtils