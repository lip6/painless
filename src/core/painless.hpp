#pragma once

#include "painless/solver.hpp"

#include "config/Parameters.hpp"
#include "containers/CSRMatrix.hpp"

#include "sharing/Sharer.hpp"

#include "solvers/SolverInterface.hpp"
#include "solvers/CDCL/SolverCDCLInterface.hpp"
#include "solvers/LocalSearch/LocalSearchInterface.hpp"

#include "working/WorkingStrategy.hpp"

#include "utils/Mutex.hpp"
#include "utils/System.hpp"
#include <type_traits>
#include <atomic>
#include <condition_variable>
#include <queue>

template <class... T> struct always_false : std::false_type {};

/* Todo, PainlessMPIImpl as sub class that sends clauses and assumptions to all
 * solvers, and centralizes exchanged, thus strategies will be mpi free */
class PainlessImpl : public Painless
{
  enum class State
  {
    CREATED,
    INITIALIZED,
    INPUT,
    SOLVING,
    INTERRUPTED,
    TERMINATED,
    FAILED
  };

public:
  PainlessImpl();
  PainlessImpl(const PainlessImpl&) = delete;
  PainlessImpl(PainlessImpl&&) = delete;
  ~PainlessImpl();

  PainlessImpl& operator=(const PainlessImpl&) = delete;
  PainlessImpl& operator=(PainlessImpl&&) = delete;

  /* IPASIR */
  /* ====== */
  std::string signature() override;
  /**
   * @brief Stores the current clause in a buffer, then saves it in the
   * m_formula attribute.
   * @note Thread-safe: When a clause is added to m_formula, a unique_lock is
   * obtained by the calling thread.
   */
  void addLiteral(lit_t lit) override;
  void assume(lit_t lit) override;
  result_t solve() override;
  // lit_t valueOf(lit_t lit) override;
  // bool failedDueTo(lit_t lit) override;
  // void setTerminateCallback(void* data, int (*terminate)(void* data))
  // override; void setLearner(void* data, int max_length, void (*learn)(void*
  // data, lit_t* clause)) override;

  /* Interface Extension */
  /* =================== */
  /// @warning if the chosen strategy waits on its solver, then this contract is
  /// broken. Make sure the working strategy chosen only launches works and do
  /// not wait on them.
  void asyncSolve() override;
  void asyncEnd() override;
  void waitOnEnd() override;
  bool waitOnEndFor(uint64_t microseconds) override;

  bool popResult(result_t& result) override;
  bool waitOnResult() override;
  bool waitOnResultFor(uint64_t microseconds) override;

  bool loadDIMACS(const std::string& filename) override;

#define PAINLESS_SET_OVERRIDE(T)                                               \
  bool set(const std::string& key, T value) override                           \
  {                                                                            \
    LOGWARNIF(                                                                 \
      m_state >= PainlessImpl::State::SOLVING &&                               \
        m_state != PainlessImpl::State::INTERRUPTED,                           \
      "Changing parameters of a running, terminated, or failed solver "        \
      "has no effect");                                                        \
    return m_parameters.set<T>(key, value);                                    \
  }

  PAINLESS_SET_OVERRIDE(int)
  PAINLESS_SET_OVERRIDE(unsigned)
  PAINLESS_SET_OVERRIDE(long)
  PAINLESS_SET_OVERRIDE(unsigned long)
  PAINLESS_SET_OVERRIDE(float)
  PAINLESS_SET_OVERRIDE(double)
  PAINLESS_SET_OVERRIDE(const char*)
  PAINLESS_SET_OVERRIDE(bool)

#undef PAINLESS_SET_OVERRIDE

  /* Painless Configuration */
  /* ====================== */
  template<typename T>
  bool addSolver(std::shared_ptr<T> solver)
  {
    if constexpr (std::is_base_of_v<SolverCDCLInterface, T>) {
      m_cdcls.push_back(solver);
    } else if constexpr (std::is_base_of_v<LocalSearchInterface, T>) {
      m_locals.push_back(solver);
    } else {
      // workaround from static_assert(false, ) before C++23 (disabled for now)
      // static_assert(always_false<T>::value, "Unsupported solver type");
    }
    m_solvers.push_back(solver);
    return true;
  }

  bool addSharer(std::shared_ptr<Sharer> sharer)
  {
    m_sharers.push_back(sharer);
    return true;
  }

  bool setWorkingStrategy(std::shared_ptr<WorkingStrategy> workingStrategy)
  {
    m_mainStrategy = workingStrategy;
    return true;
  }

  /* Painless Getters & Setters */
  /* ========================== */
  /**
   * @brief Read all the clauses starting from a certain index
   * @tparam Callback A callable type compatible with void(clause_view_t)
   * @param clsReader a callback that will define how the read clause is to be
   * used. It must be callable with a clause_view_t argument. Any return value
   * is ignored.
   * @param startIdx the index at which the reader callback will start reading
   * (default is 0)
   * @note Thread-safe: holds a shared (read) lock for the entire iteration.
   * Multiple readers can call this concurrently.
   * @note The callback is invoked while holding the lock, so it should not
   * perform lengthy operations.
   */
  template<typename Callback>
  uint readClauses(Callback&& clsReader, uint startIdx = 0)
  {
    SHARED_LOCK(std::shared_mutex, m_formulaMX, lock);
    uint clauseCount = this->clauseCount();
    uint i = startIdx;
    for (; i < clauseCount; i++) {
      // TODO make clause call a direct conversion csrMatrix row -> std::span
      // instead of passing through row_view
      clsReader(this->clause(i));
    }

    return i;
  }

