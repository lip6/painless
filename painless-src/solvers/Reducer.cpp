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
#include <mapleCOMSPS/mapleCOMSPS/utils/System.h>
#include <mapleCOMSPS/mapleCOMSPS/core/Dimacs.h>
#include <mapleCOMSPS/mapleCOMSPS/simp/SimpSolver.h>

#include "utils/Logger.h"
#include "utils/System.h"
#include "utils/Parameters.h"
#include "solvers/Reducer.h"
#include "solvers/MapleCOMSPSSolver.h"

using namespace MapleCOMSPS;

// Macros for minisat literal representation conversion
#define MINI_LIT(lit) lit > 0 ? mkLit(lit - 1, false) : mkLit((-lit) - 1, true)

#define INT_LIT(lit) sign(lit) ? -(var(lit) + 1) : (var(lit) + 1)

static void makeMiniVec(std::shared_ptr<ClauseExchange> cls, vec<Lit> &mcls)
{
   for (size_t i = 0; i < cls->size; i++)
   {
      mcls.push(MINI_LIT(cls->lits[i]));
   }
}

Reducer::Reducer(int id, SolverCdclType type) : SolverCdclInterface(id, SolverCdclType::REDUCER)
// received_lbd{0},
// reduced_lbd{0},
// received_sz{0},
// reduced_sz{0}
{
   solver = std::make_unique<MapleCOMSPSSolver>(id);
}

Reducer::~Reducer()
{
   // unique pointer will be dereferenced automatically
}

// Get the number of variables of the formula
int Reducer::getVariablesCount()
{
   return solver->getVariablesCount();
}

// Get a variable suitable for search splitting
int Reducer::getDivisionVariable()
{
   return solver->getDivisionVariable();
}

void Reducer::setPhase(const unsigned var, const bool phase)
{
   this->solver->setPhase(var, phase);
}

