// -----------------------------------------------------------------------------
// Copyright (C) 2017  Ludovic LE FRIOUX
//
// This file is part of PaInleSS.
//
// PaInleSS is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program.  If not, see <http://www.gnu.org/licenses/>.
// -----------------------------------------------------------------------------

#pragma once

#include "clauses/ClauseBuffer.h"
#include "solvers/SolverCdclInterface.hpp"
#include "utils/Threading.h"
#include "utils/BloomFilter.h"



#define STATS_LBD_MAX 20
#define STATS_SZ_MAX 100

// Some forward declatarations for MapleCOMSPS
namespace MapleCOMSPS
{
   class SimpSolver;
   class Lit;
   template <class T>
   class vec;
}

/// \ingroup solving
/// Reducer Class used by HordeStrSharing strategy to strengthen clauses.
class Reducer : public SolverCdclInterface
{
public:

   /// Get the number of variables of the current resolution.
   int getVariablesCount();

   /// Get a variable suitable for search splitting.
   int getDivisionVariable();

   /// Set initial phase for a given variable.
   void setPhase(const unsigned var, const bool phase);

   /// Bump activity of a given variable.
   void bumpVariableActivity(const int var, const int times);

   /// Interrupt resolution, solving cannot continue until interrupt is unset.
   void setSolverInterrupt();

   /// Remove the SAT solving interrupt request.
   void unsetSolverInterrupt();

   /// Solve the formula with a given cube.
   SatResult solve(const std::vector<int> &cube);

   /// Load formula from a given dimacs file, return false if failed.
   void loadFormula(const char *filename) override; 

   /// Add a permanent clause to the formula.
   void addClause(std::shared_ptr<ClauseExchange> clause);

   /// Add a list of permanent clauses to the formula.
   void addClauses(const std::vector<std::shared_ptr<ClauseExchange> > &clauses);

   /// Add a list of initial clauses to the formula.
   void addInitialClauses(const std::vector<simpleClause> &clauses, unsigned nbVars) override;

   /// Add a learned clause to the formula.
   bool importClause(std::shared_ptr<ClauseExchange> clause);

   /// Add a list of learned clauses to the formula.
   void importClauses(const std::vector<std::shared_ptr<ClauseExchange> > &clauses);

   /// Get a list of learned clauses.
   void exportClauses(std::vector<std::shared_ptr<ClauseExchange> > &clauses);

   /// Request the solver to produce more clauses.
   void increaseClauseProduction();

   /// Request the solver to produce less clauses.
   void decreaseClauseProduction();

   /// Get solver statistics.
   void printStatistics();

   /// Return the model in case of SAT result.
   std::vector<int> getModel();

   /// Return the final analysis in case of UNSAT.
   std::vector<int> getFinalAnalysis();

   /// Native diversification.
   void diversify(std::mt19937 &rng_engine, std::uniform_int_distribution<int> &uniform_dist);

   bool strengthened(std::shared_ptr<ClauseExchange> cls, std::shared_ptr<ClauseExchange> &outCls);

   void printStatsStrengthening();

   std::vector<int> getSatAssumptions();

   // bool testStrengthening()

   void initshuffle(int id){};

   /// Constructor.
   Reducer(int id, SolverCdclType type);

   /// Destructor.
   virtual ~Reducer();

protected:
   /// Pointer to a MapleCOMSPS solver.
   std::unique_ptr<SolverCdclInterface>solver;

   /// Buffer used to import clauses (units included).
   ClauseBuffer clausesToImport;

   /// Buffer used to export clauses (units included).
   ClauseBuffer clausesToExport;

   // BloomFilter filter;

   std::atomic<bool> interrupted;

   // unsigned long received_lbd[STATS_LBD_MAX];
   // unsigned long reduced_lbd[STATS_LBD_MAX];

   // unsigned long received_sz[STATS_SZ_MAX];
   // unsigned long reduced_sz[STATS_SZ_MAX];
};
