#pragma once

#include "Logger.hpp"
#include <iostream>
#include <sstream>
#include <string>

/**
 * @defgroup utils Utilities
 * @brief Different Utilities
 * @{
 */

// Define the macro for parameters, TODO: add max and min values for a better user experience
// name, type, parsed_name, default, description
#define PARAMETERS                                                                                                     \
	/* General options */                                                                                              \
	PARAM(help, bool, "help", false, "Prints this help")                                                               \
	PARAM(details, std::string, "details", "", "Get detailed information about a specific category")                   \
	PARAM(filename, std::string, "input.cnf ", "", "Input CNF file")                                                   \
	PARAM(cpus, int, "c", 0, "Number of solver threads to launch (0 = std::thread::hardware_concurrency)")             \
	PARAM(timeout, int, "t", -1, "Timeout in seconds")                                                                 \
	PARAM(verbosity, int, "v", 0, "Verbosity level")                                                                   \
	PARAM(test, bool, "test", false, "Use Test working strategy")                                                      \
	PARAM(noModel, bool, "no-model", false, "Disable model output")                                                    \
	PARAM(enableDistributed, bool, "dist", false, "Enable distributed solving, thus initializes MPI")                  \
                                                                                                                       \
	CATEGORY("Portfolio")                                                                                              \
	PARAM(solver, std::string, "solver", "kcl", "Portfolio of solvers")                                                \
	PARAM(prs, bool, "prs", false, "Use PortfolioPRS")                                                                 \
	PARAM(enableMallob, bool, "mallob", false, "Emulate Mallob's Sharing Strategy In PortfolioSimple")                 \
	PARAM(sbvaPostLocalSearchers, int, "ls-after-sbva", 2, "(PortfolioSBVA) Local search solvers after SBVA")          \
	PARAM(maxDivNoise, int, "max-div-noise", 1000, "Maximum noise for random engine in diversification")               \
	PARAM(gaInitPeriod,                                                                                                \
		  int,                                                                                                         \
		  "ga-init",                                                                                                   \
		  0,                                                                                                           \
		  "Use GaspiInitializer for phase initialization. (0) disabled, (n) all id where (id %% n) == 0")              \
	PARAM(gaSeed, int, "ga-seed", 0, "The seed to use in the random engine")                                           \
	PARAM(gaPopSize, int, "ga-pop-size", 50, "The number of candidates in each generation")                            \
	PARAM(gaMaxGen, int, "ga-max-gen", 100, "The maximum number of generations (iterations) to run")                   \
	PARAM(                                                                                                             \
		gaMutRate, float, "ga-mut-rate", 0.88f, "The mutation rate (probability), chances to randomly assign a genome") \
	PARAM(gaCrossRate,                                                                                                 \
		  float,                                                                                                       \
		  "ga-cross-rate",                                                                                             \
		  0.5f,                                                                                                        \
		  "The crossover rate (probability), i.e chances to create a crossover point")                                 \
                                                                                                                       \
	CATEGORY("Solving")                                                                                                \
	PARAM(glucoseSplitHeuristic, int, "glc-split-heur", 1, "Split heuristic")                                          \
	PARAM(defaultClauseBufferSize, int, "default-clsbuff-size", 1000, "Default ClauseBuffer size")                     \
	PARAM(localSearchFlips, int, "ls-flips", -1, "Number of local search flips")                                       \
                                                                                                                       \
	CATEGORY("Preprocessing")                                                                                          \
	SUBCATEGORY("PRS options")                                                                                         \
	PARAM(prsCircuitVar, int, "prs-circuit-var", 100'000, "PRS circuit variable limit")                                \
	PARAM(prsGaussVar, int, "prs-gauss-var", 100'000, "PRS Gauss variable limit")                                      \
	PARAM(prsCardVar, int, "prs-card-var", 100'000, "PRS cardinality variable limit")                                  \
	PARAM(prsCircuitCls, int, "prs-circuit-cls", 1'000'000, "PRS circuit clause limit")                                \
	PARAM(prsGaussClsSize, int, "prs-gauss-cls-size", 6, "PRS Gauss clause size limit")                                \
	PARAM(prsGaussCls, int, "prs-gauss-cls", 1'000'000, "PRS Gauss clause limit")                                      \
	PARAM(prsBinCls, int, "prs-bin-cls", 10'000'000, "PRS binary clause limit")                                        \
	PARAM(prsCardCls, int, "prs-card-cls", 1'000'000, "PRS cardinality clause limit")                                  \
                                                                                                                       \
	SUBCATEGORY("SBVA")                                                                                                \
	PARAM(sbvaTimeout, int, "sbva-timeout", 500, "SBVA timeout")                                                       \
	PARAM(sbvaCount, int, "sbva-count", 12, "SBVA threads count")                                                      \
	PARAM(sbvaMaxClause, int, "sbva-max-clause", 10'000'000, "SBVA maximum clause count")                              \
	PARAM(sbvaMaxAdd, int, "sbva-max-add", 0, "SBVA maximum additions (0 = unlimited)")                                \
	PARAM(sbvaNoShuffle, bool, "no-sbva-shuffle", false, "Disable SBVA shuffle")                                       \
                                                                                                                       \
	CATEGORY("Sharing")                                                                                                \
	PARAM(maxClauseSize, int, "max-cls-size", 60, "Maximum size of clauses to be added in ClauseDatabase")             \
	PARAM(initSleep, int, "init-sleep", 10'000, "Initial sleep time in microseconds for a Sharer")                     \
	PARAM(sharingStrategy, int, "shr-strat", 1, "Strategy selection for local sharing (ongoing re-organization)")      \
	PARAM(globalSharingStrategy, int, "gshr-strat", -1, "Global sharing strategy")                                      \
	PARAM(sharingSleep, int, "shr-sleep", 500'000, "Sleep time for sharer after each round")                           \
	PARAM(globalSharingSleep, int, "gshr-sleep", 600'000, "Sleep time for sharer after each round of global sharing")  \
	PARAM(oneSharer, bool, "one-sharer", false, "Use only one sharer")                                                 \
	PARAM(globalSharedLiterals, int, "gshr-lit", 2000, "Number of literals shared globally")                           \
	PARAM(sharedLiteralsPerProducer,                                                                                   \
		  int,                                                                                                         \
		  "shr-lit-per-prod",                                                                                          \
		  1500,                                                                                                        \
		  "Number of literals shared per producer. It is mainly used for local sharing")                               \
	PARAM(simpleShareLimit, int, "simple-limit", 10, "Simple share clause size limit")                                 \
	PARAM(importDB, std::string, "importDB", "d", "Solver import dabatase type")                                       \
	PARAM(importDBCap, unsigned, "importDB-cap", 10'000, "Solver import dabatase capacity")                            \
	PARAM(localSharingDB, std::string, "lshrDB", "d", "Local Sharing Strategy import dabatase type")                   \
	PARAM(globalSharingDB, std::string, "gshrDB", "m", "Global Sharing Strategy import dabatase type")                 \
                                                                                                                       \
	SUBCATEGORY("Hordesat")                                                                                            \
	PARAM(hordeInitialLbdLimit, unsigned, "horde-initial-lbd", 2, "Initial LBD value for producers")                   \
	PARAM(hordeInitRound, unsigned, "horde-init-round", 1, "Rounds before HordesatSharingAlt starts")                  \
                                                                                                                       \
	SUBCATEGORY("Mallob")                                                                                              \
	PARAM(mallobSharingsPerSecond, int, "mallob-shr-per-sec", 2, "Number of shares per second")                        \
	PARAM(mallobMaxBufferSize, int, "mallob-gshr-max-lit", 250'000, "Maximum number of literals shared globally")      \
	PARAM(mallobResharePeriod,                                                                                         \
		  int,                                                                                                         \
		  "mallob-reshare-period-us",                                                                                  \
		  15'000'000,                                                                                                  \
		  "Reshare period in microseconds for ExactFilter")                                                            \
	PARAM(mallobLBDLimit, int, "mallob-lbd-limit", 60, "Mallob LBD limit")                                             \
	PARAM(mallobSizeLimit, int, "mallob-size-limit", 60, "Mallob size limit")                                          \
	PARAM(mallobMaxCompensation, float, "max-mallob-comp", 5.0f, "Maximum Mallob compensation")

