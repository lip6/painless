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

#include "utils/Parameters.h"
#include "utils/Logger.h"
#include "utils/System.h"
#include "working/PortfolioPRS.h"
#include "working/SequentialWorker.h"
#include "painless.h"
#include <thread>

#include "solvers/SolverFactory.h"
#include "clauses/ClauseDatabaseLockFree.h"
#include "sharing/SharingStrategyFactory.h"

PortfolioPRS::PortfolioPRS()
{
#ifndef NDIST
   if (dist)
   {
      int maxClauseSize = Parameters::getIntParam("maxClauseSize", 50);
      /* The ids are to be managed separately, idDiversification, and idSharingEntity, Temp: cpus is surely not used by the solvers */
      this->globalDatabase = std::make_shared<GlobalDatabase>(cpus, std::make_shared<ClauseDatabaseLockFree>(maxClauseSize), std::make_shared<ClauseDatabaseLockFree>(maxClauseSize));
   }
#endif
}

PortfolioPRS::~PortfolioPRS()
{
   for (int i = 0; i < this->sharers.size(); i++)
      this->sharers[i]->printStats();
   this->globalDatabase->printStats();

   for (size_t i = 0; i < slaves.size(); i++)
   {
      delete slaves[i];
   }
}

void PortfolioPRS::solve(const std::vector<int> &cube)
{
   LOG(">> PortfolioPRS");
   std::vector<simpleClause> initClauses;
   std::vector<std::thread> clausesLoad;

   std::vector<std::shared_ptr<SolverCdclInterface>> cdclSolvers;
   std::vector<std::shared_ptr<LocalSearchSolver>> localSolvers;

   double approxMemoryPerSolver;

   int receivedFinalResultBcast = 0;
   unsigned varCount;
   int res;

   int nbKissat = 4;
   int nbGaspi = 3;

   strategyEnding = false;

   // Mpi rank 0 is the leader, sole executor of PRS preprocessing.
   if (0 == mpi_rank)
   {
      /* PRS */
      res = prs.do_preprocess(Parameters::getFilename());
      LOGDEBUG1("PRS returned %d", res);
      if (20 == res)
      {
         LOG("PRS answered UNSAT");
         finalResult = SatResult::UNSAT;
         this->join(this, finalResult, {});
      }
      else if (10 == res)
      {
         LOG("PRS answered SAT");
         for (int i = 1; i <= prs.vars; i++)
         {
            finalModel.push_back(prs.model[i]);
         }
         finalResult = SatResult::SAT;
         this->join(this, finalResult, finalModel);
      }

      receivedFinalResultBcast = finalResult;

      if (receivedFinalResultBcast != 0)
         goto prs_sync;

      // Free some memory
      prs.release_most();

      varCount = prs.vars;

      // PRS uses indexes from 1 to prs.clauses
      initClauses = std::move(prs.clause);
      initClauses.erase(initClauses.begin());

      if (initClauses.size() < 10 * MILLION)
      {
         std::thread sbvaRunner([this, &initClauses]()
                                {
            this->sbva.setTieBreakHeuristic(SBVATieBreak::MOSTOCCUR);
            this->sbva.printParameters();
            this->sbva.addInitialClauses(initClauses, this->prs.vars);
            this->sbva.run();
            // sbva->printStatistics();
            
            LOG2("Sbva %d ended", this->sbva.id);
            
            this->sbva.printStatistics();

            // Notify the caller that the task is completed
            std::lock_guard<std::mutex> lock(this->sbvaLock);
            this->sbvaSignal.notify_one(); });

         {
            std::unique_lock<std::mutex> lock(this->sbvaLock);
            auto ret = this->sbvaSignal.wait_for(lock, std::chrono::seconds(Parameters::getIntParam("sbva-timeout", 10)));
            LOGWARN("Portfolio PRS wakeupRet = %d , globalEnding = %d ", ret, globalEnding.load());
            this->sbva.setInterrupt();
         }
         sbvaRunner.join();

         if (sbva.isInitialized())
         {
            initClauses = std::move(sbva.getClauses());
            varCount = sbva.getVariablesCount();
            sbva.clearAll();
         }
      }

      /* ~solver mem + learned clauses ~= reduction_ratio*PRS memory : this is a vague approximation! TODO enhance the solverFactory */
      double reductionRatio = (((double)prs.vars / prs.orivars) * ((double)prs.clauses / prs.oriclauses));
      approxMemoryPerSolver = 1.2 * reductionRatio * MemInfo::getUsedMemory();

      LOG1("Reduction Ratio: %f", reductionRatio);
   }

prs_sync:
   if (dist)
      TESTRUNMPI(MPI_Bcast(&receivedFinalResultBcast, 1, MPI_INT, 0, MPI_COMM_WORLD));

   if (receivedFinalResultBcast != 0)
   {
      mpi_winner = 0;
      globalEnding = 1;
      finalResult = static_cast<SatResult>(receivedFinalResultBcast);
      LOGDEBUG1("[PRS] It is the mpi end: %d", finalResult);
      pthread_mutex_lock(&mutexGlobalEnd);
      pthread_cond_broadcast(&condGlobalEnd);
      pthread_mutex_unlock(&mutexGlobalEnd);
      return;
   }

   // Send instance via MPI from leader 0 to workers.
   if (dist)
   {
      TESTRUNMPI(MPI_Bcast(&approxMemoryPerSolver, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD));
      sendFormula(initClauses, &varCount, 0);
   }

   LOG1("ApproxMemory : %f", approxMemoryPerSolver);

   for (int i = 1; i <= nbKissat; i++)
   {
      LOGDEBUG1("Memory: %f / %f. %f / %f", MemInfo::getUsedMemory() + i * approxMemoryPerSolver, MemInfo::getTotalMemory(), i * approxMemoryPerSolver, MemInfo::getAvailableMemory());
      if (MemInfo::getAvailableMemory() <= i * approxMemoryPerSolver)
      {
         LOGERROR("SolverFactory cannot instantiate the %d th kissat (%f/%f) due to insufficient memory (to be used: %f / %f)", i, i * approxMemoryPerSolver, MemInfo::getAvailableMemory(), MemInfo::getUsedMemory() + i * approxMemoryPerSolver, MemInfo::getTotalMemory());
         nbKissat = i;
         nbGaspi = 0;
         break;
      }
      SolverFactory::createSolver('k', cdclSolvers, localSolvers);
   }

   for (int i = 1; i <= nbGaspi; i++)
   {
      LOGDEBUG1("Memory: %f / %f. %f / %f", MemInfo::getUsedMemory() + i * approxMemoryPerSolver, MemInfo::getTotalMemory(), i * approxMemoryPerSolver, MemInfo::getAvailableMemory());
      if (MemInfo::getAvailableMemory() <= i * approxMemoryPerSolver)
      {
         LOGERROR("SolverFactory cannot instantiate the %d th Gaspi (%f/%f) due to insufficient memory (to be used: %f / %f)", i, i * approxMemoryPerSolver, MemInfo::getAvailableMemory(), MemInfo::getUsedMemory() + i * approxMemoryPerSolver, MemInfo::getTotalMemory());
         nbGaspi = i;
         break;
      }
      SolverFactory::createSolver('K', cdclSolvers, localSolvers);
   }

   // one yalsat or kissat_mab
   if (initClauses.size() < 30 * MILLION && mpi_rank % 2 && (MemInfo::getAvailableMemory() > approxMemoryPerSolver))
      SolverFactory::createSolver('y', cdclSolvers, localSolvers);
   else if (MemInfo::getAvailableMemory() > approxMemoryPerSolver)
      SolverFactory::createSolver('k', cdclSolvers, localSolvers);

   SolverFactory::diversification(cdclSolvers, localSolvers, Parameters::getBoolParam("dist"), mpi_rank, world_size);

   for (auto cdcl : cdclSolvers)
   {
      clausesLoad.emplace_back(&LocalSearchSolver::addInitialClauses, cdcl, std::ref(initClauses), varCount);
   }
   for (auto local : localSolvers)
   {
      clausesLoad.emplace_back(&LocalSearchSolver::addInitialClauses, local, std::ref(initClauses), varCount);
   }

   /* Sharing */
   /* ------- */

   if (dist)
   {
      /* Init LocalStrategy */
      SharingStrategyFactory::instantiateLocalStrategies(Parameters::getIntParam("shr-strat", 1), this->localStrategies, {this->globalDatabase}, cdclSolvers);
      /* Init GlobalStrategy */
      SharingStrategyFactory::instantiateGlobalStrategies(Parameters::getIntParam("gshr-strat", 2), {this->globalDatabase}, globalStrategies);
   }
   else
   {
      SharingStrategyFactory::instantiateLocalStrategies(Parameters::getIntParam("shr-strat", 1), this->localStrategies, {}, cdclSolvers);
   }

   std::vector<std::shared_ptr<SharingStrategy>> sharingStrategiesConcat;

   sharingStrategiesConcat.clear();

   // Wait for clauses init
   for (auto &worker : clausesLoad)
      worker.join();

   clausesLoad.clear();

   LOG1("All solvers loaded the clauses");

   /* Launch sharers */
   for (auto lstrat : localStrategies)
   {
      sharingStrategiesConcat.push_back(lstrat);
   }
   for (auto gstrat : globalStrategies)
   {
      sharingStrategiesConcat.push_back(gstrat);
   }

   SharingStrategyFactory::launchSharers(sharingStrategiesConcat, this->sharers);

   for (int i = 0; i < this->sharers.size(); i++)
      this->sharers[i]->setThreadAffinity(0);

   if (globalEnding)
   {
      this->setInterrupt();
      return;
   }

   initClauses.clear();

   for (auto cdcl : cdclSolvers)
   {
      this->addSlave(new SequentialWorker(cdcl));
   }
   for (auto local : localSolvers)
   {
      this->addSlave(new SequentialWorker(local));
   }

   for (size_t i = 0; i < slaves.size(); i++)
   {
      slaves[i]->solve(cube);
   }
}

