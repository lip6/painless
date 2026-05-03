/**
 * @file Logger.hpp
 * @brief Defines logging functions and macros for the SAT solver.
 */

#pragma once

#include <atomic>
#include <iostream>
#include <memory>
#include <sstream>
#include <sys/ioctl.h>
#include <vector>

#include "painless/types.hpp"
#include "utils/ErrorCodes.hpp"
#include "utils/Mutex.hpp"

// Color definitions for console output
#define BLUE "\033[34m"
#define BOLD "\033[1m"
#define CYAN "\033[36m"
#define GREEN "\033[32m"
#define MAGENTA "\033[35m"
#define RED "\033[31m"
#define RESET "\033[0m"
#define WHITE "\033[37m"
#define YELLOW "\033[33m"
#define FUNC_STYLE "\033[2m"
#define ERROR_STYLE "\033[1m"

// Function name macro (compiler-specific)
#if defined(__GNUC__)
#ifdef __cplusplus
#define FUNCTION_NAME                                                          \
  ((__PRETTY_FUNCTION__) ? __PRETTY_FUNCTION__ : "UnknownFunction")
#else
#define FUNCTION_NAME ((__FUNCTION__) ? __FUNCTION__ : "UnknownFunction")
#endif
#else
#define FUNCTION_NAME "UnknownFunction"
#endif

class Logger final
{
public:
  Logger() = delete;
  Logger(const Logger&) = delete;
  Logger(Logger&&) = delete;
  void operator=(const Logger&) = delete;
  void operator=(Logger&&) = delete;

  /**
   * @brief Set the verbosity level for logging.
   * @param level The verbosity level to set.
   */
  void setVerbosityLevel(int level);

  void setIsQuiet() { m_isQuiet = true; }

  void resetIsQuiet() { m_isQuiet = false; }

  /**
   * @brief Log a message with a specified verbosity level and color.
   * @param verbosityLevel The verbosity level of the message.
   * @param color The color to use for the message.
   * @param fmt The format string for the message.
   * @param ... Additional arguments for the format string.
   */
  void log(int verbosityLevel, const char* color, const char* fmt...);

  /**
   * @brief Log a debug message with a specified verbosity level, color, and
   * issuer.
   * @param verbosityLevel The verbosity level of the message.
   * @param color The color to use for the message.
   * @param issuer The function or module issuing the log message.
   * @param fmt The format string for the message.
   * @param ... Additional arguments for the format string.
   */
  void logError(int verbosityLevel,
                const char* color,
                const char* issuer,
                const char* fmt...);

  /**
   * @brief Log a clause with a specified verbosity level and color.
   * @param verbosityLevel The verbosity level of the message.
   * @param color The color to use for the message.
   * @param lits Pointer to an array of literals in the clause.
   * @param size The number of literals in the clause.
   * @param fmt The format string for the message.
   * @param ... Additional arguments for the format string.
   */
  void logClause(int verbosityLevel,
                 const char* color,
                 const int* lits,
                 unsigned int size,
                 const char* fmt...);

  /**
   * @brief Log the solution status of the SAT solver.
   * @param string The solution status string to log.
   */
  void logSolution(const char* string);

  /**
   * @brief Acquire the logger mutex to ensure thread-safe logging.
   */
  void lockLogger();

  /**
   * @brief Release the logger mutex after logging.
   */
  void unlockLogger();

  /**
   * @brief Log the model (satisfying assignment) found by the SAT solver.
   * @param model A vector of integers representing the satisfying assignment.
   */
  void logModel(model_view_t model);

  static Logger& getInstance();

  static int getTerminalWidth();

  void forceColors(bool enable) { m_useColors = enable; }

private:
  Logger(uint verbosityLevel = 0, bool isQuiet = false)
    : m_verbosityLevel(verbosityLevel)
    , m_isQuiet(isQuiet)
    , m_useColors(isatty(STDOUT_FILENO))
  {
  }
  ~Logger() = default;

  const char* colorize(const char* color) const
  {
    return m_useColors ? color : "";
  }

  /**
   * @brief Global flag to enable/disable logging.
   */
  std::atomic<bool> m_isQuiet;
  std::recursive_mutex m_logMutex;
  uint m_verbosityLevel;

  bool m_useColors;
};

// Logging macros for different verbosity levels and purposes

/**
 * @brief Log a message with a specified verbosity level.
 */
#define LOG(level, ...)                                                        \
  do {                                                                         \
    Logger::getInstance().log(level, RESET, __VA_ARGS__);                      \
  } while (0)

/**
 * @brief Log an error message.
 */
#define LOGERROR(...)                                                          \
  do {                                                                         \
    Logger::getInstance().logError(0, RED, FUNCTION_NAME, __VA_ARGS__);        \
  } while (0)

/**
 * @brief Log a warning message.
 */
#define LOGWARN(...)                                                           \
  do {                                                                         \
    Logger::getInstance().logError(0, YELLOW, FUNCTION_NAME, __VA_ARGS__);     \
  } while (0)

/**
 * @brief Log a warning message if a condition is true.
 */
#define LOGWARNIF(CONDITION, ...)                                              \
  do {                                                                         \
    if (CONDITION) {                                                           \
      Logger::getInstance().logError(0, YELLOW, FUNCTION_NAME, __VA_ARGS__);   \
    }                                                                          \
  } while (0)

