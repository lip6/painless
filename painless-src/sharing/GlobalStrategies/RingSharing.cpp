#ifndef NDIST

#include "RingSharing.h"
#include "utils/ClauseUtils.h"
#include "utils/Parameters.h"
#include "utils/MpiUtils.h"
#include "utils/Logger.h"
#include "painless.h"
#include <random>

RingSharing::RingSharing(std::shared_ptr<GlobalDatabase> g_base) : GlobalSharingStrategy(g_base)
{
    this->totalSize = Parameters::getIntParam("gshr-lit", 1500 * Parameters::getIntParam("c", 28));
    requests_sent = false;
}

RingSharing::~RingSharing()
{
   
}

void RingSharing::joinProcess(int winnerRank, SatResult res, const std::vector<int> &model)
{
    MPI_Cancel(&send_request_left);
    MPI_Request_free(&send_request_left);
    MPI_Cancel(&send_request_right);
    MPI_Request_free(&send_request_right);
    this->GlobalSharingStrategy::joinProcess(winnerRank, res, model);
}

bool RingSharing::initMpiVariables()
{
    if (world_size < 2)
    {
        LOG("[Ring] I am alone or MPI was not initialized , no need for distributed mode, initialization aborted");

        return false;
    }

    right_neighbor = (mpi_rank - 1 + world_size) % world_size;
    left_neighbor = (mpi_rank + 1) % world_size;

    LOG1("[Ring] left: %d, right: %d", left_neighbor, right_neighbor);

    // Bootstrap send requests: otherwise send_flags will not be true for the first sharing round
    MPI_Isend(nullptr, 0, MPI_INT, left_neighbor, MYMPI_CLAUSES, MPI_COMM_WORLD, &send_request_left);
    MPI_Isend(nullptr, 0, MPI_INT, right_neighbor, MYMPI_CLAUSES, MPI_COMM_WORLD, &send_request_right);

    return GlobalSharingStrategy::initMpiVariables();
}

bool RingSharing::doSharing()
{
    MPI_Status status;

    // Sharing Management
    int send_flag_left;
    int send_flag_right;
    int clauses_flag;
    int received_size;

     /* Ending Detection */
    if (GlobalSharingStrategy::doSharing())
    {
        this->joinProcess(mpi_winner, finalResult, {});
        return true;
    }

    // Sharing
    // =======

    // Test my two neighbors
    MPI_Test(&send_request_left, &send_flag_left, &status);
    MPI_Test(&send_request_right, &send_flag_right, &status);

    // Get clauses to send if at least one message was received
    if (send_flag_left || send_flag_right)
    {
        tmp_serializedClauses.clear();
        gstats.sharedClauses += serializeClauses(tmp_serializedClauses);
    }

    // Send to my two neighbors my clauses
    // send only if previous message was sent
    if (send_flag_left)
    {
        clausesToSendSerializedLeft.clear();
        clausesToSendSerializedLeft = tmp_serializedClauses; // copy before send
        MPI_Isend(&clausesToSendSerializedLeft[0], clausesToSendSerializedLeft.size(), MPI_INT, left_neighbor, MYMPI_CLAUSES, MPI_COMM_WORLD, &send_request_left);
        gstats.messagesSent++;
        LOG2("[Ring] Sent a message of size %d to my left neighbor %d", clausesToSendSerializedLeft.size(), left_neighbor);
    }

    // send only if previous message was sent
    if (send_flag_right)
    {
        clausesToSendSerializedRight.clear();
        clausesToSendSerializedRight = tmp_serializedClauses; // copy before send
        MPI_Isend(&clausesToSendSerializedRight[0], clausesToSendSerializedRight.size(), MPI_INT, right_neighbor, MYMPI_CLAUSES, MPI_COMM_WORLD, &send_request_right);
        gstats.messagesSent++;
        LOG2("[Ring] Sent a message of size %d to my right neighbor %d", clausesToSendSerializedRight.size(), right_neighbor);
    }

    // Check if my neighbors sent me any clauses
    MPI_Iprobe(left_neighbor, MYMPI_CLAUSES, MPI_COMM_WORLD, &clauses_flag, &status);
    if (clauses_flag)
    {
        MPI_Get_count(&status, MPI_INT, &received_size);
        receivedClauses.resize(received_size);
        MPI_Recv(&receivedClauses[0], received_size, MPI_INT, left_neighbor, MYMPI_CLAUSES, MPI_COMM_WORLD, &status);
        deserializeClauses(receivedClauses);
        LOG2("[Ring] Received a message of size %d from my left neighbor %d", received_size, left_neighbor);
    }

    MPI_Iprobe(right_neighbor, MYMPI_CLAUSES, MPI_COMM_WORLD, &clauses_flag, &status);
    if (clauses_flag)
    {
        MPI_Get_count(&status, MPI_INT, &received_size);
        receivedClauses.resize(received_size);
        MPI_Recv(&receivedClauses[0], received_size, MPI_INT, right_neighbor, MYMPI_CLAUSES, MPI_COMM_WORLD, &status);
        deserializeClauses(receivedClauses);
        LOG2("[Ring] Received a message of size %d from my right neighbor %d", received_size, right_neighbor);
    }

    LOG2("[Ring] received cls %u shared cls %d", this->gstats.receivedClauses, this->gstats.sharedClauses);

    return false;
}