// Structure to hold all parameters
struct Parameters
{
#define PARAM(name, type, parsed_name, default_value, description) type name = default_value;
#define CATEGORY(description)
#define SUBCATEGORY(description)
	PARAMETERS
#undef PARAM
#undef CATEGORY
#undef SUBCATEGORY

	static void init(int argc, char** argv);
	static void printHelp();
	static void printDetailedHelp(std::string& category);
	static void printParams();
};

extern Parameters __globalParameters__;

#define DETAILED_HELP_DATABASES                                                                                        \
	" " BOLD "s" RESET " - SingleBuffer database\n"                                                                    \
	" " BOLD "m" RESET " - Mallob database\n"                                                                          \
	" " BOLD "d" RESET " - PerSize database (default)\n"                                                               \
	" " BOLD "e" RESET " - A Buffer Per Source (.from attribute) Database\n"

#define DETAILED_HELP_PORTFOLIO                                                                                        \
	BLUE "The solver parameter " YELLOW "(-solver=<string>)" BLUE " accepts the following characters:\n" RESET         \
		 " " BOLD "g" RESET " - Glucose Syrup solver\n"                                                                \
		 " " BOLD "l" RESET " - Lingeling solver\n"                                                                    \
		 " " BOLD "M" RESET " - MapleCOMSPS solver\n"                                                                  \
		 " " BOLD "m" RESET " - Minisat solver\n"                                                                      \
		 " " BOLD "I" RESET " - Kissat INC solver\n"                                                                   \
		 " " BOLD "K" RESET " - Kissat MAB (Multi-Armed Bandit) solver\n"                                              \
		 " " BOLD "k" RESET " - Kissat solver\n"                                                                       \
		 " " BOLD "c" RESET " - CaDiCaL solver\n"                                                                      \
		 " " BOLD "y" RESET " - YalSAT local search solver\n"                                                          \
		 " " BOLD "t" RESET " - TaSSAT local search solver\n"                                                          \
		 "\n" BLUE "Import Database Types " YELLOW "(-importDB=<char>)" RESET " :\n" DETAILED_HELP_DATABASES "\n"      \
		 "Working strategies:\n" RESET " " BOLD "Simple Portfolio" RESET                                               \
		 " (the default strategy): Run solvers in parallel with diversified configurations\n"                          \
		 " " BOLD "PRS Portfolio" RESET " (" YELLOW "-prs=true" RESET                                                  \
		 "): Mimics the parallelization strategy of the PRS framework, with different and "                            \
		 "separated groups of solvers all preceeded by the different preprocessing techniques defined in the PRS "     \
		 "framework\n"                                                                                                 \
		 "\n" BOLD "Example:" RESET " " YELLOW "-solver=gkMcy" RESET                                                   \
		 " creates a portfolio by instantiating periodically 1 Glucose, 1 Kissat, 1 MapleCOMSPS, 1 CaDiCaL, and 1 "    \
		 "YalSat solver until the number specified by " YELLOW "-c=<int>" RESET " is reached \n"                       \
		 "\n" BLUE "Diversification:\n" RESET "  " YELLOW "-max-div-noise" RESET                                       \
		 ": Sets maximum noise for random diversification between solvers\n"                                           \
		 "  Higher values create more diverse solver configurations\n"                                                 \
		 "\n" FUNC_STYLE                                                                                               \
		 "Note: Solver availability depends on compile-time options (GLUCOSE_, LINGELING_, etc.)\n" RESET

