#pragma once

#include "ErrorCodes.hpp"
#include "containers/ClauseUtils.hpp"
#include <vector>

#define MY_MPI_END 2012
#define MYMPI_CLAUSES 1
#define MYMPI_BITSET 1
#define MYMPI_OK 2
#define MYMPI_NOTOK 3
#define MYMPI_MODEL 4

#define COLOR_YES 10

#define TESTRUNMPI(func)                                                                                               \
	do {                                                                                                               \
		int result = (func);                                                                                           \
		if (dist && result != MPI_SUCCESS) {                                                                           \
			LOGERROR("MPI ERROR: %d in function %s", result, #func);                                                   \
			exit(PERR_MPI);                                                                                            \
		}                                                                                                              \
	} while (0)

/// @brief Mpi rank of this process
extern int mpi_rank;

/// @brief The number of mpi processes launched
extern int mpi_world_size;

/// @brief Mpi rank of the winner
extern int mpi_winner;

/// @ingroup utils
/// @brief A set of helper functions for distributed solver initialization and finalization using MPI
namespace mpiutils {

/// @brief Sends a formula represented by clauses and variable count over MPI.
/// @param clauses The vector of Clauses representing the formula.
/// @param varCount The number of variables in the formula (pointer).
/// @param rootRank The mpi process rank broadcasting the formula.
/// @return Returns true if the formula was successfully received, false otherwise.
bool
sendFormula(std::vector<simpleClause>& clauses, unsigned int* varCount, int rootRank);

/// @brief Serializes a vector of Clauses into a vector of integers.
/// @param clauses The vector of Clauses to be serialized.
/// @param serializedClauses The vector where the serialized clauses will be stored.
/// @return Returns true if the serialization was successful, false otherwise.
/// @todo A version with a function parameter for GlobalSharingStrategies
bool
serializeClauses(const std::vector<simpleClause>& clauses, std::vector<int>& serializedClauses);

/// @brief Deserializes a vector of characters into a vector of simpleClause objects.
/// @param serializedClauses The vector of characters that represents the serialized  array.
/// @param clauses The vector where the deserialized simpleClause objects will be stored.
/// @return Returns true if the deserialization was successful, false otherwise.
bool
deserializeClauses(const std::vector<int>& serializedClauses, std::vector<simpleClause>& clauses);

/// @brief The winner determined by the root will send the model if the answer was SATISFIABLE
void
sendModelToRoot();

}