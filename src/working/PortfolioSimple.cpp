
#include "working/PortfolioSimple.hpp"
#include "painless.hpp"
#include "utils/Logger.hpp"
#include "utils/Parameters.hpp"
#include "utils/System.hpp"
#include "working/SequentialWorker.hpp"
#include <thread>

#include "containers/ClauseDatabases/ClauseDatabaseFactory.hpp"
#include "preprocessors/PRS-Preprocessors/preprocess.hpp"
#include "sharing/GlobalStrategies/MallobSharing.hpp"

#include "preprocessors/GaspiInitializer.hpp"
#include "sharing/SharingStrategyFactory.hpp"
#include "solvers/SolverFactory.hpp"
#include "utils/Parsers.hpp"

#include "preprocessors/GaspiInitializer.hpp"

PortfolioSimple::PortfolioSimple() {}

PortfolioSimple::~PortfolioSimple()
{
	// Wait for sharers in order to have stats and mpi_winner if dist
	for (int i = 0; i < sharers.size(); i++) {
		sharers[i]->join();
	}

	// Restore Model if preprocessors were equisatisfiable
	if (mpi_rank <= 0 && finalResult == SatResult::SAT) {
		for (auto it = preprocessors.rbegin(); it != preprocessors.rend(); ++it) {
			LOGDEBUG1("preprocessor %u", std::distance(it, preprocessors.rend()) - 1);
			(*it)->restoreModel(finalModel);
		}
	}

	SolverFactory::printStats(this->cdclSolvers, this->localSolvers);

#ifndef NDEBUG
	for (size_t i = 0; i < slaves.size(); i++) {
		delete slaves[i];
	}
	LOGDEBUG1("PortfolioSimple After Buffer Clearing");
#endif
}