#define DETAILED_HELP_SOLVING                                                                                          \
	BLUE "Glucose specific options:\n" RESET "  " YELLOW "-glc-split-heur" RESET                                       \
		 ": Sets the split heuristic in Glucose (1-4)\n"                                                               \
		 "    " BOLD "1" RESET ": No splitting (default)\n"                                                            \
		 "    " BOLD "2" RESET ": Split randomly\n"                                                                    \
		 "    " BOLD "3" RESET ": Split by activity\n"                                                                 \
		 "    " BOLD "4" RESET ": Split by phase\n"                                                                    \
		 "\n" BLUE "Local Search:\n" RESET "  " YELLOW "-ls-flips" RESET ": Number of local search flips (" GREEN      \
		 "-1" RESET " = use default)\n"

#define DETAILED_HELP_PREPROCESSING                                                                                    \
	BLUE "SBVA (Structured Binary Variable Addition):\n" RESET                                                         \
		 "  A preprocessing technique that identifies and optimizes structured patterns\n"                             \
		 "  in the formula by introducing new variables to represent common substructures.\n"                          \
		 "\n"                                                                                                          \
		 "  " YELLOW "-sbva-timeout" RESET ": Maximum processing time in seconds (" GREEN "500" RESET ")\n"            \
		 "  " YELLOW "-sbva-count" RESET ": Number of parallel SBVA threads (" GREEN "12" RESET ")\n"                  \
		 "  " YELLOW "-sbva-max-clause" RESET ": Maximum clauses for SBVA to process (" GREEN "10,000,000" RESET ")\n" \
		 "  " YELLOW "-sbva-max-add" RESET ": Maximum variable additions (" GREEN "0" RESET " = unlimited)\n"          \
		 "  " YELLOW "-no-sbva-shuffle" RESET ": Disable random shuffling during SBVA\n"                               \
		 "\n" BLUE "PRS Preprocessing Techniques Details:\n" RESET "  " YELLOW "-prs-circuit-var" RESET                \
		 ": Circuit variable threshold (" GREEN "100,000" RESET ")\n"                                                  \
		 "  " YELLOW "-prs-gauss-var" RESET ": Gaussian elimination variable threshold (" GREEN "100,000" RESET ")\n"  \
		 "  " YELLOW "-prs-card-var" RESET ": Cardinality constraint variable threshold (" GREEN "100,000" RESET ")\n" \
		 "  " YELLOW "-prs-circuit-cls" RESET ": Circuit clause threshold (" GREEN "1,000,000" RESET ")\n"             \
		 "  " YELLOW "-prs-gauss-cls-size" RESET ": Gaussian elimination clause size threshold (" GREEN "6" RESET      \
		 ")\n"                                                                                                         \
		 "  " YELLOW "-prs-gauss-cls" RESET ": Gaussian elimination clause threshold (" GREEN "1,000,000" RESET ")\n"  \
		 "  " YELLOW "-prs-bin-cls" RESET ": Binary clause threshold (" GREEN "10,000,000" RESET ")\n"                 \
		 "  " YELLOW "-prs-card-cls" RESET ": Cardinality constraint clause threshold (" GREEN "1,000,000" RESET ")\n"

