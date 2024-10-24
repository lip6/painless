// MiniSat includes
#include "minisat/core/Dimacs.h"
#include "minisat/simp/SimpSolver.h"
#include "minisat/utils/System.h"

#include "utils/Parameters.hpp"

#include "containers/ClauseDatabaseSingleBuffer.hpp"

#include "MiniSat.hpp"
#include <iomanip>

using namespace Minisat;

// Macros for minisat literal representation conversion
#define MINI_LIT(lit) lit > 0 ? mkLit(lit - 1, false) : mkLit((-lit) - 1, true)

#define INT_LIT(lit) sign(lit) ? -(var(lit) + 1) : (var(lit) + 1)

static void
makeMiniVec(ClauseExchangePtr cls, vec<Lit>& mcls)
{
	for (size_t i = 0; i < cls->size; i++) {
		mcls.push(MINI_LIT(cls->lits[i]));
	}
}

void
minisatExportClause(void* issuer, vec<Lit>& cls)
{
	/* TODO: a better fake glue management ?*/
	MiniSat* ms = (MiniSat*)issuer;

	ClauseExchangePtr ncls = ClauseExchange::create(cls.size());

	// Fake glue value
	int madeUpGlue = cls.size();
	ncls->lbd = madeUpGlue;

	for (int i = 0; i < cls.size(); i++) {
		ncls->lits[i] = INT_LIT(cls[i]);
	}

	ncls->from = ms->getSharingId();

	ms->exportClause(ncls);
}

Lit
minisatImportUnit(void* issuer)
{
	MiniSat* ms = (MiniSat*)issuer;

	Lit l = lit_Undef;

	ClauseExchangePtr cls;

	if (ms->unitsToImport->getOneClause(cls) == false)
		return l;

	l = MINI_LIT(cls->lits[0]);
	return l;
}

bool
minisatImportClause(void* issuer, vec<Lit>& mcls)
{
	MiniSat* ms = (MiniSat*)issuer;

	ClauseExchangePtr cls;

	if (ms->m_clausesToImport->getOneClause(cls) == false) {
		ms->m_clausesToImport->shrinkDatabase();
		return false;
	}

	makeMiniVec(cls, mcls);
	return true;
}

MiniSat::MiniSat(int id, const std::shared_ptr<ClauseDatabase>& clauseDB)
	: SolverCdclInterface(id, clauseDB, SolverCdclType::MINISAT)
	, clausesToAdd(__globalParameters__.defaultClauseBufferSize)
{
	this->unitsToImport = std::make_unique<ClauseDatabaseSingleBuffer>(__globalParameters__.defaultClauseBufferSize);

	solver = new SimpSolver();
	solver->remove_satisfied = false;

	solver->exportClauseCallback = minisatExportClause;
	solver->importUnitCallback = minisatImportUnit;
	solver->importClauseCallback = minisatImportClause;
	solver->issuer = this;

	initializeTypeId<MiniSat>();
}

MiniSat::~MiniSat()
{
	delete solver;
}

void
MiniSat::loadFormula(const char* filename)
{
	gzFile in = gzopen(filename, "rb");

	parse_DIMACS(in, *solver);

	gzclose(in);
}

// Get the number of variables of the formula
unsigned int
MiniSat::getVariablesCount()
{
	return solver->nVars();
}

// Get a variable suitable for search splitting
int
MiniSat::getDivisionVariable()
{
	return (rand() % getVariablesCount()) + 1;
}

// Set initial phase for a given variable
void
MiniSat::setPhase(const unsigned int var, const bool phase)
{
	solver->setPolarity(var - 1, phase ? l_True : l_False);
}

