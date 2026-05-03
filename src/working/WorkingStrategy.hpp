
#pragma once

#include "config/Configurable.hpp"

#include <vector>

// Forward declaration since WorkingStrategy is included in painless.hpp
class PainlessImpl;

/**
 * @defgroup working  Working Strategies
 * @brief Working Strategies related classes
 * @{
 */

/**
 * @brief Base Interface for Working Strategies
 */
class WorkingStrategy : public Configurable
{
public:
  WorkingStrategy() = delete;
  WorkingStrategy(PainlessImpl& manager)
    : m_manager(manager)
  {
    parent = NULL;
  }

  virtual void solve(cube_view_t cube) = 0;

  virtual void join(WorkingStrategy* winner,
                    SatAnswer res,
                    const std::vector<int>& model) = 0;

  virtual void setSolverInterrupt() = 0;

  virtual void unsetSolverInterrupt() = 0;

  virtual void waitInterrupt() = 0;

  virtual void terminate() = 0;

  virtual void addWorker(WorkingStrategy* worker)
  {
    workers.push_back(worker);
    worker->parent = this;
  }

  virtual ~WorkingStrategy() {}

protected:
  WorkingStrategy* parent;

  PainlessImpl& m_manager;

  std::vector<WorkingStrategy*> workers;
};

/**
 * @} //end of working group
 */