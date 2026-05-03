/**
 * @file SystemResourceMonitor.h
 * @brief Provides utilities for monitoring system resources such as memory and
 * time (Memory monitoring is still in early development).
 * @details This header file declares functions and variables for tracking
 * system memory usage, setting memory limits, and measuring elapsed time.
 */

#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <sys/resource.h>

/**
 * @ingroup utils
 * @brief A set of utilities for monitoring and managing system resources.
 *
 * This group provides functions and variables for:
 * - Tracking system memory usage
 * - Setting and checking memory limits
 * - Measuring elapsed time
 */
namespace SystemResourceMonitor {

// ============================================================================
// PER-INSTANCE TIMING
// ============================================================================

class Timer final
{
public:
  Timer();
  ~Timer() = default;

  /**
   * @brief Get the relative time in microseconds since the timer was
   * instantiated.
   * @return std::chrono::microseconds representing the elapsed time in
   * microseconds.
   */
  std::chrono::microseconds getRelativeTimeMicro() const;

  /**
   * @brief Get the absolute time in microseconds since the epoch.
   * @return std::chrono::microseconds representing the current time in
   * microseconds.
   */
  static std::chrono::microseconds getAbsoluteTimeMicro();

  /**
   * @brief Get the relative time in microseconds since the process started.
   * @return std::chrono::microseconds representing the elapsed time in
   * microseconds.
   */
  static std::chrono::microseconds getProcessRelativeTimeMicro();

  /**
   * @brief Reset the timer to current time
   */
  void reset();

private:
  /** @brief The start time of the program. */
  std::chrono::steady_clock::time_point m_startTime;

  static std::chrono::steady_clock::time_point s_processStartTime;
};

// ============================================================================
// GLOBAL RESOURCES (global variable)
// ============================================================================

/** @brief The current memory limit for the process in kilobytes. */
extern std::atomic<rlim_t> memoryLimitKB;

/**
 * @brief Parse a specific key from /proc/meminfo.
 * @param key The key to search for in /proc/meminfo.
 * @param value Reference to store the parsed value.
 * @return True if parsing was successful, false otherwise.
 */
bool
parseMemInfo(const std::string& key, long& value);

/**
 * @brief Get the total system memory in kilobytes.
 * @return Long representing the total memory in KB.
 */
long
getTotalMemoryKB();

/**
 * @brief Get the used system memory in kilobytes.
 * @return Long representing the used memory in KB.
 */
long
getUsedMemoryKB();

/**
 * @brief Get the free system memory in kilobytes(totally free memory, not even
 * used for cache).
 * @return Long representing the free memory in KB.
 */
long
getFreeMemoryKB();

/**
 * @brief Get the available system memory in kilobytes(accounts page cache as
 * available).
 * @return Long representing the available memory in KB.
 */
long
getAvailableMemoryKB();

/**
 * @brief Set the memory limit for the process.
 * @param limitInKB The memory limit to set in kilobytes.
 * @return True if the limit was set successfully, false otherwise.
 */
bool
setMemoryLimitKB(rlim_t limitInKB);

/**
 * @brief Get the current memory limit for the process.
 * @return The memory limit in kilobytes.
 */
rlim_t
getMemoryLimitKB();

/**
 * @brief Check if the current memory usage exceeds the set limit.
 * @return True if the limit is exceeded, false otherwise.
 */
bool
isMemoryLimitExceeded();

/**
 * @brief Pretty print for the resource usage data gotten from getrusage()
 */
void
printProcessResourceUsage();
}
