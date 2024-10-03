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

#include "../solvers/SolverInterface.hpp"
#include "utils/Threading.h"
#include "working/WorkingStrategy.h"

#include <vector>


// Main executed by worker threads
static void *mainWorker(void *arg);

class SequentialWorker : public WorkingStrategy
{
public:
   SequentialWorker(std::shared_ptr<SolverInterface> solver_);

   ~SequentialWorker();

   void solve(const std::vector<int> &cube);

   void join(WorkingStrategy *winner, SatResult res,
             const std::vector<int> &model);

   void setInterrupt();

   void unsetInterrupt();

   void waitInterrupt();

   std::shared_ptr<SolverInterface> solver;

protected:
   friend void *mainWorker(void *arg);

   Thread *worker;

   std::vector<int> actualCube;

   std::atomic<bool> force;

   std::atomic<bool> waitJob;

   Mutex waitInterruptLock;

   pthread_mutex_t mutexStart;
   pthread_cond_t mutexCondStart;
};
