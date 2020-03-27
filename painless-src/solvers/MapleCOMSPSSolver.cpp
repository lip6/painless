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
#include "../solvers/MapleCOMSPSSolver.h"

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


void cbkMapleCOMSPSExportClause(void * issuer, int lbd, vec<Lit> & cls)
{
	MapleCOMSPSSolver* mp = (MapleCOMSPSSolver*)issuer;

	if (lbd > mp->lbdLimit)
		return;

	ClauseExchange * ncls = ClauseManager::allocClause(cls.size());

	for (int i = 0; i < cls.size(); i++) {
		ncls->lits[i] = INT_LIT(cls[i]);
	}

   ncls->lbd  = lbd;
   ncls->from = mp->id;

   mp->clausesToExport.addClause(ncls);
}

Lit cbkMapleCOMSPSImportUnit(void * issuer)
{
   MapleCOMSPSSolver * mp = (MapleCOMSPSSolver*)issuer;

   Lit l = lit_Undef;

   ClauseExchange * cls = NULL;

   if (mp->unitsToImport.getClause(&cls) == false)
      return l;

   l = MINI_LIT(cls->lits[0]);

   ClauseManager::releaseClause(cls);

   return l;
}

bool cbkMapleCOMSPSImportClause(void * issuer, int * lbd, vec<Lit> & mcls)
{
   MapleCOMSPSSolver* mp = (MapleCOMSPSSolver*)issuer;

   ClauseExchange * cls = NULL;

   if (mp->clausesToImport.getClause(&cls) == false)
      return false;

   makeMiniVec(cls, mcls);

   *lbd = cls->lbd + 1;

   ClauseManager::releaseClause(cls);

   return true;
}

MapleCOMSPSSolver::MapleCOMSPSSolver(int id) : SolverInterface(id, MAPLE)
{
	lbdLimit = Parameters::getIntParam("lbd-limit", 4);

	solver = new SimpSolver();

	solver->cbkExportClause = cbkMapleCOMSPSExportClause;
	solver->cbkImportClause = cbkMapleCOMSPSImportClause;
	solver->cbkImportUnit   = cbkMapleCOMSPSImportUnit;
	solver->issuer          = this;
}

MapleCOMSPSSolver::MapleCOMSPSSolver(const MapleCOMSPSSolver & other, int id) :
   SolverInterface(id, MAPLE)
{
	lbdLimit = Parameters::getIntParam("lbd-limit", 4);

	solver = new SimpSolver(*(other.solver));

	solver->cbkExportClause = cbkMapleCOMSPSExportClause;
	solver->cbkImportClause = cbkMapleCOMSPSImportClause;
	solver->cbkImportUnit   = cbkMapleCOMSPSImportUnit;
	solver->issuer          = this;
}

MapleCOMSPSSolver::~MapleCOMSPSSolver()
{
	delete solver;
}

bool
MapleCOMSPSSolver::loadFormula(const char* filename)
{
    gzFile in = gzopen(filename, "rb");

    parse_DIMACS(in, *solver);

    gzclose(in);

    return true;
}

//Get the number of variables of the formula
int
MapleCOMSPSSolver::getVariablesCount()
{
	return solver->nVars();
}

// Get a variable suitable for search splitting
int
MapleCOMSPSSolver::getDivisionVariable()
{
   return (rand() % getVariablesCount()) + 1;
}

// Set initial phase for a given variable
void
MapleCOMSPSSolver::setPhase(const int var, const bool phase)
{
	solver->setPolarity(var - 1, phase ? true : false);
}

