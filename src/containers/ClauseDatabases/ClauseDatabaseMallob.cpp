#include "containers/ClauseDatabases/ClauseDatabaseMallob.hpp"
#include "containers/ClauseExchange.hpp"
#include "containers/ClauseUtils.hpp"
#include "utils/Logger.hpp"
#include <algorithm>
#include <numeric>

ClauseDatabaseMallob::ClauseDatabaseMallob()
  : m_currentLiteralSize(0)
  , m_currentWorstIndex(1)
{
}

ClauseDatabaseMallob::ClauseDatabaseMallob(int maxClauseSize,
                                           int maxPartitioningLbd,
                                           size_t maxCapacity,
                                           int maxFreeSize,
                                           unsigned clauseBufferSize)
  : m_maxClauseSize(maxClauseSize)
  , m_maxPartitioningLbd(maxPartitioningLbd)
  , m_freeMaxSize(maxFreeSize)
  , m_totalLiteralCapacity(maxCapacity)
  , m_currentLiteralSize(0)
  , m_currentWorstIndex(1)
  , m_missedAdditionsBfr(clauseBufferSize)
{
  if (maxClauseSize <= 0) {
    throw std::invalid_argument("maxClauseSize must be positive");
  }
  if (maxPartitioningLbd <= 0) {
    throw std::invalid_argument("maxPartitioningLbd must be positive");
  }
  if (maxFreeSize <= 0) {
    throw std::invalid_argument("maxFreeSize must be positive");
  }

  this->markConfigured();

  // Print the parameters
  // LOGSTAT("ClauseDatabaseMallob Parameters:");
  // LOGSTAT("  Max Clause Size: %d", m_maxClauseSize);
  // LOGSTAT("  Max Partitioning LBD: %d", m_maxPartitioningLbd);
  // LOGSTAT("  Max Free Size: %d", m_freeMaxSize);
  // LOGSTAT("  Total Literal Capacity: %zu", m_totalLiteralCapacity);
  // LOGSTAT("  Initial Clause Vector Size: %zu", m_clauses.size());
}

ClauseDatabaseMallob::~ClauseDatabaseMallob() {}

bool
ClauseDatabaseMallob::addClause(ClauseExchangePtr clause)
{
  /*
  - From my understanding ABA problems shouldn't be an issue thanks to the
  shared_mutex with shrinkDatabase, thus the worstIndex can only grow larger in
  here
  - Even if multiple threads could check the capacity at the same time, the
  overflow will be corrected at shrinkage
  */
  int clsSize = clause->size;
  int clsLbd = clause->lbd;

  assert(clsSize > 0 && clsLbd >= 0);

  if (clsSize > m_maxClauseSize) {
    return false;
  }

  // Try to acquire the shared lock
  std::shared_lock<std::shared_mutex> sharedLock(m_shrinkMutex,
                                                 std::try_to_lock);

  if (!sharedLock.owns_lock()) {
    // If we couldn't acquire the lock, give up adding the clause in database
    m_missedAdditionsBfr.addClause(clause);
    return false;
  }

  if (clsSize == UNIT_SIZE) {
    if (m_clauses[0]->addClause(clause)) {
      m_currentLiteralSize.fetch_add(UNIT_SIZE);
      LOGD2("Added new unit clause of size %u, literalsCount: %ld",
            clause->size,
            m_currentLiteralSize.load());
    }
    return true;
  }

  /* enforced by ClauseExchange */
  // if (clsLbd < MIN_LBD)
  // 	clsLbd = MIN_LBD;

  unsigned index = getIndex(clsSize, clsLbd);

  // usage of relaxed since only the value is needed
  size_t newSize =
    m_currentLiteralSize.load() + clsSize; // test std::memory_order_relaxed
  int currentWorst =
    m_currentWorstIndex.load(); // test std::memory_order_relaxed

  /* (probable overflow) add clause only if worst clauses are present or the
   totalLiteralCapacity is not reached,
   * yet. The currentLiteralSize update can be unseen by a parallel getClause
   happening between the buffer update and atomic addition. However the
   important thing is that at shrink we have a correct value thanks the
   unique_lock
   */
  if ((newSize <= m_totalLiteralCapacity || index < currentWorst) &&
      m_clauses[index]->addClause(clause)) {
    m_currentLiteralSize.fetch_add(clsSize); // test std::memory_order_release
    LOGD2("Added new clause of size %u, literalsCount: %ld",
          clause->size,
          m_currentLiteralSize.load());
    /*
    update m_currentWorstIndex (lockfree) , the thread with the worst index will
    do the last store acq_rel for exchange: (acquire) see all writes of other
    threads, (release) and all writes of current thread are seen by other
    threads. otherwise acquire is enough for reading ?

    - threads should not encounter live-locking since the index is growing in
    value a thread gives up updating the currentWorst if its index is smaller or
    equal
    */
    while (index > currentWorst) {
      if (m_currentWorstIndex.compare_exchange_weak(currentWorst, index)) {
        LOGD2("Updated worst index: %d -> %d", currentWorst, index);
        break;
      }
    }
    return true;
  }

  return false;
}

