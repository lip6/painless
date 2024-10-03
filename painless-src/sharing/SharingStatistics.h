#pragma once

/// @brief Local Sharing statistics.
/// \ingroup sharing
/// Sharing statistics.
struct SharingStatistics
{
    /// Constructor.
    SharingStatistics()
    {
        sharedClauses = 0;
        receivedClauses = 0;
        receivedDuplicas = 0;
        promotionTiers2 = 0;
        promotionCore = 0;
        alreadyTiers2 = 0;
        alreadyCore = 0;
    }

    /// Number of shared clauses that have been shared.
    unsigned long sharedClauses;

    /// Number of shared clauses produced.
    unsigned long receivedClauses;

    unsigned long receivedDuplicas;

    unsigned long promotionTiers2;

    unsigned long promotionCore;

    unsigned long alreadyTiers2;

    unsigned long alreadyCore;
};

/// @brief Statistics of a global sharing strategy
struct GlobalSharingStatistics
{
    GlobalSharingStatistics()
    {
        receivedClauses = 0;
        sharedDuplicasAvoided = 0;
        messagesSent = 0;
        receivedDuplicas = 0;
        sharedClauses = 0;
    }

    /// @brief duplicates detected by bloom filter for toSend database
    unsigned long sharedDuplicasAvoided;

    /// @brief dpublicates detected by bloom filter for received database
    unsigned long receivedDuplicas;

    /// Number of received clauses
    unsigned long receivedClauses;

    /// @brief Number of clauses sent
    unsigned long sharedClauses;

    /// @brief Number of sent messages
    unsigned long messagesSent;
};