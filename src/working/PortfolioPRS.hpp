#pragma once

#include "utils/Parameters.hpp"
#include "working/WorkingStrategy.hpp"

#include "preprocessors/PreprocessorInterface.hpp"

#include "sharing/Sharer.hpp"
#include "sharing/SharingStrategy.hpp"

#include <condition_variable>
#include <mutex>
#include <unordered_map>

enum class PRSGroups
{
	SAT = 0,
	UNSAT = 1,
	MAPLE = 2,
	LGL = 3,
	DEFAULT = 4
};

/**
 * @brief An Implementation of WorkingStrategy that mimics the strategy used in the PRS-Distributed solver from ISC24
 *
 * @details (Distributed only portfolio) (https://github.com/shaowei-cai-group/PRS-sc24) This strategy uses the
 different
 * factories SolverFactory and SharingStrategyFactory in order to instantiate the different needed components. Not all
 * solvers are supported by this strategy, and the sharing strategies are hard coded.

 * @ingroup working
 */
class PortfolioPRS : public WorkingStrategy
{
  public:
	PortfolioPRS();

	~PortfolioPRS();

	void solve(const std::vector<int>& cube) override;

	void join(WorkingStrategy* strat, SatResult res, const std::vector<int>& model) override;

	void setSolverInterrupt() override;

	void unsetSolverInterrupt() override;

	void waitInterrupt() override;

  protected:
	void restoreModelDist(std::vector<int>& model);

	void computeNodeGroup(int worldSize, int myRank);

	std::atomic<bool> strategyEnding;

	// PRS Portfolio
	// -------------
	PRSGroups nodeGroup; /* each group share clauses only between themselves */
	std::string solversPortfolio;
	std::unordered_map<PRSGroups, uint> sizePerGroup;
	unsigned rankInMyGroup;
	int right_neighbor;
	int left_neighbor;

	// Workers
	//--------
	std::vector<std::shared_ptr<PreprocessorInterface>> preprocessors; /* kept for model restoration*/
	std::mutex preprocLock;
	std::condition_variable preprocSignal;

	// Sharing
	//--------

	/* Use weak_ptr instead of shared_ptr ? */
	std::vector<std::shared_ptr<SharingStrategy>> strategies;
	std::vector<std::unique_ptr<Sharer>> sharers;
};
