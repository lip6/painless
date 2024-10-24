#include "Lingeling.hpp"
#include "utils/Parameters.hpp"

#include <cassert>
#include <iomanip>

// Include lingeling
extern "C"
{
#include "lingeling/lglib.h"
}

#include <ctype.h>

int
termCallback(void* solverPtr)
{
	Lingeling* lp = (Lingeling*)solverPtr;
	return lp->stopSolver;
}

void
produceUnit(void* sp, int lit)
{
	Lingeling* lp = (Lingeling*)sp;

	// Create new clause
	ClauseExchangePtr ncls = ClauseExchange::create(1, 0, lp->getSharingId());

	ncls->lits[0] = lit;

	LOGDEBUG2("Lingeling %u produced a unit : %s", lp->getSolverTypeId(), ncls->toString().c_str());

	/* filtering defined by a sharing strategy */
	lp->exportClause(ncls);
}

void
produce(void* sp, int* cls, int glue)
{
	// If unit clause, call produceUnit
	if (cls[1] == 0) {
		produceUnit(sp, cls[0]);
		return;
	}

	Lingeling* lp = (Lingeling*)sp;

	// Create new clause
	int size = 0;
	while (cls[size] != 0) {
		size++;
	}

	ClauseExchangePtr ncls = ClauseExchange::create(size, glue, lp->getSharingId());

	memcpy(ncls->lits, cls, sizeof(int) * size);

	LOGDEBUG2("Lingeling %u produced: %s", lp->getSolverTypeId(), ncls->toString().c_str());

	/* filtering defined by a sharing strategy */
	lp->exportClause(ncls);
}

void
consumeUnits(void* sp, int** start, int** end)
{
	Lingeling* lp = (Lingeling*)sp;

	// used a vector to a get a constant state of the units lockfree queue (to check if worth it)
	std::vector<int> tmp;

	lp->unitsToImport.consume_all([&tmp](int unit) { tmp.push_back(unit); });

	LOGDEBUG2("Lingeling %u will assign %u units", lp->getSolverTypeId(), tmp.size());

	if (tmp.empty()) {
		*start = lp->unitsBuffer;
		*end = lp->unitsBuffer;
		return;
	}

	if (tmp.size() >= lp->unitsBufferSize) {
		lp->unitsBufferSize = 1.6 * tmp.size();
		lp->unitsBuffer = (int*)realloc((void*)lp->unitsBuffer, lp->unitsBufferSize * sizeof(int));
	}

	for (size_t i = 0; i < tmp.size(); i++) {
		lp->unitsBuffer[i] = tmp[i];
	}

	*start = lp->unitsBuffer;
	*end = *start + tmp.size();
}

void
consumeCls(void* sp, int** clause, int* glue)
{
	Lingeling* lp = (Lingeling*)sp;

	ClauseExchangePtr cls;

	if (lp->m_clausesToImport->getOneClause(cls) == false) {
		*clause = NULL;
		lp->m_clausesToImport->shrinkDatabase();
		return;
	}

	assert((cls->size > 1 && cls->lbd > 0) || (cls->size == 1 && cls->lbd >= 0));

	if (cls->size + 1 >= lp->clsBufferSize) {
		lp->clsBufferSize = 1.6 * cls->size;
		lp->clsBuffer = (int*)realloc((void*)lp->clsBuffer, lp->clsBufferSize * sizeof(int));
	}

	LOGDEBUG2("Lingeling %u will import: %s", lp->getSolverTypeId(), cls->toString().c_str());

	*glue = cls->lbd;

	memcpy(lp->clsBuffer, cls->lits, sizeof(int) * cls->size);
	lp->clsBuffer[cls->size] = 0;

	*clause = lp->clsBuffer;
}

void
Lingeling::initialize()
{
	// BCA has to be disabled for valid clause sharing (or freeze all literals)
	lglsetopt(solver, "bca", 0);
	// Based on mallob suggestion
	// Sync (i.e., export) unit clauses more frequently
	lglsetopt(solver, "syncunint", 11111); // down from 111'111

	stopSolver = false;
	unitsBufferSize = clsBufferSize = 100;
	unitsBuffer = (int*)malloc(unitsBufferSize * sizeof(int));
	clsBuffer = (int*)malloc(clsBufferSize * sizeof(int));
	lglsetproducecls(solver, produce, this);
	lglsetproduceunit(solver, produceUnit, this);
	lglsetconsumeunits(solver, consumeUnits, this);
	lglsetconsumecls(solver, consumeCls, this);
	lglseterm(solver, termCallback, this);
	initializeTypeId<Lingeling>();
}

