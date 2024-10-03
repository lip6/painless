#pragma once

#include "sharing/SharingStrategy.h"
#include "sharing/SharingStatistics.h"
#include "sharing/SharingEntityVisitor.h"

// To include the real types
#include "solvers/SolverCdclInterface.hpp"
#include "sharing/GlobalDatabase.h"
#include "sharing/SharingEntity.hpp"

#include "utils/Threading.h"

#include <vector>
#include <memory>

/**
 * \defgroup local_sharing_strat Local Sharing Strategies
 * \ingroup sharing_strat
 * \ingroup local_sharing_strat
 * @brief The interface for all the different local sharing strategies.
 *
 * The class LocalSharingStrategy follows the SharingEntityVisitor design pattern on \ref SharingEntity to implement the different
 * behavior they can have in a specific sharing strategy.
 */
class LocalSharingStrategy : public SharingEntityVisitor, public SharingStrategy
{
public:
    // constructor
    LocalSharingStrategy(std::vector<std::shared_ptr<SharingEntity>> &producers, std::vector<std::shared_ptr<SharingEntity>> &consumers);
    LocalSharingStrategy(std::vector<std::shared_ptr<SharingEntity>> &&producers, std::vector<std::shared_ptr<SharingEntity>> &&consumers);

    void addSharingEntities(std::vector<std::shared_ptr<SharingEntity>> &newEntities, std::vector<std::shared_ptr<SharingEntity>> &presentEntities);

    void removeSharingEntities(std::vector<std::shared_ptr<SharingEntity>> &entitiesToRemove, std::vector<std::shared_ptr<SharingEntity>> &presentEntities);

    /// Add a sharingEntity to the producers.
    void addProducer(std::shared_ptr<SharingEntity> sharingEntity);

    /// Add a sharingEntity to the consumers.
    void addConsumer(std::shared_ptr<SharingEntity> sharingEntity);

    /// Remove a sharingEntity from the producers.
    void removeProducer(std::shared_ptr<SharingEntity> sharingEntity);

    /// Remove a sharingEntity from the consumers.
    void removeConsumer(std::shared_ptr<SharingEntity> sharingEntity);

    void printStats() override;

    virtual ~LocalSharingStrategy();

protected:
    /* TODO: lock free lists, locking uses too much time */
    /// Mutex used to add producers and consumers.
    Mutex addLock;

    /// Mutex used to add producers and consumers.
    Mutex removeLock;

    std::atomic<bool> mustAddEntities = false;
    std::atomic<bool> mustRemoveEntities = false;

    /// Vector of sharingEntityies to add to the producers.
    std::vector<std::shared_ptr<SharingEntity>> addProducers;

    /// Vector of sharingEntities to add to the consumers.
    std::vector<std::shared_ptr<SharingEntity>> addConsumers;

    /// Vector of sharingEntities to remove from the producers.
    std::vector<std::shared_ptr<SharingEntity>> removeProducers;

    /// Vector of sharingEntities to remove from the consumers.
    std::vector<std::shared_ptr<SharingEntity>> removeConsumers;

    /// @brief The vector holding the references to the producers
    std::vector<std::shared_ptr<SharingEntity>> producers;

    /// @brief A vector hodling the references to the consumers
    std::vector<std::shared_ptr<SharingEntity>> consumers;

    /// Sharing statistics.
    SharingStatistics stats;
};