/**
 * @file SystemResourceMonitor.h
 * @brief Provides utilities for monitoring system resources such as memory and time (Memory monitoring is still in early development).
 * @details This header file declares functions and variables for tracking system memory usage,
 *          setting memory limits, and measuring elapsed time.
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
/** @brief The current memory limit for the process in kilobytes. */
extern std::atomic<rlim_t> memoryLimitKB;

/** @brief The start time of the program. */
extern const std::chrono::steady_clock::time_point startTime;

/**
 * @brief Get the relative time in seconds since the program started.
 * @return Double representing the elapsed time in seconds.
 */
double
getRelativeTimeSeconds();

/**
 * @brief Get the absolute time in seconds since the epoch.
 * @return Double representing the current time in seconds.
 */
double
getAbsoluteTimeSeconds();

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
 * @brief Get the free system memory in kilobytes(totally free memory, not even used for cache).
 * @return Long representing the free memory in KB.
 */
long
getFreeMemoryKB();

/**
 * @brief Get the available system memory in kilobytes(accounts page cache as available).
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
