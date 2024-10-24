#pragma once

#include <string>
#include <iostream>
#include <sstream>

/**
 * @defgroup utils Utilities
 * @brief Different Utilities
 * @{
*/

// Define the macro for parameters
// name, type, parsed_name, default, description
#define PARAMETERS \
    /* General options */ \
    PARAM(help, bool, "help", false, "Prints this help")\
    PARAM(filename, std::string, "input.cnf ", "", "Input CNF file") \
    PARAM(cpus, int, "c", 32, "Number of solver threads to launch") \
    PARAM(timeout, int, "t", -1, "Timeout in seconds") \
    PARAM(verbosity, int, "v", 0, "Verbosity level") \
    PARAM(test, bool, "test", false, "Use Test working strategy") \
    PARAM(noModel, bool, "no-model", false, "Disable model output") \
    PARAM(enableDistributed, bool, "dist", false, "Enable distributed solving, thus initializes MPI") \
    \
    CATEGORY("Portfolio options") \
    PARAM(solver, std::string, "solver", "kkkcl", "Portfolio of solvers") \
	PARAM(simple, bool, "simple", false, "Use PortfolioSimple") \
	PARAM(enableMallob, bool, "mallob", false, "Emulate Mallob's Sharing Strategy In PortfolioSimple") \
    \
    CATEGORY("Solving options") \
    PARAM(maxDivNoise, int, "max-div-noise", 1000, "Maximum noise for random engine in diversification") \
    PARAM(glucoseSplitHeuristic, int, "glc-split-heur", 1, "Split heuristic") \
    PARAM(defaultClauseBufferSize, int, "default-clsbuff-size", 1000, "Default ClauseBuffer size") \
    PARAM(localSearchFlips, int, "ls-flips", -1, "Number of local search flips") \
    \
    CATEGORY("Sharing options") \
    PARAM(maxClauseSize, int, "max-cls-size", 60, "Maximum size of clauses to be added in ClauseDatabase") \
    PARAM(initSleep, int, "init-sleep", 10'000, "Initial sleep time in microseconds for a Sharer") \
    PARAM(sharingStrategy, int, "shr-strat", 1, "Strategy selection for local sharing (ongoing re-organization)") \
    PARAM(globalSharingStrategy, int, "gshr-strat", 1, "Global sharing strategy") \
    PARAM(sharingSleep, int, "shr-sleep", 500'000, "Sleep time for sharer after each round") \
    PARAM(globalSharingSleep, int, "gshr-sleep", 600'000, "Sleep time for sharer after each round of global sharing") \
    PARAM(oneSharer, bool, "one-sharer", false, "Use only one sharer") \
    PARAM(globalSharedLiterals, int, "gshr-lit", 2000, "Number of literals shared globally") \
    PARAM(sharedLiteralsPerProducer, int, "shr-lit-per-prod", 1500, "Number of literals shared per producer. It is mainly used for local sharing") \
    PARAM(simpleShareLimit, int, "simple-limit", 10, "Simple share clause size limit") \
    PARAM(importDB, std::string, "importDB", "d", "Solver import dabatase type")\
    PARAM(importDBCap, unsigned, "importDB-cap", 10'000, "Solver import dabatase capacity")\
    \
    CATEGORY("Hordesat options") \
    PARAM(hordeInitialLbdLimit, unsigned, "horde-initial-lbd", 2, "Initial LBD value for producers") \
    PARAM(hordeInitRound, unsigned, "horde-init-round", 1, "Rounds before HordesatSharingAlt starts") \
    \
    CATEGORY("Mallob options") \
    PARAM(mallobSharingsPerSecond, int, "mallob-shr-per-sec", 2, "Number of shares per second") \
    PARAM(mallobMaxBufferSize, int, "mallob-gshr-max-lit", 250'000, "Maximum number of literals shared globally") \
    PARAM(mallobResharePeriod, int, "mallob-reshare-period-us", 15'000'000, "Reshare period in microseconds for ExactFilter") \
    PARAM(mallobLBDLimit, int, "mallob-lbd-limit", 60, "Mallob LBD limit") \
    PARAM(mallobSizeLimit, int, "mallob-size-limit", 60, "Mallob size limit") \
    PARAM(mallobMaxCompensation, float, "max-mallob-comp", 5.0f, "Maximum Mallob compensation") \
    \
    CATEGORY("SBVA options") \
    PARAM(sbvaTimeout, int, "sbva-timeout", 500, "SBVA timeout") \
    PARAM(sbvaCount, int, "sbva-count", 12, "SBVA threads count") \
    PARAM(sbvaPostLocalSearchers, int, "ls-after-sbva", 2, "Local search solvers after SBVA") \
    PARAM(sbvaMaxClause, int, "sbva-max-clause", 10'000'000, "SBVA maximum clause count") \
    PARAM(sbvaMaxAdd, int, "sbva-max-add", 0, "SBVA maximum additions") \
    PARAM(sbvaNoShuffle, bool, "no-sbva-shuffle", false, "Disable SBVA shuffle") \
    \
    CATEGORY("PRS options") \
    PARAM(prsCircuitVar, int, "prs-circuit-var", 100'000, "PRS circuit variable limit") \
    PARAM(prsGaussVar, int, "prs-gauss-var", 100'000, "PRS Gauss variable limit") \
    PARAM(prsCardVar, int, "prs-card-var", 100'000, "PRS cardinality variable limit") \
    PARAM(prsCircuitCls, int, "prs-circuit-cls", 1'000'000, "PRS circuit clause limit") \
    PARAM(prsGaussClsSize, int, "prs-gauss-cls-size", 6, "PRS Gauss clause size limit") \
    PARAM(prsGaussCls, int, "prs-gauss-cls", 1'000'000, "PRS Gauss clause limit") \
    PARAM(prsBinCls, int, "prs-bin-cls", 10'000'000, "PRS binary clause limit") \
    PARAM(prsCardCls, int, "prs-card-cls", 1'000'000, "PRS cardinality clause limit") \

// Structure to hold all parameters
struct Parameters {
    #define PARAM(name, type, parsed_name, default_value, description) \
        type name = default_value;
    #define CATEGORY(description)
    PARAMETERS
    #undef PARAM
    #undef CATEGORY

    static void init(int argc, char** argv);
    static void printHelp();
    static void printParams();
};

extern Parameters __globalParameters__;
/**
 * @} // end of utils group
 */