// Bump activity for a given variable
void
MiniSat::bumpVariableActivity(const int var, const int times)
{
	for (int i = 0; i < times; i++) {
		solver->varBumpActivity(var - 1);
	}
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void
MiniSat::setSolverInterrupt()
{
	LOGDEBUG1("Asking Minisat (%d, %u) to end", this->getSolverId(), this->getSolverTypeId());

	solver->interrupt();
}

void
MiniSat::unsetSolverInterrupt()
{
	solver->clearInterrupt();
}

// Diversify the solver
void
MiniSat::diversify(const SeedGenerator& getSeed)
{
	solver->random_seed = (double)getSeed(this);
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatResult
MiniSat::solve(const std::vector<int>& cube)
{
	std::vector<ClauseExchangePtr> tmp;
	clausesToAdd.getClauses(tmp);

	for (size_t ind = 0; ind < tmp.size(); ind++) {
		vec<Lit> mcls;
		makeMiniVec(tmp[ind], mcls);

		if (solver->addClause(mcls) == false) {
			printf("unsat when adding cls\n");
			return SatResult::UNSAT;
		}
	}

	vec<Lit> miniAssumptions;
	for (size_t ind = 0; ind < cube.size(); ind++) {
		miniAssumptions.push(MINI_LIT(cube[ind]));
	}

	lbool res = solver->solveLimited(miniAssumptions);

	if (res == l_True)
		return SatResult::SAT;

	if (res == l_False)
		return SatResult::UNSAT;

	return SatResult::UNKNOWN;
}

void
MiniSat::addClause(ClauseExchangePtr clause)
{
	clausesToAdd.addClause(clause);

	setSolverInterrupt();
}

bool
MiniSat::importClause(const ClauseExchangePtr& clause)
{
	assert(clause->size > 0);

	if (clause->size == 1) {
		unitsToImport->addClause(clause);
	} else {
		m_clausesToImport->addClause(clause);
	}

	return true;
}

void
MiniSat::addClauses(const std::vector<ClauseExchangePtr>& clauses)
{
	clausesToAdd.addClauses(clauses);

	setSolverInterrupt();
}

void
MiniSat::addInitialClauses(const std::vector<simpleClause>& clauses, unsigned int nbVars)
{
	for (size_t ind = 0; ind < clauses.size(); ind++) {
		vec<Lit> mcls;

		for (size_t i = 0; i < clauses[ind].size(); i++) {
			int lit = clauses[ind][i];
			int var = abs(lit);

			while (solver->nVars() < var) {
				solver->newVar();
			}

			mcls.push(MINI_LIT(lit));
		}

		if (solver->addClause(mcls) == false) {
			printf("unsat when adding initial cls\n");
		}
	}
	LOG2("The Minisat Solver %d loaded all the %u clauses with %u variables",
		 this->getSolverId(),
		 clauses.size(),
		 nbVars);
}

void
MiniSat::importClauses(const std::vector<ClauseExchangePtr>& clauses)
{
	for (size_t i = 0; i < clauses.size(); i++) {
		importClause(clauses[i]);
	}
}

void
MiniSat::printStatistics()
{
	SolvingCdclStatistics stats;

	stats.conflicts = solver->conflicts;
	stats.propagations = solver->propagations;
	stats.restarts = solver->starts;
	stats.decisions = solver->decisions;

	std::cout << std::left << std::setw(15) << ("| M" + std::to_string(this->getSolverTypeId())) << std::setw(20)
			  << ("| " + std::to_string(stats.conflicts)) << std::setw(20)
			  << ("| " + std::to_string(stats.propagations)) << std::setw(17) << ("| " + std::to_string(stats.restarts))
			  << std::setw(20) << ("| " + std::to_string(stats.decisions)) << std::setw(20) << "|"
			  << "\n";
}

void
MiniSat::printWinningLog()
{
	this->SolverCdclInterface::printWinningLog();
	LOGSTAT("The winner is MiniSat(%d, %u) ", this->getSolverId(), this->getSolverTypeId());
}

std::vector<int>
MiniSat::getModel()
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

std::vector<int>
MiniSat::getFinalAnalysis()
{
	std::vector<int> outCls;
	LOGERROR("NOT IMPLEMENTED");
	return outCls;
}

std::vector<int>
MiniSat::getSatAssumptions()
{
	std::vector<int> outCls;
	LOGERROR("NOT IMPLEMENTED");
	return outCls;
};