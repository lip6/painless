#ifndef NDIST
#pragma once

#include "GlobalSharingStrategy.hpp"

/**
 * @class GenericGlobalSharing
 * @brief Implements a generic global sharing strategy for clause exchange.
 * @ingroup global_sharing
 *
 * This class extends GlobalSharingStrategy to provide a generic implementation
 * for clause sharing among MPI processes.
 */
class GenericGlobalSharing : public GlobalSharingStrategy
{
public:
  /**
   * @brief Constructor for GenericGlobalSharing with copy semantics.
   * @param bufferSize Size of the buffer to send to subscribers.
   * @param mpiWorldSize The mpi world size where this process is.
   * @param subscriptions Vector of MPI ranks to receive clauses from.
   * @param subscribers Vector of MPI ranks to send clauses to.
   * @param sleepTime The number of microseconds to sleep between two
   * doSharing()'s.
   * @param clauseDB Shared pointer to the clause database.
   */
  GenericGlobalSharing(const ulong bufferSize,
                       const mpirank_t mpiWorldSize,
                       const std::vector<mpirank_t>& subscriptions,
                       const std::vector<mpirank_t>& subscribers,
                       const std::chrono::microseconds sleepTime,
                       const std::shared_ptr<ClauseDatabase>& clauseDB);

  /**
   * @brief Destructor for GenericGlobalSharing.
   */
  ~GenericGlobalSharing();

  /**
   * @brief Performs the clause sharing operation. An MPI process starts by
   * serializing its clauses then sends them via asynchronous sends to all its
   * subscribers, then waits on the reception of its different subscriptions.
   * @return True if the sharing didn't encounter an error, false otherwise.
   */
  bool doSharing() override;

protected:
  /**
   * @brief Serializes clauses for sharing.
   * @param serialized_v_cls Vector to store the serialized clauses.
   * @details Serialization Pattern ([size][lbd][literals])*
   * @return The number of clauses serialized.
   */
  uint serializeClauses(std::vector<lit_t>& serialized_v_cls);

  /**
   * @brief Deserializes received clauses.
   * @param serialized_v_cls Vector containing the serialized clauses.
   */
  void deserializeClauses(std::vector<int>& serialized_v_cls);

  uint m_totalSize; ///< Total size of the buffer for clause sharing
  std::vector<MPI_Request>
    m_sendRequests; ///< MPI requests for non-blocking sends
  BloomFilter
    m_bfilterSend; ///< Bloom filter for avoiding duplicate clause sends
  BloomFilter
    m_bfilterRecv; ///< Bloom filter for avoiding duplicate clause receives
  std::vector<lit_t>
    m_clausesToSendSerialized; ///< Buffer for serialized clauses to send
  std::vector<lit_t>
    m_receivedClauses; ///< Buffer for received serialized clauses
  std::vector<mpirank_t>
    m_subscriptions; ///< List of MPI ranks to receive clauses from
  std::vector<mpirank_t>
    m_subscribers; ///< List of MPI ranks to send clauses to
};
#endif