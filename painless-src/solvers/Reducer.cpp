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

// MapleCOMSPS includes
#include "mapleCOMSPS/utils/System.h"
#include "mapleCOMSPS/core/Dimacs.h"
#include "mapleCOMSPS/simp/SimpSolver.h"

#include "../utils/Logger.h"
#include "../utils/System.h"
#include "../utils/Parameters.h"
#include "../clauses/ClauseManager.h"
#include "../solvers/Reducer.h"

using namespace MapleCOMSPS;

// Macros for minisat literal representation conversion
#define MINI_LIT(lit) lit > 0 ? mkLit(lit - 1, false) : mkLit((-lit) - 1, true)

#define INT_LIT(lit) sign(lit) ? -(var(lit) + 1) : (var(lit) + 1)


static void makeMiniVec(ClauseExchange * cls, vec<Lit> & mcls)
{
   for (size_t i = 0; i < cls->size; i++) {
      mcls.push(MINI_LIT(cls->lits[i]));
   }
}

Reducer::Reducer(int id, SolverInterface *_solver) :
   SolverInterface(id, MAPLE)
{
   solver = _solver;
}

Reducer::~Reducer()
{
   delete solver;
}

bool
Reducer::loadFormula(const char* filename)
{}

//Get the number of variables of the formula
int
Reducer::getVariablesCount()
{
   return solver->getVariablesCount();
}

// Get a variable suitable for search splitting
int
Reducer::getDivisionVariable()
{
   return solver->getDivisionVariable();
}

// Set initial phase for a given variable
void
Reducer::setPhase(const int var, const bool phase)
{
   solver->setPhase(var, phase);
}

// Bump activity for a given variable
void
Reducer::bumpVariableActivity(const int var, const int times)
{
   solver->bumpVariableActivity(var, times);
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void
Reducer::setSolverInterrupt()
{
   solver->setSolverInterrupt();
}

void
Reducer::unsetSolverInterrupt()
{
   solver->unsetSolverInterrupt();
}

// Diversify the solver
void
Reducer::diversify(int id)
{
   solver->diversify(id);
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatResult
Reducer::solve(const vector<int> & cube)
{
   unsetSolverInterrupt();

   while (true) {
      ClauseExchange *cls;
      ClauseExchange *strengthenedCls;
      if (clausesToImport.getClause(&cls) == false) {
         continue;
      }
      if (strengthened(cls, &strengthenedCls))
      {
         if (strengthenedCls->size == 0) {
            return UNSAT;
         }
         ClauseManager::increaseClause(strengthenedCls);
         clausesToExport.addClause(strengthenedCls);
      }
   }
   return UNKNOWN;
}


bool
Reducer::strengthened(ClauseExchange * cls,
                               ClauseExchange ** outCls)
{
   vector<int> assumps;
   vector<int> tmpNewClause;
   for (size_t ind = 0; ind < cls->size; ind++) {
      assumps.push_back(-cls->lits[ind]);
   }
   SatResult res = solver->solve(assumps);
   if (res == UNSAT) {
      tmpNewClause = solver->getFinalAnalysis();
   } else if (res == SAT) {
      tmpNewClause = solver->getSatAssumptions();
   }

   if (tmpNewClause.size() < cls->size || res == SAT) {
      *outCls = ClauseManager::allocClause(tmpNewClause.size());
      for (int idLit = 0; idLit < tmpNewClause.size(); idLit++) {
         (*outCls)->lits[idLit] = tmpNewClause[idLit];
      }
      (*outCls)->from = this->id;
      (*outCls)->lbd  = cls->lbd;
      if ((*outCls)->size < (*outCls)->lbd) {
         (*outCls)->lbd = (*outCls)->size;
      }
      if (res == SAT) {
         solver->addClause(*outCls);
      }
   }
   return tmpNewClause.size() < cls->size;
}

void
Reducer::addClause(ClauseExchange * clause)
{
   solver->addClause(clause);
}

void
Reducer::addLearnedClause(ClauseExchange * clause)
{
   if (clause->size == 1) {
      solver->addLearnedClause(clause);
   } else {
      clausesToImport.addClause(clause);
   }
}

void
Reducer::addClauses(const vector<ClauseExchange *> & clauses)
{
   solver->addClauses(clauses);
}

void
Reducer::addInitialClauses(const vector<ClauseExchange *> & clauses)
{
   solver->addInitialClauses(clauses);
}

void
Reducer::addLearnedClauses(const vector<ClauseExchange *> & clauses)
{
   for (size_t i = 0; i < clauses.size(); i++) {
      addLearnedClause(clauses[i]);
   }
}

void
Reducer::getLearnedClauses(vector<ClauseExchange *> & clauses)
{
   clausesToExport.getClauses(clauses);
}

void
Reducer::increaseClauseProduction()
{
   solver->increaseClauseProduction();
}

void
Reducer::decreaseClauseProduction()
{
   solver->decreaseClauseProduction();
}

SolvingStatistics
Reducer::getStatistics()
{
   return solver->getStatistics();
}

void
Reducer::printStatsStrengthening()
{
   // log(0, "count,min,max,average,stdDeviation,sum\n");
   // log(0, "cls_in,%s\n", cls_in.valueString().c_str());
   // log(0, "cls_out,%s\n", cls_out.valueString().c_str());
   // log(0, "strengthened,%s\n", strengthened.valueString().c_str());
}

vector<int>
Reducer::getModel()
{
   return solver->getModel();
}

vector<int>
Reducer::getFinalAnalysis()
{
   return solver->getFinalAnalysis();
}

vector<int>
Reducer::getSatAssumptions()
{
   return solver->getSatAssumptions();
}
