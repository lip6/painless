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

#include "../painless.h"
#include "../sharing/Sharer.h"
#include "../solvers/SolverInterface.h"
#include "../utils/Logger.h"
#include "../utils/Parameters.h"

#include <algorithm>
#include <unistd.h>

/// Function exectuted by each sharer.
/// This is main of sharer threads.
/// @param  arg contains a pointeur to the associated class
/// @return return NULL if the thread exit correctly
static void * mainThrSharing(void * arg)
{
   Sharer * shr  = (Sharer *)arg;
   int round     = 0;
   int sleepTime = Parameters::getIntParam("shr-sleep", 500000);

   while (true) {
      // Sleep 
      usleep(sleepTime);
   
      if (globalEnding)
         break; // Need to stop

      round++; // New round

      SharingStatistics stats = shr->sharingStrategy->getStatistics();
      log(2, "Sharer %d enter in round  %d, received cls %ld, shared cls %ld\n",
          shr->id, round, stats.receivedClauses, stats.sharedClauses);


      // Add new solvers
      // -------------------------
      shr->addLock.lock();

      shr->producers.insert(shr->producers.end(), shr->addProducers.begin(),
                            shr->addProducers.end());
      shr->addProducers.clear();

      shr->consumers.insert(shr->consumers.end(), shr->addConsumers.begin(),
                            shr->addConsumers.end());
      shr->addConsumers.clear();

      shr->addLock.unlock();


      // Sharing phase
      shr->sharingStrategy->doSharing(shr->id, shr->producers, shr->consumers);

      
      // Remove solvers
      // -------------------------
      shr->removeLock.lock();

      for (size_t i = 0; i < shr->removeProducers.size(); i++) {
         shr->producers.erase(remove(shr->producers.begin(),
                                     shr->producers.end(),
                                     shr->removeProducers[i]),
                              shr->producers.end());
         shr->removeProducers[i]->release();
      }
      shr->removeProducers.clear();

      for (size_t i = 0; i < shr->removeConsumers.size(); i++) {
         shr->consumers.erase(remove(shr->consumers.begin(),
                                     shr->consumers.end(),
                                     shr->removeConsumers[i]),
                              shr->consumers.end());
         shr->removeConsumers[i]->release();
      }
      shr->removeConsumers.clear();

      shr->removeLock.unlock();


      if (globalEnding)
         break; // Need to stop
   }

   return NULL;
}

Sharer::Sharer(int id, SharingStrategy * sharingStrategy,
               vector<SolverInterface *> producers,
               vector<SolverInterface *> consumers)
{
   this->id              = id;
   this->sharingStrategy = sharingStrategy;
   this->producers       = producers;
   this->consumers       = consumers;

   for (size_t i = 0; i < producers.size(); i++) {
      producers[i]->increase();
   }

   for (size_t i = 0; i < consumers.size(); i++) {
      consumers[i]->increase();
   }

   sharer  = new Thread(mainThrSharing, this);
}

Sharer::~Sharer()
{
   sharer->join();
   delete sharer;

   removeLock.lock();

   for (int i = 0; i < removeProducers.size(); i++) {
      removeProducers[i]->release();
   }

   for (size_t i = 0; i < removeConsumers.size(); i++) {
      removeConsumers[i]->release();
   }

   removeLock.unlock();

   delete sharingStrategy;
}

void
Sharer::addProducer(SolverInterface * solver)
{
   solver->increase();

   addLock.lock();
   addProducers.push_back(solver);
   addLock.unlock();
}

void
Sharer::addConsumer(SolverInterface * solver)
{
   solver->increase();

   addLock.lock();
   addConsumers.push_back(solver);
   addLock.unlock();
}

void
Sharer::removeProducer(SolverInterface * solver)
{
   removeLock.lock();
   removeProducers.push_back(solver);
   removeLock.unlock();
}

void
Sharer::removeConsumer(SolverInterface * solver)
{
   removeLock.lock();
   removeConsumers.push_back(solver);
   removeLock.unlock();
}

void
Sharer::printStats()
{
   SharingStatistics stats = sharingStrategy->getStatistics();

   cout << "c Sharer " << id << " received cls "<< stats.receivedClauses
        << ", shared cls " << stats.sharedClauses << endl;
}
