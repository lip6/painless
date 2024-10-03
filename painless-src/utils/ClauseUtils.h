#pragma once

#include <vector>

typedef std::vector<int> simpleClause;

/// Not sure it works with int32_t, the original uses uint32_t
std::size_t hash_clause(std::vector<int> const &clause);

/// Compare two int vectors
bool operator==(const std::vector<int> &vector1, const std::vector<int> &vector2);

struct clauseHash
{
	std::size_t operator()(const simpleClause &clause) const
	{
		return hash_clause(clause);
	}
};
