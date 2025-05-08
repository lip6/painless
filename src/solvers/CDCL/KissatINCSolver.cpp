#include "painless.hpp"
#include "sharing/Filters/BloomFilter.hpp"
#include "utils/ErrorCodes.hpp"
#include "utils/Logger.hpp"
#include "utils/MpiUtils.hpp"
#include "utils/Parameters.hpp"
#include "utils/System.hpp"

#include <cassert>

#include "KissatINCSolver.hpp"

extern "C"
{
#include "kissat-inc/src/parse.h"
}

char
KissatIncImportClause(void* painless_interface, kissat* internal_solver)
{
	KissatINCSolver* painless_kissat = (KissatINCSolver*)painless_interface;
	/* Needed for the KISSAT macros to work */

	ClauseExchangePtr clause;

	if (!painless_kissat->m_clausesToImport->getOneClause(clause)){
		painless_kissat->m_clausesToImport->shrinkDatabase();
		return false;
	}

	assert(clause->size > 0);

	if (!kissat_inc_push_lits(internal_solver, &(clause->lits[0]), clause->size))
		return false;

	return true;
}

char
KissatIncExportClause(void* painless_interface, kissat* internal_solver)
{
	KissatINCSolver* painless_kissat = (KissatINCSolver*)painless_interface;

	unsigned int lbd = kissat_inc_get_pglue(internal_solver);

	unsigned int size = kissat_inc_pclause_size(internal_solver);

	assert(size > 0);

	ClauseExchangePtr new_clause = ClauseExchange::create(size, lbd, painless_kissat->getSharingId());

	for (unsigned int i = 0; i < size; i++) {
		new_clause->lits[i] = kissat_inc_peek_plit(internal_solver, i);
	}

	/* filtering defined by a sharing strategy */
	if (painless_kissat->exportClause(new_clause)) {
		return true;
	} else
		return false;
}

KissatINCSolver::KissatINCSolver(int id, const std::shared_ptr<ClauseDatabase>& clauseDB)
	: SolverCdclInterface(id, clauseDB, SolverCdclType::KISSATINC)
	, clausesToAdd(__globalParameters__.defaultClauseBufferSize)
{
	solver = kissat_inc_init();

	kissat_inc_set_export_call(solver, KissatIncExportClause);
	kissat_inc_set_import_call(solver, KissatIncImportClause);
	kissat_inc_set_import_unit_call(solver, nullptr);
	kissat_inc_set_painless(solver, this);
	kissat_inc_set_id(solver, id);

	initializeTypeId<KissatINCSolver>();
}

KissatINCSolver::~KissatINCSolver()
{
	// kissat_inc_print_sharing_stats(this->solver);
	kissat_inc_release(solver);
}

void
KissatINCSolver::loadFormula(const char* filename)
{
	strictness strict = NORMAL_PARSING;
	file in;
	uint64_t lineno;
	kissat_inc_open_to_read_file(&in, filename);

	int nbVars;

	kissat_inc_parse_dimacs(solver, strict, &in, &lineno, &nbVars);

	kissat_inc_close_file(&in);

	kissat_inc_set_maxVar(this->solver, nbVars);

	this->setInitialized(true);

	LOG2("The KissatInc Solver %d loaded all the formula with %u variables", this->getSolverId(), nbVars);
}

// Get the number of variables of the formula
unsigned int
KissatINCSolver::getVariablesCount()
{
	return kissat_inc_get_maxVar(this->solver);
}

// Get a variable suitable for search splitting
int
KissatINCSolver::getDivisionVariable()
{
	return (rand() % getVariablesCount()) + 1;
}

// Set initial phase for a given variable
void
KissatINCSolver::setPhase(const unsigned int var, const bool phase)
{
	/* set target, best and saved phases: best is used in walk */
	kissat_inc_set_phase(this->solver, var, (phase) ? 1 : -1);
}

