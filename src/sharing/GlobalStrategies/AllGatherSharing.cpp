#ifndef NDIST
#include "AllGatherSharing.hpp"
#include "containers/ClauseUtils.hpp"
#include "utils/ErrorCodes.hpp"
#include "utils/Logger.hpp"
#include "utils/MpiUtils.hpp"
#include <random>

AllGatherSharing::AllGatherSharing(
  const ulong bufferSize,
  const mpirank_t mpiRank,
  const mpirank_t mpiWorldSize,
  const std::chrono::microseconds sleepTime,
  const std::shared_ptr<ClauseDatabase>& clauseDB)
  : m_mpiRank(mpiRank)
  , m_mpiWorldSize(mpiWorldSize)
  , m_totalSize(bufferSize)
  , GlobalSharingStrategy(sleepTime, clauseDB)
{
  if (mpiWorldSize < 2)
    LOGWARN("[Allgather] I am alone or MPI was not initialized, no need for "
            "distributed mode");
}

AllGatherSharing::~AllGatherSharing() {}

bool
AllGatherSharing::doSharing()
{
  MPI_Status status;

  // Sharing Management
  int yes_comm_size;
  MPI_Comm yes_comm;
  int sizeExport;

  // SHARING
  //========

  // At wake up, get the clauses to export and check if empty
  m_color = COLOR_YES;

  // The call must be done only when all global sharers can arrive here.
  TESTRUNMPI(MPI_Comm_split(MPI_COMM_WORLD, m_color, m_mpiRank, &yes_comm));

  if (m_color == MPI_UNDEFINED) {
    LOG2("[Allgather] is not willing to share (%d)", m_mpiRank);
  } else // do this only if member of yes_comm
  {
    TESTRUNMPI(MPI_Comm_set_errhandler(yes_comm, MPI_ERRORS_RETURN));

    TESTRUNMPI(MPI_Comm_size(yes_comm, &yes_comm_size));

    if (yes_comm_size < 2) {
      return false;
    } else {
      LOG3("[Allgather] %d global sharers will share their clauses",
           yes_comm_size);
    }

    // get clauses to send and serialize
    m_clausesToSendSerialized.clear();

    m_gstats->sharedClauses += serializeClauses(m_clausesToSendSerialized);
    // limit the receive buffer size to contain all the exported buffers
    // resize is used to really allocate the memory (reserve doesn't work with C
    // array access)
    m_receivedClauses.clear();
    m_receivedClauses.resize((m_totalSize)*yes_comm_size);

    LOGD4("[Allgather] before allgather", m_mpiRank);

    //  C like fashion, it overwrites using pointer arithmetic
    TESTRUNMPI(MPI_Allgather(&m_clausesToSendSerialized[0],
                             m_totalSize,
                             MPI_INT,
                             &m_receivedClauses[0],
                             m_totalSize,
                             MPI_INT,
                             yes_comm));
    m_gstats->messagesSent += yes_comm_size;

    // Now I have a vector of the all gathered buffers
    deserializeClauses(m_receivedClauses, yes_comm_size);
  }

  LOG2("[Allgather] received cls %u shared cls %d",
       m_gstats->receivedClauses.load(),
       m_gstats->sharedClauses);

  return true;
}

//===============================
// Serialization/Deseralization
//===============================

int
AllGatherSharing::serializeClauses(std::vector<int>& serialized_v_cls)
{
  int nb_clauses = 0;
  unsigned int dataCount =
    serialized_v_cls
      .size(); // it is an append operation so datacount do not start from zero
  ClauseExchangePtr tmpCls;

  while (
    dataCount < m_totalSize &&
    m_clauseDB->getOneClause(
      tmpCls)) // stops when buffer is full or no more clauses are available
  {
    if (dataCount + 2 + tmpCls->size > m_totalSize) {
      LOGD2(
        "[Allgather] Serialization overflow avoided, %d/%d, wanted to add %d",
        dataCount,
        m_totalSize,
        tmpCls->size + 2);
      importClause(
        tmpCls); // reinsert the clause to the database to not lose it
      break;
    } else {
      // check with bloom filter if clause will be sent. If already sent, the
      // clause is directly released
      if (!m_bfilter.contains(tmpCls->lits, tmpCls->size)) {
        serialized_v_cls.push_back(tmpCls->size);
        serialized_v_cls.push_back(tmpCls->lbd);
        serialized_v_cls.insert(
          serialized_v_cls.end(), tmpCls->begin(), tmpCls->end());
        m_bfilter.insert(tmpCls->lits, tmpCls->size);
        nb_clauses++;

        dataCount += (tmpCls->size + 2);
      } else {
        m_gstats->sharedDuplicasAvoided++;
      }
    }
  }

  // fill with zeroes if needed
  serialized_v_cls.insert(serialized_v_cls.end(), m_totalSize - dataCount, 0);

  LOGD1("Serialized %u clauses into buffer of size %u",
        nb_clauses,
        serialized_v_cls.size());
  return nb_clauses;
}

void
AllGatherSharing::deserializeClauses(const std::vector<int>& serialized_v_cls,
                                     int num_buffers)
{
  unsigned int i = 0;
  int size, lbd;
  ClauseExchangePtr p_cls;
  int bufferSize = serialized_v_cls.size();
  int single_buffer_size = bufferSize / num_buffers;

  LOGD2("Deserializing Buffer of Size %u", bufferSize);

  while (i < bufferSize) {
    // Check for the end marker
    if (serialized_v_cls[i] == 0) {
      // Move to the next buffer
      i = ((i / single_buffer_size) + 1) * single_buffer_size;
      continue;
    }

    size = serialized_v_cls[i++];
    lbd = serialized_v_cls[i++];

    if (i + size > bufferSize) {
      LOGERROR("Deserialization error: Incomplete clause data");
      break;
    }

    if (!m_bfilter.contains(serialized_v_cls.data() + i, size)) {
      p_cls = ClauseExchange::create(
        &serialized_v_cls[i], &serialized_v_cls[i + size], lbd, getSharingId());
      if (exportClause(p_cls)) {
        m_gstats->receivedClauses++;
      }
      m_bfilter.insert(serialized_v_cls.data() + i, size);
    } else {
      m_gstats->receivedDuplicas++;
    }

    i += size;
  }
}
#endif