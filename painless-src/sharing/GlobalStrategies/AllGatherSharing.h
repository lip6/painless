#ifndef NDIST
#pragma once

#include "GlobalSharingStrategy.h"

/// @brief An inter mpi process sharing strategy using the allgather mpi procedure to exchange clauses.
/// @details This strategy uses a static size for the buffer to send via MPI. It is made like this to not have to exchange the different sizes between the nodes. So zeroes are added to fill the buffer, please check description of getSerialization() and deserializeClauses()
/// \ingroup global_sharing
class AllGatherSharing : public GlobalSharingStrategy
{
public:
    /// @brief Constructor: If the -nb-bloom-gstrat is set to 2 then b_filters[0] is used for clausesToSend and b_filters[1] is used for receivedClauses.
    AllGatherSharing( std::shared_ptr<GlobalDatabase> g_base);

    /// @brief Destructor
    ~AllGatherSharing();

    /// @brief  The method called by the global sharer to do the actual sharing
    /// @return true if global sharer can break and join, false otherwise
    bool doSharing() override;

    /// @brief Post constructor initialization done after MPI_Init()
    bool initMpiVariables() override;

    void joinProcess(int winnerRank, SatResult res, const std::vector<int> &model) override;

protected:
     /// @brief A function that gets the clauses to send and serializes them to an int vector 
    /// @details the pattern is : 6 -7 5 15 lbd 0 65 -4 36 78 24 lbd 0 0 0 0 0. The clauses are serialized with their lbd and separated by zeroes '0'. If the clauses do not fill the buffer, it is filled with zeroes. If a clauses does not fit in the buffer, it is reinserted to the database.
    /// @param serialized_v_cls the vector that will hold the serialization (it is an append operation !!)
    /// @return the number of clauses serialized
    int serializeClauses(std::vector<int> &serialized_v_cls);

    /// @brief A function that deserialize clauses from an int vector and adds them to the receivedClauses database.
    /// @details Since it is an allgather, the received buffer will contain a gather of all the buffers, so to no iterate through the zeroes used to fill the buffers, the parameters oneVectorSize and vectorCount are used to jump these zeroes. Since this uses a bloom filter, duplicates will not be added to the local database.
    /// @param serialized_v_cls the vector with the serialized data
    /// @param oneVectorSize the size of one vector in the serialization (since it may contain multiple ones)
    /// @param vectorCount the number of vectors present in the serialization
    void deserializeClauses(std::vector<int> &serialized_v_cls, unsigned oneVectorSize, unsigned vectorCount);

   /// @brief the size of the buffer to send
    int totalSize;

    /// @brief The color is used to create the group of the mpi processes willing to share.
    int color;

    /// @brief Used to manipulate serialized clauses temporarely before moving them.
    std::vector<int> clausesToSendSerialized;
    std::vector<int> receivedClauses;

    /// @brief bloom filter used to both detect duplicate when serializing and also deserializing
    /// @details since an allgather is performed each round, the clauses received on a node are all received by the others, so no need to send them in next rounds. And if it was sent, it means I am the origin
    BloomFilter b_filter;

};
#endif