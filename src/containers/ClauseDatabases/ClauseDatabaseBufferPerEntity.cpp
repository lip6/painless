#include "containers/ClauseDatabases/ClauseDatabaseBufferPerEntity.hpp"
#include "containers/ClauseDatabases/ClauseDatabasePerSize.hpp"
#include "containers/ClauseExchange.hpp"
#include "utils/Logger.hpp"
#include <algorithm>
#include <mutex>
#include <numeric>

ClauseDatabaseBufferPerEntity::ClauseDatabaseBufferPerEntity(int maxClauseSize)
	: maxClauseSize(maxClauseSize)
{
}

bool
ClauseDatabaseBufferPerEntity::addClause(ClauseExchangePtr clause)
{
	int entityId = clause->from;

	// First, try to find the buffer with shared lock
	ClauseBuffer* buffer = nullptr;
	{
		std::shared_lock<std::shared_mutex> readLock(dbmutex);
		auto it = entityDatabases.find(entityId);
		if (it != entityDatabases.end()) {
			buffer = it->second.get();
		}
	}

	// If buffer wasn't found, we need to create it
	if (!buffer) {
		std::unique_lock<std::shared_mutex> writeLock(dbmutex);
		// Double-check in case another thread created the buffer while we were waiting
		auto [it, inserted] = entityDatabases.try_emplace(entityId, std::make_unique<ClauseBuffer>(maxClauseSize)); // TODO better init size
		buffer = it->second.get();
	}

	// At this point, we have a valid buffer pointer, and we don't need to hold the lock anymore
	return buffer->addClause(clause);
}

size_t
ClauseDatabaseBufferPerEntity::giveSelection(std::vector<ClauseExchangePtr>& selectedCls, unsigned int literalCountLimit )
{
	ClauseDatabasePerSize tempDatabase(maxClauseSize);
	std::vector<ClauseExchangePtr> tempVector;

	{
		std::shared_lock<std::shared_mutex> readLock(dbmutex);
		for (auto& [entityId, buffer] : entityDatabases) {
			tempVector.clear();
			buffer->getClauses(tempVector);
			for (auto& cls : tempVector)
				tempDatabase.addClause(cls);
		}
	}

	return tempDatabase.giveSelection(selectedCls, literalCountLimit);
}

void
ClauseDatabaseBufferPerEntity::getClauses(std::vector<ClauseExchangePtr>& v_cls)
{
	std::shared_lock<std::shared_mutex> readLock(dbmutex);
	for (auto& [entityId, buffer] : entityDatabases) {
		buffer->getClauses(v_cls);
	}
}

bool
ClauseDatabaseBufferPerEntity::getOneClause(ClauseExchangePtr& cls)
{
	std::shared_lock<std::shared_mutex> readLock(dbmutex);
	for (auto& [entityId, buffer] : entityDatabases) {
		if (buffer->getClause(cls)) {
			return true;
		}
	}
	return false;
}

size_t
ClauseDatabaseBufferPerEntity::getSize() const
{
	std::shared_lock<std::shared_mutex> readLock(dbmutex);
	return std::accumulate(entityDatabases.begin(), entityDatabases.end(), 0u, [](unsigned int sum, const auto& pair) {
		return sum + pair.second->size();
	});
}

void
ClauseDatabaseBufferPerEntity::clearDatabase()
{
	std::unique_lock<std::shared_mutex> writeLock(dbmutex);
	for (auto& [entityId, buffer] : entityDatabases) {
		buffer->clear();
	}
}