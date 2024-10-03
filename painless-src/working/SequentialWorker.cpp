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

#include "utils/Logger.h"
#include "working/SequentialWorker.h"
#include "painless.h"
#include "utils/SatUtils.h"
#include "utils/Parameters.h"

#include <unistd.h>

;

// Main executed by worker threads
void *mainWorker(void *arg)
{
   SequentialWorker *sq = (SequentialWorker *)arg;

   SatResult res = UNKNOWN;

   std::vector<int> model;

   while (globalEnding == false && sq->force == false)
   {
      pthread_mutex_lock(&sq->mutexStart);

      while (sq->waitJob == true)
      {
         pthread_cond_wait(&sq->mutexCondStart, &sq->mutexStart);
      }

      pthread_mutex_unlock(&sq->mutexStart);

      sq->waitInterruptLock.lock();
      
      res = sq->solver->solve(sq->actualCube); // why pass by param what the solver already has ?

      sq->waitInterruptLock.unlock();

      if (res == SAT)
      {
         model = sq->solver->getModel();
      }

      sq->join(NULL, res, model); // assign true to force ! No need for sq->force==false in while loop (see original painless)

      model.clear();

      sq->waitJob = true;
   }

   return NULL;
}

// Constructor
SequentialWorker::SequentialWorker(std::shared_ptr<SolverInterface> solver_)
{
   solver = solver_;
   force = false;
   waitJob = true;

   pthread_mutex_init(&mutexStart, NULL);
   pthread_cond_init(&mutexCondStart, NULL);

   worker = new Thread(mainWorker, this);
}

// Destructor
SequentialWorker::~SequentialWorker()
{
   if (!force)
      setInterrupt();

   worker->join();
   delete worker;

   pthread_mutex_destroy(&mutexStart);
   pthread_cond_destroy(&mutexCondStart);
}

void SequentialWorker::solve(const std::vector<int> &cube)
{
   actualCube = cube;

   unsetInterrupt();

   waitJob = false;

   pthread_mutex_lock(&mutexStart);
   pthread_cond_signal(&mutexCondStart);
   pthread_mutex_unlock(&mutexStart);
}

void SequentialWorker::join(WorkingStrategy *winner, SatResult res,
                            const std::vector<int> &model)
{
   force = true;

   LOGDEBUG1("SequentialWorker %p is joining with res = %d.", this, res);

   if (globalEnding)
      return;

   if (parent == NULL)
   {
      worker->join();

      globalEnding = true;
      finalResult = res;

      if (res == SAT)
      {
         finalModel = model;
      }
      pthread_mutex_lock(&mutexGlobalEnd);
      pthread_cond_broadcast(&condGlobalEnd);
      pthread_mutex_unlock(&mutexGlobalEnd);
   }
   else
   {
      LOGDEBUG1("SequentialWorker %p calls its parent",this);
      parent->join(this, res, model);
   }
}

void SequentialWorker::waitInterrupt()
{
   waitInterruptLock.lock();
   waitInterruptLock.unlock();
}

void SequentialWorker::setInterrupt()
{
   force = true;
   solver->setSolverInterrupt();
}

void SequentialWorker::unsetInterrupt()
{
   force = false;
   solver->unsetSolverInterrupt();
}