#include "painless.h"

#include "utils/Parameters.h"
#include "utils/Logger.h"
#include "utils/ErrorCodes.h"
#include "utils/System.h"
#include "utils/MpiUtils.h"

#include "working/PortfolioSBVA.h"
#include "working/SequentialWorker.h"

#include "solvers/SolverFactory.h"

#include "clauses/ClauseDatabaseLockFree.h"

#include "sharing/SharingStrategyFactory.h"

void runSBVA(char *filename, std::mt19937 &engine, std::uniform_int_distribution<int> &uniform, PortfolioSBVA *master, std::vector<simpleClause> &initClauses, unsigned initVarCount, std::shared_ptr<StructuredBVA> sbva)
{
   // Parse
   std::vector<simpleClause> clauses;

   sbva->printParameters();

   sbva->addInitialClauses(initClauses, initVarCount);

   if (!sbva->isInitialized())
   {
      LOGWARN("SBVA %d was not initialized in time thus returning !", sbva->getId());
      master->joinSbva();
      return;
   }

   sbva->run();
   // sbva->printStatistics();

   LOG2("Sbva %d ended", sbva->id);
   sbva->printStatistics();

   master->joinSbva();
}

PortfolioSBVA::PortfolioSBVA()
{
   /* All this dist mess is to be solved, this portfolio doesn't support dist, simpler is always better */
#ifndef NDIST
   if (dist)
   {
      int maxClauseSize = Parameters::getIntParam("maxClauseSize", 50);
      this->globalDatabase = std::make_shared<GlobalDatabase>(cpus, std::make_shared<ClauseDatabaseLockFree>(maxClauseSize), std::make_shared<ClauseDatabaseLockFree>(maxClauseSize));
   }
#endif
}

PortfolioSBVA::~PortfolioSBVA()
{
   /* Must be after the setInterrupt of sbva and lsSolver, and cannot be in Interrupt since it can be called from workers */
   for (auto &worker : clausesLoad)
      worker.join();

   for (auto &worker : sbvaWorkers)
      worker.join();

   for (size_t i = 0; i < slaves.size(); i++)
   {
      delete slaves[i];
   }
}

