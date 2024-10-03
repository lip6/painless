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

void cbkMapleCOMSPSExportClause(void *issuer, unsigned lbd, vec<Lit> &cls)
{
   MapleCOMSPSSolver *mp = (MapleCOMSPSSolver *)issuer;

   if (lbd > mp->lbdLimit)
      return;

   std::shared_ptr<ClauseExchange> ncls(new ClauseExchange(cls.size()));

   for (int i = 0; i < cls.size(); i++)
   {
      ncls->lits[i] = INT_LIT(cls[i]);
   }

   ncls->lbd = lbd;
   ncls->from = mp->id;

   mp->clausesToExport.addClause(ncls);
}

Lit cbkMapleCOMSPSImportUnit(void *issuer)
{
   MapleCOMSPSSolver *mp = (MapleCOMSPSSolver *)issuer;

   Lit l = lit_Undef;

   std::shared_ptr<ClauseExchange> cls;

   if (mp->unitsToImport.getClause(cls) == false)
      return l;

   l = MINI_LIT(cls->lits[0]);

   return l;
}

bool cbkMapleCOMSPSImportClause(void *issuer, unsigned *lbd, vec<Lit> &mcls)
{
   MapleCOMSPSSolver *mp = (MapleCOMSPSSolver *)issuer;

   std::shared_ptr<ClauseExchange> cls;

   if (mp->clausesToImport.getClause(cls) == false)
      return false;

   makeMiniVec(cls, mcls);

   *lbd = cls->lbd;

   return true;
}

MapleCOMSPSSolver::MapleCOMSPSSolver(int id) : SolverCdclInterface(id, MAPLE)
{
   lbdLimit = 0;

   solver = new SimpSolver();

   solver->cbkExportClause = cbkMapleCOMSPSExportClause;
   solver->cbkImportClause = cbkMapleCOMSPSImportClause;
   solver->cbkImportUnit = cbkMapleCOMSPSImportUnit;
   solver->issuer = this;
}

MapleCOMSPSSolver::MapleCOMSPSSolver(const MapleCOMSPSSolver &other, int id) : SolverCdclInterface(id, MAPLE)
{
   lbdLimit = 0;

   solver = new SimpSolver(*(other.solver));

   solver->cbkExportClause = cbkMapleCOMSPSExportClause;
   solver->cbkImportClause = cbkMapleCOMSPSImportClause;
   solver->cbkImportUnit = cbkMapleCOMSPSImportUnit;
   solver->issuer = this;
}

MapleCOMSPSSolver::~MapleCOMSPSSolver()
{
   delete solver;
}

// void MapleCOMSPSSolver::loadFormula(const char *filename)
// {
//    gzFile in = gzopen(filename, "rb");

//    parse_DIMACS(in, *solver);

//    gzclose(in);

//    return true;
// }

// Get the number of variables of the formula
int MapleCOMSPSSolver::getVariablesCount()
{
   return solver->nVars();
}

// Get a variable suitable for search splitting
int MapleCOMSPSSolver::getDivisionVariable()
{
   return (rand() % getVariablesCount()) + 1;
}

// Set initial phase for a given variable
void MapleCOMSPSSolver::setPhase(const unsigned var, const bool phase)
{
   solver->setPolarity(var - 1, phase ? true : false);
}

