
#pragma once

#include "solvers/SolverInterface.hpp"

#include <vector>

/**
 * @defgroup working  Working Strategies
 * @brief Working Strategies related classes
 * @{
 */

/**
 * @brief Base Interface for Working Strategies
 */
class WorkingStrategy
{
  public:
	WorkingStrategy() { parent = NULL; }

	virtual void solve(const std::vector<int>& cube) = 0;

	virtual void join(WorkingStrategy* winner, SatResult res, const std::vector<int>& model) = 0;

	virtual void setSolverInterrupt() = 0;

	virtual void unsetSolverInterrupt() = 0;

	virtual void waitInterrupt() = 0;

	virtual void addSlave(WorkingStrategy* slave)
	{
		slaves.push_back(slave);
		slave->parent = this;
	}

	virtual ~WorkingStrategy() {}

  protected:
	WorkingStrategy* parent;

	std::vector<WorkingStrategy*> slaves;
};

/**
 * @} //end of working group
 */