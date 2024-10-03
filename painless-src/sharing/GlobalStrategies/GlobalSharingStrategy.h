#ifndef NDIST
#pragma once

#include "sharing/SharingStatistics.h"
#include "sharing/SharingStrategy.h"
#include "utils/BloomFilter.h"
#include "sharing/GlobalDatabase.h"

#include <mpi.h>

/// \defgroup global_sharing Global Sharing Strategies
/// \ingroup sharing_strat
/// \ingroup global_sharing
/// @brief The interface for all the different inter mpi processes sharing strategies
class GlobalSharingStrategy : public SharingStrategy
{

public:
    /// @brief Constructors
    /// @param id the id the sharer using this strategy
    GlobalSharingStrategy(std::shared_ptr<GlobalDatabase> g_base);

    ///  Destructor
    virtual ~GlobalSharingStrategy();

    /// @brief A function used to get the sleeping time of a sharer
    /// @return number of millisecond to sleep
    ulong getSleepingTime() override;

    virtual void joinProcess(int winnerRank, SatResult res, const std::vector<int> &model);

    /// @brief Function called to print the stats of the strategy
    void printStats() override;

    /// @brief Post constructor initialization done after MPI_Init()
    /// @return true if the initialization was done correctly, false otherwise
    virtual bool initMpiVariables();

    virtual bool doSharing();

protected:
    /// @brief GlobalDatabase of the globalSharingStrategy
    std::shared_ptr<GlobalDatabase> globalDatabase;

    /// Sharing statistics.
    GlobalSharingStatistics gstats;

    // Temporary
    // ---------
    /// @brief Bool to know if request to end was sent to the root
    bool requests_sent;

    /// @brief Requests of the non blocking receive end
    std::vector<MPI_Request> recv_end_requests;

    /// @brief Non root Isend request
    MPI_Request send_end_request;

    /// @brief Stores the result received from others
    std::vector<SatResult> receivedFinalResultRoot;
};

#endif