#include "SimpleSharing.hpp"

#include "utils/Logger.hpp"

SimpleSharing::SimpleSharing(
  const csize_t sizeLimitAtImport,
  const ulong literalsPerRound,
  const std::chrono::microseconds sleepTime,
  const std::shared_ptr<ClauseDatabase>& clauseDB,
  const std::vector<std::shared_ptr<SharingEntity>>& clients)
  : SharingStrategy(clients)
  , m_sizeLimit(sizeLimitAtImport)
  , m_literalsPerRound(literalsPerRound)
  , m_clauseDB(clauseDB)
  , m_sleepTime(sleepTime)
  , m_stats(std::make_unique<SharingStrategy::Statistics>())
{
  this->markConfigured();
}

SimpleSharing::SimpleSharing(
  const std::shared_ptr<ClauseDatabase>& clauseDB,
  const std::vector<std::shared_ptr<SharingEntity>>& clients)
  : SharingStrategy(clients)
  , m_stats(std::make_unique<SharingStrategy::Statistics>())
{
}

SimpleSharing::~SimpleSharing() {}

bool
SimpleSharing::importClause(const ClauseExchangePtr& clause)
{
  assert(clause->size > 0 && clause->from != -1);

  int id = clause->from;

  LOGD4("Solver %d: Clause with size %d is tested against limit %d",
        id,
        clause->size,
        this->m_sizeLimit);

  if (clause->size <= this->m_sizeLimit) {
    m_stats->receivedClauses++;
    return m_clauseDB->addClause(clause);
  } else {
    m_stats->filteredAtImport++;
    return false;
  }
}

bool
SimpleSharing::doSharing()
{
  // 1- Get selection
  // consumer receives the same amount as in the original
  this->m_clauseDB->giveSelection(m_selection, m_literalsPerRound);

  m_stats->sharedClauses += m_selection.size();

  // 2-Send the best clauses (all producers included) to all clients
  this->exportClauses(m_selection);

  LOGD2("TotalSize: %ld => selectedClauses: %ld, DB size: %ld",
        m_literalsPerRound,
        m_selection.size(),
        this->m_clauseDB->getSize());

  m_selection.clear();

  /* empty database to limit growth */
  this->m_clauseDB->clearDatabase();

  LOG1("[SimpleShr] received cls %ld, shared cls %ld",
       m_stats->receivedClauses.load(),
       m_stats->sharedClauses);

  return true;
}

void
SimpleSharing::setOption(const std::string& key, int value)
{
  if (key == "size-limit-at-import")
    m_sizeLimit = value;
  else if (key == "literals-per-round")
    m_literalsPerRound = value;
  else if (key == "sleep-time-us")
    m_sleepTime = std::chrono::microseconds(value);
  else if (key == "sleep-time-s")
    m_sleepTime = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::seconds(value));
  else
    PABORT(PERR_ARGS,
           "Int Option %s is not recognized by SimpleSharing!",
           key.c_str());
}
void
SimpleSharing::setOption(const std::string& key, double value)
{
  // For 1e3 syntax and support longs for some options
  long castedValue = static_cast<long>(value);
  if (key == "literals-per-round")
    m_literalsPerRound = castedValue;
  else if (key == "sleep-time-us")
    m_sleepTime = std::chrono::microseconds(castedValue);
  else
    PABORT(PERR_ARGS,
           "Double Option %s is not recognized by SimpleSharing!",
           key.c_str());
}