Lingeling::Lingeling(int id, const std::shared_ptr<ClauseDatabase>& clauseDB)
	: unitsToImport(256)
	, clausesToAdd(__globalParameters__.defaultClauseBufferSize)
	, SolverCdclInterface(id, clauseDB, SolverCdclType::LINGELING)
{
	solver = lglinit();
	initialize();
}

Lingeling::Lingeling(const Lingeling& other, int id, const std::shared_ptr<ClauseDatabase>& clauseDB)
	: unitsToImport(256)
	, clausesToAdd(__globalParameters__.defaultClauseBufferSize)
	, SolverCdclInterface(id, clauseDB, SolverCdclType::LINGELING)
{
	solver = lglclone(other.solver);
	initialize();
}

Lingeling::~Lingeling()
{
	lglrelease(solver);

	free(unitsBuffer);
	free(clsBuffer);
}

void
Lingeling::loadFormula(const char* filename)
{
	std::vector<simpleClause> initClauses;
	unsigned int varCount = 0;
	Parsers::parseCNF(filename, initClauses, &varCount);

	this->addInitialClauses(initClauses, varCount);

	lglsimp(solver, 10);
}

unsigned int
Lingeling::getVariablesCount()
{
	return lglnvars(solver);
}

// Get a variable suitable for search splitting
int
Lingeling::getDivisionVariable()
{
	lglsimp(solver, 1);

	int oldjwhred = lglgetopt(solver, "jwhred");

	int lit = lglookahead(solver);

	lglsetopt(solver, "jwhred", oldjwhred);

	return lit;
}

// Set initial phase for a given variable
void
Lingeling::setPhase(const unsigned int var, const bool phase)
{
	lglsetphase(solver, phase ? var : -var);
}

