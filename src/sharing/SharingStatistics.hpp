#pragma once

#include <atomic>
/* TODO; better management of statistics for sharing strategies */

/// @brief Local Sharing statistics.
/// \ingroup sharing
/// Sharing statistics.
struct SharingStatistics
{
	/// Number of shared clauses that have been shared.
	unsigned long sharedClauses{0};

	/// Number of shared clauses produced.
	std::atomic<unsigned long> receivedClauses{0};

	/// Number of clause filtered at import
	std::atomic<unsigned long> filteredAtImport{0};
};

/// @brief Statistics of a global sharing strategy
struct GlobalSharingStatistics : public SharingStatistics
{
	/// @brief duplicates detected by bloom filter for toSend database
	unsigned long sharedDuplicasAvoided{0};

	/// @brief dpublicates detected by bloom filter for received database
	unsigned long receivedDuplicas{0};

	/// @brief Number of sent messages
	unsigned long messagesSent{0};
};