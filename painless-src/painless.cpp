// -----------------------------------------------------------------------------
// Copyright (C) 2017  Ludovic LE FRIOUX
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

#include "utils/ErrorCodes.h"
#include "utils/Logger.h"
#include "utils/Parameters.h"
#include "utils/System.h"
#include "utils/SatUtils.h"
#include "utils/MpiUtils.h"

#include "solvers/SolverFactory.h"

#include "sharing/Sharer.h"
#include "working/SequentialWorker.h"
#include "working/PortfolioPRS.h"
#include "working/PortfolioSBVA.h"
#include "preprocessors/StructuredBva.hpp"

/* TEMP */
#include "preprocessors/PRS-Preprocessors/preprocess.hpp"

#include <unistd.h>
#include <random>
#include <thread>

// -------------------------------------------
// Useful for timeout
// -------------------------------------------
int timeout; // 0 by default

// -------------------------------------------
// Declaration of global variables
// -------------------------------------------

std::atomic<bool> globalEnding(false);
pthread_mutex_t mutexGlobalEnd;
pthread_cond_t condGlobalEnd;

std::vector<Sharer *> sharers;

// int nSharers = 0;

WorkingStrategy *working = NULL;

std::atomic<bool> dist = false;

// needed for diversification
int nb_groups;
int cpus;

SatResult finalResult = UNKNOWN;

std::vector<int> finalModel;

// -------------------------------------------
// Main of the framework
// -------------------------------------------
int main(int argc, char **argv)
{
   Parameters::init(argc, argv);

   setVerbosityLevel(Parameters::getIntParam("v", 0));

   nb_groups = Parameters::getIntParam("groups", 2);
   cpus = Parameters::getIntParam("c", 30);
   dist = Parameters::getBoolParam("dist");
   int shr_strat = Parameters::getIntParam("shr-strat", 1);
   std::string solverNames = Parameters::getParam("solver", "k");
   timeout = Parameters::getIntParam("t", 5000); // if <0 will loop till a solution is found

   double resolutionTime = 0;

   Parameters::printParams();

#ifndef NDIST

   if (dist)
   {
      // MPI Initialization
      // If MPI_Init_Thread causes problems see how to wait for the mpi_rank
      int provided;
      MPI_Init_thread(NULL, NULL, MPI_THREAD_SERIALIZED, &provided);
      MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);

      LOGDEBUG1("Thread strategy provided is %d", provided);

      if (provided < MPI_THREAD_SERIALIZED)
      {
         LOGERROR("Wanted MPI initialization is not possible !");
         dist = false;
      }
      else
      {
         TESTRUNMPI(MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank));

         char hostname[256];
         gethostname(hostname, sizeof(hostname));
         LOGDEBUG1("PID %d on %s is of rank %d", getpid(), hostname, mpi_rank);

         TESTRUNMPI(MPI_Comm_size(MPI_COMM_WORLD, &world_size));
      }
   }
#else
   if (dist)
   {
      LOGWARN("The binary was compiled with -DNDIST: -dist flag is not supported!");
      exit(PERR_DIST_COMPILE);
   }
#endif

   // Init timeout detection before starting the solvers and sharers
   pthread_mutex_init(&mutexGlobalEnd, NULL);
   pthread_cond_init(&condGlobalEnd, NULL);

   pthread_mutex_lock(&mutexGlobalEnd); // to make sure that the broadcast is done when main has done its wait

   // Init working
   if (dist)
      working = new PortfolioPRS();
   else
      working = new PortfolioSBVA();

   // Launch working
   std::vector<int> cube;

   std::thread mainWorker(&WorkingStrategy::solve, (WorkingStrategy *)working, std::ref(cube));

   int wakeupRet = 0;

   if (timeout > 0)
   {
      // Wait until end or timeout
      timespec timeoutSpec = {timeout, 0};
      timespec timespecCond;

      while ((int)getRelativeTime() < timeout && globalEnding == false) // to manage the spurious wake ups
      {
         timeoutSpec.tv_sec = (timeout - (int)getRelativeTime()); // count only seconds
         getTimeToWait(&timeoutSpec, &timespecCond);
         wakeupRet = pthread_cond_timedwait(&condGlobalEnd, &mutexGlobalEnd, &timespecCond);
         LOGWARN("main wakeupRet = %d , globalEnding = %d ", wakeupRet, globalEnding.load());
      }

      // in case the sharers have done their wait too late
      pthread_cond_broadcast(&condGlobalEnd);

      pthread_mutex_unlock(&mutexGlobalEnd);

      if ((int)getRelativeTime() >= timeout && finalResult == SatResult::UNKNOWN) // if timeout set globalEnding otherwise a solver woke me up
      {
         globalEnding = true;
         finalResult = SatResult::TIMEOUT; /* Important for DIST !!!*/
         working->setInterrupt();
         resolutionTime = timeout;

         if (!dist)
         {
            LOGSTAT("Resolution time: %f s", resolutionTime);
            LOGSTAT("Memory used %f Ko", MemInfo::getUsedMemory());
            logSolution("UNKNOWN");
            exit(0);
         }
      }
      else
      {
         resolutionTime = getRelativeTime();
      }
   }
   else
   {
      // no timeout waiting
      while (globalEnding == false) // to manage the spurious wake ups
      {
         pthread_cond_wait(&condGlobalEnd, &mutexGlobalEnd);
         LOGWARN("main wakeupRet = %d , globalEnding = %d ", wakeupRet, globalEnding.load());
      }
      resolutionTime = getRelativeTime();
      // in case the sharers have done their wait too late
      pthread_cond_broadcast(&condGlobalEnd);

      pthread_mutex_unlock(&mutexGlobalEnd);
   }

   mainWorker.join();

   if (dist)
   {
      static_cast<PortfolioPRS *>(working)->restoreModelDist();
      delete working;
      TESTRUNMPI(MPI_Finalize());
   }

   if (mpi_rank == mpi_winner)
   {
      if (finalResult == SatResult::SAT)
      {
         logSolution("SATISFIABLE");

         if (Parameters::getBoolParam("no-model") == false)
         {
            logModel(finalModel);
         }
      }
      else if (finalResult == SatResult::UNSAT)
      {
         logSolution("UNSATISFIABLE");
      }
      else // if timeout or unknown
      {
         logSolution("UNKNOWN");
         finalResult = SatResult::UNKNOWN;
      }

      LOGSTAT("Resolution time: %f s", getRelativeTime());
      LOGSTAT("Memory used %f Ko", MemInfo::getUsedMemory());
      return finalResult;
   }
   return 0;
}
