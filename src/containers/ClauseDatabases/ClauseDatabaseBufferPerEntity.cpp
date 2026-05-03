#include "containers/ClauseDatabases/ClauseDatabaseBufferPerEntity.hpp"
#include "containers/ClauseDatabases/ClauseDatabasePerSize.hpp"
#include "containers/ClauseExchange.hpp"
#include "utils/Logger.hpp"
#include <algorithm>
#include <numeric>

ClauseDatabaseBufferPerEntity::ClauseDatabaseBufferPerEntity(int maxClauseSize)
  : m_maxClauseSize(maxClauseSize)
{
  this->markConfigured();
}

ClauseDatabaseBufferPerEntity::ClauseDatabaseBufferPerEntity() {}

bool
ClauseDatabaseBufferPerEntity::addClause(ClauseExchangePtr clause)
{
  int entityId = clause->from;

  // First, try to find the buffer with shared lock
  ClauseBuffer* buffer = nullptr;
  {
    SHARED_LOCK(std::shared_mutex, m_databaseMX, readLock);
    auto it = entityDatabases.find(entityId);
    if (it != entityDatabases.end()) {
      buffer = it->second.get();
    }
  }

  // If buffer wasn't found, we need to create it
  if (!buffer) {
    UNIQUE_LOCK(std::shared_mutex, m_databaseMX, writeLock);
    // Double-check in case another thread created the buffer while we were
    // waiting
    auto [it, inserted] = entityDatabases.try_emplace(
      entityId,
      std::make_unique<ClauseBuffer>()); // TODO better init size
    buffer = it->second.get();
  }

  // At this point, we have a valid buffer pointer, and we don't need to hold
  // the lock anymore
  return buffer->addClause(clause);
}

size_t
ClauseDatabaseBufferPerEntity::giveSelection(
  std::vector<ClauseExchangePtr>& selectedCls,
  unsigned int literalCountLimit)
{
  // Heavy and ugly implementation to be reworked
  ClauseDatabasePerSize tempDatabase(m_maxClauseSize);
  std::vector<ClauseExchangePtr> tempVector;

  {
    SHARED_LOCK(std::shared_mutex, m_databaseMX, readLock);
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
  SHARED_LOCK(std::shared_mutex, m_databaseMX, readLock);
  for (auto& [entityId, buffer] : entityDatabases) {
    buffer->getClauses(v_cls);
  }
}

bool
ClauseDatabaseBufferPerEntity::getOneClause(ClauseExchangePtr& cls)
{
  SHARED_LOCK(std::shared_mutex, m_databaseMX, readLock);
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
  SHARED_LOCK(std::shared_mutex, m_databaseMX, readLock);
  return std::accumulate(entityDatabases.begin(),
                         entityDatabases.end(),
                         0u,
                         [](unsigned int sum, const auto& pair) {
                           return sum + pair.second->size();
                         });
}

void
ClauseDatabaseBufferPerEntity::clearDatabase()
{
  UNIQUE_LOCK(std::shared_mutex, m_databaseMX, writeLock);
  for (auto& [entityId, buffer] : entityDatabases) {
    buffer->clear();
  }
}

void
ClauseDatabaseBufferPerEntity::setOption(const std::string& key, int value)
{
  if (key == "max-clause-size")
    m_maxClauseSize = value;
  else
    PABORT(PERR_ARGS,
           "Int Option %s is not recognized by ClauseDatabaseBufferPerEntity!",
           key.c_str());
}