/**
 * @brief Log a statistics message.
 */
#define LOGSTAT(...)                                                           \
  do {                                                                         \
    Logger::getInstance().log(0, GREEN, __VA_ARGS__);                          \
  } while (0)

#ifndef PQUIET
/**
 * @brief Log a message with verbosity level 0.
 */
#define LOG0(...)                                                              \
  do {                                                                         \
    Logger::getInstance().log(0, RESET, __VA_ARGS__);                          \
  } while (0)

/**
 * @brief Log a message with verbosity level 1.
 */
#define LOG1(...)                                                              \
  do {                                                                         \
    Logger::getInstance().log(1, RESET, __VA_ARGS__);                          \
  } while (0)

/**
 * @brief Log a message with verbosity level 2.
 */
#define LOG2(...)                                                              \
  do {                                                                         \
    Logger::getInstance().log(2, RESET, __VA_ARGS__);                          \
  } while (0)

/**
 * @brief Log a message with verbosity level 3.
 */
#define LOG3(...)                                                              \
  do {                                                                         \
    Logger::getInstance().log(3, RESET, __VA_ARGS__);                          \
  } while (0)

/**
 * @brief Log a message with verbosity level 4.
 */
#define LOG4(...)                                                              \
  do {                                                                         \
    Logger::getInstance().log(4, RESET, __VA_ARGS__);                          \
  } while (0)

/**
 * @brief Log a vector (clause) with cyan color.
 */
#define LOGVECTOR(lits, size, ...)                                             \
  do {                                                                         \
    Logger::getInstance().logClause(1, CYAN, lits, size, __VA_ARGS__);         \
  } while (0)

#else
// Define empty macros when PQUIET is defined
#define LOG0(...)
#define LOG1(...)
#define LOG2(...)
#define LOG3(...)
#define LOG4(...)
#define LOGVECTOR(...)
#endif

#ifndef NDEBUG
/**
 * @brief Log a debug message with verbosity level 1 and blue color.
 */
#define LOGD1(...)                                                             \
  do {                                                                         \
    Logger::getInstance().log(1, BLUE, __VA_ARGS__);                           \
  } while (0)

/**
 * @brief Log a debug message with verbosity level 2 and magenta color.
 */
#define LOGD2(...)                                                             \
  do {                                                                         \
    Logger::getInstance().log(2, MAGENTA, __VA_ARGS__);                        \
  } while (0)

/**
 * @brief Log a debug message with verbosity level 4 and magenta color.
 */
#define LOGD3(...)                                                             \
  do {                                                                         \
    Logger::getInstance().log(3, MAGENTA, __VA_ARGS__);                        \
  } while (0)

/**
 * @brief Log a debug message with verbosity level 4 and magenta color.
 */
#define LOGD4(...)                                                             \
  do {                                                                         \
    Logger::getInstance().log(4, MAGENTA, __VA_ARGS__);                        \
  } while (0)

/**
 * @brief Log a vector with verbosity level 2 and cyan color.
 */
#define LOGDVECTOR2(lits, size, ...)                                           \
  do {                                                                         \
    Logger::getInstance().logClause(2, CYAN, lits, size, __VA_ARGS__);         \
  } while (0)

/**
 * @brief Log a vector with verbosity level 5 and cyan color.
 */
#define LOGDVECTOR4(lits, size, ...)                                           \
  do {                                                                         \
    Logger::getInstance().logClause(5, CYAN, lits, size, __VA_ARGS__);         \
  } while (0)

/**
 * @brief Log an error message and abort the program with a specified exit code.
 */
#define PABORT(EXIT_CODE, ...)                                                 \
  do {                                                                         \
    Logger::getInstance().logError(0, RED, FUNCTION_NAME, __VA_ARGS__);        \
    std::abort();                                                              \
  } while (0)

/**
 * @brief Log an error message and abort the program with a specified exit code
 * if a condition is true.
 */
#define PABORTIF(CONDITION, EXIT_CODE, ...)                                    \
  do {                                                                         \
    if (CONDITION) [[unlikely]] {                                              \
      Logger::getInstance().logError(0, RED, FUNCTION_NAME, __VA_ARGS__);      \
      std::abort();                                                            \
    }                                                                          \
  } while (0)

#else
// Define empty macros when NDEBUG is defined
#define LOGD1(...)
#define LOGD2(...)
#define LOGD3(...)
#define LOGD4(...)
#define LOGDVECTOR2(...)
#define LOGDVECTOR4(...)

/**
 * @brief Log an error message and abort the program with a specified exit code.
 */
#define PABORT(EXIT_CODE, ...)                                                 \
  do {                                                                         \
    Logger::getInstance().logError(0, RED, FUNCTION_NAME, __VA_ARGS__);        \
    exit(EXIT_CODE);                                                           \
  } while (0)

/**
 * @brief Log an error message and abort the program with a specified exit code
 * if a condition is true.
 */
#define PABORTIF(CONDITION, EXIT_CODE, ...)                                    \
  do {                                                                         \
    if (CONDITION) {                                                           \
      Logger::getInstance().logError(0, RED, FUNCTION_NAME, __VA_ARGS__);      \
      exit(EXIT_CODE);                                                         \
    }                                                                          \
  } while (0)

#endif