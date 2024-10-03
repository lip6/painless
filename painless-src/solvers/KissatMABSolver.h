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

#include <iostream>
#include <fstream>
#include <unordered_map>
#include "clauses/ClauseBuffer.h"
#include "../solvers/SolverCdclInterface.hpp"
#include "utils/Threading.h"
#include "utils/ClauseUtils.h"

#define KISSATMAB_


// KissatMABSolver includes
extern "C"
{
#include <kissat_mab/src/kissat.h>
}

/// Instance of a KissatMABSolver solver
class KissatMABSolver : public SolverCdclInterface
{
public:
   void setBumpVar(int v);

   /// Load formula from a given dimacs file, return false if failed.
   void loadFormula(const char *filename);

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

   /// Add a permanent clause to the formula.
   void addClause(std::shared_ptr<ClauseExchange> clause);

   /// Add a list of permanent clauses to the formula.
   void addClauses(const std::vector<std::shared_ptr<ClauseExchange>> &clauses);

   /// Add a list of initial clauses to the formula.
   void addInitialClauses(const std::vector<simpleClause> &clauses, unsigned nbVars) override;

   /// Add a learned clause to the formula.
   bool importClause(std::shared_ptr<ClauseExchange> clause);

   /// Add a list of learned clauses to the formula.
   void importClauses(const std::vector<std::shared_ptr<ClauseExchange>> &clauses);

   /// Get a list of learned clauses.
   void exportClauses(std::vector<std::shared_ptr<ClauseExchange>> &clauses);

   /// Request the solver to produce more clauses.
   void increaseClauseProduction();

   /// Request the solver to produce less clauses.
   void decreaseClauseProduction();

   /// Get solver statistics.
   void printStatistics();

   /// Return the model in case of SAT result.
   std::vector<int> getModel();

   /// Native diversification.
   void diversify(std::mt19937 &rng_engine, std::uniform_int_distribution<int> &uniform_dist) override;

   /// @brief Initializes the map @ref kissatOptions with the default configuration.
   void initkissatMABOptions();

   /// Constructor.
   KissatMABSolver(int id);

   /// Destructor.
   virtual ~KissatMABSolver();

   std::vector<int> getFinalAnalysis();

   std::vector<int> getSatAssumptions();

protected:
   int bump_var;

   /// Pointer to a KissatMABSolver solver.
   kissat *solver;

   /// @brief A map mapping a kissat option name to its value.
   std::unordered_map<std::string, int> kissatMABOptions;

   /// Buffer used to import clauses (units included).
   ClauseBuffer clausesToImport;
   ClauseBuffer unitsToImport;

   /// Buffer used to export clauses (units included).
   ClauseBuffer clausesToExport;

   /// Buffer used to add permanent clauses.
   ClauseBuffer clausesToAdd;

   /// Used to stop or continue the resolution.
   std::atomic<bool> stopSolver;

   /// Callback to export/import clauses used by real kissat.
   /* Decided to not use pointers to move because of c++ stl (cannot move an array into a vector, sharedPtr destruction) */
   friend char KissatMabImportUnit(void *, kissat *);
   /* With KissatMabImportClause(void *painless_interface, int *kclause, unsigned *size): need to not use sharedPtr*/
   friend char KissatMabImportClause(void *, kissat *); 
   friend char KissatMabExportClause(void *, kissat *);
};
