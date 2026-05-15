#include "containers/ClauseDatabases/ClauseDatabasePerSize.hpp"
#include "containers/ClauseExchange.hpp"
#include "utils/Logger.hpp"

#include <numeric>
#include <stdio.h>
#include <string.h>

ClauseDatabasePerSize::ClauseDatabasePerSize()
  : m_size(0)
  , m_maxClauseSize(0)
{
}

ClauseDatabasePerSize::ClauseDatabasePerSize(int maxClauseSize)
  : m_maxClauseSize(maxClauseSize)
  , m_size(0)
{
  if (maxClauseSize <= 0) {
    PABORT(PERR_BAD_BEHAVIOR,
           "The value %d for maxClauseSize is not supported by "
           "ClauseDatabasePerSize",
           maxClauseSize);
  }
  markConfigured();
}

ClauseDatabasePerSize::~ClauseDatabasePerSize() {}

bool
ClauseDatabasePerSize::addClause(ClauseExchangePtr clause)
{
  int clsSize = clause->size;
  if (clsSize <= 0) {
    LOGWARN("Panic, want to add a clause of size 0, clause won't be added");
    return false;
  }
  if (clsSize <= m_maxClauseSize) {
    // pre increment to not have negative size if it consumed directly
    m_size++;
    if (m_clausesPerSize[clsSize - 1]->addClause(clause)) {
      return true;
    } else
      m_size--;
  }
  return false;
}

size_t
ClauseDatabasePerSize::giveSelection(
  std::vector<ClauseExchangePtr>& selectedCls,
  unsigned int literalCountLimit)
{
  UNIQUE_LOCK(std::mutex, m_consumeMX, consume);
  unsigned int used = 0;

  for (unsigned int i = 0; i < m_maxClauseSize; ++i) {
    const unsigned int clauseLits = i + 1;
    while (true) {
      if ((clauseLits + used) > literalCountLimit)
        return used;

      ClauseExchangePtr cls;

      if (!m_clausesPerSize[i]->getClause(cls)) {
        break;
      }
      // Post decrement to not have -1 as size value if empty
      m_size--;
      // No need to increment and decrement the refcount
      selectedCls.push_back(std::move(cls));
      used += clauseLits;
    }
  }

  return used;
}

bool
ClauseDatabasePerSize::getOneClause(ClauseExchangePtr& cls)
{
  UNIQUE_LOCK(std::mutex, m_consumeMX, consume);
  for (size_t i = 0; i < m_clausesPerSize.size(); ++i) {
    if (m_clausesPerSize[i]->getClause(cls)) {
      m_size--;
      return true;
    }
  }
  return false;
}

void
ClauseDatabasePerSize::getClauses(std::vector<ClauseExchangePtr>& v_cls)
{
  UNIQUE_LOCK(std::mutex, m_consumeMX, consume);
  for (auto& clauseBuffer : m_clausesPerSize) {
    m_size -= clauseBuffer->size();
    clauseBuffer->getClauses(v_cls);
  }
}

size_t
ClauseDatabasePerSize::getSize() const
{
  return m_size;
}

void
ClauseDatabasePerSize::clearDatabase()
{
  UNIQUE_LOCK(std::mutex, m_consumeMX, consume);
  for (size_t i = 0; i < m_clausesPerSize.size(); ++i) {
    m_size -= m_clausesPerSize[i]->size();
    m_clausesPerSize[i]->clear();
  }
}

// Private
// =======
bool
ClauseDatabasePerSize::onConfigured()
{
  if (m_maxClauseSize <= 0) {
    LOGERROR("Cannot initialize database with %d max size", m_maxClauseSize);
    return false;
  }

  m_clausesPerSize.reserve(m_maxClauseSize);

  for (unsigned int i = 0; i < m_maxClauseSize; ++i) {
    m_clausesPerSize.emplace_back(std::make_unique<ClauseBuffer>());
  }

  return true;
}

void
ClauseDatabasePerSize::setOption(const std::string& key, int value)
{
  if (key == "max-clause-size") {
    m_maxClauseSize = value;
  } else
    PABORT(PERR_ARGS,
           "Int Option %s is not recognized by ClauseDatabasePerSize!",
           key.c_str());
}