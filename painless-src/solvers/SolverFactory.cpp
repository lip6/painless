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

#include "../solvers/MapleCOMSPSSolver.h"
#include "../solvers/SlimeSolver.h"
#include "../solvers/SolverFactory.h"
#include "../solvers/Reducer.h"
#include "../utils/Parameters.h"
#include "../utils/System.h"

#include <fcntl.h>
#include <unistd.h>

void
SolverFactory::sparseRandomDiversification(
      const vector<SolverInterface *> & solvers)
{
   if (solvers.size() == 0)
      return;

   int vars = solvers[0]->getVariablesCount();

   // The first solver of the group (1 LRB/1 VSIDS) keeps polarity = false for all vars
   for (int sid = 1; sid < solvers.size(); sid++) {
      srand(sid);
      for (int var = 1; var <= vars; var++) {
         if (rand() % solvers.size() == 0) {
            solvers[sid]->setPhase(var, rand() % 2 == 1);
         }
      }
   }
}

void
SolverFactory::nativeDiversification(const vector<SolverInterface *> & solvers)
{
   for (int sid = 0; sid < solvers.size(); sid++) {
      solvers[sid]->diversify(sid);
   }
}

SolverInterface *
SolverFactory::createMapleCOMSPSSolver()
{
   int id = currentIdSolver.fetch_add(1);

   SolverInterface * solver = new MapleCOMSPSSolver(id);

   solver->loadFormula(Parameters::getFilename());

   return solver;
}

SolverInterface *
SolverFactory::createSlimeSolver()
{
   int id = currentIdSolver.fetch_add(1);

   SolverInterface * solver = new SlimeSolver(id);

   solver->loadFormula(Parameters::getFilename());

   return solver;
}

SolverInterface *
SolverFactory::createReducerSolver(SolverInterface* _solver)
{
   int id = currentIdSolver.fetch_add(1);

   SolverInterface * solver = new Reducer(id, _solver);

   return solver;
}

void
SolverFactory::createMapleCOMSPSSolvers(int maxSolvers,
                                        vector<SolverInterface *> & solvers)
{
   solvers.push_back(createMapleCOMSPSSolver());

   double memoryUsed    = getMemoryUsed();
   int maxMemorySolvers = Parameters::getIntParam("max-memory", 240) * 1024 *
                          1024 / memoryUsed;

   if (maxSolvers > maxMemorySolvers) {
      maxSolvers = maxMemorySolvers;
   }

   for (int i = 1; i < maxSolvers; i++) {
      solvers.push_back(cloneSolver(solvers[0]));
   }
}

void
SolverFactory::createSlimeSolvers(int nbSolvers,
                           vector<SolverInterface *> & solvers)
{
   unsigned int seed = 0;
   int fd = open("/dev/urandom", O_RDONLY);
   for (size_t i = 0; i < nbSolvers; i++) {
      SlimeSolver* slime = (SlimeSolver*) createSlimeSolver();
      solvers.push_back(slime);
      while (seed == 0) {
         if (fd == -1 || read(fd, &seed, sizeof(seed)) == -1) {
            perror("pick seed");
         }
         seed = seed % 91648253;
      }
      slime->setSeed(seed);
      seed = 0;
   }
}

SolverInterface *
SolverFactory::cloneSolver(SolverInterface * other)
{
   SolverInterface * solver;
   
   int id = currentIdSolver.fetch_add(1);

   switch(other->type) {
      case MAPLE :
	      solver = new MapleCOMSPSSolver((MapleCOMSPSSolver &) *other, id);
      	break;

      default :
         return NULL;
   }

   return solver;
}

void
SolverFactory::printStats(const vector<SolverInterface *> & solvers)
{
   printf("c | ID | conflicts  | propagations |  restarts  | decisions  " \
          "| memPeak |\n");

   for (size_t i = 0; i < solvers.size(); i++) {
      SolvingStatistics stats = solvers[i]->getStatistics();

      printf("c | %2zu | %10ld | %12ld | %10ld | %10ld | %7d |\n",
             solvers[i]->id, stats.conflicts, stats.propagations,
             stats.restarts, stats.decisions, (int)stats.memPeak);
   }
}
