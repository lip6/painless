#pragma once

#include "sharing/SharingEntity.hpp"
#include "sharing/SharingStrategy.hpp"
#include "utils/Logger.hpp"
#include "utils/Threading.hpp"

/**
 * @brief Function executed by each sharer thread.
 * @param arg Pointer to the associated Sharer object.
 * @return NULL if the thread exits correctly.
 */
static void* mainThrSharing(void* arg);

/**
 * @brief A sharer is a thread responsible for executing a list of SharingStrategies.
 * @ingroup sharing
 */
class Sharer
{
  public:
    /**
     * @brief Constructor with multiple sharing strategies.
     * @param id_ The ID of the sharer.
     * @param sharingStrategies A vector of sharing strategies.
     */
    Sharer(int id_, std::vector<std::shared_ptr<SharingStrategy>>& sharingStrategies);

    /**
     * @brief Constructor with a single sharing strategy.
     * @param id_ The ID of the sharer.
     * @param sharingStrategy A single sharing strategy.
     */
    Sharer(int id_, std::shared_ptr<SharingStrategy> sharingStrategy);

    /**
     * @brief Destructor.
     */
    virtual ~Sharer();

    /**
     * @brief Print sharing statistics.
     */
    virtual void printStats();

    /**
     * @brief Join the thread of this sharer object.
     */
    inline void join()
    {
        if (sharer == nullptr)
            return;
        sharer->join();
        delete sharer;
        sharer = nullptr;
        LOGDEBUG1("Sharer %d joined", this->getId());
    }

    /**
     * @brief Set the thread affinity for this sharer.
     * @param coreId The ID of the core to set affinity to.
     */
    inline void setThreadAffinity(int coreId) { this->sharer->setThreadAffinity(coreId); }

    /**
     * @brief Get the ID of this sharer.
     * @return The sharer's ID.
     */
    inline int getId() { return this->m_sharerId; }

    /**
     * @brief Set the ID of this sharer.
     * @param id The new ID for the sharer.
     */
    inline void setId(int id) { this->m_sharerId = id; }

  protected:
    /// Pointer to the thread in charge of sharing.
    Thread* sharer;

    /// @brief Heuristic for strategy implementation comparison
    double totalSharingTime = 0;

    /// Number of sharing rounds completed.
    unsigned int round;

    /// Strategy/Strategies used to share clauses.
    std::vector<std::shared_ptr<SharingStrategy>> sharingStrategies;

    /**
     * @brief Working function that will call sharingStrategy doSharing()
     * @param  sharer the sharer object
     * @return NULL if well ended
     */
    friend void* mainThrSharing(void*);

    /// The ID of this sharer.
    int m_sharerId;
};