//==============================
// Serialization/Deseralization
//==============================

// Pattern: this->idSharer 2 6 lbd 0 6 -9 3 5 lbd 0 -4 -2 lbd 0 nb_buffers_aggregated
int RingSharing::serializeClauses(std::vector<int> &serialized_v_cls)
{
    unsigned clausesSelected = 0;
    std::shared_ptr<ClauseExchange> tmp_cls;

    unsigned dataCount = serialized_v_cls.size();

    while (dataCount < totalSize && globalDatabase->getClauseToSend(tmp_cls))
    {
        if (dataCount + tmp_cls->size + 2 > totalSize)
        {
            LOGDEBUG1("[Ring] Serialization overflow avoided, %d/%d, wanted to add %d", dataCount, totalSize, tmp_cls->size + 2);
            globalDatabase->importClause(tmp_cls); // reinsert to clauseToSend if doesn't fit
            break;
        }

        // check with bloom filter if clause will be sent. If already sent, the clause is directly released
        if (!this->b_filter_send.contains(tmp_cls->lits))
        {
            serialized_v_cls.insert(serialized_v_cls.end(), tmp_cls->lits.begin(), tmp_cls->lits.end());
            serialized_v_cls.push_back((tmp_cls->lbd == 0) ? -1 : tmp_cls->lbd);
            serialized_v_cls.push_back(0);
            this->b_filter_send.insert(tmp_cls->lits);
            clausesSelected++;

            dataCount += (2 + tmp_cls->size);
        }
        else
        {
            gstats.sharedDuplicasAvoided++;
        }
    }

    if (dataCount > totalSize)
    {
        LOGWARN("Panic!! datacount(%d) > totalsize(%d). Deserialization will create erroenous clauses", dataCount, totalSize);
    }
    return clausesSelected;
}

void RingSharing::deserializeClauses(std::vector<int> &serialized_v_cls)
{
    std::vector<int> tmp_cls;
    int current_cls = 0;
    int nb_clauses = 0;
    int lbd;
    const unsigned source_id = globalDatabase->getId();
    const unsigned buffer_size = serialized_v_cls.size();
    std::shared_ptr<ClauseExchange> p_cls;

    for (int i = 0; i < buffer_size; i++)
    {
        if (serialized_v_cls[i] == 0)
        {
            tmp_cls.clear();
            tmp_cls.insert(tmp_cls.end(), serialized_v_cls.begin() + current_cls, serialized_v_cls.begin() + i);
            lbd = (tmp_cls.back() == -1) ? 0 : tmp_cls.back();
            tmp_cls.pop_back();

            if (!this->b_filter_received.contains(tmp_cls)) // check if received from another neighbor : worth it ?
            {
                p_cls = std::make_shared<ClauseExchange>(std::move(tmp_cls), lbd, source_id);
                if (globalDatabase->addReceivedClause(p_cls))
                    gstats.receivedClauses++;
                this->b_filter_received.insert(tmp_cls);

                // To propagate only if it is the first time it is received
                if (!this->b_filter_send.contains(p_cls->lits)) // check if already sent: tmp_cls was moved
                {
                    p_cls = std::make_shared<ClauseExchange>(std::move(tmp_cls), lbd, source_id);
                    globalDatabase->importClause(p_cls); // add to clausesToSend may not be added if > maxClauseSize

                    this->b_filter_send.insert(p_cls->lits);
                }
                else
                {
                    gstats.sharedDuplicasAvoided++;
                }
            }
            else
            {
                gstats.receivedDuplicas++;
            }

            current_cls = i + 1;
        }
    }
}
#endif