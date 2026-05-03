#pragma once

#include "sharing/SharingEntity.hpp"
#include "sharing/SharingStrategy.hpp"
#include "utils/Logger.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>

class PainlessImpl;

/**
 * @brief Function executed by each sharer thread.
 * @param arg Pointer to the associated Sharer object.
 * @return NULL if the thread exits correctly.
 */
static void*
mainThrSharing(void* arg);

/**
 * @brief A sharer is a thread responsible for executing a list of
 * SharingStrategies.
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
  Sharer(PainlessImpl& manager,
         int id_,
         std::vector<std::shared_ptr<SharingStrategy>>& sharingStrategies);

  /**
   * @brief Constructor with a single sharing strategy.
   * @param id_ The ID of the sharer.
   * @param sharingStrategy A single sharing strategy.
   */
  Sharer(PainlessImpl& manager,
         int id_,
         std::shared_ptr<SharingStrategy> sharingStrategy);

  /**
   * @brief Destructor.
   */
  virtual ~Sharer();

  /**
   * @brief Print sharing statistics.
   */
  virtual void printStats();

  /**
   * @brief Get the ID of this sharer.
   * @return The sharer's ID.
   */
  inline plid_t getId() { return this->m_sharerId; }

  /**
   * @brief Set the ID of this sharer.
   * @param id The new ID for the sharer.
   */
  inline void setId(plid_t id) { this->m_sharerId = id; }

  void join();

  inline void asyncTerminate() { shouldTerminate = true; }

protected:
  /**
   * @brief Working function that will call sharingStrategy doSharing()
   * @param  sharer the sharer object
   * @return NULL if well ended
   */
  friend void* mainThrSharing(void*);

  /// Bool to notify the sharer to terminate
  std::atomic<bool> shouldTerminate;

  std::mutex m_sharerMX;
  std::condition_variable m_sharerCV;

  /// Pointer to the thread in charge of sharing.
  std::thread m_thread;

  /// @brief The manager defining the end
  PainlessImpl& m_manager;

  /// @brief Heuristic for strategy implementation comparison
  std::chrono::microseconds m_totalSharingTime;

  /// Number of sharing rounds completed.
  uint m_round;

  /// Strategy/Strategies used to share clauses.
  std::vector<std::shared_ptr<SharingStrategy>> m_sharingStrategies;

  /// The ID of this sharer.
  id_t m_sharerId;
};