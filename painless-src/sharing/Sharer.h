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

#pragma once

#include "../sharing/SharingStrategy.h"
#include "../utils/Threading.h"


static void * mainThrSharing(void * arg);


/// A sharer is a thread responsible to share clauses between solvers.
class Sharer
{
public:
   /// Constructor.
   Sharer(int id_, SharingStrategy * sharingStrategy_,
          vector<SolverInterface *> producers_,
          vector<SolverInterface *> consumer_);

   /// Destructor.
   ~Sharer();

   /// Add a solver to the producers.
   void addProducer(SolverInterface * solver);
   
   /// Add a solver to the consumers.
   void addConsumer(SolverInterface * solver);

   /// Remove a solver from the producers.
   void removeProducer(SolverInterface * solver);

   /// Remove a solver from the consumers.
   void removeConsumer(SolverInterface * solver);

   /// Print sharing statistics.
   void printStats();

protected:
   friend void * mainThrSharing(void *);

   /// Id of the sharer.
   int id;

   /// Strategy used to shared clauses.
   SharingStrategy * sharingStrategy;

   /// Mutex used to add producers and consumers.
   Mutex addLock;

   /// Mutex used to add producers and consumers.
   Mutex removeLock;

   /// Vector of solvers to add to the producers.
   vector<SolverInterface *> addProducers;

   /// Vector of solvers to add to the consumers.
   vector<SolverInterface *> addConsumers;

   /// Vector of solvers to remove from the producers.
   vector<SolverInterface *> removeProducers;
   
   /// Vector of solvers to remove from the consumers.
   vector<SolverInterface *> removeConsumers;
   
   /// Vector of the producers.
   vector<SolverInterface *> producers;
   
   /// Vector of the consumers.
   vector<SolverInterface *> consumers;
   
   /// Pointer to the thread in chrage of sharing.
   Thread * sharer;
};
