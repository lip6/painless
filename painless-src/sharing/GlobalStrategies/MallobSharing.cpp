#ifndef NDIST

#include "MallobSharing.h"
#include "utils/ClauseUtils.h"
#include "utils/Parameters.h"
#include "utils/MpiUtils.h"
#include "utils/Logger.h"
#include "painless.h"
#include <cmath>
#include <algorithm>
#include <random>

MallobSharing::MallobSharing(std::shared_ptr<GlobalDatabase> g_base) : GlobalSharingStrategy(g_base)
{
    this->maxClauseSize = Parameters::getIntParam("max-cls-size", 30);
    this->totalSize = 0;
    this->defaultSize = Parameters::getIntParam("gshr-lit", 1500 * Parameters::getIntParam("c", 28));
    requests_sent = false;

    // root_end = 0;
}

MallobSharing::~MallobSharing()
{
}

void MallobSharing::joinProcess(int winnerRank, SatResult res, const std::vector<int> &model)
{
    this->GlobalSharingStrategy::joinProcess(winnerRank, res, model);
}

bool MallobSharing::initMpiVariables()
{
    if (world_size < 2)
    {
        LOG("[Mallob] I am alone or MPI was not initialized, no need for distributed mode, initialization aborted");
        return false;
    }
    // [0]: parent, [1]: right, [2]: left. If it has one child it must be a right one
    right_child = (mpi_rank * 2 + 1 < world_size) ? mpi_rank * 2 + 1 : MPI_UNDEFINED;
    left_child = (mpi_rank * 2 + 2 < world_size) ? mpi_rank * 2 + 2 : MPI_UNDEFINED;
    nb_children = (right_child == MPI_UNDEFINED) ? 0 : (left_child == MPI_UNDEFINED) ? 1
                                                                                     : 2;
    father = (mpi_rank == 0) ? MPI_UNDEFINED : (mpi_rank - 1) / 2;
    LOG1("[Mallob] parent:%d, left: %d, right: %d", father, left_child, right_child);

    // the equation is described in mallob paper and the best value for alpha: https://doi.org/10.1007/978-3-030-80223-3_35
    // int nb_buffers_aggregated = countDescendants(world_size, mpi_rank);
    // this->totalSize = nb_buffers_aggregated * pow(0.875, log2(nb_buffers_aggregated)) * default_size;
    return GlobalSharingStrategy::initMpiVariables();
}

// check if can be done with Recv_Init
bool MallobSharing::doSharing()
{
    MPI_Status status;

    /* Ending Detection */
    if (GlobalSharingStrategy::doSharing())
    {
        this->joinProcess(mpi_winner, finalResult, {});
        return true;
    }

    // Sharing Management
    std::vector<int> my_clauses_buffer;
    int received_buffer_size = 0;
    int root_size = 0;
    int buffer_size = 0;
    int nb_buffers_aggregated = 1; // my buffer is accounted here

    // SHARING
    //========
    // if not leaf , wait for children clauses
    if (nb_children >= 1) // has a right child wanting to share
    {
        receivedClauses.clear();

        MPI_Probe(right_child, MYMPI_CLAUSES, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_INT, &received_buffer_size);
        receivedClauses.resize(received_buffer_size);

        MPI_Recv(&receivedClauses[0], received_buffer_size, MPI_INT, right_child, MYMPI_CLAUSES, MPI_COMM_WORLD, &status);

        nb_buffers_aggregated += receivedClauses.back(); // get nb buffers aggregated of my right child
        receivedClauses.pop_back();                      // remove the nb_buffers_aggregated
        // add the clauses to the mergeBuffer
        buffers.push_back(std::move(receivedClauses));

        if (nb_children == 2) // has a left child wanting to share
        {
            receivedClauses.clear();

            MPI_Probe(left_child, MYMPI_CLAUSES, MPI_COMM_WORLD, &status);
            MPI_Get_count(&status, MPI_INT, &received_buffer_size);
            receivedClauses.resize(received_buffer_size);

            MPI_Recv(&receivedClauses[0], received_buffer_size, MPI_INT, left_child, MYMPI_CLAUSES, MPI_COMM_WORLD, &status);

            nb_buffers_aggregated += receivedClauses.back(); // get nb buffers aggregated of my right child
            receivedClauses.pop_back();                      // remove the nb_buffers_aggregated
            // add the clause to the mergeBuffer
            buffers.push_back(std::move(receivedClauses));
        }

        this->totalSize = nb_buffers_aggregated * pow(0.875, log2(nb_buffers_aggregated)) * defaultSize;
        LOG2("[Mallob] TotalSize = %d(%d)", totalSize, nb_buffers_aggregated);

        my_clauses_buffer.clear();

        serializeClauses(my_clauses_buffer);

        clausesToSendSerialized.clear();

        // add my clauses to the merge buffer
        buffers.push_back(std::move(my_clauses_buffer));

        // if I am leaf merge does nothing
        gstats.sharedClauses += mergeSerializedBuffersWithMine(buffers, clausesToSendSerialized);

        clausesToSendSerialized.push_back(nb_buffers_aggregated); // number of buffers aggregated

        // Send to my parent the merge result,
        MPI_Send(&clausesToSendSerialized[0], clausesToSendSerialized.size(), MPI_INT, father, MYMPI_CLAUSES, MPI_COMM_WORLD);
        gstats.messagesSent++;
    }
    else // leaf
    {
        this->totalSize = nb_buffers_aggregated * pow(0.875, log2(nb_buffers_aggregated)) * defaultSize;
        LOG2("[Mallob] TotalSize = %d(%d)", totalSize, nb_buffers_aggregated);
        
        gstats.sharedClauses += serializeClauses(my_clauses_buffer);
        my_clauses_buffer.push_back(nb_buffers_aggregated); // number of buffers aggregated

        //  I am a leaf so i will only send my clauses (no merge)
        MPI_Send(&my_clauses_buffer[0], my_clauses_buffer.size(), MPI_INT, father, MYMPI_CLAUSES, MPI_COMM_WORLD);
        gstats.messagesSent++;
    }

    // If not root wait for parent
    if (father != MPI_UNDEFINED)
    {
        // Wait for my parent's response
        // Now I will wait for my father's response
        MPI_Probe(father, MYMPI_CLAUSES, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_INT, &received_buffer_size);
        receivedClauses.resize(received_buffer_size);

        MPI_Recv(&receivedClauses[0], received_buffer_size, MPI_INT, father, MYMPI_CLAUSES, MPI_COMM_WORLD, &status);
    }
    else
    {
        receivedClauses = std::move(clausesToSendSerialized);
        received_buffer_size = receivedClauses.size();
    }

    // Response to my children
    if (nb_children >= 1)
    {
        MPI_Send(&receivedClauses[0], received_buffer_size, MPI_INT, right_child, MYMPI_CLAUSES, MPI_COMM_WORLD);
        gstats.messagesSent++;
        if (nb_children == 2)
        {
            MPI_Send(&receivedClauses[0], received_buffer_size, MPI_INT, left_child, MYMPI_CLAUSES, MPI_COMM_WORLD);
            gstats.messagesSent++;
        }
    }

    // deserialize for all
    deserializeClauses(receivedClauses, &GlobalDatabase::addReceivedClause);

    LOG2("[Mallob] received cls %u shared cls %d", this->gstats.receivedClauses, this->gstats.sharedClauses);
    return false;
}

