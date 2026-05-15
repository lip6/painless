#include "HordeSatSharing.hpp"
#include "sharing/Filters/BloomFilter.hpp"
#include "utils/Logger.hpp"

#include <chrono>

HordeSatSharing::HordeSatSharing(
  const ulong literalsPerProducerPerRound,
  const lbd_t initialLbdLimit,
  const uint roundsBeforeLbdIncrease,
  const std::chrono::microseconds sleepTime,
  const std::shared_ptr<ClauseDatabase>& clauseDB,
  const std::vector<std::shared_ptr<SharingEntity>>& clients)
  : SharingStrategy(clients)
  , m_literalsPerProducerPerRound(literalsPerProducerPerRound)
  , m_initialLbdLimit(initialLbdLimit)
  , m_roundsBeforeIncrease(roundsBeforeLbdIncrease)
  , m_sleepTime(sleepTime)
  , m_clauseDB(clauseDB)
  , m_stats(std::make_unique<SharingStrategy::Statistics>())
  , m_underUtilizationThreshold(75)
  , m_overUtilizationThreshold(98)
{
  this->m_round = 0;

  LOGSTAT("[HordeSat] Consumers: %d, Initial Lbd limit: %u, round "
          "before increase: %d, literals per round: %d",
          getClientCount(),
          m_initialLbdLimit,
          m_roundsBeforeIncrease,
          m_literalsPerProducerPerRound);
}

HordeSatSharing::HordeSatSharing(
  const std::shared_ptr<ClauseDatabase>& clauseDB,
  const std::vector<std::shared_ptr<SharingEntity>>& clients)
  : SharingStrategy(clients)
  , m_clauseDB(clauseDB)
  , m_stats(std::make_unique<SharingStrategy::Statistics>())
  , m_underUtilizationThreshold(75)
  , m_overUtilizationThreshold(98)
{
  this->m_round = 0;
}

HordeSatSharing::~HordeSatSharing() {}

bool
HordeSatSharing::importClause(const ClauseExchangePtr& clause)
{
  assert(clause->size > 0 && clause->from != -1);

  // Should be very careful on the order of the locks to not have deadlocks
  // lock producer metadata on read
  SHARED_LOCK(std::shared_mutex, m_producersMX, read);
  // The producer id
  uint pidx = this->m_producerIdToIndex.at(clause->from);
  assert(m_producersMeta.size() > pidx);

  const auto& producerMeta = m_producersMeta[pidx];

  LOGD4("Producer %d: Clause with lbd %d is tested against limit %d",
        clause->from,
        clause->lbd,
        producerMeta->lbdLimit.load());

  bool pushed = false;
  if (clause->lbd <= producerMeta->lbdLimit) {
    // Always push on the clauses vector
    producerMeta->clauses.addClause(clause);
    return true;
  } else {
    m_stats->filteredAtImport++;
  }

  return false;
}

