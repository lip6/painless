#ifndef NDIST
#pragma once

#include "GlobalSharingStrategy.hpp"
#include "containers/Bitset.hpp"
#include "containers/ClauseUtils.hpp"
#include <vector>

struct ClauseMeta
{
  int32_t productionEpoch; /* last epoch clause was produced */
  int32_t sharedEpoch; /* last epoch where clause was shared and imported! */
  uint64_t sources;    /* acts like a bitset */
};

/**
 * @class MallobSharing
 * @brief Implements a global sharing strategy based on the Mallob algorithm.
 *
 * This class extends GlobalSharingStrategy to implement a specific sharing
 * mechanism inspired by the Mallob algorithm for distributed SAT solving
 * (ref:https://doi.org/10.1613/jair.1.15827).
 *
 * @ingroup global_sharing
 */
class MallobSharing : public GlobalSharingStrategy
{
public:
  /**
   * @brief Constructor for MallobSharing.
   * @param mpiRank The mpi rank of this process.
   * @param mpiWorldSize The mpi world size where this process is.
   * @param sleepTime The number of microseconds to sleep between two
   * doSharing()'s.
   * @param baseBufferSize The base size of the buffer in computation
   * @param maxBufferSize The maxmimum size of the buffer in computation
   * @param lbdLimitAtImport The maximum lbd value of imported clauses
   * @param sizeLimitAtImport The maximum size of imported clauses
   * @param roundsPerSecond The number of sharings done in a second
   * @param maxCompensation The maximum compensation factor
   * @param resharePeriodMicroSec The resharing period for the exact filter
   * @param clauseDB Shared pointer to the clause database.
   */
  MallobSharing(const mpirank_t mpiRank,
                const mpirank_t mpiWorldSize,
                const std::chrono::microseconds sleepTime,
                const ulong baseBufferSize,
                const ulong maxBufferSize,
                const uint lbdLimitAtImport,
                const uint sizeLimitAtImport,
                const uint roundsPerSecond,
                const float maxCompensation,
                /*Filter Options*/
                const uint resharePeriodMicroSec,
                const std::shared_ptr<ClauseDatabase>& clauseDB);

  /**
   * @brief Destructor for MallobSharing.
   */
  ~MallobSharing();

  /**
   * @brief Performs the clause sharing operation.
   * @return True if the sharing didn't encounter an error, false otherwise.
   */
  bool doSharing() override;

  /**
   * @brief Imports a clause into the clause database.
   * @param cls Pointer to the clause to be imported.
   * @return true if the clause was successfully imported, false otherwise.
   */
  bool importClause(const ClauseExchangePtr& cls) override;

  /**
   * @brief Gets the sleeping time for the sharing strategy.
   * @return The sleeping time in microseconds.
   */
  std::chrono::microseconds getSleepingTime() override;

protected:
  /**
   * @brief Exports a clause to a specific client.
   * @param clause Pointer to the clause to be exported.
   * @param client Shared pointer to the client receiving the clause.
   * @return true if the clause was successfully exported, false otherwise.
   */
  bool exportClauseToClient(const ClauseExchangePtr& clause,
                            std::shared_ptr<SharingEntity> client) override;

  /**
   * @brief Deserializes received clauses.
   * @param serialized_v_cls Vector containing the serialized clauses.
   */
  void deserializeClauses(const std::vector<lit_t>& serialized_v_cls);

  /**
   * @brief Merges serialized buffers with local clauses from the m_clauseDB
   * database.
   * @details Serialization Pattern ([size][lbd][literals])*
   * @param buffers Vector of references to buffers containing serialized
   * clauses.
   * @param result Vector to store the merged result.
   * @param nonFreeLiteralsCount Output parameter to store the count of non-free
   * literals.
   * @return The number of clauses merged.
   */
  uint mergeSerializedBuffersWithMine(
    std::vector<std::reference_wrapper<std::vector<int>>>& buffers,
    std::vector<lit_t>& result,
    size_t& nonFreeLiteralsCount);

  /**
   * @brief Computes the buffer size based on the number of aggregations.
   * @param aggregationsCount The number of aggregations.
   */
  void computeBufferSize(uint aggregationsCount);

  /**
   * @brief Wrapper for getting one clause from the database. This remove the
   * need to make the filter thread-safe
   * @param cls Output parameter to store the retrieved clause.
   * @return true if a clause was successfully retrieved, false otherwise.
   */
  bool getOneClauseWrapper(ClauseExchangePtr& cls)
  {
    if (this->m_clauseDB->getOneClause(cls)) {
      return insertClause(cls);
    }
    return false;
  }

  /**
   * @brief Computes the compensation factor for volume balancing.
   */
  void computeCompensation();

  uint defaultBufferSize; ///< Default size of the sharing buffer
  const uint baseSize;    ///< Base size for buffer calculations
  const uint maxSize;     ///< Maximum size for buffer calculations

  std::vector<std::reference_wrapper<std::vector<int>>>
    buffers; ///< Buffers for clause sharing

  std::vector<lit_t>
    clausesToSendSerialized; ///< Buffer for serialized clauses to send
  std::vector<lit_t>
    receivedClausesLeft; ///< Buffer for clauses received from left child
  std::vector<lit_t>
    receivedClausesRight; ///< Buffer for clauses received from right child
  std::vector<lit_t>
    receivedClausesFather; ///< Buffer for clauses received from parent

  std::vector<ClauseExchangePtr>
    deserializedClauses; ///< Buffer for deserialized clauses

