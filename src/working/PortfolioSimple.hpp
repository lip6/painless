#pragma once

#include "solvers/CDCL/SolverCDCLInterface.hpp"
#include "solvers/LocalSearch/LocalSearchInterface.hpp"

#include "sharing/Sharer.hpp"

#include "sharing/SharingStrategy.hpp"

#include "working/WorkingStrategy.hpp"

/**
 * @brief A Simple Implementation of WorkingStrategy for the portfolio parallel
 * strategy This strategy uses the different factories SolverFactory and
 * SharingStrategyFactory in order to instantiate the different needed
 * components specified by the parameters.
 * @ingroup working
 */
class PortfolioSimple : public WorkingStrategy
{
public:
  PortfolioSimple(PainlessImpl& manager,
                  const std::vector<std::shared_ptr<SolverInterface>>& solvers);

  ~PortfolioSimple();

  void solve(cube_view_t cube) override;

  void join(WorkingStrategy* strat,
            SatAnswer res,
            const std::vector<int>& model) override;

  void setSolverInterrupt() override;

  void unsetSolverInterrupt() override;

  void waitInterrupt() override;

  void terminate() override;

protected:
  std::atomic<bool> strategyTerminated;
  std::atomic<bool> strategyJoined;

  // Solvers
  //--------
  std::vector<std::shared_ptr<SolverCDCLInterface>> cdclSolvers;
  std::vector<std::shared_ptr<LocalSearchInterface>> localSearchers;
  // std::vector<std::shared_ptr<PreprocessorInterface>> preprocessors; /* kept
  // for model restoration */
};