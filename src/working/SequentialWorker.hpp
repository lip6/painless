#pragma once

#include "../solvers/SolverInterface.hpp"
#include "utils/Threading.hpp"
#include "working/WorkingStrategy.hpp"

#include <vector>

// Main executed by worker threads
static void*
mainWorker(void* arg);

/**
 * @brief Basic Implementation of WorkingStrategy for a sequential execution
 * @ingroup working
 */
class SequentialWorker : public WorkingStrategy
{
public:
  SequentialWorker(PainlessImpl& manager,
                   std::shared_ptr<SolverInterface> solver_);

  ~SequentialWorker();

  void solve(cube_view_t cube);

  void join(WorkingStrategy* winner,
            SatAnswer res,
            const std::vector<int>& model);

  void setSolverInterrupt();

  void unsetSolverInterrupt();

  void waitInterrupt();

  void terminate();

  std::shared_ptr<SolverInterface> solver;

protected:
  friend void* mainWorker(void* arg);

  Thread* worker;

  std::vector<int> actualCube;

  std::atomic<bool> shouldTerminate;

  std::atomic<bool> waitJob;

  Mutex waitInterruptLock;

  pthread_mutex_t mutexStart;
  pthread_cond_t mutexCondStart;
};
