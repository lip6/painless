
#include "working/PortfolioSimple.hpp"
#include "painless.hpp"
#include "utils/Logger.hpp"
#include "utils/Parameters.hpp"
#include "utils/System.hpp"
#include "working/SequentialWorker.hpp"
#include <thread>

#include "containers/ClauseDatabaseMallob.hpp"
#include "sharing/GlobalStrategies/MallobSharing.hpp"

#include "sharing/SharingStrategyFactory.hpp"
#include "solvers/SolverFactory.hpp"
#include "utils/Parsers.hpp"

PortfolioSimple::PortfolioSimple() {}

PortfolioSimple::~PortfolioSimple()
{
	// Wait for sharers in order to have stats and mpi_winner if dist
	for (int i = 0; i < sharers.size(); i++) {
		sharers[i]->join();
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

	strategyEnding = false;

	std::vector<simpleClause> initClauses;
	unsigned int varCount;

	if (mpi_rank <= 0)
		Parsers::parseCNF(__globalParameters__.filename.c_str(), initClauses, &varCount);

	// Send instance via MPI from leader 0 to workers.
	if (dist) {
		mpiutils::sendFormula(initClauses, &varCount, 0);
	}

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

	for (auto cdcl : cdclSolvers) {
		cdcl->addInitialClauses(initClauses, varCount);
	}

	for (auto local : localSolvers) {
		local->addInitialClauses(initClauses, varCount);
	}

	initClauses.clear();

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

	sharingStrategiesConcat.clear();

	LOG1("All solvers loaded the clauses");

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

	for (auto cdcl : cdclSolvers) {
		this->addSlave(new SequentialWorker(cdcl));
	}
	for (auto local : localSolvers) {
		this->addSlave(new SequentialWorker(local));
	}

	for (size_t i = 0; i < slaves.size(); i++) {
		slaves[i]->solve(cube);
	}

	SharingStrategyFactory::launchSharers(sharingStrategiesConcat, this->sharers);
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