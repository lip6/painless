#include "core/painless.hpp"

#include "utils/NumericConstants.hpp"
#include "utils/ErrorCodes.hpp"

#include "utils/Parsers.hpp"

#include "utils/Logger.hpp"

#include <random>
#include <thread>
#include <unistd.h>

PainlessImpl::PainlessImpl()
  : m_shouldEndSolving(false)
  , m_shouldTerminate(false)
  , m_state(PainlessImpl::State::CREATED)
  , m_workerCount(0)
// , m_interruptedWorkerCount(0)
{
}

PainlessImpl::~PainlessImpl()
{
  LOG0("Destroying Painless");
  LOGWARNIF(m_state < PainlessImpl::State::INTERRUPTED,
            "You are destroying a solver that didn't finish or fail");
  m_state = PainlessImpl::State::TERMINATED;

  m_shouldTerminate = true;
  m_shouldEndSolving = true;

  LOGD1("Terminating the main strategy");
  m_mainStrategy->terminate();

  LOGD1("Terminating the sharers");
  for (auto& sharer : m_sharers) {
    sharer->asyncTerminate();
  }

  // Wake up everyone after setting all the terminating bools
  {
    LOGD1("Notifying wait on startup");
    std::lock_guard lock(m_startupMX);
    m_startupCV.notify_all(); // wake up every one waiting on startup
  }

  {
    LOGD1("Notifying wait on endRequest");
    std::lock_guard lock(m_endRequestMX);
    m_endRequestCV.notify_all(); // wake up every one waiting on end
  }

  {
    LOGD1("Will notify the end");
    std::lock_guard lock(m_endCompleteMX);
    m_endCompleteCV.notify_all(); // wake up every one waiting on termination
  }

  LOGD1("Joining the sharers");
  for (auto& sharer : m_sharers) {
    sharer->join();
  }

  if (m_watchdogThread.joinable())
    m_watchdogThread.join();

  for (auto& pair : m_modelSeenCount) {
    LOGWARN("I have seen the model %llu, %u times (including this one)",
            pair.first,
            pair.second);
  }
}

std::string
PainlessImpl::signature()
{
  return "PArallel INstantiabLE Sat Solver (PaInleSS) v1.27.0";
}

bool
PainlessImpl::loadDIMACS(const std::string& filename)
{
  return Parsers::loadCNF(filename.c_str(), [this](int lit) {
    this->addLiteral(lit);
    return true;
  });
}

void
PainlessImpl::addLiteral(lit_t lit)
{
  PABORTIF(m_state < PainlessImpl::State::INITIALIZED,
           PERR_NOT_SUPPORTED,
           "Cannot add literals if the solver is not initialized");
  PABORTIF(m_state > PainlessImpl::State::INTERRUPTED,
           PERR_NOT_SUPPORTED,
           "Cannot add literals if the solver finished solving");
  if (lit) {
    m_bufferedCls.push_back(lit);
  } else {
    // for (auto& solver : m_solvers) {
    // 	solver->addClause(m_bufferedCls);
    // }
    // Producer side, requires a unique lock
    UNIQUE_LOCK(std::shared_mutex, m_formulaMX, lock);
    m_formula.push_row(m_bufferedCls.begin(), m_bufferedCls.end());
    m_bufferedCls.clear();
  }

  /* Solver remains in Solving state even after adding new clauses (useful for
   * event subscription bug catching)*/
  if (m_state < PainlessImpl::State::INPUT)
    m_state = PainlessImpl::State::INPUT;
}

void
PainlessImpl::assume(lit_t lit)
{
  PABORTIF(m_state < PainlessImpl::State::INPUT,
           PERR_NOT_SUPPORTED,
           "Cannot add assumptions if the solver didn't add any clause");
  m_cube.push_back(lit);
}

#include <utils/xxhash64.hpp>

result_t
PainlessImpl::solve()
{
  this->asyncSolve();
  this->waitOnEnd();

  result_t result;

  if (result.answer == SatAnswer::UNKNOWN) {
    PABORTIF(!popLastResult(result),
             PERR_BAD_BEHAVIOR,
             "No result found (check if it was not popped before)");
  }

  LOGD2("Painless::solve() will return %u, model is of size %u",
        static_cast<int>(result.answer),
        result.model.size());

  return result;
}

