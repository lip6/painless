#include "sharing/Sharer.hpp"
#include "utils/Logger.hpp"
#include "utils/NumericConstants.hpp"
#include "utils/System.hpp"

#include <algorithm>
#include <chrono>
#include <thread>
#include <unistd.h>

#include "core/painless.hpp"

/// Function exectuted by each sharer.
/// This is main of sharer threads.
/// @param  arg contains a pointer to the associated class
/// @return return NULL if the thread exit correctly
void*
mainThrSharing(void* arg)
{
  Sharer* shr = static_cast<Sharer*>(arg);
  shr->m_round = 0;
  int nbStrats = shr->m_sharingStrategies.size();
  int lastStrategy = -1;

  std::chrono::microseconds sleepTime, sharingTime(0);

  // To be notified when solver starts solving
  shr->m_manager.waitOnSolving();

  while (!shr->shouldTerminate) {

    if (shr->m_manager.shouldEndSolving()) {
      LOGD1("Sharer %d shouldTerminate = %d will wait on solving",
            shr->getId(),
            static_cast<int>(shr->shouldTerminate));
      shr->m_manager.waitOnSolving();
    }

    lastStrategy = shr->m_round % nbStrats;

    // Sharing phase
    sharingTime = SystemResourceMonitor::Timer::getAbsoluteTimeMicro();
    shr->m_sharingStrategies[lastStrategy]->doSharing();
    sharingTime =
      SystemResourceMonitor::Timer::getAbsoluteTimeMicro() - sharingTime;

    shr->m_totalSharingTime += sharingTime;

    sleepTime = shr->m_sharingStrategies[lastStrategy]->getSleepingTime();
    LOG2("[Sharer %d] Sharing round %u done in %lu us. Will sleep for %llu us",
         shr->getId(),
         shr->m_round,
         sharingTime.count(),
         sleepTime.count());

    if (!shr->m_manager.shouldEndSolving()) {
      UNIQUE_LOCK(std::mutex, shr->m_sharerMX, sleep);
      shr->m_sharerCV.wait_for(lsleep, sleepTime);
      LOGD1("Sharer %d shouldTerminate = %d",
            shr->getId(),
            static_cast<int>(shr->shouldTerminate));
    }

    shr->m_round++; // New round
  }

  shr->printStats();
  return NULL;
}

Sharer::Sharer(
  PainlessImpl& manager,
  int _id,
  std::vector<std::shared_ptr<SharingStrategy>>& _sharingStrategies)
  : m_manager(manager)
  , m_sharerId(_id)
  , m_sharingStrategies(_sharingStrategies)
  , m_totalSharingTime(0)
{
  m_thread = std::thread(mainThrSharing, this);
}

Sharer::Sharer(PainlessImpl& manager,
               int _id,
               std::shared_ptr<SharingStrategy> _sharingStrategy)
  : m_manager(manager)
  , m_sharerId(_id)
  , shouldTerminate(false)
{
  m_sharingStrategies.push_back(_sharingStrategy);
  m_thread = std::thread(mainThrSharing, this);
}

Sharer::~Sharer() {}

void
Sharer::join()
{
  UNIQUE_LOCK(std::mutex, m_sharerMX, wake);
  m_sharerCV.notify_one();
  lwake.unlock();
  if (m_thread.joinable()) {
    m_thread.join();
    LOGD1("Sharer %d joined", this->getId());
  }
}

void
Sharer::printStats()
{
  double sharingTimeSec =
    static_cast<double>(this->m_totalSharingTime.count()) / MILLION;
  LOGSTAT("Sharer %d: executionTime: %lf s, rounds: %u, average: %lf s/round",
          this->getId(),
          sharingTimeSec,
          this->m_round,
          sharingTimeSec / this->m_round);
  for (unsigned int i = 0; i < m_sharingStrategies.size(); i++) {
    LOGSTAT("Strategy '%s': %s",
            typeid(*m_sharingStrategies[i]).name(),
            m_sharingStrategies[i]->getStatistics().toString().c_str());
  }
}