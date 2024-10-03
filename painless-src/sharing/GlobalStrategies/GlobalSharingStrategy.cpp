#ifndef NDIST

#include "GlobalSharingStrategy.h"
#include "painless.h"
#include "utils/Parameters.h"
#include "utils/MpiUtils.h"
#include <list>

GlobalSharingStrategy::GlobalSharingStrategy(std::shared_ptr<GlobalDatabase> g_base) : SharingStrategy()
{
    this->globalDatabase = g_base;
}

GlobalSharingStrategy::~GlobalSharingStrategy()
{
}

void GlobalSharingStrategy::printStats()
{
    LOGSTAT(" Global Strategy: receivedCls %d, sharedCls %d, receivedDuplicas %d, sharedDuplicasAvoided %d, messagesSent %d",
        gstats.receivedClauses, gstats.sharedClauses, gstats.receivedDuplicas, gstats.sharedDuplicasAvoided, gstats.messagesSent);
}

ulong GlobalSharingStrategy::getSleepingTime()
{
    return (Parameters::getIntParam("shr-sleep", 500000) * 3) / 2;
}

bool GlobalSharingStrategy::initMpiVariables()
{
    // For End Management
    if (mpi_rank == 0)
    {
        this->receivedFinalResultRoot.resize(world_size - 1);
        this->recv_end_requests.resize(world_size - 1);

        for (int i = 0; i < world_size - 1; i++)
        {
            TESTRUNMPI(MPI_Irecv(&receivedFinalResultRoot[i], 1, MPI_INT, i + 1, MYMPI_END, MPI_COMM_WORLD, &this->recv_end_requests[i]));
        }
    }
    return true;
}

void GlobalSharingStrategy::joinProcess(int winnerRank, SatResult res,
                                        const std::vector<int> &model)
{
    //=================================================================
    // Temporary
    // ---------
    MPI_Status status;

    if (requests_sent && 0 != mpi_rank)
    {
        MPI_Wait(&send_end_request, &status);
    }

    if (0 == mpi_rank)
    {
        int flag;
        for (int i = 0; i < world_size - 1; i++)
        {
            LOGDEBUG1("Root waiting to finalize recv with %d", i+1);
            MPI_Wait(&this->recv_end_requests[i], &status);
        }
    }
    //=================================================================
    globalEnding = true;

    finalResult = res;

    mpi_winner = winnerRank;

    if (res == SatResult::SAT && model.size() > 0)
    {
        finalModel = model;
    }

    if(res != SatResult::UNKNOWN && res != SatResult::TIMEOUT)
        LOGSTAT("The winner is mpi process %d", winnerRank);
    pthread_mutex_lock(&mutexGlobalEnd);
    pthread_cond_broadcast(&condGlobalEnd);
    pthread_mutex_unlock(&mutexGlobalEnd);
    LOGDEBUG1("Broadcasted the end");
}

bool GlobalSharingStrategy::doSharing()
{
    // Ending Management
    int end_flag;
    short int rank_winner = 0;

    MPI_Status status;

    int receivedFinalResultBcast = 0;

    /* This ending detection costs too much per round, djkstra algorithm to be implemented ? */
    if (globalEnding && !requests_sent && mpi_rank != 0) // send end only once
    {
        LOG1("[GStrat] It is the end, now I will send end to the root", mpi_rank);
        MPI_Isend(&finalResult, 1, MPI_INT, 0, MYMPI_END, MPI_COMM_WORLD, &this->send_end_request);
        requests_sent = true;
    }

    if (mpi_rank == 0)
    {
        if (globalEnding) // to check if the root found the solution
        {
            receivedFinalResultBcast = finalResult;
            LOGDEBUG1("[GStrat] It is the end, now I will send end to all descendants (%d)", receivedFinalResultBcast);
            if(receivedFinalResultBcast != (int)SatResult::TIMEOUT)
                        rank_winner = 0;
        }
        else // check recv only if root didn't end
        {
            for (int i = 0; i < world_size - 1; i++)
            {
                MPI_Test(&recv_end_requests[i], &end_flag, &status);
                if (end_flag)
                {
                    LOG1("[GStrat] Ending received from node %d (value: %d)", status.MPI_SOURCE, receivedFinalResultRoot[i]);
                    receivedFinalResultBcast = receivedFinalResultRoot[i];
                    if(receivedFinalResultBcast != (int)SatResult::TIMEOUT)
                        rank_winner = status.MPI_SOURCE;
                    // root_end = 1;
                    // end arrived to the root. Know it should tell everyone
                }
            }
        }
        
        receivedFinalResultBcast |= (int)(rank_winner << 16);
        LOGDEBUG1("toSend: %d, rank_winner: %d, shifted: %d", receivedFinalResultBcast, rank_winner, (int)(rank_winner << 16));
    }

    // Broadcast from 0 "most significant 16bits are the winner_rank"
    MPI_Bcast(&receivedFinalResultBcast, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (receivedFinalResultBcast != 0)
    {
        finalResult = static_cast<SatResult>(receivedFinalResultBcast&0x0000FFFF);
        mpi_winner = (receivedFinalResultBcast&0xFFFF0000) >> 16;
        LOGDEBUG1("[GStrat] It is the mpi end: %d",finalResult);
        if (!requests_sent && 0 != mpi_rank)
        {
            LOGDEBUG1("[GStrat] Sending last synchro message to root");
            MPI_Isend(&finalResult, 1, MPI_INT, 0, MYMPI_END, MPI_COMM_WORLD, &this->send_end_request);
        }
        return true;
    }
    return false;
}

#endif