// Bump activity for a given variable
void
Lingeling::bumpVariableActivity(const int lit, const int times)
{
	for (int i = times; i < times; i++) {
		// TODO: is it good ?
		lglsetimportant(solver, lit);
	}
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void
Lingeling::setSolverInterrupt()
{
	LOGDEBUG1("Asking Lingeling (%d, %u) to end", this->getSolverId(), this->getSolverTypeId());
	stopSolver = true;
}

void
Lingeling::unsetSolverInterrupt()
{
	stopSolver = false;
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatResult
Lingeling::solve(const std::vector<int>& cube)
{
	// add the clauses
	std::vector<ClauseExchangePtr> tmp;
	clausesToAdd.getClauses(tmp);

	for (size_t i = 0; i < tmp.size(); i++) {
		for (size_t j = 0; j < tmp[i]->size; j++) {
			lgladd(solver, tmp[i]->lits[j]);
		}

		lgladd(solver, 0);
	}

	// set the assumptions
	for (size_t i = 0; i < cube.size(); i++) {
		// freezing problems ???
		if (lglusable(solver, cube[i])) {
			lglassume(solver, cube[i]);
		}
	}

	// Simplify the problem
	int res = lglsimp(solver, 0);

	switch (res) {

		case LGL_SATISFIABLE:
			LOG2("Lingeling Simp %d responded with SAT", this->getSolverId());
			return SatResult::SAT;
		case LGL_UNSATISFIABLE:
			LOG2("Lingeling Simp %d responded with UNSAT", this->getSolverId());
			return SatResult::UNSAT;
	}

	// Solve the problem
	res = lglsat(solver);

	switch (res) {
		case LGL_SATISFIABLE:
			LOG2("Lingeling Sat %d responded with SAT", this->getSolverId());
			return SatResult::SAT;
		case LGL_UNSATISFIABLE:
			LOG2("Lingeling Sat %d responded with UNSAT", this->getSolverId());
			return SatResult::UNSAT;
	}

	return SatResult::UNKNOWN;
}

// Add a permanent clause to the formula
void
Lingeling::addClause(ClauseExchangePtr clause)
{
	clausesToAdd.addClause(clause);
}

void
Lingeling::addClauses(const std::vector<ClauseExchangePtr>& clauses)
{
	clausesToAdd.addClauses(clauses);
}

void
Lingeling::addInitialClauses(const std::vector<simpleClause>& clauses, unsigned int nbVars)
{
	for (size_t i = 0; i < clauses.size(); i++) {
		for (size_t j = 0; j < clauses[i].size(); j++) {
			lgladd(solver, clauses[i][j]);
		}

		lgladd(solver, 0);
	}
	LOG2("The Lingeling Solver %d loaded all the %u clauses with %u variables",
		 this->getSolverId(),
		 clauses.size(),
		 nbVars);
}

// Add a learned clause to the formula
bool
Lingeling::importClause(const ClauseExchangePtr& clause)
{
	assert(clause->size > 0);

	if (clause->size == 1) {
		unitsToImport.push(clause->lits[0]);
	} else {
		m_clausesToImport->addClause(clause);
	}

	return true;
}

void
Lingeling::importClauses(const std::vector<ClauseExchangePtr>& clauses)
{
	for (size_t i = 0; i < clauses.size(); i++) {
		if (clauses[i]->size == 1) {
			unitsToImport.push(clauses[i]->lits[0]);
		} else {
			m_clausesToImport->addClause(clauses[i]);
		}
	}
}

void
Lingeling::printWinningLog()
{
	this->SolverCdclInterface::printWinningLog();
	LOGSTAT("The winner is Lingeling(%d, %u) ", this->getSolverId(), this->getSolverTypeId());
}

void
Lingeling::printStatistics()
{
	SolvingCdclStatistics stats;

	stats.conflicts = lglgetconfs(solver);
	stats.decisions = lglgetdecs(solver);
	stats.propagations = lglgetprops(solver);
	stats.memPeak = lglmaxmb(solver);
	stats.restarts = lglgetrestarts(solver);

	std::cout << std::left << std::setw(15) << ("| L" + std::to_string(this->getSolverTypeId())) << std::setw(20)
			  << ("| " + std::to_string(stats.conflicts)) << std::setw(20)
			  << ("| " + std::to_string(stats.propagations)) << std::setw(17) << ("| " + std::to_string(stats.restarts))
			  << std::setw(20) << ("| " + std::to_string(stats.decisions)) << std::setw(20) << "|"
			  << "\n";
}

void
Lingeling::diversify(const SeedGenerator& getSeed)
{
	int id = this->getSolverTypeId();
	lglsetopt(solver, "seed", id);
	lglsetopt(solver, "classify", 0);
	lglsetopt(solver, "phase", (id % 2) ? 1 : -1);
	if (0 == id) {
		// Local Searcher Yalsat only
		lglsetopt(solver, "plain", 1);
		lglsetopt(solver, "locs", -1);
		lglsetopt(solver, "locsrtc", 1);
		lglsetopt(solver, "locswait", 0);
		lglsetopt(solver, "locsclim", (1 << 24));
	} else {
		switch (id % 10) {
			case 0:
				lglsetopt(solver, "gluescale", 5);
				break; // from 3 (value "ld" moved)
			case 1:
				lglsetopt(solver, "plain", 1);
				lglsetopt(solver, "decompose", 1);
				break;
			case 2:
				lglsetopt(solver, "restartint", 100);
				break;
			case 3:
				lglsetopt(solver, "sweeprtc", 1);
				break;
			case 4:
				lglsetopt(solver, "restartint", 1000);
				break;
			case 5:
				lglsetopt(solver, "scincinc", 50);
				break;
			case 6:
				lglsetopt(solver, "restartint", 4);
				break;
			case 7:
				lglsetopt(solver, "phase", 1);
				break;
			case 8:
				lglsetopt(solver, "phase", -1);
				break;
			case 9:
				lglsetopt(solver, "block", 0);
				lglsetopt(solver, "cce", 0);
				break;
		}
	}

	// lglsetopt(solver, "restartint", 2 + (id % 10));

	// 	// Special Configurations
	// 	switch (id % 8) {
	// 		case 0:
	// 			lglsetopt(solver, "gluescale", 5);
	// 			break;
	// 		case 1:
	// 			lglsetopt(solver, "plain", 1);
	// 			lglsetopt(solver, "decompose", 1);
	// 			break;
	// 		case 2:
	// 			lglsetopt(solver, "block", 0);
	// 			lglsetopt(solver, "cce", 0);
	// 			break;
	// 		case 3:
	// 			lglsetopt(solver, "sweeprtc", 1);
	// 			break;
	// 		case 4:
	// 			lglsetopt(solver, "restartint", 100 + (id % 10));
	// 			break;
	// 		case 5:
	// 			lglsetopt(solver, "scincinc", 50);
	// 			break;
	// 		case 6:
	// 			lglsetopt(solver, "restartint", 4);
	// 			break;
	// 		case 7:
	// 			lglsetopt(solver, "restartint", 200 + (id % 20));
	// 			break;
	// 	}
	// }
}

std::vector<int>
Lingeling::getModel()
{
	std::vector<int> model;
	for (int i = 1; i <= lglmaxvar(solver); i++) {
		int lit = (lglderef(solver, i) > 0) ? i : -i;
		model.push_back(lit);
	}

	return model;
}

std::vector<int>
Lingeling::getFinalAnalysis()
{
	std::vector<int> outCls;
	LOGERROR("NOT IMPLEMENTED");
	return outCls;
}

std::vector<int>
Lingeling::getSatAssumptions()
{
	std::vector<int> outCls;
	LOGERROR("NOT IMPLEMENTED");
	return outCls;
};