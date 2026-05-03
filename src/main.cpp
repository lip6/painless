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

  setupExitHandlers();

  // Init timeout detection before starting the solvers and sharers

  unsigned int varCount;
  // int receivedFinalResultBcast = 0;

  if (!painless.loadDIMACS(painless.parameters().filename.c_str())) {
    PABORT(PERR_PARSING, "Error at parsing!");
  }

  result_t result = painless.solve();

  assert(!painless.popResult(result));

  // // To be abstracted inside PainlessImpl and its sub classes
  // if (painless.mpiRank() == painless.mpiWinner()) {
  // 	if (painless.result() == SatAnswer::SAT) {
  // 		Logger::getInstance().logSolution("SATISFIABLE");

  // 		if (!painless.parameters().noModel) {
  // 			model_t model;
  // 			painless.popModel(model.data(), model.size());
  // 			Logger::getInstance().logModel(model);
  // 		}
  // 	} else if (painless.result() == SatAnswer::UNSAT) {
  // 		Logger::getInstance().logSolution("UNSATISFIABLE");
  // 	} else // if timeout or unknown
  // 	{
  // 		Logger::getInstance().logSolution("UNKNOWN");
  // 		painless.result() = SatAnswer::UNKNOWN;
  // 	}

  // 	LOGSTAT("Resolution time: %f s", painless.getRelativeTimeMicro());
  // } else
  // 	painless.result() = SatAnswer::UNKNOWN; /* mpi will be forced to suspend
  // job only by the winner */

  // LOGD1("Mpi process %d returns %d", painless.mpiRank(),
  // static_cast<int>(painless.result().load()));

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