//==============================
// Serialization/Deseralization
//==============================

// Pattern: this->idSharer 2 6 lbd 0 6 -9 3 5 lbd 0 -4 -2 lbd 0 nb_buffers_aggregated
int MallobSharing::serializeClauses(std::vector<int> &serialized_v_cls)
{
    int clausesSelected = 0;
    std::shared_ptr<ClauseExchange> tmp_cls;

    unsigned dataCount = serialized_v_cls.size(); // it is an append operation so datacount do not start from zero

    LOGDEBUG1("Serializing my local clauses, dataCount = %u, totalSize = %u", dataCount, totalSize);

    while (dataCount < totalSize && globalDatabase->getClauseToSend(tmp_cls))
    {
        // to not overflow the sending buffer: do not add if the clause size + lbd and the 0 will cause an overflow
        if (dataCount + tmp_cls->size + 2 > totalSize)
        {
            LOGDEBUG1("Mallob] Serialization overflow avoided, %d/%d, wanted to add %d", dataCount, totalSize, tmp_cls->size + 2);
            globalDatabase->importClause(tmp_cls); // reinsert if doesn't fit
            break;
        }

        // check if not received previously and is not already serialized or sent previously
        if (!this->b_filter_final.contains(tmp_cls->lits))
        {
            serialized_v_cls.insert(serialized_v_cls.end(), tmp_cls->lits.begin(), tmp_cls->lits.end());
            serialized_v_cls.push_back((tmp_cls->lbd == 0) ? -1 : tmp_cls->lbd);
            serialized_v_cls.push_back(0);
            this->b_filter_final.insert(tmp_cls->lits);
            clausesSelected++;

            dataCount += (2 + tmp_cls->size);
            // lbd and 0 and clause size
            // decrement ref
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

void MallobSharing::deserializeClauses(std::vector<int> &serialized_v_cls, bool (GlobalDatabase::*add)(std::shared_ptr<ClauseExchange> cls))
{
    std::vector<int> tmp_cls;
    int current_cls = 0;
    int nb_clauses = 0;
    int lbd;
    const unsigned source_id = globalDatabase->getId();
    const unsigned buffer_size = serialized_v_cls.size();
    std::shared_ptr<ClauseExchange> p_cls;

    LOGDEBUG1("Deserializing vector of size %u", buffer_size);

    for (int i = 0; i < buffer_size; i++)
    {
        if (serialized_v_cls[i] == 0)
        {
            // this makes a move if possible, then tmp_cls is moved in clause_init.
            // std::move(v_cls.begin() + current_cls, v_cls.begin() + i, tmp_cls.begin()); // doesn't work right know
            tmp_cls.clear();
            tmp_cls.insert(tmp_cls.end(), serialized_v_cls.begin() + current_cls, serialized_v_cls.begin() + i);
            lbd = (tmp_cls.back() == -1) ? 0 : tmp_cls.back();
            tmp_cls.pop_back();

            if (!this->b_filter_final.contains(tmp_cls))
            {
                p_cls = std::make_shared<ClauseExchange>(std::move(tmp_cls), lbd, source_id);
                if ((globalDatabase.get()->*add)(p_cls))
                    gstats.receivedClauses++;
                this->b_filter_final.insert(tmp_cls); // either added or not wanted (>maxclauseSize)
            }
            else
            {
                gstats.receivedDuplicas++;
            }

            current_cls = i + 1; // jumps the 0
        }
    }
}

int MallobSharing::mergeSerializedBuffersWithMine(std::vector<std::vector<int>> &buffers, std::vector<int> &result)
{
    unsigned dataCount = 0;
    unsigned nb_clauses = 0;
    unsigned lbd = 0;
    unsigned buffer_count = buffers.size();
    std::vector<unsigned> indexes(buffer_count, 0);
    std::vector<unsigned> buffer_sizes(buffer_count);
    std::vector<unsigned> current_cls(buffer_count, 0);
    std::vector<std::vector<int>> tmp_clauses;

    std::vector<std::vector<int>>::iterator min_cls;

    // Temporary bloom filter to not push twice the same clause, no need to check with b_filter_final since everyone checks at serialization
    BloomFilter b_filter;

    for (int i = 0; i < buffer_count; i++)
    {
        buffer_sizes[i] = buffers[i].size();
    }

    while (dataCount < totalSize) // while I can add data
    {
        // check if a buffer was all seen; if all buffers are seen, loop will consume tmp_clauses before breaking
        for (int k = 0; k < buffer_count; k++)
        {
            if (indexes[k] >= buffer_sizes[k]) // a buffer is all seen, erase its cell in all vectors
            {
                LOGDEBUG1("Mallob] Buffer at %d will be removed since it is empty", k);
                buffers.erase(buffers.begin() + k);
                indexes.erase(indexes.begin() + k);
                buffer_sizes.erase(buffer_sizes.begin() + k);
                current_cls.erase(current_cls.begin() + k);
                buffer_count--;
                k--; // since now element at k is the next one
            }
            else // I can produce a new clause
            {
                if (buffers[k][indexes[k]] == 0) // clause finished
                {
                    tmp_clauses.emplace_back(std::vector<int>(buffers[k].begin() + current_cls[k], buffers[k].begin() + indexes[k] + 1)); // i+1 so that the zero is included since we need a serialized buffer at the end
                    current_cls[k] = indexes[k] + 1;                                                                                      // won't go out of bound since if i == size the buffer is removed
                }
                indexes[k]++;
            }
        }

        if (tmp_clauses.size() == buffer_count && buffer_count > 0 || tmp_clauses.size() > 0 && buffer_count == 0) // there is enough clauses to compare OR add the last clauses remaining
        {
            // chooses the min by size, and if the same compare the lbds
            // iterator_of_min - iterator_begin = index
            min_cls = std::min_element(tmp_clauses.begin(), tmp_clauses.end(), [](const std::vector<int> &a, const std::vector<int> &b)
                                       { return (a.size() < b.size() || (a.size() == b.size() && a.back() <= b.back())); });
            if (dataCount + min_cls->size() <= totalSize)
            {
                if (!b_filter.contains_or_insert(std::vector(min_cls->begin(), min_cls->end() - 2))) // check without the lbd and 0
                {
                    dataCount += min_cls->size();
                    result.insert(result.end(), min_cls->begin(), min_cls->end());
                    nb_clauses++;
                }
                else
                {
                    gstats.sharedDuplicasAvoided++;
                }

                min_cls->clear();
                tmp_clauses.erase(min_cls);
            }
            else
            {
                LOGDEBUG1("Overflow avoided[1] , %d / %d ", dataCount, totalSize);
                break;
            }
        }
        else if (tmp_clauses.size() == 0 && buffer_count == 0) // no more data to be added
            break;
    }

    // adding remaining clauses to clausesToSend database
    if (buffer_count > 0)
    { // clauses remained
        std::vector<std::shared_ptr<ClauseExchange>> v_cls;
        std::vector<int> tmp_buff;
        for (int j = 0; j < buffer_count; j++)
        {
            tmp_buff.insert(tmp_buff.end(), buffers[j].begin() + current_cls[j], buffers[j].end());
        }
        for (auto cls : tmp_clauses)
        {
            tmp_buff.insert(tmp_buff.end(), cls.begin(), cls.end());
        }
        deserializeClauses(tmp_buff, &GlobalDatabase::importClause);
    }

    return nb_clauses;
}

#endif