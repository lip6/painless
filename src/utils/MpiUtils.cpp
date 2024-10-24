#include "MpiUtils.hpp"
#include "Logger.hpp"
#include "painless.hpp"
#include <mpi.h>

#include <algorithm>
#include <numeric>
#include <vector>

#include <cstring>
#include <zlib.h>


int mpi_rank = -1;
int mpi_world_size = -1;
int mpi_winner = -1;

namespace mpiutils {

// Helper function for compression
static std::vector<unsigned char>
compressBuffer(const std::vector<int>& input)
{
	std::vector<unsigned char> output;
	unsigned long compressedSize = compressBound(input.size() * sizeof(int));
	output.resize(compressedSize);

	if (compress2(output.data(),
				  &compressedSize,
				  reinterpret_cast<const unsigned char*>(input.data()),
				  input.size() * sizeof(int),
				  Z_BEST_SPEED) != Z_OK) {
		throw std::runtime_error("Compression failed");
	}

	output.resize(compressedSize);
	return output;
}

// Helper function for decompression
static std::vector<int>
decompressBuffer(const std::vector<unsigned char>& input, size_t originalSize)
{
	std::vector<int> output(originalSize / sizeof(int));
	unsigned long decompressedSize = originalSize;

	if (uncompress(reinterpret_cast<unsigned char*>(output.data()), &decompressedSize, input.data(), input.size()) !=
		Z_OK) {
		throw std::runtime_error("Decompression failed");
	}

	return output;
}

typedef std::vector<int> simpleClause;

bool
serializeClauses(const std::vector<simpleClause>& clauses, std::vector<int>& serializedClauses)
{
	// Calculate the total size needed for serialization
	size_t totalSize = std::accumulate(clauses.begin(), clauses.end(), 0, [](size_t sum, const simpleClause& clause) {
		return sum + clause.size() + 1;
	});

	// Reserve space to avoid reallocation
	serializedClauses.clear();
	serializedClauses.reserve(totalSize);

	// Serialize each clause
	for (const auto& clause : clauses) {
		// Store the size of the clause
		serializedClauses.push_back(static_cast<int>(clause.size()));

		// Store the elements of the clause
		serializedClauses.insert(serializedClauses.end(), clause.begin(), clause.end());
	}

	return true;
}

/* TODO use a functor for generic filtering during deserialization and serialization */

bool
deserializeClauses(const std::vector<int>& serializedClauses, std::vector<simpleClause>& clauses)
{
	clauses.clear();
	size_t index = 0;

	while (index < serializedClauses.size()) {
		// Get the size of the next clause
		int clauseSize = serializedClauses[index++];

		if (index + clauseSize > serializedClauses.size()) {
			return false; // Incomplete data
		}

		// Create a new clause and fill it with data
		clauses.emplace_back(serializedClauses.begin() + index, serializedClauses.begin() + index + clauseSize);

		// Move to the next clause
		index += clauseSize;
	}

	return true;
}

bool
sendFormula(std::vector<simpleClause>& clauses, unsigned int* varCount, int rootRank)
{
	// Broadcast varCount
	TESTRUNMPI(MPI_Bcast(varCount, 1, MPI_UNSIGNED, rootRank, MPI_COMM_WORLD));

	LOGDEBUG1("VarCount = %u", *varCount);

	std::vector<int> serializedClauses;
	std::vector<unsigned char> compressedBuffer;
	int64_t originalSize = 0;
	int64_t compressedSize = 0;

	if (mpi_rank == rootRank) {
		LOGDEBUG1("Root clauses number: %u", clauses.size());
		// Serialize clauses
		if (!serializeClauses(clauses, serializedClauses)) {
			LOGERROR("Error serializing clauses");
			originalSize = -1; // Use as error flag
		} else {
			originalSize = static_cast<int64_t>(serializedClauses.size() * sizeof(int));
			try {
				compressedBuffer = compressBuffer(serializedClauses);
				compressedSize = static_cast<int64_t>(compressedBuffer.size());
			} catch (const std::exception& e) {
				LOGERROR("Compression failed: %s", e.what());
				originalSize = -1; // Use as error flag
			}
		}
	}

	// Broadcast the original size (or error flag)
	TESTRUNMPI(MPI_Bcast(&originalSize, 1, MPI_INT64_T, rootRank, MPI_COMM_WORLD));

	if (originalSize == -1) {
		LOGERROR("Root failed to serialize or compress clauses");
		return false;
	}

	// Broadcast the compressed size
	TESTRUNMPI(MPI_Bcast(&compressedSize, 1, MPI_INT64_T, rootRank, MPI_COMM_WORLD));

	// Resize buffer for non-root processes
	if (mpi_rank != rootRank) {
		compressedBuffer.resize(compressedSize);
		LOGDEBUG1("Data to transfer: %ld bytes (compressed from %ld bytes)", compressedSize, originalSize);
	}

	// Broadcast the compressed data
	TESTRUNMPI(MPI_Bcast(compressedBuffer.data(), compressedSize, MPI_UNSIGNED_CHAR, rootRank, MPI_COMM_WORLD));

	if (mpi_rank != rootRank) {
		try {
			serializedClauses = decompressBuffer(compressedBuffer, originalSize);
		} catch (const std::exception& e) {
			LOGERROR("Decompression failed: %s", e.what());
			return false;
		}

		// Deserialize clauses
		if (!deserializeClauses(serializedClauses, clauses)) {
			LOGERROR("Error deserializing clauses");
			return false;
		}
		LOGDEBUG1("Worker: deserialized %u clauses", clauses.size());
	}

	return true;
}

void
sendModelToRoot()
{
	if (mpi_winner == 0 || finalResult != SatResult::SAT)
		return;

	LOG1("Model to root mpi_winner=%d, finalResult=%d", mpi_winner, static_cast<int>(finalResult.load()));

	MPI_Status status;

	if (mpi_rank == mpi_winner) {
		LOG1("Winner %d sending model of size %d", mpi_winner, finalModel.size());
		TESTRUNMPI(MPI_Send(finalModel.data(), finalModel.size(), MPI_INT, 0, MYMPI_MODEL, MPI_COMM_WORLD));
	}

	else if (0 == mpi_rank) {
		int size;
		LOGDEBUG1("Root is waiting for model");

		TESTRUNMPI(MPI_Probe(mpi_winner, MYMPI_MODEL, MPI_COMM_WORLD, &status));
		TESTRUNMPI(MPI_Get_count(&status, MPI_INT, &size));

		assert(mpi_winner == status.MPI_SOURCE);
		assert(MYMPI_MODEL == status.MPI_TAG);

		finalModel.resize(size);

		TESTRUNMPI(MPI_Recv(finalModel.data(), size, MPI_INT, mpi_winner, MYMPI_MODEL, MPI_COMM_WORLD, &status));
		LOG1("Root received a model of size %d", size);
	}

	mpi_winner = 0;
}

}