size_t
ClauseDatabaseMallob::giveSelection(std::vector<ClauseExchangePtr>& selectedCls,
                                    unsigned int literalCountLimit)
{
  SHARED_LOCK(std::shared_mutex, m_shrinkMutex, sharedLock);

  size_t selectedLiterals = 0;
  size_t trulySelectedLiterals = 0;
  LOGD2("Before selection count: %ld/%lu. Worst index %u",
        m_currentLiteralSize.load(),
        m_totalLiteralCapacity,
        m_currentWorstIndex.load());

  // load all units separately since currentLiteralSize doesn't count them
  // (trulySelectedLitrals is not update)
  while (!m_clauses[0]->empty() && selectedLiterals < literalCountLimit) {
    ClauseExchangePtr cls;
    if (m_clauses[0]->getClause(cls)) {
      // count unit size only if m_freeMaxSize is null
      if (1 > m_freeMaxSize) {
        selectedLiterals += cls->size;
      }
      selectedCls.push_back(cls);
    }
  }

  // start iterating from the index 1 and fill selectedCls (clauses are popped)
  for (size_t i = 1;
       i < m_clauses.size() && selectedLiterals < literalCountLimit;
       ++i) {
    auto& bucket = m_clauses[i];
    // stop if selectedLiterals >= literalCountLimit or no more clauses to
    // consume
    while (!bucket->empty() && selectedLiterals < literalCountLimit) {
      ClauseExchangePtr cls;
      if (bucket->getClause(cls)) {
        trulySelectedLiterals += cls->size;
        // if actual cls.size() <= freeMaxSize, do not update selectedLiterals
        if (cls->size > m_freeMaxSize) {
          selectedLiterals += cls->size;
        }
        selectedCls.push_back(cls);
      }
    }
  }

  m_currentLiteralSize.fetch_sub(
    trulySelectedLiterals); // test std::memory_order_release

  LOGD2("After selection count: %ld/%lu. Worst index %u. Selected Literals %lu",
        m_currentLiteralSize.load(),
        m_totalLiteralCapacity,
        m_currentWorstIndex.load(),
        selectedLiterals);

  return selectedLiterals;
}

void
ClauseDatabaseMallob::getClauses(std::vector<ClauseExchangePtr>& v_cls)
{
  // get all clauses
  SHARED_LOCK(std::shared_mutex, m_shrinkMutex, sharedLock);

  for (size_t i = 0; i < m_currentWorstIndex.load(); ++i) {
    m_clauses[i]->getClauses(v_cls);
  }

  size_t literalsConsumed = ClauseUtils::getLiteralsCount(v_cls);
  m_currentLiteralSize.fetch_sub(
    literalsConsumed); // can be negative for a while
}

bool
ClauseDatabaseMallob::getOneClause(ClauseExchangePtr& cls)
{
  SHARED_LOCK(std::shared_mutex, m_shrinkMutex, sharedLock);

  for (size_t i = 0; i < m_currentWorstIndex.load(); ++i) {

    if (m_clauses[i]->getClause(cls)) {
      LOGD2("Gotten Clause of size %u, currentLits: %ld",
            cls->size,
            m_currentLiteralSize.load());
      m_currentLiteralSize.fetch_sub(
        cls->size); // can be negative for a while, until all additions in
                    // addClause are done
      return true;
    }
  }
  return false;
}

size_t
ClauseDatabaseMallob::getSize() const
{
  return std::accumulate(
    m_clauses.begin(),
    m_clauses.end(),
    0u,
    [](unsigned int sum, const std::unique_ptr<ClauseBuffer>& buffer) {
      return sum + buffer->size();
    });
}