void PortfolioPRS::join(WorkingStrategy *strat, SatResult res,
                        const std::vector<int> &model)
{
   if (res == UNKNOWN || strategyEnding)
      return;

   strategyEnding = true;

   setInterrupt();

   if (parent == NULL)
   { // If it is the top strategy
      globalEnding = true;
      finalResult = res;

      if (res == SAT)
      {
         finalModel = model;

         if (mpi_rank == 0)
         {
            finalModel.resize(this->prs.vars);
            prs.restoreModel(finalModel);
         }
      }
      if (strat != this)
      {
         SequentialWorker *winner = (SequentialWorker *)strat;
         winner->solver->printWinningLog();
      }
      else
      {
         LOGSTAT("The winner is of type: PRS-PRE");
      }
      pthread_mutex_lock(&mutexGlobalEnd);
      pthread_cond_broadcast(&condGlobalEnd);
      pthread_mutex_unlock(&mutexGlobalEnd);
      LOGDEBUG1("Broadcasted the end");
   }
   else
   { // Else forward the information to the parent strategy
      parent->join(this, res, model);
   }
}

void PortfolioPRS::restoreModelDist()
{
   for (int i = 0; i < this->sharers.size(); i++)
      sharers[i]->join();

   LOG1("Restoring Dist Model mpi_winner=%d, finalResult=%d", mpi_winner, finalResult);

   if (mpi_winner == 0 || finalResult != SatResult::SAT)
      return;

   MPI_Status status;

   if (mpi_rank == mpi_winner)
   {
      LOG1("Winner %d sending model of size %d", mpi_winner, finalModel.size());
      MPI_Send(finalModel.data(), finalModel.size(), MPI_INT, 0, MYMPI_MODEL, MPI_COMM_WORLD);
   }

   else if (0 == mpi_rank)
   {
      int size;
      LOGDEBUG1("Root is waiting for model");

      MPI_Probe(mpi_winner, MYMPI_MODEL, MPI_COMM_WORLD, &status);
      MPI_Get_count(&status, MPI_INT, &size);

      assert(mpi_winner == status.MPI_SOURCE);
      assert(MYMPI_MODEL == status.MPI_TAG);

      finalModel.resize(size);

      MPI_Recv(finalModel.data(), size, MPI_INT, mpi_winner, MYMPI_MODEL, MPI_COMM_WORLD, &status);
      LOG1("Root received a model of size %d", size);

      finalModel.resize(this->prs.vars); /* remove BVA new variables */
      this->prs.restoreModel(finalModel);
   }

   mpi_winner = 0;
}

void PortfolioPRS::setInterrupt()
{
   for (size_t i = 0; i < slaves.size(); i++)
   {
      slaves[i]->setInterrupt();
   }
}

void PortfolioPRS::unsetInterrupt()
{
   for (size_t i = 0; i < slaves.size(); i++)
   {
      slaves[i]->unsetInterrupt();
   }
}

void PortfolioPRS::waitInterrupt()
{
   for (size_t i = 0; i < slaves.size(); i++)
   {
      slaves[i]->waitInterrupt();
   }
}
