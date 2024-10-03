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
#include "solvers/SolverFactory.h"
#include "utils/Parameters.h"
#include "utils/System.h"
#include "utils/Logger.h"
#include "painless.h"

#include <cassert>
#include <random>

void SolverFactory::diversification(const std::vector<std::shared_ptr<SolverCdclInterface>> &cdclSolvers, const std::vector<std::shared_ptr<LocalSearchSolver>> &localSolvers, bool dist, int mpi_rank, int world_size)
{
   /* More flexibile diversification */
   unsigned nb_solvers = cdclSolvers.size(); // cdclSolvers.size() + localSolvers.size();
   unsigned maxId = (dist) ? nb_solvers * world_size : nb_solvers;

   /* TODO: solvers must init this by themselves in the constructor*/
   if (dist)
   {
      for (auto &solver : cdclSolvers)
      {
         assert(mpi_rank >= 0);
         solver->setId(nb_solvers * mpi_rank + solver->id);
      }
   }

   if (maxId <= 2)
   {
      LOGWARN("Not enough solvers (%d) to perform a good diversification (need >= 2)", maxId);
   }

   // std::random_device uses /dev/urandom by default (to be checked)
   std::mt19937 engine(std::random_device{}());
   std::uniform_int_distribution<int> uniform(1, Parameters::getIntParam("max-div-noise", 100)); /* test with a normal dist*/

   SolverDivFamily family = SolverDivFamily::MIXED_SWITCH;

   LOGDEBUG1("Diversification on: nb_solvers = %d, dist = %d, mpi_rank = %d, world_size = %d, maxId = %d", nb_solvers, dist, mpi_rank, world_size, maxId);

   for (auto cdclSolver : cdclSolvers)
   {
      /* TODO: Use mpi_rank if dist and certain Global Strat*/
      /* % maxId is useful when new solvers are added afterwards */
      family = ((cdclSolver->id % maxId) < maxId / 3) ? SolverDivFamily::UNSAT_FOCUSED : ((cdclSolver->id % maxId) < maxId * 2 / 3) ? SolverDivFamily::SAT_STABLE
                                                                                                                                    : SolverDivFamily::MIXED_SWITCH;
      cdclSolver->setFamily(family);
      cdclSolver->diversify(engine, uniform);
      // solver->printParameters();
   }

   for (auto localSolver : localSolvers)
   {
      localSolver->diversify(engine, uniform);
   }

   LOG("Diversification done");
}

void SolverFactory::createSolver(char type, std::vector<std::shared_ptr<SolverCdclInterface>> &cdclSolvers, std::vector<std::shared_ptr<LocalSearchSolver>> &localSolvers)
{
   int id = currentIdSolver.fetch_add(1);
   if(id >= cpus)
   {
      LOGWARN("Solver of type '%c' will not be instantiated, the number of solvers %d reached the maximum %d.", type, id, cpus);
      return;
   }
   switch (type)
   {
#ifdef GLUCOSE_
   case 'g':
      // GLUCOSE implementation
      break;
#endif

#ifdef LINGELING_
   case 'l':
      // LINGELING implementation
      break;
#endif

#ifdef MAPLECOMSPS_
   case 'a':
      cdclSolvers.emplace_back(std::make_shared<MapleCOMSPSSolver>(id));
      break;
#endif

#ifdef MINISAT_
   case 'm':
      // MINISAT implementation
      break;
#endif

#ifdef KISSATMAB_
   case 'k':
      cdclSolvers.emplace_back(std::make_shared<KissatMABSolver>(id));
      break;
#endif

#ifdef KISSATGASPI_
   case 'K':
      cdclSolvers.emplace_back(std::make_shared<KissatGASPISolver>(id));
      break;
#endif

#ifdef YALSAT_
   case 'y':
      localSolvers.emplace_back(std::make_shared<YalSat>(id));
      break;
#endif

   default:
      LOGERROR("The SolverCdclType %d specified is not available!", type);
      exit(-1);
      break;
   }
}

void SolverFactory::createSolvers(int maxSolvers, std::string portfolio, std::vector<std::shared_ptr<SolverCdclInterface>> &cdclSolvers, std::vector<std::shared_ptr<LocalSearchSolver>> &localSolvers)
{
   unsigned typeCount = portfolio.size();
   for (size_t i = 0; i < maxSolvers; i++)
   {
      createSolver(portfolio.at(i % typeCount), cdclSolvers, localSolvers);
   }
}

Reducer *
SolverFactory::createReducerSolver()
{
   int id = currentIdSolver.fetch_add(1);

   Reducer *solver = new Reducer(id, SolverCdclType::MAPLE);

   return solver;
}

void SolverFactory::printStats(const std::vector<std::shared_ptr<SolverCdclInterface>> &cdclSolvers, const std::vector<std::shared_ptr<LocalSearchSolver>> &localSolvers)
{
   /*LOGSTAT(" | ID | conflicts  | propagations |  restarts  | decisions  "
       "| memPeak |");*/

   for (auto s : cdclSolvers)
   {
      // if (s->testStrengthening())
      s->printStatistics();
   }

   /*LOGSTAT(" | ID | flips  | unsatClauses |  restarts | memPeak |");*/
   for (auto s : localSolvers)
   {
      // if (s->testStrengthening())
      s->printStatistics();
   }
}
