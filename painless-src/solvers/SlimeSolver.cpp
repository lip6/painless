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
#include "slime/utils/System.h"
#include "slime/core/Dimacs.h"
#include "slime/simp/SimpSolver.h"

#include "../utils/Logger.h"
#include "../utils/System.h"
#include "../utils/Parameters.h"
#include "../clauses/ClauseManager.h"
#include "../solvers/SlimeSolver.h"


using namespace SLIME;

// Macros for minisat literal representation conversion
#define MINI_LIT(lit) lit > 0 ? mkLit(lit - 1, false) : mkLit((-lit) - 1, true)

#define INT_LIT(lit) sign(lit) ? -(var(lit) + 1) : (var(lit) + 1)


static void makeMiniVec(ClauseExchange * cls, vec<Lit> & mcls)
{
   for (size_t i = 0; i < cls->size; i++) {
      mcls.push(MINI_LIT(cls->lits[i]));
   }
}


void cbkSlimeExportClause(void * issuer, int lbd, vec<Lit> & cls)
{
	SlimeSolver* mp = (SlimeSolver*)issuer;

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

Lit cbkSlimeImportUnit(void * issuer)
{
   SlimeSolver * mp = (SlimeSolver*)issuer;

   Lit l = lit_Undef;

   ClauseExchange * cls = NULL;

   if (mp->unitsToImport.getClause(&cls) == false)
      return l;

   l = MINI_LIT(cls->lits[0]);

   ClauseManager::releaseClause(cls);

   return l;
}

bool cbkSlimeImportClause(void * issuer, int * lbd, vec<Lit> & mcls)
{
   SlimeSolver* mp = (SlimeSolver*)issuer;

   ClauseExchange * cls = NULL;

   if (mp->clausesToImport.getClause(&cls) == false)
      return false;

   makeMiniVec(cls, mcls);

   *lbd = cls->lbd;

   ClauseManager::releaseClause(cls);

   return true;
}

SlimeSolver::SlimeSolver(int id) : SolverInterface(id, MAPLE)
{
	lbdLimit = Parameters::getIntParam("lbd-limit", 2);

	solver = new SimpSolver();
	solver->id = id;

   if (Parameters::getIntParam("shr-strat", 0) > 0) {
	   solver->cbkExportClause = cbkSlimeExportClause;
	   solver->cbkImportClause = cbkSlimeImportClause;
	   solver->cbkImportUnit   = cbkSlimeImportUnit;
   }
	solver->issuer          = this;
}

SlimeSolver::~SlimeSolver()
{
	delete solver;
}

bool
SlimeSolver::loadFormula(const char* filename)
{

    FILE *in = fopen(filename, "rb");
    if (in == NULL) {
        std::cout << "c ERROR! Could not open file: " << filename << std::endl;
        return EXIT_FAILURE;
    }
    parse_DIMACS(in, *solver);
    fclose(in);
    return true;
}

//Get the number of variables of the formula
int
SlimeSolver::getVariablesCount()
{
	return solver->nVars();
}

// Get a variable suitable for search splitting
int
SlimeSolver::getDivisionVariable()
{
   return (rand() % getVariablesCount()) + 1;
}

// Set initial phase for a given variable
void
SlimeSolver::setPhase(const int var, const bool phase)
{
	solver->setPolarity(var - 1, phase ? true : false);
}

// Bump activity for a given variable
void
SlimeSolver::bumpVariableActivity(const int var, const int times)
{
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void
SlimeSolver::setSolverInterrupt()
{
   stopSolver = true;

   solver->interrupt();
}

void
SlimeSolver::unsetSolverInterrupt()
{
   stopSolver = false;

	solver->clearInterrupt();
}

// Diversify the solver
void
SlimeSolver::diversify(int id)
{
   int cpus = Parameters::getIntParam("c", 30);
   solver->log = Parameters::getIntParam("v", 0);
   switch (Parameters::getIntParam("slime", 1))
   {
   case 1: // SLIME or SLIME-BMM depending on the parameters
      solver->sls = Parameters::getBoolParam("sls");
      solver->polarity_init_method = Parameters::getIntParam("pol-init", 1);
      solver->activity_init_method = Parameters::getIntParam("act-init", 1);
      solver->restarts_gap = Parameters::getIntParam("restart-gap", 300);
      break;
   case 2: // Half SLIME/Half SLIME-BMM
      if (id <= (cpus/2 -1)) {
         solver->sls = false;
         solver->polarity_init_method = 1;
         solver->activity_init_method = 1;
         solver->restarts_gap = 50;
      } else {
         solver->sls = true;
         solver->polarity_init_method = 0;
         solver->activity_init_method = 0;
         // solver->restarts_gap = Parameters::getIntParam("restart-gap", 300);
      }
      break;
   default:
      break;
   }
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatResult
SlimeSolver::solve(const vector<int> & cube)
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
SlimeSolver::addClause(ClauseExchange * clause)
{
   clausesToAdd.addClause(clause);

   setSolverInterrupt();
}

void
SlimeSolver::addLearnedClause(ClauseExchange * clause)
{
   if (clause->size == 1) {
      unitsToImport.addClause(clause);
   } else {
      clausesToImport.addClause(clause);
   }
}

void
SlimeSolver::addClauses(const vector<ClauseExchange *> & clauses)
{
   clausesToAdd.addClauses(clauses);

   setSolverInterrupt();
}

void
SlimeSolver::addInitialClauses(const vector<ClauseExchange *> & clauses)
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
SlimeSolver::addLearnedClauses(const vector<ClauseExchange *> & clauses)
{
   for (size_t i = 0; i < clauses.size(); i++) {
      addLearnedClause(clauses[i]);
   }
}

void
SlimeSolver::getLearnedClauses(vector<ClauseExchange *> & clauses)
{
   clausesToExport.getClauses(clauses);
}

void
SlimeSolver::increaseClauseProduction()
{
   lbdLimit++;
}

void
SlimeSolver::decreaseClauseProduction()
{
   if (lbdLimit > 2) {
      lbdLimit--;
   }
}

SolvingStatistics
SlimeSolver::getStatistics()
{
   SolvingStatistics stats;

   stats.conflicts    = solver->conflicts;
   stats.propagations = solver->propagations;
   stats.restarts     = solver->starts;
   stats.decisions    = solver->decisions;
   stats.memPeak      = memUsedPeak();

   return stats;
}

std::vector<int>
SlimeSolver::getModel()
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
SlimeSolver::getFinalAnalysis()
{
   vector<int> outCls;

   for (int i = 0; i < solver->conflict.size(); i++) {
      outCls.push_back(INT_LIT(solver->conflict[i]));
   }

   return outCls;
}


vector<int>
SlimeSolver::getSatAssumptions() {
   vector<int> outCls;
};


void
SlimeSolver::setSeed(int seed) {
   solver->init_seed(seed);
}