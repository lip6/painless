#include "utils/Logger.hpp"
#include "utils/System.hpp"

#include "MpiUtils.hpp"
#include "utils/Parameters.hpp"
#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>

#include <atomic>
#include <cmath>
#include <mutex>

static std::recursive_mutex logMutex;

std::atomic<bool> quiet(false);

void
lockLogger()
{
	logMutex.lock();
}
void
unlockLogger()
{
	logMutex.unlock();
}

static int verbosityLevelSetting = 0;

void
setVerbosityLevel(int level)
{
	verbosityLevelSetting = level;
}

/**
 * Logging function
 * @param verbosityLevel to filter out some logs using program arguments
 * @param color in certain cases (debug and error) use different color for better readability
 * @param issuer the function name that logged this message (for debug and error)
 * @param fmt the message logged
 */
void
logDebug(int verbosityLevel, const char* color, const char* issuer, const char* fmt...)
{
	if (verbosityLevel <= verbosityLevelSetting && !quiet) {
		std::lock_guard<std::recursive_mutex> lockLog(logMutex);

		va_list args;

		va_start(args, fmt);

		printf("c%s", color);

		printf("[%.2f] ", SystemResourceMonitor::getRelativeTimeSeconds());

		if (__globalParameters__.enableDistributed)
			printf("[mpi:%d] ", mpi_rank);

		printf("%s(%s) %s%s%s", FUNC_STYLE, issuer, RESET, color, ERROR_STYLE);

		vprintf(fmt, args);

		va_end(args);

		printf("%s\n", RESET);

		fflush(stdout);
	}
}

void
log(int verbosityLevel, const char* color, const char* fmt...)
{
	if (verbosityLevel <= verbosityLevelSetting && !quiet) {
		std::lock_guard<std::recursive_mutex> lockLog(logMutex);

		va_list args;

		va_start(args, fmt);

		printf("c%s", color);

		printf("[%.2f] ", SystemResourceMonitor::getRelativeTimeSeconds());

		if (__globalParameters__.enableDistributed)
			printf("[mpi:%d] ", mpi_rank);

		vprintf(fmt, args);

		va_end(args);

		printf("%s\n", RESET);

		fflush(stdout);
	}
}

void
logClause(int verbosityLevel, const char* color, const int* lits, unsigned int size, const char* fmt...)
{
	if (verbosityLevel <= verbosityLevelSetting && !quiet) {

		va_list args;

		va_start(args, fmt);

		printf("cc%s", color);

		if (__globalParameters__.enableDistributed)
			printf("[mpi:%d] ", mpi_rank);

		vprintf(fmt, args);

		printf(" [%u] ", size);

		for (unsigned int i = 0; i < size; i++) {
			printf("%d ", lits[i]);
		}

		va_end(args);

		printf("%s", RESET);

		printf("\n");

		fflush(stdout);
	}
}

void
logSolution(const char* string)
{
	std::lock_guard<std::recursive_mutex> lockLog(logMutex);
	printf("s %s\n", string);
}

static unsigned int
intWidth(int i)
{
	if (i == 0)
		return 1;

	return (i < 0) + 1 + (unsigned int)log10(fabs(i));
}

void
logModel(const std::vector<int>& model)
{
	std::lock_guard<std::recursive_mutex> lockLog(logMutex);
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