void PortfolioSBVA::solve(const std::vector<int> &cube)
{
   LOG(">> PortfolioSBVA");

   double approxMemoryPerSolver;

   // Timeout management
   unsigned sbvaTimeout = Parameters::getIntParam("sbva-timeout", 500);

   unsigned nbSBVA = Parameters::getIntParam("sbva-count", 12);
   int nbLs = Parameters::getIntParam("ls-after-sbva", 2);
   int initNbSlaves = cpus - nbSBVA;

   /* TODO: I should really generalize sequentialWorker and extend Factory to simplify all this mess */

   std::vector<std::shared_ptr<SolverCdclInterface>> cdclSolvers;
   std::vector<std::shared_ptr<LocalSearchSolver>> localSolvers;

   if (initNbSlaves < 0)
   {
      LOGERROR("The specified number of threads %d is less than the wanted number of sbvas %d !!", cpus, nbSBVA);
      exit(PERR_ARGS_ERROR);
   }

   this->strategyEnding = false;

   // parseFormula(Parameters::getFilename(), this->initClauses, &this->beforeSbvaVarCount);

   LOG1("Memory 1: %f", MemInfo::getUsedMemory());

   /* PRS */
   int res = prs.do_preprocess(Parameters::getFilename());
   LOGDEBUG1("PRS returned %d", res);
   if (20 == res)
   {
      LOG("PRS answered UNSAT");
      finalResult = SatResult::UNSAT;
      this->join(this, finalResult, {});
      return;
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
      return;
   }
   // Free some memory
   this->prs.release_most();

   LOG1("Memory 2: %f", MemInfo::getUsedMemory());

   // PRS uses indexes from 1 to prs.clauses
   this->initClauses = std::move(prs.clause);
   this->initClauses.erase(this->initClauses.begin());
   this->beforeSbvaVarCount = prs.vars;

   if (nbSBVA && initClauses.size() > Parameters::getIntParam("sbva-max-clause", MILLION * 10))
   {
      nbSBVA = 1;
      sbvaTimeout = 2000;
      initNbSlaves = cpus - nbSBVA;
      nbLs = 0;
      LOGWARN("Too much clauses, sbva will be used only for one kissat with timeout %d. No Yalsat will be instantiated", sbvaTimeout);
   }

   /* ~solver mem + learned clauses ~= reduction_ratio*PRS memory : this is a vague approximation! TODO enhance the solverFactory */
   double reductionRatio = (((double)prs.vars / prs.orivars) * ((double)prs.clauses / prs.oriclauses));
   approxMemoryPerSolver = 1.2 * reductionRatio * MemInfo::getUsedMemory();

   LOG1("Reduction Ratio: %f, Memory : %f", reductionRatio, approxMemoryPerSolver);

   // Add initial slaves
   for (int i = 1; i <= initNbSlaves; i++)
   {
      /* i * solverMem */
      LOGDEBUG1("Memory: %f / %f. %f / %f", MemInfo::getUsedMemory() + i * approxMemoryPerSolver, MemInfo::getTotalMemory(), i * approxMemoryPerSolver, MemInfo::getAvailableMemory());
      if (MemInfo::getAvailableMemory() <= i * approxMemoryPerSolver)
      {
         LOGERROR("SolverFactory cannot instantiate the %d th solver (%f/%f) due to insufficient memory (to be used: %f / %f)", i, i * approxMemoryPerSolver, MemInfo::getAvailableMemory(), MemInfo::getUsedMemory() + i * approxMemoryPerSolver, MemInfo::getTotalMemory());
         initNbSlaves = i;
         nbSBVA = 0;
         nbLs = 0;
         break;
      }
      SolverFactory::createSolver('k', cdclSolvers, localSolvers);
   }
   // Launch sbva
   if (nbSBVA > 1)
   {
      for (int i = 0; i < nbSBVA; i++)
      {
         this->sbvas.emplace_back(new StructuredBVA(initNbSlaves + i));
         this->sbvas.back()->diversify(engine, uniform);
      }
   }
   else if (1 == nbSBVA && (MemInfo::getAvailableMemory() > approxMemoryPerSolver))
   {
      this->sbvas.emplace_back(new StructuredBVA(initNbSlaves));
      this->sbvas.back()->setTieBreakHeuristic(SBVATieBreak::MOSTOCCUR); /* THREEHOPS uses too much memory */
      /* Do not call diversify */
   }
   else
   {
      nbSBVA = 0;
      LOG("No SBVA will be executed");
   }

   /* Diversification must be done before addInitialClauses because of kissat_reserve (options changes the required memory) */
   /* Do not diversify localSearch not even srand */
   SolverFactory::diversification(cdclSolvers, {}, Parameters::getBoolParam("dist"), mpi_rank, world_size);

   /* Load clauses */
   for (auto cdclSolver : cdclSolvers)
   {
      this->addSlave(new SequentialWorker(cdclSolver));
      clausesLoad.emplace_back(&SolverCdclInterface::addInitialClauses, cdclSolver.get(), std::ref(this->initClauses), this->beforeSbvaVarCount);
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

   /* Launch SBVA */
   /* ----------- */

   for (int i = 0; i < nbSBVA; i++)
   {
      sbvaWorkers.emplace_back(runSBVA, Parameters::getFilename(), std::ref(this->engine), std::ref(this->uniform), this, std::ref(initClauses), this->beforeSbvaVarCount, this->sbvas[i]);
   }

   for (auto &worker : clausesLoad)
      worker.join();

   LOG("Loaded clauses in initial %d kissats", initNbSlaves);

   /* TODO: asynchronous exit for better management than all the ifs used before each major step */
   if (globalEnding)
   {
      this->setInterrupt();
      return;
   }

   clausesLoad.clear();
   cdclSolvers.clear();
   localSolvers.clear();

   /* Launch Initial Solvers */
   for (auto slave : slaves)
      slave->solve(cube);

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

   sharingStrategiesConcat.clear();

   if (!sbvas.size())
   {
      LOGWARN("No sbvas added, launching only kissats");
      return;
   }

   /* All this exists because I didn't want to change SequentialWorker or make SBVA inherits SolverInterface: big changes are coming */
   if (!globalEnding)
   {
      std::unique_lock<std::mutex> lock(this->sbvaLock);
      auto ret = this->sbvaSignal.wait_for(lock, std::chrono::seconds(sbvaTimeout));
      LOGWARN("Portfolio SBVA wakeupRet = %d , globalEnding = %d ", ret, globalEnding.load());
   }

   /* no else to execute after wakeup */
   if (globalEnding)
   {
      this->setInterrupt();
      return;
   }

   this->stopSbva();

   // choose best; clear clauses of the losers
   for (auto &worker : this->sbvaWorkers)
      worker.join();

   sbvaWorkers.clear();

   /* Didn't use [0] since sbva[0] may be unantialized, must use <= & >= */
   unsigned leastClauses = 0; /* looking for max */
   unsigned sbvaVarCount = 0;

   this->leastClausesIdx = -1;

   for (int i = 0; i < nbSBVA; i++)
   {
      if (this->sbvas[i]->isInitialized() && this->sbvas[i]->getNbClausesDeleted() >= leastClauses)
      {
         LOGDEBUG1("SBVA %d is initialized (%d)", this->sbvas[i]->getId(), this->sbvas[i]->isInitialized());
         leastClauses = this->sbvas[i]->getNbClausesDeleted();
         leastClausesIdx = i;
      }
   }

   if (globalEnding)
   {
      this->setInterrupt();
      return;
   }

   /* TODO: redo the local search nbunsat with different techniques ? */
   std::vector<simpleClause> bestClauses;

   if (leastClausesIdx >= 0)
   {
      bestClauses = std::move(this->sbvas[leastClausesIdx]->getClauses());
      sbvaVarCount = this->sbvas[leastClausesIdx]->getVariablesCount();
      LOG("Best SBVA according to nbClausesDeleted is : %d", this->sbvas[leastClausesIdx]->getId());
   }
   else
   {
      LOGWARN("All SBVAS were not initialized, thus launching with initial clauses");
      bestClauses = std::move(initClauses);
      sbvaVarCount = this->beforeSbvaVarCount;
   }

   initClauses.clear();

   if (nbLs > 0)
   {
      if (nbLs > nbSBVA)
      {
         LOGWARN("The number of local searches %d is greater than the number of sbvas %d. Only locals will be launched", nbLs, nbSBVA);
         nbLs = nbSBVA; // only locals
      }
      for (int i = 0; i < nbLs; i++)
      {
         SolverFactory::createSolver('y', cdclSolvers, localSolvers);
      }
      nbSBVA -= nbLs;
   }

   if (globalEnding)
   {
      this->setInterrupt();
      return;
   }

   /* Instantiate new solvers */
   for (int i = 0; i < nbSBVA; i++)
   {
      /* Didn't use factory because of Id */
      SolverFactory::createSolver('k', cdclSolvers, localSolvers);
   }

   // Diversification must be done before adding clauses
   /* TODO separate Local And Cdcl diversification */
   SolverFactory::diversification(cdclSolvers, localSolvers, Parameters::getBoolParam("dist"), mpi_rank, world_size);

   for (int i = 0; i < nbLs; i++)
   {
      clausesLoad.emplace_back(&LocalSearchSolver::addInitialClauses, localSolvers[i].get(), std::ref(bestClauses), sbvaVarCount);
   }
   for (int i = 0; i < nbSBVA; i++)
   {
      clausesLoad.emplace_back(&SolverCdclInterface::addInitialClauses, cdclSolvers[i].get(), std::ref(bestClauses), sbvaVarCount);
   }

   // Wait for clauses init
   for (auto &worker : clausesLoad)
      worker.join();

   clausesLoad.clear();

   if (globalEnding)
   {
      this->setInterrupt();
      return;
   }

   bestClauses.clear();

   /* Update slaves and clear uneeded workers */
   this->workersLock.lock();
   if (globalEnding)
   {
      this->workersLock.unlock();
      return;
   }

   for (auto newSolver : cdclSolvers)
   {
      this->addSlave(new SequentialWorker(newSolver));
   }
   for (auto newLocal : localSolvers)
   {
      this->addSlave(new SequentialWorker(newLocal));
   }

   SharingStrategyFactory::addEntitiesToLocal(this->localStrategies, cdclSolvers);

   cdclSolvers.clear();
   localSolvers.clear();

   // clear all sbvas to free some memory
   this->sbvas.clear();

   this->workersLock.unlock();

   // Launch slaves
   for (int i = initNbSlaves; i < slaves.size(); i++)
   {
      slaves[i]->solve({});
   }

   // Not needed anymore
   localStrategies.clear();
   globalStrategies.clear();
}

void PortfolioSBVA::join(WorkingStrategy *strat, SatResult res,
                         const std::vector<int> &model)
{
   LOGDEBUG1("SBVA joining from slave %p with res %d. GlobalEnding = %d, StrategyEnding = %d", strat, res, globalEnding.load(), strategyEnding.load());

   if (res == UNKNOWN || strategyEnding || globalEnding)
      return;

   strategyEnding = true;

   setInterrupt();

   LOGDEBUG1("Interrupted all workers");

   if (parent == NULL)
   { // If it is the top strategy

      globalEnding = true;

      finalResult = res;

      if (res == SatResult::SAT)
      {
         finalModel = model; /* Make model non const for safe move ? */
         /* remove potential sbva added variables */
         finalModel.resize(this->beforeSbvaVarCount);
         prs.restoreModel(finalModel);
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
      /* All this is because I didn't want to change SequentialWorker or make SBVA inherits SolverInterface: big changes are coming */
      this->joinSbva();
      pthread_mutex_lock(&mutexGlobalEnd);
      pthread_cond_broadcast(&condGlobalEnd);
      pthread_mutex_unlock(&mutexGlobalEnd);
      LOGDEBUG1("Broadcasted the end");

      for (int i = 0; i < this->sharers.size(); i++)
         this->sharers[i]->printStats();
   }
   else
   { // Else forward the information to the parent strategy
      parent->join(this, res, model);
   }
}

void PortfolioSBVA::setInterrupt()
{
   this->workersLock.lock();
   for (int i = 0; i < slaves.size(); i++)
   {
      slaves[i]->setInterrupt();
   }

   for (auto sbva : sbvas)
      sbva->setInterrupt();

   this->workersLock.unlock();
}

void PortfolioSBVA::unsetInterrupt()
{
   for (int i = 0; i < slaves.size(); i++)
   {
      slaves[i]->unsetInterrupt();
   }

   for (auto sbva : sbvas)
      sbva->unsetInterrupt();
}

void PortfolioSBVA::waitInterrupt()
{
   for (size_t i = 0; i < slaves.size(); i++)
   {
      slaves[i]->waitInterrupt();
   }
}

void PortfolioSBVA::stopSbva()
{
   for (auto sbva : sbvas)
      sbva->setInterrupt();
}

void PortfolioSBVA::joinSbva()
{
   std::lock_guard<std::mutex> lock(this->sbvaLock);
   /* globalEnding for classical join from solvers */
   if (globalEnding || (++this->sbvaEnded) >= this->sbvas.size())
      this->sbvaSignal.notify_one(); /* only one thread must wait on this */
}