  pl::Bitset myBitVector; ///< Bitset for tracking shared clauses

  const mpirank_t m_mpiRank;
  const mpirank_t m_mpiWorldSize;
  mpirank_t father;      ///< MPI rank of the parent node
  mpirank_t left_child;  ///< MPI rank of the left child
  mpirank_t right_child; ///< MPI rank of the right child
  uint nb_children;      ///< Number of children nodes

  bool addChildClauses; ///< Flag to determine if child clauses should be added

  const int lbdLimitAtImport;  ///< LBD limit for clause import
  const int sizeLimitAtImport; ///< Size limit for clause import
  unsigned
    m_freeSize; ///< Clause size not counted in the buffer at serialization

  float accumulatedAdmittedLiterals; ///< Accumulated number of literals shared
                                     ///< to parent
  float accumulatedDesiredLiterals; ///< Accumulated number of literals received
                                    ///< from children
  size_t
    lastEpochAdmittedLits; ///< Number of literals in last epoch final buffer
  size_t lastEpochReceivedLits; ///< Number of literals received in last epoch
  float compensationFactor;     ///< Factor for volume compensation
  float maxCompensationFactor;  ///< Maximum Compensation factor
  float estimatedIncomingLits;  ///< Estimated number of incoming literals
  float estimatedSharedLits;    ///< Estimated number of shared literals

  // Filter (To be moved when Filter Interface is established)
  // ======
public:
  /**
   * @brief Initializes the sharing filter with given parameters.
   *
   * @param resharingPeriod Period in microseconds before a clause can be
   * reshared.
   * @param sharingsPerSecond Number of sharing operations per second.
   * @param maxProducerId Maximum ID for producers (default 63, as we use a
   * 64-bit integer for tracking).
   * @throw std::invalid_argument If maxProducerId > 63 or sharingsPerSecond ==
   * 0.
   */
  void initializeFilter(uint resharingPeriod,
                        uint sharingsPerSecond,
                        uint maxProducerId = 63);

  /**
   * @brief Checks if a clause exists in the sharing map.
   *
   * @param cls Pointer to the clause to check.
   * @return true if the clause is present in the map, false otherwise.
   */
  bool doesClauseExist(const ClauseExchangePtr& cls) const;

  /**
   * @brief Updates the metadata for an existing clause.
   *
   * @param cls Pointer to the clause to update.
   * @note Updates the sources bitset and sets the production epoch to the
   * current epoch.
   */
  void updateClause(const ClauseExchangePtr& cls);

  /**
   * @brief Inserts a new clause or updates an existing one in the sharing map.
   *
   * @param cls Pointer to the clause to insert or update.
   * @return true if the clause was newly inserted, false if it was updated.
   *
   * @details If the clause doesn't exist:
   *          - Sets sharingEpoch to -resharingPeriodInEpochs
   *          - Initializes the sources bitset with the clause's origin
   *          - Sets productionEpoch to the current epoch
   *          If the clause exists:
   *          - Updates the existing clause information
   */
  bool insertClause(const ClauseExchangePtr& cls);

  /**
   * @brief Checks if a clause has been shared recently.
   *
   * @param cls Pointer to the clause to check.
   * @return true if the clause is in the map and (currentEpoch - sharingEpoch
   * <= resharingPeriod), false otherwise.
   *
   * @note The condition is always true for newly inserted clauses due to
   * sharingEpoch initialization. This allows resharing of clauses not yet
   * removed from the filter.
   */
  bool isClauseShared(const ClauseExchangePtr& cls) const;

  /**
   * @brief Determines if a consumer can import a given clause.
   *
   * @param cls Pointer to the clause to check.
   * @param consumerId ID of the consumer attempting to import the clause.
   * @return true if the consumer can import the clause, false otherwise.
   *
   * @details Checks if the clause is in the filter (produced but not shared)
   * and if the consumer was not one of its producers.
   */
  bool canConsumerImportClause(const ClauseExchangePtr& cls,
                               unsigned consumerId);

  /**
   * @brief Increments the current epoch.
   */
  void incrementEpoch();

  /**
   * @brief Marks a clause as shared, updating its metadata.
   *
   * @param cls Pointer to the clause to mark as shared.
   *
   * @details Updates the shared epoch to the current epoch and resets the
   * sources bitset to 0.
   */
  void markClauseAsShared(ClauseExchangePtr& cls);

  /**
   * @brief Shrinks the filter to remove entries of clauses that can be
   * reshared. Once a clause was shared m_resharingPeriod rpochs back it can be
   * reshared.
   * @return The number of entries removed from the filter.
   */
  size_t shrinkFilter();

private:
  unsigned m_sharingPerSecond; /**< Number of sharing operations per second. */
  unsigned m_maxProducerId;    /**< Maximum ID for producers, equivalent to the
                                  maximum ID in the producers. */
  int m_currentEpoch;          /**< Current epoch or round. */
  int m_resharingPeriodInEpochs; /**< Resharing period in epochs, computed using
                                    sharingPerSecond and given period in
                                                                    microseconds.
                                  */

  /**
   * @brief Map storing clause metadata.
   *
   * Uses custom hash and equality functions for ClauseExchangePtr keys.
   */
  std::unordered_map<ClauseExchangePtr,
                     ClauseMeta,
                     ClauseUtils::ClauseExchangePtrHash,
                     ClauseUtils::ClauseExchangePtrEqual>
    m_clauseMetaMap;
};
#endif