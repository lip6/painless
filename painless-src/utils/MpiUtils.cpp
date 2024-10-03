#include "MpiUtils.h"
#include <mpi.h>
#include "Logger.h"
#include "painless.h"
#include "protobuf/Clauses.pb.h"

#ifndef NDIST
int mpi_rank = -1;
int world_size = -1;
int mpi_winner = -1;
#else
int mpi_rank = 0;
int world_size = 0;
int mpi_winner = 0;
#endif

bool serializeClauses(const std::vector<simpleClause> &clauses, std::vector<char> &serializedClauses)
{
    Clauses protobufClauses;
    for (const auto &clause : clauses)
    {
        Clause *tempClause = protobufClauses.add_clauses();
        for (auto lit : clause)
            tempClause->add_literals(lit);
    }
    size_t size = protobufClauses.ByteSizeLong();
    serializedClauses.resize(size);

    return protobufClauses.SerializeToArray(serializedClauses.data(), size);
}

bool deserializeClauses(const std::vector<char> &serializedClauses, std::vector<simpleClause> &clauses)
{
    Clauses protobufClauses;
    if (!protobufClauses.ParseFromArray(serializedClauses.data(), serializedClauses.size()))
    {
        LOGERROR("Error parsing protobuf message");
        return false;
    }

    clauses.clear();
    for (const auto &clause : protobufClauses.clauses())
    {
        simpleClause tempClause;
        for (int i = 0; i < clause.literals_size(); ++i)
            tempClause.push_back(clause.literals(i));
        clauses.push_back(tempClause);
    }

    return true;
}

bool sendFormula(std::vector<simpleClause> &clauses, unsigned *varCount, int rootRank)
{

    // Bcast varCount
    MPI_Bcast(varCount, 1, MPI_UNSIGNED, rootRank, MPI_COMM_WORLD);

    LOGDEBUG1("VarCount = %u", *varCount);

    u_int64_t bufferSize;
    std::vector<char> serializedClauses;

    if (mpi_rank == rootRank)
    {
        LOGDEBUG1("Root clauses number: %u", clauses.size());
        // Serialize clauses
        if (!serializeClauses(clauses, serializedClauses))
        {
            LOGERROR("Error serializing clauses");
            return false;
        }

        // compress serializeClauses

        bufferSize = serializedClauses.size();
    }

    // Need to broadcast the size of the buffer first to be able to use Bcast.
    MPI_Bcast(&bufferSize, 1, MPI_UNSIGNED_LONG_LONG, rootRank, MPI_COMM_WORLD);

    LOGDEBUG1("Size of buffer is %lu", bufferSize);

    if (mpi_rank != rootRank)
        serializedClauses.resize(bufferSize);

    MPI_Bcast(serializedClauses.data(), bufferSize, MPI_CHAR, rootRank, MPI_COMM_WORLD);

    if (mpi_rank != rootRank)
    {
        // deserialize serializedClauses into clauses
        if (!deserializeClauses(serializedClauses, clauses))
        {
            LOGERROR("Error deserializing clauses");
            return false;
        }
        LOGDEBUG1("Workers, deserialized %u clauses", clauses.size());
    }
    return true;
}