void
PortfolioSimple::solve(const std::vector<int>& cube)
{
	LOG0(">> PortfolioSimple");

	LOGWARN("New version was not tested on distirbuted mode, yet");

	strategyEnding = false;

	std::vector<simpleClause> initClauses;
	unsigned int varCount;
	unsigned int clausesCount;
	int receivedFinalResultBcast = 0;

	// TODO: merge these threads with sequential workers in next version, for less OS intensive calls
	std::vector<std::thread> solverInitializers;

	// TODO Reimplement (and separate) PRS techniques compatible with zero ended clauses, in order to not loose time in
	// serialization for mpi, and have better locality

	if (mpi_rank <= 0) {
		if (__globalParameters__.prs) {
			/* PRS */
			this->preprocessors.push_back(std::make_shared<preprocess>(0));
			this->preprocessors.at(0)->loadFormula(__globalParameters__.filename.c_str());
			for (auto& preproc : preprocessors) {
				SatResult res = preproc->solve({});
				LOGDEBUG1("PRS returned %d", res);
				if (20 == static_cast<int>(res)) {
					LOG0("PRS answered UNSAT");
					finalResult = SatResult::UNSAT;
					this->join(this, finalResult, {});
				} else if (10 == static_cast<int>(res)) {
					LOG0("PRS answered SAT");
					finalModel = preproc->getModel();
					finalResult = SatResult::SAT;
					this->join(this, finalResult, finalModel);
				}
			}

			receivedFinalResultBcast = static_cast<int>(finalResult.load());

			// Not solved during preprocessing
			if (receivedFinalResultBcast == 0) {
				// Free some memory
				for (auto& preproc : preprocessors)
					preproc->releaseMemory();

				auto lastSimplification = preprocessors.back();
				varCount = lastSimplification->getVariablesCount();

				initClauses = std::move(lastSimplification->getSimplifiedFormula());
			}
		} else if (!Parsers::parseCNF(__globalParameters__.filename.c_str(), initClauses, &varCount)) {
			PABORT(PERR_PARSING, "Error at parsing!");
		}
	}

	// Send instance via MPI from leader 0 to workers.
	if (dist) {
		TESTRUNMPI(MPI_Bcast(&receivedFinalResultBcast, 1, MPI_INT, 0, MPI_COMM_WORLD));

		if (receivedFinalResultBcast != 0) {
			mpi_winner = 0;
			finalResult = static_cast<SatResult>(receivedFinalResultBcast);
			globalEnding = true;
			LOGDEBUG1("[PRS] It is the mpi end: %d", receivedFinalResultBcast);
			mutexGlobalEnd.lock();
			condGlobalEnd.notify_all();
			mutexGlobalEnd.unlock();
			return;
		} else // send formula if not solved by preprocessing
			mpiutils::sendFormula(initClauses, &varCount, 0);
	}

	// Init Database Factory For Solvers (Is it better to put this in the SolverFactory as for SharingFactory ?)
	ClauseDatabaseFactory::initialize(__globalParameters__.maxClauseSize, __globalParameters__.importDBCap, 2, 1);

	SolverFactory::createSolvers(__globalParameters__.cpus,
								 __globalParameters__.importDB.c_str()[0],
								 __globalParameters__.solver,
								 cdclSolvers,
								 localSolvers);

	IDScaler globalIDScaler;
	IDScaler typeIDScaler;
	if (dist) {
		globalIDScaler = [rank = mpi_rank,
						  size = __globalParameters__.cpus](const std::shared_ptr<SolverInterface>& solver) {
			return rank * size + solver->getSolverId();
		};
		typeIDScaler = [rank = mpi_rank,
						size = __globalParameters__.cpus](const std::shared_ptr<SolverInterface>& solver) {
			return rank * solver->getSolverTypeCount() + solver->getSolverTypeId();
		};
	} else {
		globalIDScaler = [](const std::shared_ptr<SolverInterface>& solver) { return solver->getSolverId(); };
		typeIDScaler = [](const std::shared_ptr<SolverInterface>& solver) { return solver->getSolverTypeId(); };
	}
	SolverFactory::diversification(cdclSolvers, localSolvers, globalIDScaler, typeIDScaler);

	LOG0("Diversified all solvers");

	/* Sharing */
	/* ------- */
	if (__globalParameters__.enableMallob && dist) {
		// only global strategy
		SharingStrategyFactory::instantiateGlobalStrategies(2, globalStrategies);
		for (auto& cdcl : cdclSolvers) {
			globalStrategies.back()->addProducer(cdcl);
			globalStrategies.back()->addClient(cdcl);
			globalStrategies.back()->connectProducer(cdcl);
		}
	} else {
		SharingStrategyFactory::instantiateLocalStrategies(
			__globalParameters__.sharingStrategy, this->localStrategies, cdclSolvers);

		if (dist) {
			SharingStrategyFactory::instantiateGlobalStrategies(__globalParameters__.globalSharingStrategy,
																globalStrategies);
		}

		/* This is part of the sharing strategy: how local and global are connected */
		for (auto& lstrat : this->localStrategies) {
			for (auto& gstrat : this->globalStrategies) {
				/* Gstrat is a producer (pushes clauses) and consumer (get clauses) of lstrat*/
				lstrat->addProducer(gstrat);
				lstrat->addClient(gstrat);
				lstrat->connectProducer(gstrat); // This adds lstrat as a client to gstrat
			}
		}
	}

	std::vector<std::shared_ptr<SharingStrategy>> sharingStrategiesConcat;

	/* Launch sharers */
	for (auto lstrat : localStrategies) {
		sharingStrategiesConcat.push_back(lstrat);
	}
	for (auto gstrat : globalStrategies) {
		sharingStrategiesConcat.push_back(gstrat);
	}

	if (globalEnding) {
		this->setSolverInterrupt();
		return;
	}

	/* Solving */
	// Load formula in solvers in parallel using solverInitializers

	for (auto& cdcl : cdclSolvers) {
		SequentialWorker* myworker = new SequentialWorker(cdcl);
		this->addSlave(myworker);
		solverInitializers.emplace_back([myworker, &cube, &cdcl, &initClauses, varCount, clausesCount] {
			cdcl->addInitialClauses(initClauses, varCount);
			myworker->solve(cube);
		});
	}

	for (auto& local : localSolvers) {
		SequentialWorker* myworker = new SequentialWorker(local);
		this->addSlave(myworker);
		solverInitializers.emplace_back([myworker, &cube, &local, &initClauses, varCount, clausesCount] {
			local->addInitialClauses(initClauses, varCount);
			myworker->solve(cube);
		});
	}

	// Wait for solver initialization
	for (auto& initializer : solverInitializers)
		initializer.join();

	LOG0("All solvers are fully initialized and launched");

	SharingStrategyFactory::launchSharers(sharingStrategiesConcat, this->sharers);

	// In case GASPI is enabled
	if (__globalParameters__.gaInitPeriod) {

		if (__globalParameters__.gaPopSize < cdclSolvers.size())
			__globalParameters__.gaPopSize = cdclSolvers.size();

		LOG0("GA Initialized");
		saga::GeneticAlgorithm gaInitializer(__globalParameters__.gaPopSize,
											 varCount,
											 __globalParameters__.gaMaxGen,
											 __globalParameters__.gaMutRate,
											 __globalParameters__.gaCrossRate,
											 __globalParameters__.gaSeed,
											 clausesCount,
											 varCount,
											 initClauses);

		gaInitializer.solve();

		LOG0("GA Finished");
		// !! Warning !! The getters return references, thus two threads shouldn't access the same solution,
		// otherwise datarace !
		for (auto& cdcl : cdclSolvers) {
			// One in gaInitPeriod cdcl solver will have his phase set by GaspiInitializer  use modulo for better
			// distribution accross the different families of kissat
			if (!(cdcl->getSolverId() % __globalParameters__.gaInitPeriod)) {
				// 0 is the best solution, the first solver gets it
				uint solIdx = cdcl->getSolverId() / __globalParameters__.gaInitPeriod;
				LOGDEBUG1("Solver %u receiving solution %u", cdcl->getSolverId(), solIdx);
				saga::Solution& initPhases = gaInitializer.getNthSolution(solIdx);

				// Todo fix the +1 on solution size, it is confusing
				for (int i = 1; i < varCount; i++) {
					cdcl->setPhase(i, initPhases[i]);
				}

				LOGDEBUG1("Phases set for solver %u", cdcl->getSolverId());
			}
		}

		for (auto& local : localSolvers) {
			// One in gaInitPeriod cdcl solver will have his phase set by GaspiInitializer  use modulo for better
			// distribution accross the different families of kissat
			if (!(local->getSolverId() % __globalParameters__.gaInitPeriod)) {
				// 0 is the best solution, the first solver gets it
				uint solIdx = local->getSolverId() / __globalParameters__.gaInitPeriod;
				LOGDEBUG1("Solver %u receiving solution %u", local->getSolverId(), solIdx);
				saga::Solution& initPhases = gaInitializer.getNthSolution(solIdx);

				// Todo fix the +1 on solution size, it is confusing
				for (int i = 1; i < varCount; i++) {
					local->setPhase(i, initPhases[i]);
				}

				LOGDEBUG1("Phases set for solver %u", local->getSolverId());
			}
		}
	}

	initClauses.clear();
}

