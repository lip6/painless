#include "LocalSharingStrategy.h"
#include "painless.h"

#include <algorithm>

LocalSharingStrategy::LocalSharingStrategy(std::vector<std::shared_ptr<SharingEntity>> &producers, std::vector<std::shared_ptr<SharingEntity>> &consumers) : SharingStrategy()
{
    this->producers = producers;
    this->consumers = consumers;
}

LocalSharingStrategy::LocalSharingStrategy(std::vector<std::shared_ptr<SharingEntity>> &&producers, std::vector<std::shared_ptr<SharingEntity>> &&consumers) : SharingStrategy()
{
    this->producers = producers;
    this->consumers = consumers;
}

void LocalSharingStrategy::printStats()
{
    LOGSTAT("Local Strategy: receivedCls %d, sharedCls %d, receivedDuplicas %d, promotionTiers2 %d, promotionsCore %d, alreadyTiers2 %d, alreadyCore %d",
        stats.receivedClauses, stats.sharedClauses, stats.receivedDuplicas, stats.promotionTiers2, stats.promotionCore, stats.alreadyTiers2, stats.alreadyCore);
}

LocalSharingStrategy::~LocalSharingStrategy()
{
}

void LocalSharingStrategy::addProducer(std::shared_ptr<SharingEntity> sharingEntity)
{

    addLock.lock();
    addProducers.push_back(sharingEntity);
    addLock.unlock();
    this->mustAddEntities = true;
}

void LocalSharingStrategy::addConsumer(std::shared_ptr<SharingEntity> sharingEntity)
{
    addLock.lock();
    addConsumers.push_back(sharingEntity);
    addLock.unlock();
    this->mustAddEntities = true;
}

void LocalSharingStrategy::removeProducer(std::shared_ptr<SharingEntity> sharingEntity)
{
    removeLock.lock();
    removeProducers.push_back(sharingEntity);
    removeLock.unlock();
    this->mustRemoveEntities = true;
}

void LocalSharingStrategy::removeConsumer(std::shared_ptr<SharingEntity> sharingEntity)
{
    removeLock.lock();
    removeConsumers.push_back(sharingEntity);
    removeLock.unlock();
    this->mustRemoveEntities = true;
}

void LocalSharingStrategy::removeSharingEntities(std::vector<std::shared_ptr<SharingEntity>> &entitiesToRemove, std::vector<std::shared_ptr<SharingEntity>> &presentEntities)
{
    /* TODO optimize */
    for (size_t i = 0; i < entitiesToRemove.size(); i++)
    {
        presentEntities.erase(std::remove(presentEntities.begin(), presentEntities.end(), entitiesToRemove[i]), presentEntities.end());
    }
    entitiesToRemove.clear();
}

void LocalSharingStrategy::addSharingEntities(std::vector<std::shared_ptr<SharingEntity>> &newEntities, std::vector<std::shared_ptr<SharingEntity>> &presentEntities)
{
    presentEntities.insert(presentEntities.end(), newEntities.begin(), newEntities.end());
    newEntities.clear();
}