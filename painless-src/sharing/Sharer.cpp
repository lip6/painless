// -----------------------------------------------------------------------------
// Copyright (C) 2017  Ludovic LE FRIOUX
//
// This file is part of PaInleSS.
//
// PaInleSS is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program.  If not, see <http://www.gnu.org/licenses/>.
// -----------------------------------------------------------------------------

#include "painless.h"
#include "sharing/Sharer.h"
#include "utils/Logger.h"
#include "utils/Parameters.h"
#include "utils/System.h"

#include <algorithm>
#include <unistd.h>

/// Function exectuted by each sharer.
/// This is main of sharer threads.
/// @param  arg contains a pointer to the associated class
/// @return return NULL if the thread exit correctly
void *mainThrSharing(void *arg)
{
   Sharer *shr = (Sharer *)arg;

   int round = 0;
   int nbStrats = shr->sharingStrategies.size();
   int lastStrategy = -1;
   int sleepTime = shr->sharingStrategies.front()->getSleepingTime() / nbStrats; // TODO : to be replaced
   double sharingTime = 0;
   int wakeupRet = 0;
   timespec sleepTimeSpec;
   timespec timespecCond;

   getTimeSpecMicro(sleepTime, &sleepTimeSpec);
   usleep(Parameters::getIntParam("init-sleep", 10000)*shr->id); /* attempt to desync when there are multiple sharers */
   LOG1("Sharer %d will start now", shr->id);

   bool can_break = false;

   while (!can_break)
   {

      lastStrategy = round % nbStrats;

      // Sharing phase
      sharingTime = getAbsoluteTime();
      can_break = shr->sharingStrategies[lastStrategy]->doSharing();
      sharingTime = getAbsoluteTime() - sharingTime;
      LOG2("[Sharer %d] Sharing round %d done in %f s.", shr->id, round, sharingTime);

      // sleep
      if (!globalEnding)
      {
         // wait for sleeptime time then wakeup (spurious ok)
         // if signaled the globalending will be managed by the strategy
         pthread_mutex_lock(&mutexGlobalEnd);
         getTimeToWait(&sleepTimeSpec, &timespecCond);
         wakeupRet = pthread_cond_timedwait(&condGlobalEnd, &mutexGlobalEnd, &timespecCond);
         pthread_mutex_unlock(&mutexGlobalEnd);
         LOGDEBUG1("sharer wakeupRet = %d , globalEnding = %d ", wakeupRet, globalEnding.load());
         // usleep(sleepTime);
      }

      round++; // New round
      shr->totalSharingTime += sharingTime;
   }

   // Removed strategy that ended
   LOG3("Sharer %d strategy %d ended", shr->id, lastStrategy);
   nbStrats--;

   LOG3("Sharer %d has %d remaining strategies.", shr->id, nbStrats);

   // Launch a final doSharing to make the other strategies finalize correctly (removed from the previous while(1) to lessen ifs)
   for (int i = 0; i < nbStrats; i++)
   {
      if (i == lastStrategy)
         continue;
      LOG3("Sharer %d will end strategy %d", shr->id, i);
      if (!shr->sharingStrategies[i]->doSharing())
      {
         LOGERROR("Panic, strategy %d didn't detect ending!", i);
      }
   }
   return NULL;
}

Sharer::Sharer(int _id, std::vector<std::shared_ptr<SharingStrategy>> &_sharingStrategies) : Entity(_id), sharingStrategies(_sharingStrategies)
{
   sharer = new Thread(mainThrSharing, this);
}

Sharer::Sharer(int _id, std::shared_ptr<SharingStrategy> _sharingStrategy) : Entity(_id)
{
   sharingStrategies.push_back(_sharingStrategy);
   sharer = new Thread(mainThrSharing, this);
}

Sharer::~Sharer()
{
   this->join();
}

void Sharer::printStats()
{
   LOGSTAT("Sharer %d: executionTime: %f", this->getId(), this->totalSharingTime);
   for (int i = 0; i < sharingStrategies.size(); i++)
   {
      LOGSTAT("Strategy '%s': ", typeid(*sharingStrategies[i]).name());
      sharingStrategies[i]->printStats();
   }
}