void
PortfolioSimple::join(WorkingStrategy* strat, SatResult res, const std::vector<int>& model)
{
	if (res == SatResult::UNKNOWN || strategyEnding)
		return;

	strategyEnding = true;

	setSolverInterrupt();

	if (parent == NULL) { // If it is the top strategy
		finalResult = res;
		globalEnding = true;

		if (res == SatResult::SAT) {
			finalModel = model;
		}

		if (strat != this) {
			SequentialWorker* winner = (SequentialWorker*)strat;
			winner->solver->printWinningLog();
		}

		mutexGlobalEnd.lock();
		condGlobalEnd.notify_all();
		mutexGlobalEnd.unlock();
		LOGDEBUG1("Broadcasted the end");
	} else { // Else forward the information to the parent strategy
		parent->join(this, res, model);
	}
}

void
PortfolioSimple::setSolverInterrupt()
{
	for (size_t i = 0; i < slaves.size(); i++) {
		LOGDEBUG1("Interrupting slave %u", i);
		slaves[i]->setSolverInterrupt();
	}
}

void
PortfolioSimple::unsetSolverInterrupt()
{
	for (size_t i = 0; i < slaves.size(); i++) {
		slaves[i]->unsetSolverInterrupt();
	}
}

void
PortfolioSimple::waitInterrupt()
{
	for (size_t i = 0; i < slaves.size(); i++) {
		slaves[i]->waitInterrupt();
	}
}