void Reducer::bumpVariableActivity(const int var, const int times)
{
   this->solver->bumpVariableActivity(var, times);
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void Reducer::setSolverInterrupt()
{
   solver->setSolverInterrupt();
   interrupted = true;
}

void Reducer::unsetSolverInterrupt()
{
   solver->unsetSolverInterrupt();
   interrupted = false;
}

// Diversify the solver
void Reducer::diversify(std::mt19937 &rng_engine, std::uniform_int_distribution<int> &uniform_dist)
{
   solver->diversify(rng_engine, uniform_dist);
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatResult
Reducer::solve(const std::vector<int> &cube)
{
   unsetSolverInterrupt();

   while (!interrupted)
   {
      std::shared_ptr<ClauseExchange> cls;
      std::shared_ptr<ClauseExchange> strengthenedCls;
      if (clausesToImport.getClause(cls) == false)
      {
         continue;
      }
      // ++received_lbd[cls->lbd >= STATS_LBD_MAX ? STATS_LBD_MAX - 1 : cls->lbd - 1];
      // ++received_sz[cls->size >= STATS_SZ_MAX ? STATS_SZ_MAX - 1: cls->size - 1];
      if (strengthened(cls, strengthenedCls))
      {
         // ++reduced_lbd[cls->lbd >= STATS_LBD_MAX ? STATS_LBD_MAX - 1 : cls->lbd - 1];
         // ++reduced_sz[cls->size >= STATS_SZ_MAX ? STATS_SZ_MAX - 1: cls->size - 1];
         if (strengthenedCls->size == 0)
         {
            return UNSAT;
         }
         LOGDEBUG1("Reducer generated a clause of size %d -> %d", cls->size, strengthenedCls->size);
         clausesToExport.addClause(strengthenedCls);
      }
   }
   return UNKNOWN;
}

bool Reducer::strengthened(std::shared_ptr<ClauseExchange> cls,
                           std::shared_ptr<ClauseExchange> &outCls)
{
   std::vector<int> assumps;
   std::vector<int> tmpNewClause;
   for (size_t ind = 0; ind < cls->size; ind++)
   {
      assumps.push_back(-cls->lits[ind]);
   }
   SatResult res = solver->solve(assumps);
   if (res == UNSAT)
   {
      tmpNewClause = solver->getFinalAnalysis();
   }
   else if (res == SAT)
   {
      tmpNewClause = solver->getSatAssumptions();
   }

   if (tmpNewClause.size() < cls->size || res == SAT)
   {
      outCls = std::make_shared<ClauseExchange>(tmpNewClause.size());
      for (int idLit = 0; idLit < tmpNewClause.size(); idLit++)
      {
         (outCls)->lits[idLit] = tmpNewClause[idLit];
      }
      (outCls)->from = this->id;
      (outCls)->lbd = cls->lbd;
      if ((outCls)->size < (outCls)->lbd)
      {
         (outCls)->lbd = (outCls)->size;
      }
      if (res == SAT)
      {
         solver->addClause(outCls);
      }
   }
   return tmpNewClause.size() < cls->size;
}

void Reducer::loadFormula(const char *filename)
{
   this->solver->loadFormula(filename);
}

void Reducer::addClause(std::shared_ptr<ClauseExchange> clause)
{
   solver->addClause(clause);
}

bool Reducer::importClause(std::shared_ptr<ClauseExchange> clause)
{
   if (clause->size == 1)
   {
      solver->importClause(clause);
   }
   else
   {
      clausesToImport.addClause(clause);
   }
   return true;
}

void Reducer::addClauses(const std::vector<std::shared_ptr<ClauseExchange>> &clauses)
{
   solver->addClauses(clauses);
}

void Reducer::addInitialClauses(const std::vector<simpleClause> &clauses, unsigned nbVars)
{
   solver->addInitialClauses(clauses, nbVars);
}

void Reducer::importClauses(const std::vector<std::shared_ptr<ClauseExchange>> &clauses)
{
   for (auto cls : clauses)
   {
      importClause(cls);
   }
}

void Reducer::exportClauses(std::vector<std::shared_ptr<ClauseExchange>> &clauses)
{
   clausesToExport.getClauses(clauses);
}

void Reducer::increaseClauseProduction()
{
   solver->increaseClauseProduction();
}

void Reducer::decreaseClauseProduction()
{
   solver->decreaseClauseProduction();
}

void
Reducer::printStatistics()
{
   /*LOG(" R(%d);lbd;", id);
   for (int i = 0; i < STATS_LBD_MAX; i++) {
      LOGDEBUG1("%d:%lu:%lu%s", i+1, received_lbd[i], reduced_lbd[i], i == STATS_LBD_MAX - 1 ? "": ";");
   }
   LOG(" R(%d);size;", id);
   unsigned long sum_rcvd_range = 0;
   unsigned long sum_sz_range = 0;
   for (int i = 0; i < STATS_SZ_MAX; i++) {
      if (i < 10) {
         LOGDEBUG1("%d:%lu:%lu;", i+1, received_sz[i], reduced_sz[i]);
      } else if((i+1) % 10 == 0) {
         LOGDEBUG1("%d:%lu:%lu%s", i+1, sum_rcvd_range, sum_sz_range, i == STATS_SZ_MAX - 1 ? "": ";");
         sum_rcvd_range = 0;
         sum_sz_range = 0;
      } else {
         sum_rcvd_range += received_sz[i];
         sum_sz_range += reduced_sz[i];
      }
   }
   unsigned long sum_rcvd_lbd = 0;
   unsigned long sum_rdcd_lbd = 0;
   for (int i = 0; i < STATS_LBD_MAX; ++i) {
      sum_rcvd_lbd += received_lbd[i];
      sum_rdcd_lbd += reduced_lbd[i];
   }
   LOG(" R(%d) received %lu clauses, reduced %lu clauses", id, sum_rcvd_lbd, sum_rdcd_lbd);*/
   solver->printStatistics();
}
void Reducer::printStatsStrengthening()
{
   // #ifndef QUIET
   // LOG( "count,min,max,average,stdDeviation,sum");
   // #endif
   // #ifndef QUIET
   // LOG( "cls_in,%s", cls_in.valueString().c_str());
   // #endif
   // #ifndef QUIET
   // LOG( "cls_out,%s", cls_out.valueString().c_str());
   // #endif
   // #ifndef QUIET
   // LOG( "strengthened,%s", strengthened.valueString().c_str());
   // #endif
}

std::vector<int>
Reducer::getModel()
{
   return solver->getModel();
}

std::vector<int>
Reducer::getFinalAnalysis()
{
   return solver->getFinalAnalysis();
}

std::vector<int>
Reducer::getSatAssumptions()
{
   return solver->getSatAssumptions();
}

// bool Reducer::testStrengthening()
// {
//    return true;
// }