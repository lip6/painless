#pragma once

#include "GlobalSharingStrategy.hpp"

/**
 * @class AllGatherSharing
 * @brief Implements a global sharing strategy using MPI_Allgather for clause exchange.
 *
 * @ingroup global_sharing
 *
 * This class extends GlobalSharingStrategy to implement a specific sharing mechanism
 * using MPI's Allgather collective operation. Here, the size of the buffer is strict,
 * and the value totalSize should take into account the metadata.
 */
class AllGatherSharing : public GlobalSharingStrategy
{
  public:
	/**
	 * @brief Constructor for AllGatherSharing.
	 * @param clauseDB Shared pointer to the clause database.
	 * @param bufferSize Size of the buffer to send to all the other processes.
	 */
	AllGatherSharing(const std::shared_ptr<ClauseDatabase>& clauseDB, unsigned long bufferSize);

	/**
	 * @brief Destructor for AllGatherSharing.
	 */
	~AllGatherSharing();

	/**
	 * @brief Performs the clause sharing operation.
	 * A group is created for the process willing to shared only. The different processes
	 * serialize their clauses and share them via a single MPI_Allgather call.
	 *
	 * @todo a real decision on willing to share or not.
	 * @return True if sharing is complete and the executor can terminate, false otherwise.
	 */
	bool doSharing() override;

	/**
	 * @brief Initializes MPI-related variables.
	 * @return True if initialization was successful, false otherwise.
	 */
	bool initMpiVariables() override;

	/**
	 * @brief Handles the process of joining when a solution is found.
	 * @param winnerRank The rank of the mpi_process that found the solution.
	 * @param res The result of the SAT solving process.
	 * @param model The satisfying assignment, if any.
	 */
	void joinProcess(int winnerRank, SatResult res, const std::vector<int>& model) override;

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
	void deserializeClauses(const std::vector<int>& serialized_v_cls, int num_buffers);

	int totalSize; ///< Total size of the buffer for clause sharing
	int color;	   ///< Color used for MPI communicator splitting

	std::vector<int> clausesToSendSerialized; ///< Buffer for serialized clauses to send
	std::vector<int> receivedClauses;		  ///< Buffer for received serialized clauses

	BloomFilter b_filter; ///< Bloom filter for duplicate clause detection
};