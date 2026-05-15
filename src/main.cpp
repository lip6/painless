#include "config/PainlessConfigurator.hpp"
#include "utils/NumericConstants.hpp"
#include "utils/Parsers.hpp"
#include <future>

// -------------------------------------------
// Signal Handling
// -------------------------------------------
#include <csignal>

// Cleanup function to be called at exit
void
cleanup()
{
  // SystemResourceMonitor::printProcessResourceUsage();
}
// Signal handler function
void
signalHandler(int signum)
{
  // Call cleanup directly for immediate signals
  cleanup();

  // Re-raise the signal after cleaning up
  std::signal(signum, SIG_DFL); /* Use defailt signal handler */
  std::raise(signum);
}

// Function to set up exit handlers
void
setupExitHandlers()
{
  // Register cleanup function to be called at normal program termination
  std::atexit(cleanup);

  // Set up signal handlers
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);
  // std::signal(SIGABRT, signalHandler);
  // Add more signals as needed
}

int
main(int argc, char** argv)
{

  PainlessImpl painless;
  PainlessConfigurator::configurePainlessFromCLI(argc, argv, painless);

  // Todo check all configurable instances are configured by using
  // requireConfigured.

  setupExitHandlers();

  if (!painless.loadDIMACS(painless.parameters().filename.c_str())) {
    PABORT(PERR_PARSING, "Error at parsing!");
  }

  result_t result = painless.solve();

  assert(!painless.popResult(result));

  if (result.answer == SatAnswer::SAT) {
    Logger::getInstance().logSolution("SATISFIABLE");
    Logger::getInstance().logModel(result.model);
  } else if (result.answer == SatAnswer::UNSAT) {
    Logger::getInstance().logSolution("UNSATISFIABLE");
  } else // if timeout or unknown
  {
    Logger::getInstance().logSolution("UNKNOWN");
  }

  LOGSTAT("Resolution time: %lf s",
          static_cast<double>(painless.getRelativeTimeMicro().count()) /
            MILLION);

  return static_cast<int>(result.answer);
}