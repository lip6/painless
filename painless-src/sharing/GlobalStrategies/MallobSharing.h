#ifndef NDIST
#pragma once

#include <vector>
#include "GlobalSharingStrategy.h"

/// @brief An inter mpi processes sharing strategy using a tree topology, inspired by the Mallob paper
/// \ingroup global_sharing
class MallobSharing : public GlobalSharingStrategy
{
public:
    /// @brief Constructor: If the -nb-bloom-gstrat is set to 1 then b_filters[0] is used for clausesToSend and receivedClauses. else no bloom
    MallobSharing( std::shared_ptr<GlobalDatabase> g_base);
    /// Destructor
    ~MallobSharing();

    /// @brief Post constructor initialization done after MPI_Init()
    bool initMpiVariables() override;

    /// @brief The method called by the global sharer to do the actual sharing
    /// @return true if sharer can break and join, false otherwise
    bool doSharing() override;

    void joinProcess(int winnerRank, SatResult res, const std::vector<int> &model) override;

protected:
    /// @brief A function that serializes ClauseExchange from clausesToSend database to an int vector ( no bloom version)
    /// @param serialized_v_cls the vector that will hold the serialization (it is an append operation !!)
    /// @return the number of clauses serialized
    int serializeClauses(std::vector<int> &serialized_v_cls);

    /// @brief A function that deserialize a buffer of int to ClauseExchange object and adds them to a given database (no bloom version)
    /// @param serialized_v_cls the vector with the serialized data
    /// @param add defines the method to be called to add the deserialized clause to a database
    void deserializeClauses(std::vector<int> &serialized_v_cls, bool (GlobalDatabase::*add)(std::shared_ptr<ClauseExchange> cls));

    /// @brief A function that adds the received buffers to clauseTosend then generates the serialized buffer to send (no bloom version)
    /// @details after the k-merge, we keep the remaining received clauses since they may be still useful in future rounds
    /// @param buffers the list of received buffers to merge
    /// @param result the vector that will hold the result of the merge
    /// @return the number of clauses inside the resulted serialized vector
    int mergeSerializedBuffersWithMine(std::vector<std::vector<int>> &buffers, std::vector<int> &result);

    /// @brief the size of the buffer to send
    unsigned totalSize;

    /// @brief the default size given as parameter
    unsigned defaultSize;

    /// @brief The max clause size to accept (needed in merge)
    int maxClauseSize;

    /// @brief Nombre de bloom filter a utilisé.
    int bloomCount;

    /// @brief bloom filter deserialization(final buffer: received from root)
    /// @details since the final buffer is the same for all the nodes it is not worth sending the clauses next round.
    BloomFilter b_filter_final;

    /// @brief the buffers with the data to be merged.
    std::vector<std::vector<int>> buffers;

    /// @brief Used to manipulate serialized clauses temporarely before moving them.
    std::vector<int> clausesToSendSerialized;
    std::vector<int> receivedClauses;

    // Tree management
    // @brief Used before each sharing to check if should end or not
    // int root_end;

    /// @brief MPI rank of the parent, MPI_UNDIFINED if root
    int father;

    /// @brief MPI rank for left child if present otherwise MPI_UNDIFINED
    int left_child;

    /// @brief MPI rank for right child if present otherwise MPI_UNDIFINED
    int right_child;

    /// @brief number of children
    int nb_children;

    /// @brief to configure if a node add only the roots clauses to its receivedClauses database or also its children.
    bool addChildClauses;
};
#else
#endif