size_t
ClauseDatabaseMallob::shrinkDatabase()
{

  /* Push missed clauses in previous shrink, if it is done after a shrink
   directly we could easily overflow the capacity, thus going against the
   concept of shrinking */
  if (m_missedAdditionsBfr.size() > 0) {
    LOGD2("Previous shrinking made me miss %u clauses",
          m_missedAdditionsBfr.size());
    ClauseExchangePtr missedCls;
    while (m_missedAdditionsBfr.getClause(missedCls)) {
      this->addClause(missedCls);
    }
  }

  // Acquire an exclusive lock to prevent concurrent modifications during
  // shrinkage
  UNIQUE_LOCK(std::shared_mutex, m_shrinkMutex, lock);

  size_t removedClausesInBucket = 0;
  size_t totalRemovedClauses = 0;
  // acquire memory ordering makes sure all updates from addClause are seen here
  // (but the shared_mutex already ensure this)
  long currentSize = m_currentLiteralSize.load();
  assert(currentSize >= 0);
  size_t newWorst = 1; /* reset worst to first non unit */

  LOGD2("Before shrink count: %ld/%lu. Worst index %u",
        currentSize,
        m_totalLiteralCapacity,
        m_currentWorstIndex.load());

  // Iterate backwards through the clause buckets (i > 0: units are never
  // shrinked, only consumed)
  for (unsigned int i = m_clauses.size() - 1; i > 0; --i) {

    size_t bucketSize = m_clauses[i]->size();

    if (bucketSize == 0)
      continue;

    removedClausesInBucket = 0;

    // Condition here for correct newWorst update
    if (currentSize > m_totalLiteralCapacity) {
      int clauseSize = getSizeFromIndex(i);
      size_t literalsInBucket = bucketSize * clauseSize;
      LOGD2("Bucket at %i has %u clauses of size == %u literals",
            i,
            bucketSize,
            clauseSize,
            literalsInBucket);

      // Check if removing the entire bucket would bring us under capacity
      if (currentSize - literalsInBucket < m_totalLiteralCapacity) {
        // Remove clauses one by one until we're under capacity
        while (currentSize > m_totalLiteralCapacity) {
          ClauseExchangePtr cls;
          if (m_clauses[i]->getClause(cls)) {
            assert(cls->size == clauseSize);
            ++removedClausesInBucket;
          } else {
            break; // Bucket is empty
          }
        }
        currentSize -= removedClausesInBucket * clauseSize;
        totalRemovedClauses += removedClausesInBucket;
      } else {
        // Remove the entire bucket
        LOGD2("Removing the whole bucket at index %d of size %u(lits:%u)",
              i,
              bucketSize,
              literalsInBucket);
        currentSize -= literalsInBucket;
        totalRemovedClauses += bucketSize;
        m_clauses[i]->clear();
      }
    }

    assert(currentSize >= 0);

    // Update the worst index if this is the first non empty bucket we see
    if (1 == newWorst && !m_clauses[i]->empty()) {
      newWorst = i;
      break;
    }
  }
  assert(currentSize <= m_totalLiteralCapacity);
  // Update the current literal size atomically
  m_currentLiteralSize.store(currentSize); // test std::memory_order_release
  // Update the real worstIndex
  m_currentWorstIndex.store(newWorst); // test std::memory_order_release

  LOGD2("After shrink count: %ld/%lu. Worst index %u. Removed Clauses %u",
        m_currentLiteralSize.load(),
        m_totalLiteralCapacity,
        m_currentWorstIndex.load(),
        totalRemovedClauses);

  return totalRemovedClauses;
}

void
ClauseDatabaseMallob::clearDatabase()
{
  for (auto& bucket : m_clauses) {
    bucket->clear();
  }
  m_currentLiteralSize.store(0);
  m_currentWorstIndex.store(1);
}

bool
ClauseDatabaseMallob::onConfigured()
{
  bool error = false;
  if (m_maxClauseSize <= 0) {
    LOGERROR("Cannot initialize database with %d max size", m_maxClauseSize);
    error = true;
  }
  if (m_maxPartitioningLbd <= 0) {
    LOGERROR("Cannot initialize database with %d lbd partitions",
             m_maxClauseSize);
    error = true;
  }

  if (error)
    return false;

  m_clauses.resize(m_maxClauseSize * (m_maxPartitioningLbd));
  for (auto& clause : m_clauses) {
    // TODO remove magic number or centralize it
    clause = std::make_unique<ClauseBuffer>(1000);
  }

  return true;
}

void
ClauseDatabaseMallob::setOption(const std::string& key, int value)
{
  if (key == "max-clause-size") {
    m_maxClauseSize = value;
  } else if (key == "max-partition-lbd") {
    m_maxPartitioningLbd = value;
  } else if (key == "free-max-size")
    m_freeMaxSize = value;
  else if (key == "literal-capacity")
    m_totalLiteralCapacity = value;
  else
    PABORT(PERR_ARGS,
           "Int Option %s is not recognized by ClauseDatabaseMallob!",
           key.c_str());
}
void
ClauseDatabaseMallob::setOption(const std::string& key, double value)
{
  long castedValue = static_cast<long>(value);
  if (key == "literal-capacity")
    m_totalLiteralCapacity = castedValue;
  else
    PABORT(PERR_ARGS,
           "Double Option %s is not recognized by ClauseDatabaseMallob!",
           key.c_str());
}