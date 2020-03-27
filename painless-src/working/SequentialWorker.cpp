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

#include "../utils/Logger.h"
#include "../working/SequentialWorker.h"

#include <unistd.h>

using namespace std;

// Main executed by worker threads
void * mainWorker(void *arg)
{
   SequentialWorker * sq = (SequentialWorker *)arg;

   SatResult res = UNKNOWN;

   vector<int> model;

   while (globalEnding == false && sq->force == false) {
      pthread_mutex_lock(&sq->mutexStart);

      if (sq->waitJob == true) {
         pthread_cond_wait(&sq->mutexCondStart, &sq->mutexStart);
      }

      pthread_mutex_unlock(&sq->mutexStart);

      sq->waitInterruptLock.lock();

      do {
         res = sq->solver->solve(sq->actualCube);
      } while (sq->force == false && res == UNKNOWN);

      sq->waitInterruptLock.unlock();

      if (res == SAT) {
         model = sq->solver->getModel();
      }

      sq->join(NULL, res, model);

      model.clear();

      sq->waitJob = true;
   }

   return NULL;
}

// Constructor
SequentialWorker::SequentialWorker(SolverInterface * solver_)
{
   solver  = solver_;
   force   = false;
   waitJob = true;

   pthread_mutex_init(&mutexStart, NULL);
   pthread_cond_init (&mutexCondStart, NULL);

   worker = new Thread(mainWorker, this);
}

// Destructor
SequentialWorker::~SequentialWorker()
{
   setInterrupt();

   worker->join();
   delete worker;

   pthread_mutex_destroy(&mutexStart);
   pthread_cond_destroy (&mutexCondStart);

   solver->release();
}

void
SequentialWorker::solve(const vector<int> & cube)
{
   actualCube = cube;
   
   unsetInterrupt();

   waitJob = false;

   pthread_mutex_lock  (&mutexStart);
   pthread_cond_signal (&mutexCondStart);
   pthread_mutex_unlock(&mutexStart);
}

void
SequentialWorker::join(WorkingStrategy * winner, SatResult res,
                       const vector<int> & model)
{
   force = true;

   if (globalEnding)
      return;

   if (parent == NULL) {
      globalEnding = true;
      finalResult  = res;

      if (res == SAT) {
         finalModel = model;
      }
   } else {
      parent->join(this, res, model);
   }
}

void
SequentialWorker::waitInterrupt()
{
   waitInterruptLock.lock();
   waitInterruptLock.unlock();
}

void
SequentialWorker::setInterrupt()
{
   force = true;
   solver->setSolverInterrupt();
}

void
SequentialWorker::unsetInterrupt()
{
   force = false;
   solver->unsetSolverInterrupt();
}

int
SequentialWorker::getDivisionVariable()
{
   return solver->getDivisionVariable();
}

void
SequentialWorker::setPhase(const int var, const bool phase)
{
   solver->setPhase(var, phase);
}

void
SequentialWorker::bumpVariableActivity(const int var, const int times)
{
   solver->bumpVariableActivity(var, times);
}
