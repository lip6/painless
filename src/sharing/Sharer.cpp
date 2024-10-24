#include "sharing/Sharer.hpp"
#include "painless.hpp"
#include "utils/Logger.hpp"
#include "utils/Parameters.hpp"
#include "utils/System.hpp"

#include <algorithm>
#include <chrono>
#include <thread>
#include <unistd.h>

/// Function exectuted by each sharer.
/// This is main of sharer threads.
/// @param  arg contains a pointer to the associated class
/// @return return NULL if the thread exit correctly
void*
mainThrSharing(void* arg)
{
	Sharer* shr = static_cast<Sharer*>(arg);
	shr->round = 0;
	int nbStrats = shr->sharingStrategies.size();
	int lastStrategy = -1;

	// Convert microseconds to std::chrono::microseconds
	std::chrono::microseconds sleepTime;

	double sharingTime = 0;

	// Initial sleep to desynchronize multiple sharers
	std::this_thread::sleep_for(std::chrono::microseconds(__globalParameters__.initSleep));
	LOG1("Sharer %d will start now", shr->getId());

	bool can_break = false;

	while (!can_break) {

		lastStrategy = shr->round % nbStrats;

		// Sharing phase
		sharingTime = SystemResourceMonitor::getAbsoluteTimeSeconds();
		can_break = shr->sharingStrategies[lastStrategy]->doSharing();
		sharingTime = SystemResourceMonitor::getAbsoluteTimeSeconds() - sharingTime;

		sleepTime = shr->sharingStrategies[lastStrategy]->getSleepingTime();
		LOG2("[Sharer %d] Sharing round %d done in %f s. Will sleep for %llu us",
			 shr->getId(),
			 shr->round,
			 sharingTime,
			 sleepTime.count());

		// Sleep phase, woken up if need to end
		if (!globalEnding) {
			std::unique_lock<std::mutex> lock(mutexGlobalEnd);
			auto wakeupStatus = condGlobalEnd.wait_for(lock, sleepTime);
			LOGDEBUG2("Sharer %d wakeupStatus = %s, globalEnding = %d",
					  shr->getId(),
					  (wakeupStatus == std::cv_status::timeout ? "timeout" : "no_timeout"),
					  globalEnding.load());
		}

		shr->round++; // New round
		shr->totalSharingTime += sharingTime;
	}

	// Handle ending of strategies
	LOG3("Sharer %d strategy %d ended", shr->getId(), lastStrategy);
	nbStrats--;
	LOG3("Sharer %d has %d remaining strategies.", shr->getId(), nbStrats);

	// Final sharing for remaining strategies
	for (unsigned int i = 0; i < shr->sharingStrategies.size(); i++) {
		if (i == lastStrategy)
			continue;
		LOG3("Sharer %d will end strategy %d", shr->getId(), i);
		while (!shr->sharingStrategies[i]->doSharing()) {
			LOGWARN("Strategy %d didn't detect ending!", i);
		}
	}

	shr->printStats();
	return NULL;
}

Sharer::Sharer(int _id, std::vector<std::shared_ptr<SharingStrategy>>& _sharingStrategies)
	: m_sharerId(_id)
	, sharingStrategies(_sharingStrategies)
{
	sharer = new Thread(mainThrSharing, this);
}

Sharer::Sharer(int _id, std::shared_ptr<SharingStrategy> _sharingStrategy)
	: m_sharerId(_id)
{
	sharingStrategies.push_back(_sharingStrategy);
	sharer = new Thread(mainThrSharing, this);
}

Sharer::~Sharer() {}

void
Sharer::printStats()
{
	LOGSTAT("Sharer %d: executionTime: %f, rounds: %d, average: %f",
			this->getId(),
			this->totalSharingTime,
			this->round,
			this->totalSharingTime / this->round);
	for (unsigned int i = 0; i < sharingStrategies.size(); i++) {
		LOGSTAT("Strategy '%s': ", typeid(*sharingStrategies[i]).name());
		sharingStrategies[i]->printStats();
	}
}