  // Explicit std::function overload for clarity (delegates to template version)
  uint readClauses(ClauseReader clsReader, uint startIdx = 0)
  {
    return readClauses<ClauseReader&>(clsReader, startIdx);
  }

  // Check if the atomic is still needed or enableDistributed is enough
  bool shouldEndSolving() const { return m_shouldEndSolving; }
  bool hasSolvingEnded() const { return m_solvingHasEnded; }
  bool shouldTerminate() const { return m_shouldTerminate; }
  std::chrono::microseconds getRelativeTimeMicro() const
  {
    return m_timer.getRelativeTimeMicro();
  }
  // Reference getters do not have get in their (think about getters and setters
  // )
  const Parameters& parameters() const { return m_parameters; }
  void setParameters(Parameters& parameters)
  {
    LOGWARNIF(m_state >= PainlessImpl::State::SOLVING &&
                m_state != PainlessImpl::State::INTERRUPTED,
              "Changing parameters of a running, terminated, or failed solver "
              "has no effect");
    m_parameters = parameters;
  }

  bool pushResult(SatAnswer answer, model_t&& model);
  bool pushResult(SatAnswer answer, model_view_t model);

  void setInitialized()
  {
    PABORTIF(m_state >= PainlessImpl::State::INITIALIZED,
             PERR_NOT_SUPPORTED,
             "Cannot set to configured an already configured solver");
    m_state = PainlessImpl::State::INITIALIZED;
  }

  /* Additionnal Event Subscription */
  /* ============================== */
  void waitOnSolving();
  // Return previous value of the mask
  // bool markAsInterrupted(uint workerIdx);
  // Return previous value of the mask
  // bool unmaskAsInterrupted(uint workerIdx);

private:
  clause_view_t clause(uint idx) { return m_formula.span_at(idx); }
  uint clauseCount() { return m_formula.row_count(); }
  bool popLastResult(result_t& result);

  // Configuration
  // -------------

  /// Painless State
  std::atomic<State> m_state;

  /// Timer for relative time since instantiation
  SystemResourceMonitor::Timer m_timer;

  /// The strategy to execute
  std::shared_ptr<WorkingStrategy> m_mainStrategy;

  /// Parameters of this instance (TODO to become a constructor variable)
  Parameters m_parameters;

  // Workers
  // -------

  std::atomic<uint> m_workerCount;

  /// Solvers used in the strategy
  std::vector<std::shared_ptr<SolverInterface>> m_solvers;
  std::vector<std::shared_ptr<SolverCDCLInterface>> m_cdcls;
  std::vector<std::shared_ptr<LocalSearchInterface>> m_locals;

  /// Shares used in the strategy
  std::vector<std::shared_ptr<Sharer>> m_sharers;

  // Solving Lifecycle Management
  // ----------------------------

  /// Request to end the CURRENT solving run (stops all the parallel strategy's
  /// workers)
  std::atomic<bool> m_shouldEndSolving;

  /// The CURRENT solving run has fully ended (cleanup done)
  std::atomic<bool> m_solvingHasEnded;

  /// The entire instance should be terminated and destroyed
  std::atomic<bool> m_shouldTerminate;

  // Synchronization for END REQUEST (asyncEnd wakes watchdog)
  std::mutex m_endRequestMX;
  std::condition_variable m_endRequestCV;

  // Synchronization for END COMPLETION (watchdog signals waiters)
  std::mutex m_endCompleteMX;
  std::condition_variable m_endCompleteCV;

  // Synchronization for startup/resume
  std::mutex m_startupMX;
  std::condition_variable m_startupCV;

  /// Watchdog checking the termination
  std::thread m_watchdogThread;

  // IPASIR
  // ======

  /// Buffered clause thourgh addLiteral
  clause_t m_bufferedCls;

  /// Assumptions (a cube of lits)
  cube_t m_cube;

  /// Formula
  formula_t m_formula;
  std::shared_mutex m_formulaMX;

  // /// Data for termination check
  // void* m_terminationData;

  // /// Callback for termination check with termination data
  // std::function<int(void*)> m_checkTermination;

  // /// Learner State
  // void* m_learnerState;

  // /// Learned callback
  // std::function<void(void*, lit_t*)> m_exportToLearner;

  // /// @brief  Learner max clause length
  // uint m_learnerMaxLength;

  // Extension Variables
  // ===================

  /// Models for SAT instances
  std::mutex m_resultQueueMX;
  std::condition_variable m_resultQueueCV;
  std::deque<result_t> m_resultQueue;

  // Temps Variables for study
  // =========================
  std::unordered_map<uint64_t, uint> m_modelSeenCount;
};