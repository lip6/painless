#include "containers/ClauseDatabasePerSize.hpp"
#include "containers/ClauseExchange.hpp"
#include "utils/Logger.hpp"

#include <numeric>
#include <stdio.h>
#include <string.h>

ClauseDatabasePerSize::ClauseDatabasePerSize(int maxClauseSize)
	: maxClauseSize(maxClauseSize)
{
	if (maxClauseSize <= 0) {
		LOGWARN("The value %d for maxClauseSize is not supported by ClauseDatabasePerSize, it will be "
				"set to 80",
				maxClauseSize);
		this->maxClauseSize = 80;
	}
	initializeQueues(this->maxClauseSize);
}

ClauseDatabasePerSize::~ClauseDatabasePerSize() {}

bool
ClauseDatabasePerSize::addClause(ClauseExchangePtr clause)
{
	int clsSize = clause->size;
	if (clsSize <= 0) {
		LOGWARN("Panic, want to add a clause of size 0, clause won't be added and will be released");
		return false;
	}
	if (clsSize <= this->maxClauseSize) {
		if (clauses[clsSize - 1]->addClause(clause)) {
			return true;
		}
	}
	return false;
}

size_t
ClauseDatabasePerSize::giveSelection(std::vector<ClauseExchangePtr>& selectedCls, unsigned int literalCountLimit)
{
	int used = 0;
	ClauseExchangePtr tmp_clause;

	for (unsigned int i = 0; i < maxClauseSize && literalCountLimit - used >= i + 1; ++i) {
		while (clauses[i]->getClause(tmp_clause) && (literalCountLimit <= 0 || literalCountLimit - used >= i + 1)) {
			selectedCls.push_back(std::move(tmp_clause));
			used += i + 1;
		}
	}

	return used;
}

bool
ClauseDatabasePerSize::getOneClause(ClauseExchangePtr& cls)
{
	for (size_t i = 0; i < clauses.size(); ++i) {
		if (clauses[i]->getClause(cls)) {
			return true;
		}
	}
	return false;
}

void
ClauseDatabasePerSize::getClauses(std::vector<ClauseExchangePtr>& v_cls)
{
	for (auto& clauseBuffer : clauses) {
		clauseBuffer->getClauses(v_cls);
	}
}

size_t
ClauseDatabasePerSize::getSize() const
{
	return std::accumulate(
		clauses.begin(), clauses.end(), 0u, [](unsigned int sum, const std::unique_ptr<ClauseBuffer>& buffer) {
			return sum + (buffer ? buffer->size() : 0);
		});
}

void
ClauseDatabasePerSize::clearDatabase()
{
	for (size_t i = 0; i < clauses.size(); ++i) {
		clauses[i]->clear();
	}
}

// Private
// =======
void
ClauseDatabasePerSize::initializeQueues(unsigned int maxClsSize)
{
	this->initLiteralCount = 10000; /* Is it worth it to parametrize this ?*/
	clauses.reserve(maxClauseSize);

	for (unsigned int i = 0; i < maxClauseSize; ++i) {
		size_t queueSize = initLiteralCount / (i + 1);
		clauses.emplace_back(std::make_unique<ClauseBuffer>(queueSize));
	}
}