// Bump activity for a given variable
void MapleCOMSPSSolver::bumpVariableActivity(const int var, const int times)
{
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void MapleCOMSPSSolver::setSolverInterrupt()
{
   stopSolver = true;

   solver->interrupt();
}

void MapleCOMSPSSolver::unsetSolverInterrupt()
{
   stopSolver = false;

   solver->clearInterrupt();
}

// Diversify the solver
void MapleCOMSPSSolver::diversify(std::mt19937 &rng_engine, std::uniform_int_distribution<int> &uniform_dist)
{
   if (id == ID_XOR)
   {
      solver->GE = true;
   }
   else
   {
      solver->GE = false;
   }

   if (id % 2)
   {
      solver->VSIDS = false;
   }
   else
   {
      solver->VSIDS = true;
   }

   if (id % 4 >= 2)
   {
      solver->verso = false;
   }
   else
   {
      solver->verso = true;
   }
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatResult
MapleCOMSPSSolver::solve(const std::vector<int> &cube)
{
   unsetSolverInterrupt();

   std::vector<std::shared_ptr<ClauseExchange>> tmp;

   tmp.clear();
   clausesToAdd.getClauses(tmp);

   for (size_t ind = 0; ind < tmp.size(); ind++)
   {
      vec<Lit> mcls;
      makeMiniVec(tmp[ind], mcls);

      if (solver->addClause(mcls) == false)
      {
         LOG(" unsat when adding cls");
         return UNSAT;
      }
   }

   vec<Lit> miniAssumptions;
   for (size_t ind = 0; ind < cube.size(); ind++)
   {
      miniAssumptions.push(MINI_LIT(cube[ind]));
   }

   lbool res = solver->solveLimited(miniAssumptions);

   if (res == l_True)
      return SAT;

   if (res == l_False)
      return UNSAT;

   return UNKNOWN;
}

void MapleCOMSPSSolver::loadFormula(const char *filename)
{
   gzFile in = gzopen(filename, "rb");
   parse_DIMACS(in, *solver);
   gzclose(in);
}

void MapleCOMSPSSolver::addClause(std::shared_ptr<ClauseExchange> clause)
{
   clausesToAdd.addClause(clause);

   setSolverInterrupt();
}

bool MapleCOMSPSSolver::importClause(std::shared_ptr<ClauseExchange> clause)
{
   if (clause->size == 1)
   {
      unitsToImport.addClause(clause);
   }
   else
   {
      clausesToImport.addClause(clause);
   }
   return true;
}

void MapleCOMSPSSolver::addClauses(const std::vector<std::shared_ptr<ClauseExchange>> &clauses)
{
   clausesToAdd.addClauses(clauses);

   setSolverInterrupt();
}

void MapleCOMSPSSolver::addInitialClauses(const std::vector<simpleClause> &clauses, unsigned nbVars)
{
   for (size_t ind = 0; ind < clauses.size(); ind++)
   {
      vec<Lit> mcls;

      for (size_t i = 0; i < clauses[ind].size(); i++)
      {
         int lit = clauses[ind][i];
         int var = abs(lit);

         while (solver->nVars() < var)
         {
            solver->newVar();
         }

         mcls.push(MINI_LIT(lit));
      }

      if (solver->addClause(mcls) == false)
      {
         LOG(" unsat when adding initial cls");
      }
   }

   LOG2("The Maple Solver %d loaded all the clauses", this->id);
}

void MapleCOMSPSSolver::importClauses(const std::vector<std::shared_ptr<ClauseExchange>> &clauses)
{
   for (auto cls : clauses)
   {
      importClause(cls);
   }
}

void MapleCOMSPSSolver::exportClauses(std::vector<std::shared_ptr<ClauseExchange>> &clauses)
{
   clausesToExport.getClauses(clauses);
}

void MapleCOMSPSSolver::increaseClauseProduction()
{
   lbdLimit++;
}

void MapleCOMSPSSolver::decreaseClauseProduction()
{
   if (lbdLimit > 2)
   {
      lbdLimit--;
   }
}

void MapleCOMSPSSolver::printStatistics()
{
   LOGSTAT("Solver %d:\n\t\
   * Conflicts: %lu\n\t\
   * Propapgations: %lu\n\t\
   * Restars: %lu\n\t\
   * Decisions: %lu\n\t\
   * Maximum memory: %f Ko",
           this->id, solver->conflicts, solver->propagations, solver->starts, solver->decisions, memUsedPeak());
}

std::vector<int>
MapleCOMSPSSolver::getModel()
{
   std::vector<int> model;

   for (int i = 0; i < solver->nVars(); i++)
   {
      if (solver->model[i] != l_Undef)
      {
         int lit = solver->model[i] == l_True ? i + 1 : -(i + 1);
         model.push_back(lit);
      }
   }

   return model;
}

std::vector<int>
MapleCOMSPSSolver::getFinalAnalysis()
{
   std::vector<int> outCls;

   for (int i = 0; i < solver->conflict.size(); i++)
   {
      outCls.push_back(INT_LIT(solver->conflict[i]));
   }

   return outCls;
}

std::vector<int>
MapleCOMSPSSolver::getSatAssumptions()
{
   std::vector<int> outCls;
   vec<Lit> lits;
   solver->getAssumptions(lits);
   for (int i = 0; i < lits.size(); i++)
   {
      outCls.push_back(-(INT_LIT(lits[i])));
   }
   return outCls;
};

void MapleCOMSPSSolver::setStrengthening(bool b)
{
   solver->setStrengthening(b);
}