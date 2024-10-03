#pragma once

#include "ErrorCodes.h"
#include <vector>
#include "ClauseUtils.h"

#define MYMPI_END 2012
#define MYMPI_CLAUSES 1
#define MYMPI_OK 2
#define MYMPI_NOTOK 3
#define MYMPI_MODEL 4

#define COLOR_YES 10

#define TESTRUNMPI(func)                 \
    do                                   \
    {                                    \
        if (dist && func != MPI_SUCCESS) \
        {                                \
            LOGERROR("MPI ERROR");       \
            exit(PERR_MPI);              \
        }                                \
    } while (0)

/// @brief Mpi rank of this process
extern int mpi_rank;

/// @brief The number of mpi processes launched
extern int world_size;

/// @brief Mpi rank of the winner
extern int mpi_winner;

/// @brief Sends a formula represented by clauses and variable count over MPI.
/// @param clauses The vector of Clauses representing the formula.
/// @param varCount The number of variables in the formula (pointer).
/// @param rootRank The mpi process rank broadcasting the formula.
/// @return Returns true if the formula was successfully received, false otherwise.
bool sendFormula(std::vector<simpleClause> &clauses, unsigned *varCount, int rootRank);

/// @brief Serializes a vector of Clauses into a vector of integers.
/// @param clauses The vector of Clauses to be serialized.
/// @param serializedClauses The vector where the serialized protobuf clauses will be stored.
/// @return Returns true if the serialization was successful, false otherwise.
/// @todo A version with a function parameter for GlobalSharingStrategies
bool serializeClauses(const std::vector<simpleClause> &clauses, std::vector<char> &serializedClauses);

/// @brief Deserializes a vector of characters into a vector of simpleClause objects.
/// @param serializedClauses The vector of characters that represents the serialized protobuf array.
/// @param clauses The vector where the deserialized simpleClause objects will be stored.
/// @return Returns true if the deserialization was successful, false otherwise.
bool deserializeClauses(const std::vector<char> &serializedClauses, std::vector<simpleClause> &clauses);