// Bump activity for a given variable
void
KissatINCSolver::bumpVariableActivity(const int var, const int times)
{
	LOGERROR("Not Implemented, yet");
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void
KissatINCSolver::setSolverInterrupt()
{
	stopSolver = true;
	kissat_inc_terminate(this->solver);
	LOGDEBUG1("Asking KissatINC (%d, %u) to end", this->getSolverId(), this->getSolverTypeId());
}

void
KissatINCSolver::unsetSolverInterrupt()
{
	stopSolver = false;
}

void
KissatINCSolver::setBumpVar(int v)
{
	bump_var = v;
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatResult
KissatINCSolver::solve(const std::vector<int>& cube)
{
	if (!this->isInitialized()) {
		LOGWARN("KissatInc %d was not initialized to be launched!", this->getSolverId());
		return SatResult::UNKNOWN;
	}
	unsetSolverInterrupt();

	// Ugly fix, to be enhanced.
	if (kissat_inc_check_searches(this->solver)) {
		LOGERROR("KissatInc solver %d was asked to solve more than once !!", this->getSolverId());
		exit(PERR_NOT_SUPPORTED);
	}

	for (int lit : cube) {
		// kissat_inc_set_value(this->solver, lit);
		this->setPhase(std::abs(lit), (lit > 0));
	}

	int res = kissat_inc_solve(solver);

	if (res == 10) {
		LOG2("KissatInc %d responded with SAT", this->getSolverId());
		kissat_inc_check_model(solver);
		return SatResult::SAT;
	}
	if (res == 20) {
		LOG2("KissatInc %d responded with UNSAT", this->getSolverId());
		return SatResult::UNSAT;
	}
	LOGDEBUG2("KissatInc %d responded with %d (UNKNOWN)", this->getSolverId(), res);
	return SatResult::UNKNOWN;
}

void
KissatINCSolver::addClause(ClauseExchangePtr clause)
{
	unsigned int maxVar = kissat_inc_get_maxVar(this->solver);
	for (int lit : clause->lits) {
		if (std::abs(lit) > maxVar) {
			LOGERROR("[KissatInc %d] literal %d is out of bound, maxVar is %d", this->getSolverId(), lit, maxVar);
			return;
		}
	}

	for (int lit : clause->lits)
		kissat_inc_add(this->solver, lit);
	kissat_inc_add(this->solver, 0);
}

bool
KissatINCSolver::importClause(const ClauseExchangePtr& clause)
{
	m_clausesToImport->addClause(clause);
	return true;
}

void
KissatINCSolver::importClauses(const std::vector<ClauseExchangePtr>& clauses)
{
	for (auto cls : clauses) {
		importClause(cls);
	}
}

void
KissatINCSolver::addClauses(const std::vector<ClauseExchangePtr>& clauses)
{
	for (auto clause : clauses)
		this->addClause(clause);
}

void
KissatINCSolver::addInitialClauses(const std::vector<simpleClause>& clauses, unsigned int nbVars)
{
	kissat_inc_set_maxVar(this->solver, nbVars);
	kissat_inc_reserve(this->solver, nbVars);

	for (auto& clause : clauses) {
		for (int lit : clause)
			kissat_inc_add(this->solver, lit);
		kissat_inc_add(this->solver, 0);
	}
	this->setInitialized(true);
	LOG2("The KissatInc Solver %d loaded all the %u clauses with %u variables",
		 this->getSolverId(),
		 clauses.size(),
		 nbVars);
}

void
KissatINCSolver::addInitialClauses(const lit_t* literals, unsigned int clsCount, unsigned int nbVars)
{
	kissat_inc_set_maxVar(this->solver, nbVars);
	kissat_inc_reserve(this->solver, nbVars);

	unsigned int clausesCount = 0;

	int lit;
	for (lit = *literals; clausesCount < clsCount; literals++, lit = *literals) {
		kissat_inc_add(this->solver, lit);
		if (!lit)
			clausesCount++;
	}

	this->setInitialized(true);

	LOG2("KissatInc %d loaded all the %d clauses with %u variables", this->getSolverId(), clausesCount, nbVars);
}

void
KissatINCSolver::printStatistics()
{
	LOGERROR("Not implemented yet !!");
}

std::vector<int>
KissatINCSolver::getModel()
{
	std::vector<int> model;
	unsigned int maxVar = kissat_inc_get_maxVar(this->solver);

	for (unsigned int i = 1; i <= maxVar; i++) {
		int tmp = kissat_inc_value(solver, i);
		if (!tmp)
			tmp = i;
		model.push_back(tmp);
	}

	return model;
}

std::vector<int>
KissatINCSolver::getFinalAnalysis()
{
	std::vector<int> outCls;
	return outCls;
}

std::vector<int>
KissatINCSolver::getSatAssumptions()
{
	std::vector<int> outCls;
	return outCls;
};

void
KissatINCSolver::diversify(const SeedGenerator& getSeed)
{
	if (this->isInitialized()) {
		LOGERROR("Diversification must be done before adding clauses because "
				 "of kissat_inc_reserve()");
		exit(PERR_NOT_SUPPORTED);
	}
	int i = this->getSolverTypeId(); /* prs do not have typeId, they used the general id*/
	// kissat_inc_set_option(this->solver, "seed", i);
	kissat_inc_set_option(this->solver, "check", 0);
	kissat_inc_set_option(this->solver, "quiet", 1);

	if (family == KissatFamily::SAT_STABLE) {
		/* shuffle wasn't used, thus I didn't copy it*/
		kissat_inc_set_option(this->solver, "stable", 1);
		kissat_inc_set_option(this->solver, "target", 2);
		kissat_inc_set_option(this->solver, "phase", 1);

		if (i == 2)
			kissat_inc_set_option(this->solver, "stable", 0);
		else if (i == 6)
			kissat_inc_set_option(this->solver, "stable", 2);

		if (i == 7)
			kissat_inc_set_option(this->solver, "target", 0);
		else if (i == 0 || i == 2 || i == 3 || i == 6)
			kissat_inc_set_option(this->solver, "target", 1);

		if (i == 3 || i == 11 || i == 12 || i == 13 || i == 15)
			kissat_inc_set_option(this->solver, "phase", 0);

	} else if (family == KissatFamily::UNSAT_FOCUSED) {
		kissat_inc_set_option(this->solver, "chrono", 1);
		kissat_inc_set_option(this->solver, "stable", 0);
		kissat_inc_set_option(this->solver, "target", 1);
		if (i == 0 || i == 1 || i == 7 || i == 8 || i == 11 || i == 15)
			if (i == 3 || i == 6 || i == 8 || i == 11 || i == 12 || i == 13)
				kissat_inc_set_option(this->solver, "chrono", 0);
		if (i == 2)
			kissat_inc_set_option(this->solver, "stable", 1);
		if (i == 9)
			kissat_inc_set_option(this->solver, "target", 0);
		else if (i == 14)
			kissat_inc_set_option(this->solver, "target", 2);

	} else {
		/* shuffle wasn't used, thus I didn't copy it*/
		if (i == 13 || i == 14 || i == 9)
			kissat_inc_set_option(this->solver, "tier1", 3);
		else
			kissat_inc_set_option(this->solver, "tier1", 2);
		if (i == 3 || i == 6 || i == 8 || i == 11 || i == 12 || i == 13 || i == 14 || i == 15)
			kissat_inc_set_option(this->solver, "chrono", 0);
		else
			kissat_inc_set_option(this->solver, "chrono", 1);

		if (i == 2 || i == 15)
			kissat_inc_set_option(this->solver, "stable", 0);
		else if (i == 6 || i == 14)
			kissat_inc_set_option(this->solver, "stable", 2);
		else
			kissat_inc_set_option(this->solver, "stable", 1);

		if (i == 10)
			kissat_inc_set_option(this->solver, "walkinitially", 1);
		else
			kissat_inc_set_option(this->solver, "walkinitially", 0);

		if (i == 7 || i == 8 || i == 9 || i == 11)
			kissat_inc_set_option(this->solver, "target", 0);
		else if (i == 0 || i == 2 || i == 3 || i == 4 || i == 5 || i == 6 || i == 10)
			kissat_inc_set_option(this->solver, "target", 1);
		else
			kissat_inc_set_option(this->solver, "target", 2);

		if (i == 4 || i == 5 || i == 8 || i == 9 || i == 12 || i == 13 || i == 15)
			kissat_inc_set_option(this->solver, "phase", 0);
		else
			kissat_inc_set_option(this->solver, "phase", 1);
	}

	// ReInit Inc options
	kissat_inc_mabvars_init(solver);
	LOG2("Diversification of KissatInc (%d,%u) of family %d",
		 this->getSolverId(),
		 this->getSolverTypeId(),
		 this->family);
}

void
KissatINCSolver::printWinningLog()
{
	this->SolverCdclInterface::printWinningLog();
	int family = static_cast<int>(this->family);
	LOGSTAT("The winner is KissatInc(%d, %u) of family %s",
			this->getSolverId(),
			this->getSolverTypeId(),
			(family) ? (family == 1) ? "MIXED_SWITCH" : "UNSAT_FOCUSED" : "SAT_STABLE");
}