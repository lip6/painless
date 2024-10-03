#pragma once

#include "utils/Parameters.h"
#include "working/WorkingStrategy.h"

#include "solvers/LocalSearchInterface.hpp"

#include "preprocessors/StructuredBva.hpp"
#include "preprocessors/PRS-Preprocessors/preprocess.hpp"

#include "sharing/GlobalDatabase.h"
#include "sharing/LocalStrategies/LocalSharingStrategy.h"
#include "sharing/GlobalStrategies/GlobalSharingStrategy.h"

#include <thread>
#include <mutex>
#include <condition_variable>

class PortfolioSBVA : public WorkingStrategy
{
public:
    PortfolioSBVA();

    ~PortfolioSBVA();

    void solve(const std::vector<int> &cube);

    void join(WorkingStrategy *strat, SatResult res, const std::vector<int> &model);

    void setInterrupt();

    void stopSbva();

    void joinSbva();

    void unsetInterrupt();

    void waitInterrupt();

    bool isStrategyEnding() { return strategyEnding; }

protected:
    // Strategy Management
    //--------------------

    /* All this is because I didn't want to change SequentialWorker or make SBVA inherits SolverInterface: big changes are coming */
    std::atomic<bool> strategyEnding;
    std::mutex sbvaLock;
    std::condition_variable sbvaSignal;
    /* Not atomic to force use of sbvaLock */
    unsigned sbvaEnded = 0;

    // Workers
    //--------

    std::vector<std::thread> sbvaWorkers;
    std::vector<std::thread> clausesLoad;
    std::vector<std::shared_ptr<StructuredBVA>> sbvas;
    preprocess prs{-1};

    /* Mutex needed to manage the workers: the mutex synchronize the workers, the main thread and the portfolio thread */
    Mutex workersLock;

    // Data
    //-----

    std::vector<simpleClause> initClauses;
    unsigned beforeSbvaVarCount = 0;
    int leastClausesIdx = -1;

    // Diversification
    //----------------

    std::mt19937 engine{std::random_device{}()};
    std::uniform_int_distribution<int> uniform{1, Parameters::getIntParam("max-div-noise", 100)};

    // Sharing
    //--------

    /* Use weak_ptr instead of shared_ptr ? */
    std::vector<std::shared_ptr<LocalSharingStrategy>> localStrategies;
    std::vector<std::shared_ptr<GlobalSharingStrategy>> globalStrategies;
    std::shared_ptr<GlobalDatabase> globalDatabase;
    std::vector<std::unique_ptr<Sharer>> sharers;
};
