// Glucose includes
#include "GlucoseSyrup.hpp"

#include <glucose/core/Dimacs.h>
#include <glucose/parallel/ParallelSolver.h>
#include <glucose/utils/System.h>

#include "containers/ClauseDatabases/ClauseDatabaseSingleBuffer.hpp"
#include "utils/Parameters.hpp"

#include <iomanip>

// Macros to converte glucose literals
#define GLUE_LIT(lit) lit > 0 ? Glucose::mkLit(lit - 1, false) : Glucose::mkLit((-lit) - 1, true)

#define INT_LIT(lit) Glucose::sign(lit) ? -(Glucose::var(lit) + 1) : (Glucose::var(lit) + 1)

static bool
checkLiteral(const Glucose::Lit& l, Glucose::Solver* glucose)
{
	// Assert if not negative (error)
	assert(l.x >= 0);
	// Check if the literal is not out of bound
	if (glucose->nVars() <= var(l)) {
		LOGDEBUG2("Literal %d is out of bound for Glucose", l.x);
		return false;
	}
	return true;
}

bool
makeGlueVec(ClauseExchangePtr cls, Glucose::vec<Glucose::Lit>& gcls, Glucose::Solver* glucose)
{
	for (size_t i = 0; i < cls->size; i++) {
		Glucose::Lit internalLit = GLUE_LIT(cls->lits[i]);
		gcls.push(internalLit);
		// Checks
		if (!checkLiteral(internalLit, glucose))
			return false;
	}

	return true;
}

void
glucoseExportUnary(void* issuer, Glucose::Lit& l)
{
	GlucoseSyrup* gs = (GlucoseSyrup*)issuer;

	ClauseExchangePtr ncls = ClauseExchange::create(1, 1, gs->getSharingId());

	ncls->lits[0] = INT_LIT(l);

	/* filtering defined by a sharing strategy */
	gs->exportClause(ncls);
}

void
glucoseExportClause(void* issuer, Glucose::Clause& cls)
{
	GlucoseSyrup* gs = (GlucoseSyrup*)issuer;

	ClauseExchangePtr ncls = ClauseExchange::create(cls.size(), cls.lbd(), gs->getSharingId());

	for (unsigned int i = 0; i < cls.size(); i++) {
		ncls->lits[i] = INT_LIT(cls[i]);
	}

	/* filtering defined by a sharing strategy */
	gs->exportClause(ncls);
}

Glucose::Lit
glucoseImportUnary(void* issuer)
{
	GlucoseSyrup* gs = (GlucoseSyrup*)issuer;

	Glucose::Lit l;

	ClauseExchangePtr cls;

	while (gs->unitsToImport->getOneClause(cls)) {
		assert(cls->size == 1);
		l = GLUE_LIT(cls->lits[0]);

		if (checkLiteral(l, gs->solver) == true) {
			assert(l.x >= 0);
			LOGDEBUG2("Importing to Glucose %u unit literal %d", gs->getSharingId(), l.x);
			return l;
		}
	}
	return Glucose::lit_Undef;
}

bool
glucoseImportClause(void* issuer, int* from, int* lbd, Glucose::vec<Glucose::Lit>& gcls)
{
	GlucoseSyrup* gs = (GlucoseSyrup*)issuer;

	ClauseExchangePtr cls;

	while (gs->m_clausesToImport->getOneClause(cls)) {
		if (makeGlueVec(cls, gcls, gs->solver)) {
			*from = cls->from;
			*lbd = cls->lbd;

			LOGCLAUSE1(cls->lits, cls->size, "Importing to Glucose %u clause: ", gs->getSharingId());
			assert(gcls.size() > 1 && gcls[0].x >= 0);

			return true;
		}
		gcls.clear();
	}

	gs->m_clausesToImport->shrinkDatabase();
	return false;
}