bool
HordeSatSharing::doSharing()
{
  // Lock on read the producers' meta data
  SHARED_LOCK(std::shared_mutex, m_producersMX, read);

  uint producerCount = m_producersMeta.size();

  std::vector<ulong> producedLiterals(m_producersMeta.size(), 0L);

  /* Step 1: Add producer clauses into the selected database for further
   * filtering */
  for (uint pidx = 0; pidx < producerCount; pidx++) {

    // Get All current clauses
    std::vector<ClauseExchangePtr> clauses;
    m_producersMeta[pidx]->clauses.getClauses(clauses);

    for (auto& cls : clauses) {
      producedLiterals[pidx] += cls->size;
      m_clauseDB->addClause(cls);
    }

    m_stats->receivedClauses += clauses.size();
    clauses.clear();
  }

  // Step 2: Get the clause selection
  this->m_clauseDB->giveSelection(
    m_selection, m_literalsPerProducerPerRound * producerCount);

  // Step 3: Process producers
  for (uint pidx = 0; pidx < producerCount; pidx++) {
    const ulong produced = producedLiterals[pidx];
    const ulong producedPercent =
      (100 * produced) / m_literalsPerProducerPerRound;

    LOG2("[HordeSat] Production rate of %d = %d (%lu)", pidx, producedPercent, produced);

    // Adjust production based on utilization
    if (m_roundsBeforeIncrease < m_round &&
        producedPercent < m_underUtilizationThreshold) {
      // Increase clause production
      m_producersMeta[pidx]->lbdLimit.fetch_add(1, std::memory_order_relaxed);
      LOG3("[HordeSat] production increase for entity %d.", pidx);
    } else if (producedPercent > m_overUtilizationThreshold) {
      // Decrease clause production
      lbd_t currentLimit =
        m_producersMeta[pidx]->lbdLimit.load(std::memory_order_relaxed);
      if (currentLimit > 2) {
        m_producersMeta[pidx]->lbdLimit.fetch_sub(1, std::memory_order_relaxed);
        LOG3("[HordeSat] production decrease for entity %d.", pidx);
      }
    }
  }

  m_stats->sharedClauses += m_selection.size();
  LOGD4("TotalSize: %ld => selectedClauses: %ld",
        m_literalsPerProducerPerRound * producerCount,
        m_selection.size());

  // Step 4: Export clauses to clients
  this->exportClauses(m_selection);

  // Step 5: Clear selection vector
  m_selection.clear();
  m_clauseDB->shrinkDatabase();

  m_round++;
  LOG2("[HordeSat] received cls %ld, shared cls %ld",
       m_stats->receivedClauses.load(),
       m_stats->sharedClauses);

  return true;
}

void
HordeSatSharing::setOption(const std::string& key, int value)
{
  // Dangerous to do it while other threads use doSharing / importClause
  if (key == "literals-per-producer-per-round")
    m_literalsPerProducerPerRound = value;
  else if (key == "initial-lbd-limit")
    m_initialLbdLimit = value;
  else if (key == "rounds-before-increase")
    m_roundsBeforeIncrease = value;
  else if (key == "under-utilization-threshold")
    m_underUtilizationThreshold = value;
  else if (key == "over-utilization-threshold")
    m_overUtilizationThreshold = value;
  else if (key == "sleep-time-us")
    m_sleepTime = std::chrono::microseconds(value);
  else if (key == "sleep-time-s")
    m_sleepTime = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::seconds(value));
  else
    PABORT(PERR_ARGS,
           "Int Option %s is not recognized by HordeSatSharing!",
           key.c_str());
}
void
HordeSatSharing::setOption(const std::string& key, double value)
{
  long castedValue = static_cast<long>(value);
  if (key == "literals-per-producer-per-round")
    m_literalsPerProducerPerRound = castedValue;
  else if (key == "rounds-before-increase")
    m_roundsBeforeIncrease = castedValue;
  else if (key == "sleep-time-us")
    m_sleepTime = std::chrono::microseconds(castedValue);
  else
    PABORT(PERR_ARGS,
           "Double Option %s is not recognized by HordeSatSharing!",
           key.c_str());
}

void
HordeSatSharing::setOption(const std::string& key, const std::string& value)
{
  if (key == "producer-ids") {
    m_producersList = value;
  } else
    PABORT(PERR_ARGS,
           "String Option %s is not recognized by HordeSatSharing!",
           key.c_str());
}

bool
HordeSatSharing::onConfigured()
{
  std::vector<plid_t> producerIds;
  std::string strNumber;

  for (const char c : m_producersList) {
    if (std::isdigit(c))
      strNumber.push_back(c);
    else {
      // Add new id
      producerIds.push_back(std::stoul(strNumber));
      strNumber.clear();
    }
  }
  if (!strNumber.empty())
    producerIds.push_back(std::stoul(strNumber));

  uint producerCount = producerIds.size();

  if (!producerCount) {
    LOGERROR("Cannot initialize Hordesat with 0 producerCount");
    return false;
  }

  UNIQUE_LOCK(std::shared_mutex, m_producersMX, initialize);

  for (uint i = 0; i < producerCount; i++) {
    m_producerIdToIndex.emplace(producerIds[i], i);
    LOGD1("Linked pid %u with index %u", producerIds[i], i);

    m_producersMeta.push_back(
      std::make_unique<HordeSatSharing::ProducerMeta>());

    m_producersMeta.back()->lbdLimit = m_initialLbdLimit;
  }

  assert(m_producersMeta.size() == producerCount);

  return true;
}
