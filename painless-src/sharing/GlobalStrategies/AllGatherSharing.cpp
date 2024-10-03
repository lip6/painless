#ifndef NDIST

#include "AllGatherSharing.h"
#include "utils/ClauseUtils.h"
#include "utils/Parameters.h"
#include "utils/MpiUtils.h"
#include "utils/Logger.h"
#include "painless.h"
#include <random>

AllGatherSharing::AllGatherSharing(std::shared_ptr<GlobalDatabase> g_base) : GlobalSharingStrategy(g_base)
{
    this->totalSize = Parameters::getIntParam("gshr-lit", 1500 * Parameters::getIntParam("c", 31));
    requests_sent = false;
}

AllGatherSharing::~AllGatherSharing()
{
}

void AllGatherSharing::joinProcess(int winnerRank, SatResult res, const std::vector<int> &model)
{
    this->GlobalSharingStrategy::joinProcess(winnerRank, res, model);
}

bool AllGatherSharing::initMpiVariables()
{
    if (world_size < 2)
    {
        LOG("[Allgather] I am alone or MPI was not initialized, no need for distributed mode, initialization aborted");
        return false;
    }

    return GlobalSharingStrategy::initMpiVariables();
}

bool AllGatherSharing::doSharing()
{
    bool can_break;
    MPI_Status status;

    // Sharing Management
    int yes_comm_size;
    MPI_Comm yes_comm;
    int sizeExport;


    /* Ending Detection */
    if (GlobalSharingStrategy::doSharing())
    {
        this->joinProcess(mpi_winner, finalResult, {});
        return true;
    }

    // SHARING
    //========

    // At wake up, get the clauses to export and check if empty
    if (requests_sent)
    {
        // if empty, not worth it or local is ending
        this->color = MPI_UNDEFINED; // to not include this mpi process in the yes_comm
    }
    else
    {
        this->color = COLOR_YES;
    }

    LOGDEBUG1("[Allgather] before splitting", mpi_rank);

    // The call must be done only when all global sharers can arrive here.
    if (MPI_Comm_split(MPI_COMM_WORLD, this->color, mpi_rank, &yes_comm) != MPI_SUCCESS)
    {
        LOGERROR("Error in spliting the MPI_COMM_WORLD by color, aborting the sharing!!");
        return false;
    }

    if (this->color == MPI_UNDEFINED)
    {
        LOG2("[Allgather] is not willing to share (%d)", mpi_rank);
    }
    else // do this only if member of yes_comm
    {
        MPI_Comm_set_errhandler(yes_comm, MPI_ERRORS_RETURN);

        if (MPI_Comm_size(yes_comm, &yes_comm_size) != MPI_SUCCESS)
        {
            LOGERROR("Error in getting the new COMM size, aborting the sharing!!");
            return false;
        }

        if (yes_comm_size < 2)
        {
            return true; // can break and end the global sharer thread since it is doing nothing
        }
        else
        {
            LOG3("[Allgather] %d global sharers will share their clauses", yes_comm_size);
        }

        // get clauses to send and serialize
        clausesToSendSerialized.clear();

        gstats.sharedClauses += serializeClauses(clausesToSendSerialized);
        // limit the receive buffer size to contain all the exported buffers
        // resize is used to really allocate the memory (reserve doesn't work with C array access)
        receivedClauses.clear();
        receivedClauses.resize((totalSize)*yes_comm_size);

        LOGDEBUG2("[Allgather] before allgather", mpi_rank);
        //  do not clear  the receivedClause vector before allgather to no cause segmentation faults, MPI access it in a C like fashion, it overwrites using pointer arithmetic
        if (MPI_Allgather(&clausesToSendSerialized[0], totalSize, MPI_INT, &receivedClauses[0], totalSize, MPI_INT, yes_comm) != MPI_SUCCESS)
        {
            LOGERROR("Error in AllGather for yes_comm, aborting sharing !!");
            // return true;
        }
        gstats.messagesSent += yes_comm_size;

        // Now I have a vector of the all the gathered buffers
        deserializeClauses(receivedClauses, totalSize, yes_comm_size);
    }

    LOG2("[Allgather] received cls %u shared cls %d", this->gstats.receivedClauses, this->gstats.sharedClauses);

    return false;
}

