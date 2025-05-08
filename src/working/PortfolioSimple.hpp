#pragma once

#include "utils/Parameters.hpp"
#include "working/WorkingStrategy.hpp"

#include "solvers/CDCL/SolverCdclInterface.hpp"
#include "preprocessors/PreprocessorInterface.hpp"
#include "solvers/LocalSearch/LocalSearchInterface.hpp"

#include "sharing/Sharer.hpp"

#include "sharing/GlobalStrategies/GlobalSharingStrategy.hpp"
#include "sharing/SharingStrategy.hpp"

#include <condition_variable>
#include <mutex>

/**
 * @brief A Simple Implementation of WorkingStrategy for the portfolio parallel strategy
 * This strategy uses the different factories SolverFactory and SharingStrategyFactory in order to instantiate the
 * different needed components specified by the parameters.
 * @ingroup working
 */
class PortfolioSimple : public WorkingStrategy
{
  public:
	PortfolioSimple();

	~PortfolioSimple();

	void solve(const std::vector<int>& cube) override;

	void join(WorkingStrategy* strat, SatResult res, const std::vector<int>& model) override;

	void setSolverInterrupt() override;

	void unsetSolverInterrupt() override;

	void waitInterrupt() override;

  protected:
	std::atomic<bool> strategyEnding;

	// Solvers
	//--------
	std::vector<std::shared_ptr<SolverCdclInterface>> cdclSolvers;
	std::vector<std::shared_ptr<LocalSearchInterface>> localSolvers;
	std::vector<std::shared_ptr<PreprocessorInterface>> preprocessors; /* kept for model restoration*/

	// Sharing
	//--------

	std::vector<std::shared_ptr<SharingStrategy>> localStrategies;
	std::vector<std::shared_ptr<GlobalSharingStrategy>> globalStrategies;
	std::vector<std::unique_ptr<Sharer>> sharers;
};