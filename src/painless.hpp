#pragma once

#include "solvers/SolverInterface.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>

/// Is it the end of the search
extern std::atomic<bool> globalEnding;

/// @brief  Mutex for timeout cond
extern std::mutex mutexGlobalEnd;

/// @brief Cond to wait on timeout or wakeup on globalEnding
extern std::condition_variable condGlobalEnd;

/// Final result
extern std::atomic<SatResult> finalResult;

/// Model for SAT instances
extern std::vector<int> finalModel;

/// To check if painless is using distributed mode
extern std::atomic<bool> dist;