#define DETAILED_HELP_SHARING                                                                                          \
	BLUE "Local Sharing Strategies " YELLOW "(-shr-strat)" BLUE ":\n" RESET "  " BOLD "1" RESET                        \
		 ": HordeSat sharing with per-entity buffer (default)\n"                                                       \
		 "  " BOLD "1" RESET ": HordeSat sharing\n"                                                                    \
		 "  " BOLD "2" RESET ": HordeSat sharing with 2 groups of producers\n"                                         \
		 "  " BOLD "3" RESET ": Simple sharing \n"                                                                     \
		 "\n" BLUE "Global Sharing Strategies " YELLOW "(-gshr-strat)" BLUE ":\n" RESET "  " BOLD "1" RESET            \
		 ": AllGatherSharing - Exchange clauses using MPI_Allgather (default)\n"                                       \
		 "  " BOLD "2" RESET ": MallobSharing - Mallob-based exchange algorithm (adaptive)\n"                          \
		 "  " BOLD "3" RESET ": GenericGlobalSharing (Ring topology)\n"                                                \
		 "\n" BLUE "Clause Database Types " YELLOW "(-lshrDB, -gshrDB)" RESET ":\n" DETAILED_HELP_DATABASES "\n"       \
		 "Size and quality limits:\n" RESET "  " YELLOW "-max-cls-size" RESET ": Maximum clause size to share\n"       \
		 "  " YELLOW "-shr-lit-per-prod" RESET ": Literals per producer for local sharing\n"                           \
		 "  " YELLOW "-gshr-lit" RESET ": Number of literals shared globally\n"

#define DETAILED_HELP_GLOBAL                                                                                           \
	BLUE "General parameters:\n" RESET "  " YELLOW "-c" RESET ": Number of solver threads to launch (default: " GREEN  \
		 "32" RESET ")\n"                                                                                              \
		 "  " YELLOW "-t" RESET ": Timeout in seconds (" GREEN "-1" RESET " = no timeout)\n"                           \
		 "  " YELLOW "-v" RESET ": Verbosity level (" GREEN "0-5" RESET ")\n"                                          \
		 "\n" BLUE "Distributed solving:\n" RESET "  " YELLOW "-dist" RESET ": Enable distributed solving using MPI\n" \
		 "  Each node runs its own solvers and participates in global clause sharing\n"                                \
		 "\n" BLUE "Output options:\n" RESET "  " YELLOW "-no-model" RESET                                             \
		 ": Only report satisfiability, not the model\n"
/**
 * @} // end of utils group
 */