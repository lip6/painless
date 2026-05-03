
#include "working/PortfolioSimple.hpp"
#include "utils/Logger.hpp"
#include "utils/System.hpp"
#include "working/SequentialWorker.hpp"

#include "core/painless.hpp"

#include <thread>

using IDScaler =
  std::function<unsigned(const std::shared_ptr<SolverInterface>&)>;

PortfolioSimple::PortfolioSimple(
  PainlessImpl& manager,
  const std::vector<std::shared_ptr<SolverInterface>>& solvers)
  : WorkingStrategy(manager)
  , strategyTerminated(false)
  , strategyJoined(false)
{
  for (auto& solver : solvers) {
    if (auto cdcl = std::dynamic_pointer_cast<SolverCDCLInterface>(solver)) {
        cdclSolvers.push_back(cdcl);
    } else if (auto ls = std::dynamic_pointer_cast<LocalSearchInterface>(solver)) {
        localSearchers.push_back(ls);
    } else {
        LOGWARN("Unsupported solver type");
    }
}

  // Diversification
  // ===============

  IDScaler globalIDScaler;
  IDScaler typeIDScaler;
  /* Once MPI painless Class is finished, this will need to be done
   * differently*/
  // if (m_manager.isDist()) {
  // 	globalIDScaler = [rank = m_manager.mpiRank(),
  // 					  size =
  // m_manager.parameters().cpus](const std::shared_ptr<SolverInterface>&
  // solver) { 		return rank * size + solver->getSolverId();
  // 	};
  // 	typeIDScaler = [rank = m_manager.mpiRank(),
  // 					size =
  // m_manager.parameters().cpus](const std::shared_ptr<SolverInterface>&
  // solver) { 		return rank * solver->getSolverTypeCount() +
  // solver->getSolverTypeId();
  // 	};
  // } else {
  globalIDScaler = [](const std::shared_ptr<SolverInterface>& solver) {
    return solver->getSolverId();
  };
  typeIDScaler = [](const std::shared_ptr<SolverInterface>& solver) {
    return solver->getSolverTypeId();
  };
  // }

  // Disable diversification in order to not override values of the json
  if (m_manager.parameters().topology.empty()) {
    for (auto& solver : cdclSolvers) {
      solver->setSolverId(globalIDScaler(solver));
      solver->setSolverTypeId(typeIDScaler(solver));
      LOGD1(
        "Computing Id %d,%d", solver->getSolverId(), solver->getSolverTypeId());
      solver->diversify();
    }

    for (auto& solver : localSearchers) {
      solver->setSolverId(globalIDScaler(solver));
      solver->setSolverTypeId(typeIDScaler(solver));
      LOGD1(
        "Computing Id %d,%d", solver->getSolverId(), solver->getSolverTypeId());
      solver->diversify();
    }

    LOG1("Diversification done");
  }

  for (auto& cdcl : cdclSolvers) {
    SequentialWorker* myworker = new SequentialWorker(m_manager, cdcl);
    this->addWorker(myworker);
  }

  for (auto& local : localSearchers) {
    SequentialWorker* myworker = new SequentialWorker(m_manager, local);
    this->addWorker(myworker);
  }

  LOG1("SequentialWorkers created!");
}

PortfolioSimple::~PortfolioSimple()
{
  // Restore Model if preprocessors were equisatisfiable
  // if (m_manager.mpiRank() <= 0 && m_manager.result() == SatAnswer::SAT) {
  // 	for (auto it = preprocessors.rbegin(); it != preprocessors.rend(); ++it)
  // { 		LOGD1("preprocessor %u", std::distance(it, preprocessors.rend())
  // - 1);
  // 		(*it)->restoreModel(m_manager.model());
  // 	}
  // }

  if (!strategyTerminated)
    terminate();
  SolverCDCLInterface::printCDCLStats(this->cdclSolvers);

  // #ifndef NDEBUG
  for (size_t i = 0; i < workers.size(); i++) {
    delete workers[i];
  }
  LOGD1("PortfolioSimple After Workers Clearing");
  // #endif
}

void
PortfolioSimple::solve(cube_view_t cube)
{
  PABORTIF(strategyTerminated,
           PERR_BAD_BEHAVIOR,
           "Cannot launch a terminated strategy");
  LOG0(">> PortfolioSimple with %lu assumption%c",
       cube.size(),
       (cube.size() > 1) ? 's' : '\0');

  strategyJoined = false;

  /* Solving */
  for (auto& worker : workers) {
    worker->solve(cube);
  }
  LOG0("All solvers are launched");
}

void
PortfolioSimple::join(WorkingStrategy* strat,
                      SatAnswer res,
                      const std::vector<int>& model)
{
  // Return if terminated, or already joined
  if (strategyJoined.exchange(true))
    return;

  if (parent == NULL) { // If it is the top strategy

    if (strat != this) {
      SequentialWorker* winner = (SequentialWorker*)strat;
    }

    m_manager.pushResult(res, model);

    m_manager.asyncEnd();
  } else { // Else forward the information to the parent strategy
    parent->join(this, res, model);
  }
}

void
PortfolioSimple::setSolverInterrupt()
{
  for (size_t i = 0; i < workers.size(); i++) {
    workers[i]->setSolverInterrupt();
  }
}

void
PortfolioSimple::unsetSolverInterrupt()
{
  for (size_t i = 0; i < workers.size(); i++) {
    workers[i]->unsetSolverInterrupt();
  }
}

void
PortfolioSimple::waitInterrupt()
{
  for (size_t i = 0; i < workers.size(); i++) {
    LOGD2("Portfolio waiting for worker %lu, with solver %u", i);
    workers[i]->waitInterrupt();
  }
}

void
PortfolioSimple::terminate()
{
  // PABORTIF(!strategyJoined,
  //          PERR_BAD_BEHAVIOR,
  //          "Cannot terminate portfolio that didn't join");

  // Terminate only once thanks to test and set primitive
  if (!strategyTerminated.exchange(true)) {
    for (size_t i = 0; i < workers.size(); i++) {
      workers[i]->terminate();
    }
  }
}