//===============================
// Serialization/Deseralization
//===============================

// Pattern: this->idSharer 2 6 lbd 0 6 -9 3 5 lbd 0 -4 -2 lbd 0 0 0 0 0 0 0 0 0 0 0 0 0
int AllGatherSharing::serializeClauses(std::vector<int> &serialized_v_cls)
{
    int nb_clauses = 0;
    unsigned dataCount = serialized_v_cls.size(); // it is an append operation so datacount do not start from zero
    std::shared_ptr<ClauseExchange> tmp_cls;

    while (dataCount <= totalSize && globalDatabase->getClauseToSend(tmp_cls)) // stops when buffer is full or no more clauses are available
    {
        if (dataCount + tmp_cls->size + 2 > totalSize - 1)
        {
            LOGDEBUG1("[Allgather] Serialization overflow avoided, %d/%d, wanted to add %d", dataCount, totalSize, tmp_cls->size + 2);
            globalDatabase->importClause(tmp_cls); // reinsert the clause to the database to not loose it
            break;
        }
        else
        {
            // check with bloom filter if clause will be sent. If already sent, the clause is directly released
            if (!this->b_filter.contains(tmp_cls->lits))
            {
                serialized_v_cls.insert(serialized_v_cls.end(), tmp_cls->lits.begin(), tmp_cls->lits.end());
                serialized_v_cls.push_back((tmp_cls->lbd == 0) ? -1 : tmp_cls->lbd);
                serialized_v_cls.push_back(0);
                this->b_filter.insert(tmp_cls->lits);
                nb_clauses++;

                dataCount += (2 + tmp_cls->size);
                // lbd and 0 and clause size
            }
            else
            {
                gstats.sharedDuplicasAvoided++;
            }
        }
    }

    // fill with zero if needed
    serialized_v_cls.insert(serialized_v_cls.end(), totalSize - dataCount, 0);
    return nb_clauses;
}

void AllGatherSharing::deserializeClauses(std::vector<int> &serialized_v_cls, unsigned oneVectorSize, unsigned vectorCount)
{
    std::vector<int> tmp_cls;
    int vectorsSeen = 0;
    int current_cls = 0;
    int nb_clauses = 0;
    int lbd = -1;
    std::shared_ptr<ClauseExchange> p_cls;
    int bufferSize = serialized_v_cls.size();

    for (int i = 0; i < bufferSize; i++)
    {
        if (current_cls >= bufferSize)
        {
            break;
        }
        if (serialized_v_cls[current_cls] == 0)
        { // 2 successive zeros jump to next vector if present
            ++vectorsSeen;
            LOG2("[Allgather] Deserialization stopped at %d for vector %d/%d", current_cls, vectorsSeen, vectorCount);
            if (vectorsSeen >= vectorCount)
            {
                break;
            }

            current_cls = oneVectorSize * vectorsSeen;
            i = current_cls;
            continue;
        }
        if (serialized_v_cls[i] == 0)
        {
            // this makes a move if possible, then tmp_cls is moved in clause_init.
            // std::move(v_cls.begin() + current_cls, v_cls.begin() + i, tmp_cls.begin()); // doesn't work right know
            tmp_cls.clear();
            tmp_cls.insert(tmp_cls.end(), serialized_v_cls.begin() + current_cls, serialized_v_cls.begin() + i);
            lbd = (tmp_cls.back() == -1) ? 0 : tmp_cls.back();
            tmp_cls.pop_back();

            if (!this->b_filter.contains(tmp_cls))
            {
                p_cls = std::make_shared<ClauseExchange>(std::move(tmp_cls), lbd, globalDatabase->getId());
                if (globalDatabase->addReceivedClause(p_cls))
                    gstats.receivedClauses++;
                this->b_filter.insert(tmp_cls); // either added or not wanted (> maxClauseSize)
            }
            else
            {
                gstats.receivedDuplicas++;
            }

            current_cls = i + 1; // jumps the 0
        }
    }
}
#endif