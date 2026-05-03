#include "utils/Logger.hpp"
#include "utils/System.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>

#include "utils/NumericConstants.hpp"
#include <atomic>
#include <cmath>

int
Logger::getTerminalWidth()
{
  struct winsize w;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
    return w.ws_col - 5;
  }
  return 80;
}

Logger&
Logger::getInstance()
{
  static Logger instance(0, false);
  return instance;
}

void
Logger::lockLogger()
{
  m_logMutex.lock();
}
void
Logger::unlockLogger()
{
  m_logMutex.unlock();
}

void
Logger::setVerbosityLevel(int level)
{
  m_verbosityLevel = level;
}

/**
 * Logging function
 * @param verbosityLevel to filter out some logs using program arguments
 * @param color in certain cases (debug and error) use different color for
 * better readability
 * @param issuer the function name that logged this message (for debug and
 * error)
 * @param fmt the message logged
 */
void
Logger::logError(int verbosityLevel,
                 const char* color,
                 const char* issuer,
                 const char* fmt...)
{
  if (verbosityLevel <= m_verbosityLevel) {
    LOCK_GUARD(std::recursive_mutex, m_logMutex, lockLog);

    va_list args;

    va_start(args, fmt);

    printf("c%s", colorize(color));

    printf(
      "[%.3lf] ",
      static_cast<double>(
        SystemResourceMonitor::Timer::getProcessRelativeTimeMicro().count()) /
        MILLION);

    printf("%s(%s) %s%s%s", colorize(FUNC_STYLE), issuer, colorize(RESET), colorize(color), colorize(ERROR_STYLE));

    vprintf(fmt, args);

    va_end(args);

    printf("%s\n", colorize(RESET));

    fflush(stdout);
  }
}

void
Logger::log(int verbosityLevel, const char* color, const char* fmt...)
{
  if (verbosityLevel <= m_verbosityLevel && !m_isQuiet) {
    LOCK_GUARD(std::recursive_mutex, m_logMutex, lockLog);

    va_list args;

    va_start(args, fmt);

    printf("c%s", colorize(color));

    printf(
      "[%.3lf] ",
      static_cast<double>(
        SystemResourceMonitor::Timer::getProcessRelativeTimeMicro().count()) /
        MILLION);

    vprintf(fmt, args);

    va_end(args);

    printf("%s\n", colorize(RESET));

    fflush(stdout);
  }
}

void
Logger::logClause(int verbosityLevel,
                  const char* color,
                  const int* lits,
                  unsigned int size,
                  const char* fmt...)
{
  if (verbosityLevel <= m_verbosityLevel && !m_isQuiet) {

    va_list args;

    va_start(args, fmt);

    printf("cc%s", colorize(color));

    vprintf(fmt, args);

    printf(" [%u] ", size);

    for (unsigned int i = 0; i < size; i++) {
      printf("%d ", lits[i]);
    }

    va_end(args);

    printf("%s", colorize(RESET));

    printf("\n");

    fflush(stdout);
  }
}

void
Logger::logSolution(const char* string)
{
  if (!m_isQuiet) {
    LOCK_GUARD(std::recursive_mutex, m_logMutex, lockLog);
    printf("s %s\n", string);
  }
}

static unsigned int
intWidth(int i)
{
  if (i == 0)
    return 1;

  return (i < 0) + 1 + (unsigned int)log10(fabs(i));
}

void
Logger::logModel(model_view_t model)
{
  if (!m_isQuiet) {
    LOCK_GUARD(std::recursive_mutex, m_logMutex, lockLog);
    unsigned int usedWidth = 0;

    for (unsigned int i = 0; i < model.size(); i++) {
      if (usedWidth + 1 + intWidth(model[i]) > 80) {
        printf("\n");
        usedWidth = 0;
      }

      if (usedWidth == 0) {
        usedWidth += printf("v");
      }

      usedWidth += printf(" %d", model[i]);
    }

    printf(" 0");

    printf("\n");
  }
}