void
PainlessImpl::asyncSolve()
{
  PABORTIF(
    m_state < PainlessImpl::State::INPUT,
    PERR_NOT_SUPPORTED,
    "Cannot run solve if the solver was not in INPUT state at least once");

  PABORTIF(m_state > PainlessImpl::State::INTERRUPTED,
           PERR_NOT_SUPPORTED,
           "Cannot run solve if the solver finished solving");

  // Launch End Watchdog only the first time (TODO should this be set outside of PainlessImp ?)
  if (this->parameters().timeout > 0 && !m_watchdogThread.joinable()) {
    m_workerCount++;

    m_watchdogThread = std::thread([this]() {
      
      while (!this->shouldTerminate()) {
        this->waitOnSolving();

        UNIQUE_LOCK(std::mutex, this->m_endRequestMX, lock);

        std::chrono::microseconds timeout =
          std::chrono::microseconds(this->parameters().timeout * MILLION);

        bool isEnd = false;

        LOGD1("WatchDog will sleep for %u us", timeout.count());

        // If condition became true, then the thread woke up before timeout.
        // !isEnd == timedOut
        isEnd = this->m_endRequestCV.wait_for(
          llock, timeout, [this] { return this->shouldEndSolving(); });

        LOGD1("WatchDog wakeupStatus = %s , isPainlessEnd = %d ",
              (!isEnd ? "timeout" : "notimeout"),
              static_cast<int>(this->shouldEndSolving()));

        llock.unlock(); // asyncEnd needs it
        // Timeout passed and no solution
        if (!isEnd) {
          // Lock is reaquired by cv
          // Push the timeout result before notification for good behavior of
          // popResult after waitOnEnd
          this->pushResult(SatAnswer::TIMEOUT, model_t{});
          this->asyncEnd();
        }
      }
    });
  } // else no watchdog

  m_shouldEndSolving = false;
  m_solvingHasEnded = false;
  m_mainStrategy->unsetSolverInterrupt();

  m_mainStrategy->solve(
    m_cube); // Should return after launching solving threads

  // Wake up the workers (watchdog and sharer)
  if (m_state == PainlessImpl::State::INPUT ||
      m_state == PainlessImpl::State::INTERRUPTED) {
    m_startupMX.lock();
    m_startupCV.notify_all();
    m_startupMX.unlock();
    m_state = PainlessImpl::State::SOLVING;
  }

  // Clear assumption for next solve
  m_cube.clear();
}

void
PainlessImpl::asyncEnd()
{
  m_shouldEndSolving = true;
  m_state = PainlessImpl::State::INTERRUPTED;

  m_mainStrategy->setSolverInterrupt();

  {
    std::lock_guard lock(m_endRequestMX);
    m_endRequestCV.notify_all();
  }

  LOGD1("Interrupted main strategy and notified watch dog to wake up");

  {
    m_mainStrategy->waitInterrupt();
    std::lock_guard lock(m_endCompleteMX);
    m_endCompleteCV.notify_all();

    LOGD1("Waited for main strategy and notified end");
  }
}

void
PainlessImpl::waitOnEnd()
{
  PABORTIF(m_state < PainlessImpl::State::SOLVING,
           PERR_NOT_SUPPORTED,
           "The solver wasn't launched, you cannot wait on its result.");

  UNIQUE_LOCK(std::mutex, m_endCompleteMX, lock);
  m_endCompleteCV.wait(llock, [this] { return this->shouldEndSolving(); });
}

bool
PainlessImpl::waitOnEndFor(uint64_t microseconds)
{
  PABORTIF(m_state < PainlessImpl::State::SOLVING,
           PERR_NOT_SUPPORTED,
           "The solver wasn't launched, you cannot wait on its result.");

  UNIQUE_LOCK(std::mutex, m_endCompleteMX, lock);
  m_endCompleteCV.wait_for(llock,
                           std::chrono::microseconds(microseconds),
                           [this] { return this->hasSolvingEnded(); });

  return m_solvingHasEnded;
}

// bool
// PainlessImpl::markAsInterrupted(uint workerIdx)
// {
//   PABORTIF(m_interruptedWorkersMask.at(workerIdx),
//            PERR_BAD_BEHAVIOR,
//            "Worker %u is was already set as interrupted",
//            workerIdx);

//   bool oldValue = m_interruptedWorkersMask[workerIdx];
//   m_interruptedWorkersMask[workerIdx] = true;

//   m_interruptedWorkerCount++;
//   if (m_interruptedWorkerCount == m_workerCount) {
//     m_solvingHasEnded = true;
//     LOGD1("Will notify the end");
//     LOCK_GUARD(std::mutex, m_endCompleteMX, l);
//     m_endCompleteCV.notify_all();
//   }

//   return oldValue;
// }

// bool
// PainlessImpl::unmaskAsInterrupted(uint workerIdx)
// {
//   PABORTIF(!m_interruptedWorkersMask.at(workerIdx),
//            PERR_BAD_BEHAVIOR,
//            "Worker %u is was already set as not interrupted",
//            workerIdx);
//   bool oldValue = m_interruptedWorkersMask[workerIdx];
//   m_interruptedWorkersMask[workerIdx] = false;
//   m_interruptedWorkerCount--;

//   return oldValue;
// }

void
PainlessImpl::waitOnSolving()
{
  LOGWARNIF(m_state == PainlessImpl::State::SOLVING,
            "The solver is in SOLVING state, no need to wait on its startup.");
  UNIQUE_LOCK(std::mutex, m_startupMX, lock);
  m_startupCV.wait(llock, [this] {
    return this->m_state == PainlessImpl::State::SOLVING ||
           this->shouldTerminate();
  });
}

