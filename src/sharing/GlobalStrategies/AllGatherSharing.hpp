#ifndef NDIST
#pragma once

#include "GlobalSharingStrategy.hpp"

/**
 * @class AllGatherSharing
 * @brief Implements a global sharing strategy using MPI_Allgather for clause
 * exchange.
 *
 * @ingroup global_sharing
 *
 * This class extends GlobalSharingStrategy to implement a specific sharing
 * mechanism using MPI's Allgather collective operation. Here, the size of the
 * buffer is strict, and the value totalSize should take into account the
 * metadata.
 */
class AllGatherSharing : public GlobalSharingStrategy
{
public:
  /**
   * @brief Constructor for GenericGlobalSharing with copy semantics.
   * @param bufferSize Size of the buffer to send to subscribers.
   * @param mpiRank The mpi rank of this process.
   * @param mpiWorldSize The mpi world size where this process is.
   * @param sleepTime The number of microseconds to sleep between two
   * doSharing()'s.
   * @param clauseDB Shared pointer to the clause database.
   */
  AllGatherSharing(const ulong bufferSize,
                   const mpirank_t mpiRank,
                   const mpirank_t mpiWorldSize,
                   const std::chrono::microseconds sleepTime,
                   const std::shared_ptr<ClauseDatabase>& clauseDB);

  /**
   * @brief Destructor for AllGatherSharing.
   */
  ~AllGatherSharing();

  /**
   * @brief Performs the clause sharing operation.
   * A group is created for the process willing to shared only. The different
   * processes serialize their clauses and share them via a single MPI_Allgather
   * call.
   *
   * @todo a real decision on willing to share or not.
   * @return True if the sharing didn't encounter an error, false otherwise.
   */
  bool doSharing() override;

protected:
  /**
   * @brief Serializes clauses for sharing.
   * @details Serialization Pattern ([size][lbd][literals])*0+
   * @param serialized_v_cls Vector to store the serialized clauses.
   * @return The number of clauses serialized.
   */
  int serializeClauses(std::vector<int>& serialized_v_cls);

  /**
   * @brief Deserializes received clauses.
   * @param serialized_v_cls Vector containing the serialized clauses.
   * @param num_buffers Number of buffers received.
   */
  void deserializeClauses(const std::vector<int>& serialized_v_cls,
                          int num_buffers);

  uint m_totalSize; ///< Total size of the buffer for clause sharing
  int m_color;      ///< Color used for MPI communicator splitting

  std::vector<lit_t>
    m_clausesToSendSerialized; ///< Buffer for serialized clauses to send
  std::vector<lit_t>
    m_receivedClauses; ///< Buffer for received serialized clauses

  const mpirank_t m_mpiRank;
  const mpirank_t m_mpiWorldSize;

  BloomFilter m_bfilter; ///< Bloom filter for duplicate clause detection
};
#endif