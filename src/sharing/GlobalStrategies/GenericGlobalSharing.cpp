#ifndef NDIST
#include "GenericGlobalSharing.hpp"
#include "containers/ClauseUtils.hpp"
#include "utils/ErrorCodes.hpp"
#include "utils/Logger.hpp"
#include "utils/MpiUtils.hpp"
#include <random>

GenericGlobalSharing::GenericGlobalSharing(
  const ulong bufferSize,
  const mpirank_t mpiWorldSize,
  const std::vector<mpirank_t>& subscriptions,
  const std::vector<mpirank_t>& subscribers,
  const std::chrono::microseconds sleepTime,
  const std::shared_ptr<ClauseDatabase>& clauseDB)
  : GlobalSharingStrategy(sleepTime, clauseDB)
  , m_subscriptions(subscriptions)
  , m_subscribers(subscribers)
  , m_totalSize(bufferSize)
{
  if (mpiWorldSize < 2) {
    LOGWARN("[Generic] I am alone or MPI was not initialized , no need for "
            "distributed mode.");
  }

  LOGVECTOR(&m_subscriptions[0],
            m_subscriptions.size(),
            "[Generic Sharing] Producers: ");
  LOGVECTOR(
    &m_subscribers[0], m_subscribers.size(), "[Generic Sharing] Consumers: ");
}

GenericGlobalSharing::~GenericGlobalSharing() {}

bool
GenericGlobalSharing::doSharing()
{
  // Sharing Management
  int received_size;
  int old_size;
  MPI_Status status;

  MPI_Request tmp_request;

  /* Sharing */
  m_clausesToSendSerialized.clear();
  m_gstats->sharedClauses += serializeClauses(m_clausesToSendSerialized);
  unsigned int serializedSize = m_clausesToSendSerialized.size();

  // Send to my subscribers my clauses
  for (unsigned int i = 0; i < m_subscribers.size(); i++) {
    TESTRUNMPI(MPI_Isend(&m_clausesToSendSerialized[0],
                         serializedSize,
                         MPI_INT,
                         m_subscribers[i],
                         MYMPI_CLAUSES,
                         MPI_COMM_WORLD,
                         &tmp_request));
    LOG2("[Generic] Sent a message of size %d to %d",
         serializedSize,
         m_subscribers[i]);
    m_sendRequests.push_back(tmp_request);
    m_gstats->messagesSent++;
  }

  // Check if my subscriptions sent me any clauses
  m_receivedClauses.clear();

  for (unsigned int i = 0; i < m_subscriptions.size(); i++) {

    old_size = m_receivedClauses.size();

    TESTRUNMPI(
      MPI_Probe(m_subscriptions[i], MYMPI_CLAUSES, MPI_COMM_WORLD, &status));
    TESTRUNMPI(MPI_Get_count(&status, MPI_INT, &received_size));

    m_receivedClauses.resize(old_size + received_size);
    LOGD2("Oldsize = %d, NewSize = %d", old_size, old_size + received_size);
    MPI_Recv(&m_receivedClauses[old_size],
             received_size,
             MPI_INT,
             m_subscriptions[i],
             MYMPI_CLAUSES,
             MPI_COMM_WORLD,
             &status);
    LOG2("[Generic] Received a message of size %d from %d",
         received_size,
         m_subscriptions[i]);
  }

  deserializeClauses(m_receivedClauses);

  for (auto sendRequest : m_sendRequests)
    TESTRUNMPI(MPI_Wait(&sendRequest, &status));

  m_sendRequests.clear();
  LOG2("[Generic] received cls %u shared cls %d",
       m_gstats->receivedClauses.load(),
       m_gstats->sharedClauses);

  return true;
}

//==============================
// Serialization/Deseralization
//==============================

uint
GenericGlobalSharing::serializeClauses(std::vector<lit_t>& serialized_v_cls)
{
  unsigned int clausesSelected = 0;
  ClauseExchangePtr tmpCls;

  unsigned int dataCount = serialized_v_cls.size();

  while (m_clauseDB->getOneClause(tmpCls) &&
         (m_totalSize <= 0 || dataCount < m_totalSize)) {
    if (m_totalSize > 0 && dataCount + tmpCls->size > m_totalSize) {
      LOGD2("[Generic] Serialization overflow avoided, %d/%d, wanted to add %d",
            dataCount,
            m_totalSize,
            tmpCls->size);
      this->importClause(tmpCls); // reinsert to clauseToSend if doesn't fit
      break;
    }

    // check with bloom filter if clause will be sent. If already sent, the
    // clause is directly released
    if (!this->m_bfilterSend.contains(tmpCls->lits, tmpCls->size)) {
      serialized_v_cls.push_back(tmpCls->size);
      serialized_v_cls.push_back(tmpCls->lbd);
      serialized_v_cls.insert(
        serialized_v_cls.end(), tmpCls->begin(), tmpCls->end());
      this->m_bfilterSend.insert(tmpCls->lits, tmpCls->size);
      clausesSelected++;

      dataCount += (tmpCls->size);
    } else {
      m_gstats->sharedDuplicasAvoided++;
    }
  }

  if (m_totalSize > 0 && dataCount > m_totalSize + 2 * clausesSelected) {
    LOGWARN("Panic!! datacount(%d) > totalsize(%d).", dataCount, m_totalSize);
  }
  return clausesSelected;
}

void
GenericGlobalSharing::deserializeClauses(std::vector<int>& serialized_v_cls)
{
  unsigned int i = 0;
  int size;
  int lbd;
  ClauseExchangePtr p_cls;
  int bufferSize = serialized_v_cls.size();

  while (i < bufferSize) {
    size = serialized_v_cls[i++];
    lbd = serialized_v_cls[i++];

    if (i + size > bufferSize) {
      LOGERROR("Deserialization error: Incomplete clause data");
      break;
    }

    if (!this->m_bfilterRecv.contains(serialized_v_cls.data() + i, size)) {
      p_cls = ClauseExchange::create(&serialized_v_cls[i],
                                     &serialized_v_cls[i + size],
                                     lbd,
                                     this->getSharingId());
      if (this->exportClause(p_cls))
        m_gstats->receivedClauses++;
      this->m_bfilterRecv.insert(
        serialized_v_cls.data() + i,
        size); // either added or not wanted (> maxClauseSize)
    } else {
      m_gstats->receivedDuplicas++;
    }

    i += size;
  }
}
#endif