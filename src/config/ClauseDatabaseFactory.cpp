#include "ClauseDatabaseFactory.hpp"

#include "containers/ClauseDatabases/ClauseDatabaseBufferPerEntity.hpp"
#include "containers/ClauseDatabases/ClauseDatabaseMallob.hpp"
#include "containers/ClauseDatabases/ClauseDatabasePerSize.hpp"
#include "containers/ClauseDatabases/ClauseDatabaseSingleBuffer.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtils.hpp"

ClauseDatabaseFactory::ClauseDatabaseFactory(unsigned int maxClauseSize,
                                             size_t maxCapacity,
                                             int mallobMaxPartitioningLbd,
                                             int mallobMaxFreeSize,
                                             unsigned int bufferSize)
  : m_maxClauseSize(maxClauseSize)
  , m_maxCapacity(maxCapacity)
  , m_mallobMaxPartitioningLbd(mallobMaxPartitioningLbd)
  , m_mallobMaxFreeSize(mallobMaxFreeSize)
{
}

std::shared_ptr<ClauseDatabase>
ClauseDatabaseFactory::createDatabase(const std::string& originalDBName)
{
  std::string name = pl::str::toLower(originalDBName);

  if (name == "singlebuffer") {
    LOG2("DB>> Creating Single Buffer database");
    return std::make_shared<ClauseDatabaseSingleBuffer>();
  } else if (name == "persize") {
    LOG2("DB>> Creating PerSize database");
    return std::make_shared<ClauseDatabasePerSize>();
  } else if (name == "bufferperentity") {
    LOG2("DB>> Creating PerEntity database");
    return std::make_shared<ClauseDatabaseBufferPerEntity>();
  } else if (name == "mallob") {
    LOG2("DB>> Creating Mallob database");
    return std::make_shared<ClauseDatabaseMallob>();
  } else {
    PABORT(PERR_UNKNOWN_DATABASE, "Unknown database type %s", name.c_str());
  }
}

std::shared_ptr<ClauseDatabase>
ClauseDatabaseFactory::createDatabase(char dbTypeChar)
{
  switch (dbTypeChar) {
    case 's': {
      LOG2("DB>> Creating Single Buffer database with max clause capacity %u",
           m_maxCapacity);
      return std::make_shared<ClauseDatabaseSingleBuffer>(m_maxCapacity);
    }
    case 'd': {
      LOG2("DB>> Creating PerSize database with max clause size %u",
           m_maxClauseSize);
      return std::make_shared<ClauseDatabasePerSize>(m_maxClauseSize);
    }

    case 'e': {
      LOG2("DB>> Creating PerEntity database with max clause size %u",
           m_maxClauseSize);
      return std::make_shared<ClauseDatabaseBufferPerEntity>(m_maxClauseSize);
    }

    case 'm': {
      LOG2("DB>> Creating Mallob database with max clause size %u, lbd %d, "
           "capacity %zu, freeSize %d",
           m_maxClauseSize,
           m_mallobMaxPartitioningLbd,
           m_maxCapacity,
           m_mallobMaxFreeSize);
      return std::make_shared<ClauseDatabaseMallob>(m_maxClauseSize,
                                                    m_mallobMaxPartitioningLbd,
                                                    m_maxCapacity,
                                                    m_mallobMaxFreeSize,
                                                    m_clauseBuffer);
    }

    default: {
      LOGWARN("Unknown database type '%c', defaulting to PerSize", dbTypeChar);
      LOG2("DB>> Creating PerSize database with max clause size %u",
           m_maxClauseSize);
      return std::make_shared<ClauseDatabasePerSize>(m_maxClauseSize);
    }
  }
}