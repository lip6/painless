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
	 * @param clauseDB Shared pointer to the clause database.
	 * @param subscriptions Vector of MPI ranks to receive clauses from.
	 * @param subscribers Vector of MPI ranks to send clauses to.
	 * @param bufferSize Size of the buffer to send to subscribers
	 */
	GenericGlobalSharing(const std::shared_ptr<ClauseDatabase>& clauseDB,
						 const std::vector<int>& subscriptions,
						 const std::vector<int>& subscribers,
						 unsigned long bufferSize);

	/**
	 * @brief Destructor for GenericGlobalSharing.
	 */
	~GenericGlobalSharing();

	/**
	 * @brief Initializes MPI-related variables.
	 * @return true if initialization was successful, false otherwise.
	 */
	bool initMpiVariables() override;

	/**
	 * @brief Performs the clause sharing operation. An MPI process starts by serializing its clauses then sends them
	 * via asynchronous sends to all its subscribers, then waits on the reception of its different subscriptions.
	 * @return true if sharing is complete and the process can terminate, false otherwise.
	 */
	bool doSharing() override;

	/**
	 * @brief Handles the process of joining when a solution is found.
	 * @param winnerRank The rank of the process that found the solution.
	 * @param res The result of the SAT solving process.
	 * @param model The satisfying assignment, if any.
	 */
	void joinProcess(int winnerRank, SatResult res, const std::vector<int>& model) override;

  protected:
	/**
	 * @brief Serializes clauses for sharing.
	 * @param serialized_v_cls Vector to store the serialized clauses.
	 * @details Serialization Pattern ([size][lbd][literals])*
	 * @return The number of clauses serialized.
	 */
	int serializeClauses(std::vector<int>& serialized_v_cls);

	/**
	 * @brief Deserializes received clauses.
	 * @param serialized_v_cls Vector containing the serialized clauses.
	 */
	void deserializeClauses(std::vector<int>& serialized_v_cls);

	unsigned int totalSize; ///< Total size of the buffer for clause sharing

	std::vector<MPI_Request> sendRequests; ///< MPI requests for non-blocking sends

	BloomFilter b_filter_send; ///< Bloom filter for avoiding duplicate clause sends

	BloomFilter b_filter_recv; ///< Bloom filter for avoiding duplicate clause receives

	std::vector<int> clausesToSendSerialized; ///< Buffer for serialized clauses to send

	std::vector<int> receivedClauses; ///< Buffer for received serialized clauses

	std::vector<int> subscriptions; ///< List of MPI ranks to receive clauses from

	std::vector<int> subscribers; ///< List of MPI ranks to send clauses to
};