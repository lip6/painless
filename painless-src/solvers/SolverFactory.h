#pragma once

#include "solvers/SolverCdclInterface.hpp"
#include "solvers/LocalSearchInterface.hpp"
#include "solvers/KissatMABSolver.h"
#include "solvers/KissatGASPISolver.h"
#include "solvers/MapleCOMSPSSolver.h"
#include "solvers/YalSatSolver.h"
#include "solvers/Reducer.h"

#include <vector>

static std::atomic<int> currentIdSolver(0);

/// \ingroup solving
/// @brief Factory to create solvers.
class SolverFactory
{
public:
   static void createSolver(char type, std::vector<std::shared_ptr<SolverCdclInterface>> &cdclSolvers, std::vector<std::shared_ptr<LocalSearchSolver>> &localSolvers);

   static Reducer *createReducerSolver();

   static void createSolvers(int groupSize, std::string portfolio, std::vector<std::shared_ptr<SolverCdclInterface>> &cdclSolvers, std::vector<std::shared_ptr<LocalSearchSolver>> &localSolvers);

   /// Print stats of a groupe of solvers.
   static void printStats(const std::vector<std::shared_ptr<SolverCdclInterface>> &cdclSolvers, const std::vector<std::shared_ptr<LocalSearchSolver>> &localSolvers);

   /// Apply a diversification on solvers.
   static void diversification(const std::vector<std::shared_ptr<SolverCdclInterface>> &cdclSolvers, const std::vector<std::shared_ptr<LocalSearchSolver>> &localSolvers, bool dist = false, int mpi_rank = 0, int world_size = 0);
};