GlucoseSyrup::GlucoseSyrup(int id, const std::shared_ptr<ClauseDatabase>& clauseDB)
	: SolverCdclInterface(id, clauseDB, SolverCdclType::GLUCOSE)
	, clausesToAdd(__globalParameters__.defaultClauseBufferSize)
{
	/* use sharing id to not have the assert(importedFromThread != thn) fail */
	solver = new Glucose::ParallelSolver(this->getSharingId());

	this->unitsToImport = std::make_unique<ClauseDatabaseSingleBuffer>(__globalParameters__.defaultClauseBufferSize);

	switch (__globalParameters__.glucoseSplitHeuristic) {
		case 2:
			solver->useFlip = true;
			break;
		case 3:
			solver->usePR = true;
			break;
		default:;
	}

	solver->exportUnary = glucoseExportUnary;
	solver->exportClause = glucoseExportClause;
	solver->importUnary = glucoseImportUnary;
	solver->importClause = glucoseImportClause;
	solver->issuer = this;

	initializeTypeId<GlucoseSyrup>();
}

GlucoseSyrup::GlucoseSyrup(const GlucoseSyrup& other, int id, const std::shared_ptr<ClauseDatabase>& clauseDB)
	: SolverCdclInterface(id, clauseDB, SolverCdclType::GLUCOSE)
	, clausesToAdd(__globalParameters__.defaultClauseBufferSize)
{
	solver = new Glucose::ParallelSolver(*(other.solver), this->getSharingId());

	this->unitsToImport = std::make_unique<ClauseDatabaseSingleBuffer>(__globalParameters__.defaultClauseBufferSize);

	switch (__globalParameters__.glucoseSplitHeuristic) {
		case 2:
			solver->useFlip = true;
			break;
		case 3:
			solver->usePR = true;
			break;
		default:;
	}

	solver->exportUnary = glucoseExportUnary;
	solver->exportClause = glucoseExportClause;
	solver->importUnary = glucoseImportUnary;
	solver->importClause = glucoseImportClause;
	solver->issuer = this;

	initializeTypeId<GlucoseSyrup>();
}

GlucoseSyrup::~GlucoseSyrup()
{
	std::vector<ClauseExchangePtr> tmp;

	unitsToImport->getClauses(tmp);
	m_clausesToImport->getClauses(tmp);
	clausesToAdd.getClauses(tmp);

	//   for(size_t i=0;i<tmp.size();i++){
	//     if(tmp[i]==NULL){ printf("NAON :'(\n");abort();}
	//     ClauseManager::releaseClause(tmp[i]);
	//     }
	delete solver;
}

void
GlucoseSyrup::loadFormula(const char* filename)
{
	gzFile in = gzopen(filename, "rb");

	parse_DIMACS(in, *solver);

	gzclose(in);
}

// Get the number of variables of the formula
unsigned int
GlucoseSyrup::getVariablesCount()
{
	return solver->nVars();
}

// Get a variable suitable for search splitting
int
GlucoseSyrup::getDivisionVariable()
{
	Glucose::Lit res;

	switch (__globalParameters__.glucoseSplitHeuristic) {
		case 2:
			res = solver->pickBranchLitUsingFlipActivity();
			break;
		case 3:
			res = solver->pickBranchLitUsingPropagationRate();
			break;
		default:
			res = solver->pickBranchLit();
	}

	return INT_LIT(res);
}

// Set initial phase for a given variable
void
GlucoseSyrup::setPhase(const unsigned int var, const bool phase)
{
	solver->setPolarity(var - 1, phase);
}

