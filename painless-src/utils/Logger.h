#pragma once

 #include<memory>
 #include<vector>

/* TODO: Maybe use the boost header to support other compilers than gcc */

// GCC-specific macro for function names
#if defined(__GNUC__)

// Define FUNCTION_NAME macro based on whether in C or C++
#ifdef __cplusplus
#define FUNCTION_NAME ((__PRETTY_FUNCTION__) ? __PRETTY_FUNCTION__ : "UnknownFunction")
#else
#define FUNCTION_NAME ((__FUNCTION__) ? __FUNCTION__ : "UnknownFunction")
#endif

#else
// Non-GCC compilers or other scenarios
#define FUNCTION_NAME "UnknownFunction"
#endif

// Rest of your code...

#define BLUE "\033[34m"
#define BOLD "\033[1m"
#define CYAN "\033[36m"
#define GREEN "\033[32m"
#define MAGENTA "\033[35m"
#define RED "\033[31m"
#define RESET "\033[0m"
#define WHITE "\037[34m"
#define YELLOW "\033[33m" // for Kissat LogPainless

/// Set the verbosity level.
void setVerbosityLevel(int level);

/// Function to print log with a certain level of verbosity.
void log(int verbosityLevel, const char *color, const char *issuer, const char *fmt...);
void logClause(int verbosityLevel, const char *color, const int *lits, unsigned size, const char *fmt...);
void logSolution(const char *string);

/// Print the model correctly in stdout.
void logModel(const std::vector<int> &model);

/* To be able to disable logs dynamically */
extern std::atomic<bool> quiet;

#define LOGERROR(...)                            \
    do                                           \
    {                                            \
        log(0, RED, FUNCTION_NAME, __VA_ARGS__); \
    } while (0)

#define LOGWARN(...)                                 \
    do                                               \
    {                                                \
        log(0, MAGENTA, FUNCTION_NAME, __VA_ARGS__); \
    } while (0)

#define LOGSTAT(...)                     \
    do                                   \
    {                                    \
        log(0, GREEN, "-", __VA_ARGS__); \
    } while (0)

#ifndef PQUIET

#define LOG(...)                         \
    do                                   \
    {                                    \
        log(0, RESET, "-", __VA_ARGS__); \
    } while (0)

#define LOG1(...)                        \
    do                                   \
    {                                    \
        log(1, RESET, "-", __VA_ARGS__); \
    } while (0)

#define LOG2(...)                        \
    do                                   \
    {                                    \
        log(2, RESET, "-", __VA_ARGS__); \
    } while (0)

#define LOG3(...)                        \
    do                                   \
    {                                    \
        log(3, RESET, "-", __VA_ARGS__); \
    } while (0)

#define LOG4(...)                        \
    do                                   \
    {                                    \
        log(4, RESET, "-", __VA_ARGS__); \
    } while (0)

#define LOGVECTOR(lits, size, ...)                    \
    do                                                 \
    {                                                  \
        logClause(1, YELLOW, lits, size, __VA_ARGS__); \
    } while (0)

#else

#define LOG(...) \
    do           \
    {            \
    } while (0)

#define LOG1(...) \
    do            \
    {             \
    } while (0)

#define LOG2(...) \
    do            \
    {             \
    } while (0)

#define LOG3(...) \
    do            \
    {             \
    } while (0)

#define LOG4(...) \
    do            \
    {             \
    } while (0)

#define LOGVECTOR(...) \
    do                  \
    {                   \
    } while (0)

#endif

#ifndef NDEBUG

#define LOGDEBUG1(...)                            \
    do                                            \
    {                                             \
        log(2, BLUE, FUNCTION_NAME, __VA_ARGS__); \
    } while (0)

#define LOGDEBUG2(...)                            \
    do                                            \
    {                                             \
        log(5, BLUE, FUNCTION_NAME, __VA_ARGS__); \
    } while (0)

#define LOGCLAUSE(lits, size, ...)                   \
    do                                               \
    {                                                \
        logClause(5, CYAN, lits, size, __VA_ARGS__); \
    } while (0)

#else

#define LOGDEBUG1(...) \
    do                 \
    {                  \
    } while (0)

#define LOGDEBUG2(...) \
    do                 \
    {                  \
    } while (0)

#define LOGCLAUSE(...) \
    do                 \
    {                  \
    } while (0)

#endif