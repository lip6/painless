
// MiniSat includes
#include <minisat/core/Dimacs.h>
#include <minisat/simp/SimpSolver.h>
#include <minisat/utils/System.h>

#include "utils/Parameters.hpp"

#include "containers/ClauseDatabases/ClauseDatabaseSingleBuffer.hpp"

#include "MiniSat.hpp"
#include <iomanip>

// Macros for minisat literal representation conversion
#define MINI_LIT(lit) lit > 0 ? Minisat::mkLit(lit - 1, false) : Minisat::mkLit((-lit) - 1, true)

#define INT_LIT(lit) Minisat::sign(lit) ? -(Minisat::var(lit) + 1) : (Minisat::var(lit) + 1)

static bool
checkLiteral(const Minisat::Lit& l, Minisat::Solver* minisat)
{
	// Assert if not negative (error)
	assert(l.x >= 0);
	// Check if the literal is not out of bound
	if (minisat->nVars() <= var(l)) {
		LOGDEBUG2("Literal %d is out of bound for Minisat", l.x);
		return false;
	}
	return true;
}

static bool
makeMiniVec(ClauseExchangePtr cls, Minisat::vec<Minisat::Lit>& mcls, Minisat::Solver* minisat)
{
	for (size_t i = 0; i < cls->size; i++) {
		Minisat::Lit internalLit = MINI_LIT(cls->lits[i]);
		mcls.push(internalLit);
		// Checks
		if (!checkLiteral(internalLit, minisat))
			return false;
	}
	return true;
}

void
minisatExportClause(void* issuer, Minisat::vec<Minisat::Lit>& cls)
{
	/* TODO: a better fake glue management ?*/
	MiniSat* ms = (MiniSat*)issuer;

	// Fake glue value
	ClauseExchangePtr ncls = ClauseExchange::create(cls.size(), cls.size(), ms->getSharingId());

	for (int i = 0; i < cls.size(); i++) {
		ncls->lits[i] = INT_LIT(cls[i]);
	}

	ncls->from = ms->getSharingId();

	ms->exportClause(ncls);
}

Minisat::Lit
minisatImportUnit(void* issuer)
{
	MiniSat* ms = (MiniSat*)issuer;

	Minisat::Lit l;

	ClauseExchangePtr cls;

	while (ms->unitsToImport->getOneClause(cls)) {
		assert(cls->size == 1);
		l = MINI_LIT(cls->lits[0]);

		if (checkLiteral(l, ms->solver) == true) {
			assert(l.x >= 0);
			LOGDEBUG2("Importing to Minisat %u unit literal %d", ms->getSharingId(), l.x);
			return l;
		}
	}
	return Minisat::lit_Undef;
}

bool
minisatImportClause(void* issuer, Minisat::vec<Minisat::Lit>& mcls)
{
	MiniSat* ms = (MiniSat*)issuer;

	ClauseExchangePtr cls;

	while (ms->m_clausesToImport->getOneClause(cls)) {
		if (makeMiniVec(cls, mcls, ms->solver)) {
			LOGCLAUSE1(cls->lits, cls->size, "Importing to Minisat %u clause: ", ms->getSharingId());
			assert(mcls.size() > 1 && mcls[0].x >= 0);

			return true;
		}
		mcls.clear();
	}

	ms->m_clausesToImport->shrinkDatabase();
	return false;
}

MiniSat::MiniSat(int id, const std::shared_ptr<ClauseDatabase>& clauseDB)
	: SolverCdclInterface(id, clauseDB, SolverCdclType::MINISAT)
	, clausesToAdd(__globalParameters__.defaultClauseBufferSize)
{
	this->unitsToImport = std::make_unique<ClauseDatabaseSingleBuffer>(__globalParameters__.defaultClauseBufferSize);

	solver = new Minisat::SimpSolver();
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
	solver->setPolarity(var - 1, phase ? Minisat::l_True : Minisat::l_False);
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
		Minisat::vec<Minisat::Lit> mcls;
		makeMiniVec(tmp[ind], mcls, this->solver);

		if (solver->addClause(mcls) == false) {
			LOGWARN("unsat when adding cls");
			return SatResult::UNSAT;
		}
	}

	Minisat::vec<Minisat::Lit> miniAssumptions;
	for (size_t ind = 0; ind < cube.size(); ind++) {
		miniAssumptions.push(MINI_LIT(cube[ind]));
	}

	Minisat::lbool res = solver->solveLimited(miniAssumptions);

	if (res == Minisat::l_True)
		return SatResult::SAT;

	if (res == Minisat::l_False)
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
	while (solver->nVars() < nbVars) {
		solver->newVar();
	}

	for (size_t ind = 0; ind < clauses.size(); ind++) {
		Minisat::vec<Minisat::Lit> mcls;

		for (size_t i = 0; i < clauses[ind].size(); i++) {
			int lit = clauses[ind][i];
			mcls.push(MINI_LIT(lit));
		}

		if (solver->addClause(mcls) == false) {
			LOGWARN("unsat when adding initial cls");
			return;
		}
	}
	LOG2("The Minisat Solver %d loaded all the %u clauses with %u variables",
		 this->getSolverId(),
		 clauses.size(),
		 nbVars);
}

void
MiniSat::addInitialClauses(const lit_t* literals, unsigned int clsCount, unsigned int nbVars)
{
	unsigned int clausesCount = 0;
	int lit;

	while (solver->nVars() < nbVars) {
		solver->newVar();
	}

	for (unsigned int i = 0; clausesCount < clsCount; i++) {

		Minisat::vec<Minisat::Lit> mcls;

		for (lit = *literals; lit; literals++, lit = *literals) {
			if (!lit)
				break;
			mcls.push(MINI_LIT(lit));
		}

		// Jump zero
		literals++;
		clausesCount++;

		if (!solver->addClause(mcls)) {
			LOGWARN("unsat when adding initial cls");
			return;
		}
	}

	this->setInitialized(true);

	LOG2(
		"The MiniSat Solver %d loaded all the %u clauses with %u variables", this->getSolverId(), clausesCount, nbVars);
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
		if (solver->model[i] != Minisat::l_Undef) {
			int lit = solver->model[i] == Minisat::l_True ? i + 1 : -(i + 1);
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