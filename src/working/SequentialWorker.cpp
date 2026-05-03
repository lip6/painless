#include "working/SequentialWorker.hpp"
#include "core/painless.hpp"
#include "utils/Logger.hpp"
#include "utils/Parsers.hpp"

#include <unistd.h>

// Main executed by worker threads
void*
mainWorker(void* arg)
{
  SequentialWorker* sq = (SequentialWorker*)arg;

  SatAnswer res = SatAnswer::UNKNOWN;

  std::vector<int> model;

  while (sq->shouldTerminate == false) {

    LOGD2("Sequential Worker for solver of %s %d loops",
          typeid(*(sq->solver)).name(),
          sq->solver->getSolverId());
    pthread_mutex_lock(&sq->mutexStart);

    LOGD2("Sequential Worker for solver of %s %d will wait for a job",
          typeid(*(sq->solver)).name(),
          sq->solver->getSolverId());
    while ((sq->waitJob == true && sq->shouldTerminate == false)) {
      pthread_cond_wait(&sq->mutexCondStart, &sq->mutexStart);
    }

    pthread_mutex_unlock(&sq->mutexStart);

    if (sq->shouldTerminate == false) {
      LOGD1("Sequential Worker for solver of %s %d before solve with cube of "
            "size %u",
            typeid(*(sq->solver)).name(),
            sq->solver->getSolverId(),
            sq->actualCube.size());

      sq->waitInterruptLock.lock();

      res = sq->solver->solve(sq->actualCube);

      sq->waitInterruptLock.unlock();

      LOGD2("Sequential Worker for solver of %s %d after solve",
            typeid(*(sq->solver)).name(),
            sq->solver->getSolverId());

      if (res == SatAnswer::SAT) {
        model = sq->solver->getModel();
      }

      sq->join(NULL, res, model);

      model.clear();
    } else {
      LOGD2("Sequential Worker for solver of %s %d woke up to end",
            typeid(*(sq->solver)).name(),
            sq->solver->getSolverId());
    }
  }

  return NULL;
}

// Constructor
SequentialWorker::SequentialWorker(PainlessImpl& manager,
                                   std::shared_ptr<SolverInterface> solver_)
  : WorkingStrategy(manager)
{
  solver = solver_;
  shouldTerminate = false;
  waitJob = true;

  pthread_mutex_init(&mutexStart, NULL);
  pthread_cond_init(&mutexCondStart, NULL);

  worker = new Thread(mainWorker, this);
}

// Destructor
SequentialWorker::~SequentialWorker()
{
  if (!shouldTerminate)
    terminate();

  worker->join();
  delete worker;

  pthread_mutex_destroy(&mutexStart);
  pthread_cond_destroy(&mutexCondStart);
}

void
SequentialWorker::solve(cube_view_t cube)
{
  actualCube.clear();
  for (auto lit : cube)
    actualCube.push_back(lit);

  LOGD1("Solve with %u assumptions", actualCube.size());

  waitJob = false;

  pthread_mutex_lock(&mutexStart);
  pthread_cond_signal(&mutexCondStart);
  pthread_mutex_unlock(&mutexStart);
}

void
SequentialWorker::join(WorkingStrategy* winner,
                       SatAnswer res,
                       const std::vector<int>& model)
{
  LOGD1("SequentialWorker %p of solver %s is joining with res = %d.",
        this,
        typeid(*solver).name(),
        res);

  if (!waitJob) {
    waitJob = true;
    solver->setSolverInterrupt();
  }

  if (parent == NULL) {
    if (m_manager.pushResult(res, model))
      this->terminate();
  } else {
    LOGD1("SequentialWorker %p calls its parent", this);
    parent->join(this, res, model);
  }
}

void
SequentialWorker::waitInterrupt()
{
  waitInterruptLock.lock();
  waitInterruptLock.unlock();
}

void
SequentialWorker::terminate()
{
  shouldTerminate = true;
  pthread_mutex_lock(&mutexStart);
  pthread_cond_signal(&mutexCondStart);
  pthread_mutex_unlock(&mutexStart);

  // worker->join();
}

void
SequentialWorker::setSolverInterrupt()
{
  LOGD1("Interrupting worker %u", solver->getSolverId());
  if (waitJob || shouldTerminate)
    return;
  waitJob = true;
  solver->setSolverInterrupt();
  // pthread_mutex_lock(&mutexStart);
  // pthread_cond_signal(&mutexCondStart);
  // pthread_mutex_unlock(&mutexStart);
}

void
SequentialWorker::unsetSolverInterrupt()
{
  LOGD1("De-Interrupting worker %u", solver->getSolverId());
  waitJob = false;
  solver->unsetSolverInterrupt();
}