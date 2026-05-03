#include "HordeSatSharing.hpp"
#include "sharing/Filters/BloomFilter.hpp"
#include "utils/Logger.hpp"

#include <chrono>

HordeSatSharing::HordeSatSharing(
  const uint producerCount,
  const ulong literalsPerProducerPerRound,
  const lbd_t initialLbdLimit,
  const uint roundsBeforeLbdIncrease,
  const std::chrono::microseconds sleepTime,
  const std::shared_ptr<ClauseDatabase>& clauseDB,
  const std::vector<std::shared_ptr<SharingEntity>>& clients)
  : SharingStrategy(clients)
  , m_producerCount(producerCount)
  , m_literalsPerProducerPerRound(literalsPerProducerPerRound)
  , m_initialLbdLimit(initialLbdLimit)
  , m_roundsBeforeIncrease(roundsBeforeLbdIncrease)
  , m_sleepTime(sleepTime)
  , m_clauseDB(clauseDB)
  , m_stats(std::make_unique<SharingStrategy::Statistics>())
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
  , m_producerCount(0)
  , m_clauseDB(clauseDB)
  , m_stats(std::make_unique<SharingStrategy::Statistics>())
{
  this->m_round = 0;
}

HordeSatSharing::~HordeSatSharing()
{
  std::stringstream sstr;
  sstr << "\n";
  for (uint pid = 0; pid < m_producerCount; pid++) {
    sstr << "c Producer " << pid << " : " << m_producerMeanLbd[pid] << std::endl;
  }
  LOGSTAT("%s", sstr.str().c_str());
}

bool
HordeSatSharing::importClause(const ClauseExchangePtr& clause)
{
  assert(clause->size > 0 && clause->from != -1);

  // The producer id
  uint pidx = clause->from;
  assert(m_producerCount > pidx);

  LOGD4("Producer %d: Clause with lbd %d is tested against limit %d",
        clause->from,
        clause->lbd,
        m_lbdLimitPerProducer[pidx].load());

  if (!m_producerMeanLbd[pidx])
    m_producerMeanLbd[pidx] = clause->lbd;
  else
    m_producerMeanLbd[pidx] = (m_producerMeanLbd[pidx] + clause->lbd) / 2;

  if (clause->lbd <= m_lbdLimitPerProducer[pidx]) {
    m_stats->receivedClauses++;
    if (m_clauseDB->addClause(clause)) {
      m_literalsPerProducer[pidx] += clause->size;
      return true;
    } else
      return false;
  } else {
    m_stats->filteredAtImport++;
    return false;
  }
}

bool
HordeSatSharing::doSharing()
{
  // Step 1: Get new clause selection
  this->m_clauseDB->giveSelection(
    m_selection, m_literalsPerProducerPerRound * m_producerCount);

  // Step 2: Process producers
  for (uint pidx = 0; pidx < m_producerCount; pidx++) {
    const ulong produced = m_literalsPerProducer[pidx].load();
    const ulong producedPercent =
      (100 * produced) / m_literalsPerProducerPerRound;

    LOG3("[HordeSat] Production rate of %d = %d", pidx, producedPercent);

    // Adjust production based on utilization
    if (m_roundsBeforeIncrease < m_round &&
        producedPercent < HordeSatSharing::UNDER_UTILIZATION_THRESHOLD) {
      // Increase clause production
      m_lbdLimitPerProducer[pidx].fetch_add(1);
      LOG3("[HordeSat] production increase for entity %d.", pidx);
    } else if (producedPercent > HordeSatSharing::OVER_UTILIZATION_THRESHOLD) {
      // Decrease clause production (one writer, one reader scenario)
      lbd_t currentLimit = m_lbdLimitPerProducer[pidx].load();
      if (currentLimit > 2) {
        m_lbdLimitPerProducer[pidx].store(currentLimit - 1);
        LOG3("[HordeSat] production decrease for entity %d.", pidx);
      }
    }

    // Substraction instead of reset for better consistency
    m_literalsPerProducer[pidx] -= produced;
  }

  m_stats->sharedClauses += m_selection.size();
  LOGD4("TotalSize: %ld => selectedClauses: %ld",
        m_literalsPerProducerPerRound * m_producerCount,
        m_selection.size());

  // Step 3: Export clauses to clients
  this->exportClauses(m_selection);

  // Step 4: Clear selection vector
  m_selection.clear();

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

  m_producerCount = producerIds.size();

  if (!m_producerCount) {
    LOGERROR("Cannot initialize Hordesat with 0 producerCount");
    return false;
  }

  m_producerMeanLbd.resize(m_producerCount, 0.f);

  m_lbdLimitPerProducer =
    std::make_unique<std::atomic<uint>[]>(m_producerCount);
  m_literalsPerProducer =
    std::make_unique<std::atomic<ulong>[]>(m_producerCount);

  for (int i = 0; i < m_producerCount; i++) {
    m_lbdLimitPerProducer[i] = m_initialLbdLimit;
    m_literalsPerProducer[i] = 0;
  }

  return true;
}