bool
PainlessImpl::waitOnResult()
{
  PABORTIF(m_state < PainlessImpl::State::SOLVING,
           PERR_NOT_SUPPORTED,
           "The solver wasn't launched, you cannot wait on its modelQueue.");
  UNIQUE_LOCK(std::mutex, m_resultQueueMX, lock);
  m_resultQueueCV.wait(llock, [this] { return !this->m_resultQueue.empty(); });
  return !m_resultQueue.empty();
}

bool
PainlessImpl::waitOnResultFor(uint64_t microseconds)
{
  PABORTIF(m_state < PainlessImpl::State::SOLVING,
           PERR_NOT_SUPPORTED,
           "The solver wasn't launched, you cannot wait on its modelQueue.");
  UNIQUE_LOCK(std::mutex, m_resultQueueMX, lock);
  m_resultQueueCV.wait_for(llock,
                           std::chrono::microseconds(microseconds),
                           [this] { return !this->m_resultQueue.empty(); });
  return !m_resultQueue.empty();
}

bool
PainlessImpl::pushResult(SatAnswer answer, model_t&& model)
{
  if (shouldEndSolving()) {
    LOGWARN("Results are not accepted when the solver is interrupted");
    return true;
  }

  if (model.size()) {
    uint64_t modelHash =
      XXHash64::hash(model.data(), model.size() * sizeof(model.at(0)), 0);
    m_modelSeenCount[modelHash]++;
  }

  {
    LOCK_GUARD(std::mutex, m_resultQueueMX, l);
    m_resultQueue.emplace_back(answer, std::move(model));
    m_resultQueueCV.notify_all();
  }

  // Return true if everything was done fine
  return true;
}

bool
PainlessImpl::pushResult(SatAnswer answer, model_view_t modelView)
{
  if (shouldEndSolving()) {
    LOGWARN("Results are not accepted when the solver is interrupted");
    return true;
  }

  LOGD2("Pushing a result %d with model fo size %u",
        static_cast<int>(answer),
        modelView.size());

  if (modelView.size()) {
    uint64_t modelHash = XXHash64::hash(
      modelView.data(), modelView.size() * sizeof(modelView[0]), 0);
    m_modelSeenCount[modelHash]++;
  }

  {
    LOCK_GUARD(std::mutex, m_resultQueueMX, l);
    model_t model(modelView.begin(), modelView.end());
    m_resultQueue.emplace_back(answer, std::move(model));
    m_resultQueueCV.notify_all();
  }

  return true;
}

bool
PainlessImpl::popResult(result_t& result)
{
  PABORTIF(m_state < PainlessImpl::State::SOLVING,
           PERR_NOT_SUPPORTED,
           "You cannot pop a result from a solver that didn't start solving");

  LOCK_GUARD(std::mutex, m_resultQueueMX, l);
  if (m_resultQueue.empty())
    return false;
  else {
    result = m_resultQueue.front();
    m_resultQueue.pop_front();
  }
  return true;
}

bool
PainlessImpl::popLastResult(result_t& result)
{
  PABORTIF(m_state < PainlessImpl::State::SOLVING,
           PERR_NOT_SUPPORTED,
           "You cannot pop a result from a solver that didn't start solving");

  LOCK_GUARD(std::mutex, m_resultQueueMX, l);
  if (m_resultQueue.empty())
    return false;
  else {
    result = m_resultQueue.back();
    m_resultQueue.pop_back();
  }
  return true;
}

// lit_t
// PainlessImpl::valueOf(lit_t lit)
// {
// 	PABORTIF(m_result.answer != SatAnswer::SAT, PERR_BAD_BEHAVIOR, "Latest
// saved result answer is not SAT!");

// 	/* TODO centralize this transformation for the whole framework */
// 	return m_result.model.at(std::abs(lit) - 1);
// }

// bool
// PainlessImpl::failedDueTo(lit_t lit)
// {
// 	PABORT(PERR_NOT_SUPPORTED, "Not implemented, yet. Coming soon.");
// }

// void
// PainlessImpl::setTerminateCallback(void* data, int (*terminate)(void* data))
// {
// 	PABORTIF(m_state > PainlessImpl::State::INITIALIZED,
// 			 PERR_NOT_SUPPORTED,
// 			 "Cannot set the termination checker after the
// initialization"); 	m_terminationData = data; 	m_checkTermination =
// terminate; 	LOGWARN("For now, painless do not check outside termination
// clauses, it will be added later!");
// }

// void
// PainlessImpl::setLearner(void* data, int max_length, void (*learn)(void*
// data, lit_t* clause))
// {
// 	/* Fill clause array with a null terminated learned clause, if
// max_length >= learned.size */ 	PABORTIF(m_state >
// PainlessImpl::State::INITIALIZED, 			 PERR_NOT_SUPPORTED,
// "Cannot set the learner after the initialization"); 	m_learnerState = data;
// m_learnerMaxLength = max_length; 	m_exportToLearner = learn;
// LOGWARN("For now, painless do not export clauses, it will be added very
// soon!");
// }
