
#include "containers/ClauseUtils.hpp"
#include "sharing/Filters/BloomFilter.hpp"
#include "utils/Logger.hpp"

#include <numeric>

namespace ClauseUtils {

std::size_t
hashClause(const simpleClause& clause)
{
	if (clause.empty())
		return 0;

	hash_t hash = lookup3_hash(clause[0]);
	for (size_t i = 1; i < clause.size(); ++i) {
		hash ^= lookup3_hash(clause[i]);
	}
	return hash;
}

bool
operator==(const simpleClause& lhs, const simpleClause& rhs)
{
	if (lhs.size() != rhs.size())
		return false;
	if (hashClause(lhs) != hashClause(rhs))
		return false;

	LOGDEBUG1("Comparing two vectors with same hash: %lu", hashClause(lhs));

	for (const auto& elem : lhs) {
		if (std::find(rhs.begin(), rhs.end(), elem) == rhs.end()) {
			return false;
		}
	}
	return true;
}

int
getLiteralsCount(const std::vector<ClauseExchangePtr>& clauses)
{
	return std::accumulate(
		clauses.begin(), clauses.end(), 0, [](int sum, const auto& clause) { return sum + clause->size; });
}

} // namespace ClauseUtils