// Bump activity for a given variable
void
MapleCOMSPSSolver::bumpVariableActivity(const int var, const int times)
{
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void
MapleCOMSPSSolver::setSolverInterrupt()
{
   stopSolver = true;

   solver->interrupt();
}

void
MapleCOMSPSSolver::unsetSolverInterrupt()
{
   stopSolver = false;

	solver->clearInterrupt();
}

// Diversify the solver
void
MapleCOMSPSSolver::diversify(int id)
{
   if (id == ID_XOR) {
      solver->GE = true;
   } else {
      solver->GE = false;
   }

   if (id % 2 == 0) {
      solver->VSIDS = false;
   } else {
      solver->VSIDS = true;
   }

   if (id % 4 == 0 || (id - 1) % 4 == 0) {
      solver->recto = false;
   } else {
      solver->recto = true;
   }
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatResult
MapleCOMSPSSolver::solve(const vector<int> & cube)
{
   unsetSolverInterrupt();

   vector<ClauseExchange *> tmp;

   tmp.clear();
   clausesToAdd.getClauses(tmp);

   for (size_t ind = 0; ind < tmp.size(); ind++) {
      vec<Lit> mcls;
      makeMiniVec(tmp[ind], mcls);

      ClauseManager::releaseClause(tmp[ind]);

      if (solver->addClause(mcls) == false) {
         printf("c unsat when adding cls\n");
         return UNSAT;
      }
   }

   vec<Lit> miniAssumptions;
   for (size_t ind = 0; ind < cube.size(); ind++) {
      miniAssumptions.push(MINI_LIT(cube[ind]));
   }

   lbool res = solver->solveLimited(miniAssumptions);

   if (res == l_True)
      return SAT;

   if (res == l_False)
      return UNSAT;

   return UNKNOWN;
}

void
MapleCOMSPSSolver::addClause(ClauseExchange * clause)
{
   clausesToAdd.addClause(clause);

   setSolverInterrupt();
}

void
MapleCOMSPSSolver::addLearnedClause(ClauseExchange * clause)
{
   if (clause->size == 1) {
      unitsToImport.addClause(clause);
   } else {
      clausesToImport.addClause(clause);
   }
}

void
MapleCOMSPSSolver::addClauses(const vector<ClauseExchange *> & clauses)
{
   clausesToAdd.addClauses(clauses);

   setSolverInterrupt();
}

void
MapleCOMSPSSolver::addInitialClauses(const vector<ClauseExchange *> & clauses)
{
   for (size_t ind = 0; ind < clauses.size(); ind++) {
      vec<Lit> mcls;

      for (size_t i = 0; i < clauses[ind]->size; i++) {
         int lit = clauses[ind]->lits[i];
         int var = abs(lit);

         while (solver->nVars() < var) {
            solver->newVar();
         }

         mcls.push(MINI_LIT(lit));
      }

      if (solver->addClause(mcls) == false) {
         printf("c unsat when adding initial cls\n");
      }
   }
}

void
MapleCOMSPSSolver::addLearnedClauses(const vector<ClauseExchange *> & clauses)
{
   for (size_t i = 0; i < clauses.size(); i++) {
      addLearnedClause(clauses[i]);
   }
}

void
MapleCOMSPSSolver::getLearnedClauses(vector<ClauseExchange *> & clauses)
{
   clausesToExport.getClauses(clauses);
}

void
MapleCOMSPSSolver::increaseClauseProduction()
{
   lbdLimit++;
}

void
MapleCOMSPSSolver::decreaseClauseProduction()
{
   lbdLimit = lbdLimit.load() / 2;

   if (lbdLimit < 2) {
      lbdLimit = 2;
   }
}

SolvingStatistics
MapleCOMSPSSolver::getStatistics()
{
   SolvingStatistics stats;

   stats.conflicts    = solver->conflicts;
   stats.propagations = solver->propagations;
   stats.restarts     = solver->starts;
   stats.decisions    = solver->decisions;
   stats.memPeak      = memUsedPeak();

   return stats;
}

vector<int>
MapleCOMSPSSolver::getModel()
{
   std::vector<int> model;

   for (int i = 0; i < solver->nVars(); i++) {
      if (solver->model[i] != l_Undef) {
         int lit = solver->model[i] == l_True ? i + 1 : -(i + 1);
         model.push_back(lit);
      }
   }

   return model;
}

vector<int>
MapleCOMSPSSolver::getFinalAnalysis()
{
   vector<int> outCls;

   for (int i = 0; i < solver->conflict.size(); i++) {
      outCls.push_back(INT_LIT(solver->conflict[i]));
   }

   return outCls;
}


vector<int>
MapleCOMSPSSolver::getSatAssumptions() {
   vector<int> outCls;
   vec<Lit> lits;
   solver->getAssumptions(lits);
   for (int i = 0; i < lits.size(); i++) {
     outCls.push_back(-(INT_LIT(lits[i])));
   }
   return outCls;
};