// Bump activity for a given variable
void
GlucoseSyrup::bumpVariableActivity(const int var, const int times)
{
	for (int i = 0; i < times; i++) {
		solver->varBumpActivity(var - 1);
	}
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void
GlucoseSyrup::setSolverInterrupt()
{
	LOGDEBUG1("Asking Glucose %d to end.", this->getSolverId());
	solver->interrupt();
}

// Diversify the solver
void
GlucoseSyrup::diversify(const SeedGenerator& getSeed)
{
	unsigned int id = this->getSolverTypeId();
	int idMod = id ? id <= 9 : id % 8;

	switch (idMod) {
		case 1:
			solver->var_decay = 0.94;
			solver->max_var_decay = 0.96;
			solver->firstReduceDB = 600;
			break;

		case 2:
			solver->var_decay = 0.90;
			solver->max_var_decay = 0.97;
			solver->firstReduceDB = 500;
			break;

		case 3:
			solver->var_decay = 0.85;
			solver->max_var_decay = 0.93;
			solver->firstReduceDB = 400;
			break;

		case 4:
			solver->var_decay = 0.95;
			solver->max_var_decay = 0.95;
			solver->firstReduceDB = 4000;
			solver->sizeLBDQueue = 100;
			solver->K = 0.7;
			solver->incReduceDB = 500;
			solver->lbdQueue.growTo(100);
			break;

		case 5:
			solver->var_decay = 0.93;
			solver->max_var_decay = 0.96;
			solver->firstReduceDB = 100;
			solver->incReduceDB = 500;
			break;

		case 6:
			solver->var_decay = 0.75;
			solver->max_var_decay = 0.94;
			solver->firstReduceDB = 2000;
			break;

		case 7:
			solver->var_decay = 0.94;
			solver->max_var_decay = 0.96;
			solver->firstReduceDB = 800;
			break;

		case 8:
			solver->reduceOnSize = true;
			break;

		case 9:
			solver->reduceOnSize = true;
			solver->reduceOnSizeSize = 14;
			break;

		case 0:
		default:
			break;
	}

	if (id > 9) {
		int noiseFactor = id / 8;
		double noisevar_decay = 0.005 + noiseFactor * 0.006;
		int noiseReduceDB = 50 + noiseFactor * 25;

		solver->var_decay += noisevar_decay;
		solver->firstReduceDB += noiseReduceDB;
	}
}

void
GlucoseSyrup::unsetSolverInterrupt()
{
	solver->clearInterrupt();
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatResult
GlucoseSyrup::solve(const std::vector<int>& cube)
{
	unsetSolverInterrupt();

	std::vector<ClauseExchangePtr> tmp;
	clausesToAdd.getClauses(tmp);

	for (size_t i = 0; i < tmp.size(); i++) {
		Glucose::vec<Glucose::Lit> gcls;
		makeGlueVec(tmp[i], gcls, this->solver);

		if (solver->addClause(gcls) == false) {
			LOGWARN("unsat when adding cls");
			return SatResult::UNSAT;
		}
	}

	Glucose::vec<Glucose::Lit> gAssumptions;
	for (unsigned int i = 0; i < cube.size(); i++) {
		Glucose::Lit l = GLUE_LIT(cube[i]);
		if (!solver->isEliminated(var(l))) {
			gAssumptions.push(l);
		}
	}

	Glucose::lbool res = solver->solveLimited(gAssumptions);

	if (res == l_True)
		return SatResult::SAT;

	if (res == l_False)
		return SatResult::UNSAT;

	return SatResult::UNKNOWN;
}

bool
GlucoseSyrup::importClause(const ClauseExchangePtr& clause)
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
GlucoseSyrup::importClauses(const std::vector<ClauseExchangePtr>& clauses)
{
	for (size_t i = 0; i < clauses.size(); i++) {
		if (clauses[i]->size == 1) {
			unitsToImport->addClause(clauses[i]);
		} else {
			m_clausesToImport->addClause(clauses[i]);
		}
	}
}

void
GlucoseSyrup::addClause(ClauseExchangePtr clause)
{
	clausesToAdd.addClause(clause);

	setSolverInterrupt();
}

void
GlucoseSyrup::addClauses(const std::vector<ClauseExchangePtr>& clauses)
{
	clausesToAdd.addClauses(clauses);

	setSolverInterrupt();
}

void
GlucoseSyrup::addInitialClauses(const std::vector<simpleClause>& clauses, unsigned int nbVars)
{
	while (solver->nVars() < nbVars) {
		solver->newVar();
	}

	for (size_t ind = 0; ind < clauses.size(); ind++) {
		Glucose::vec<Glucose::Lit> mcls;

		for (size_t i = 0; i < clauses[ind].size(); i++) {
			int lit = clauses[ind][i];
			int var = abs(lit);

			mcls.push(GLUE_LIT(lit));
		}

		if (!solver->addClause(mcls)) {
			LOGWARN("Unsat when adding initial cls\n");
			return;
		}
	}
	LOG2("The Glucose Solver %d loaded all the %u clauses with %u variables",
		 this->getSolverId(),
		 clauses.size(),
		 nbVars);
}

void
GlucoseSyrup::addInitialClauses(const lit_t* literals, unsigned int clsCount, unsigned int nbVars)
{
	unsigned int clausesCount = 0;
	int lit;
	for (unsigned int i = 0; clausesCount < clsCount; i++) {
		Glucose::vec<Glucose::Lit> mcls;

		while (solver->nVars() < nbVars) {
			solver->newVar();
		}

		for (lit = *literals; lit; literals++, lit = *literals) {
			if(!lit) break;
			mcls.push(GLUE_LIT(lit));
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
		"The Glucose Solver %d loaded all the %u clauses with %u variables", this->getSolverId(), clausesCount, nbVars);
}

void
GlucoseSyrup::printStatistics()
{
	SolvingCdclStatistics stats;

	stats.conflicts = solver->conflicts;
	stats.propagations = solver->propagations;
	stats.restarts = solver->starts;
	stats.decisions = solver->decisions;
	std::cout << std::left << std::setw(15) << ("| G" + std::to_string(this->getSolverTypeId())) << std::setw(20)
			  << ("| " + std::to_string(stats.conflicts)) << std::setw(20)
			  << ("| " + std::to_string(stats.propagations)) << std::setw(17) << ("| " + std::to_string(stats.restarts))
			  << std::setw(20) << ("| " + std::to_string(stats.decisions)) << std::setw(20) << "|"
			  << "\n";
}

void
GlucoseSyrup::printWinningLog()
{
	this->SolverCdclInterface::printWinningLog();
	LOGSTAT("The winner is GlucoseSyrup(%d, %u) ", this->getSolverId(), this->getSolverTypeId());
}

std::vector<int>
GlucoseSyrup::getModel()
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

void
GlucoseSyrup::getHeuristicData(std::vector<int>** flipActivity,
							   std::vector<int>** nbPropagations,
							   std::vector<int>** nbDecisionVar)
{
	if (flipActivity != NULL) {
		*flipActivity = &(solver->flipActivity);
	}
	if (nbPropagations != NULL) {
		*nbPropagations = &(solver->nbPropagations);
	}
	if (nbDecisionVar != NULL) {
		*nbDecisionVar = &(solver->nbDecisionVar);
	}
}

void
GlucoseSyrup::setHeuristicData(std::vector<int>* flipActivity,
							   std::vector<int>* nbPropagations,
							   std::vector<int>* nbDecisionVar)
{
	if (flipActivity != NULL) {
		solver->flipActivity = *flipActivity;
	}
	if (nbPropagations != NULL) {
		solver->nbPropagations = *nbPropagations;
	}
	if (nbDecisionVar != NULL) {
		solver->nbDecisionVar = *nbDecisionVar;
	}
}

std::vector<int>
GlucoseSyrup::getFinalAnalysis()
{
	std::vector<int> outCls;
	LOGERROR("NOT IMPLEMENTED");
	return outCls;
}

std::vector<int>
GlucoseSyrup::getSatAssumptions()
{
	std::vector<int> outCls;
	LOGERROR("NOT IMPLEMENTED");
	return outCls;
};