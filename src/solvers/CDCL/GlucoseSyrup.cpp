// Glucose includes
#include "core/Dimacs.h"
#include "parallel/ParallelSolver.h"

#include "glucose/utils/System.h"

#include "GlucoseSyrup.hpp"
#include "utils/Parameters.hpp"

#include "containers/ClauseDatabaseSingleBuffer.hpp"

#include <iomanip>

using namespace Glucose;

// Macros to converte glucose literals
#define GLUE_LIT(lit) lit > 0 ? mkLit(lit - 1, false) : mkLit((-lit) - 1, true)

#define INT_LIT(lit) sign(lit) ? -(var(lit) + 1) : (var(lit) + 1)

void
makeGlueVec(ClauseExchangePtr cls, Glucose::vec<Lit>& gcls)
{
	for (size_t i = 0; i < cls->size; i++) {
		gcls.push(GLUE_LIT(cls->lits[i]));
	}
}

void
glucoseExportUnary(void* issuer, Lit& l)
{
	GlucoseSyrup* gs = (GlucoseSyrup*)issuer;

	ClauseExchangePtr ncls = ClauseExchange::create(1);

	ncls->from = gs->getSharingId();
	ncls->lbd = 1;
	ncls->lits[0] = INT_LIT(l);

	/* filtering defined by a sharing strategy */
	gs->exportClause(ncls);
}

void
glucoseExportClause(void* issuer, Clause& cls)
{
	GlucoseSyrup* gs = (GlucoseSyrup*)issuer;

	ClauseExchangePtr ncls = ClauseExchange::create(cls.size(), cls.lbd(), gs->getSharingId());

	for (unsigned int i = 0; i < cls.size(); i++) {
		ncls->lits[i] = INT_LIT(cls[i]);
	}

	/* filtering defined by a sharing strategy */
	gs->exportClause(ncls);
}

Lit
glucoseImportUnary(void* issuer)
{
	GlucoseSyrup* gs = (GlucoseSyrup*)issuer;

	Lit l = lit_Undef;

	ClauseExchangePtr cls;

	if (gs->unitsToImport->getOneClause(cls) == false)
		return l;

	l = GLUE_LIT(cls->lits[0]);

	return l;
}

bool
glucoseImportClause(void* issuer, int* from, vec<Lit>& gcls)
{
	GlucoseSyrup* gs = (GlucoseSyrup*)issuer;

	ClauseExchangePtr cls;

	if (gs->m_clausesToImport->getOneClause(cls) == false) {
		gs->m_clausesToImport->shrinkDatabase();
		return false;
	}

	makeGlueVec(cls, gcls);

	*from = cls->from;

	return true;
}

GlucoseSyrup::GlucoseSyrup(int id, const std::shared_ptr<ClauseDatabase>& clauseDB)
	: SolverCdclInterface(id, clauseDB, SolverCdclType::GLUCOSE)
	, clausesToAdd(__globalParameters__.defaultClauseBufferSize)
{
	/* use sharing id to not have the assert(importedFromThread != thn) fail */
	solver = new ParallelSolver(this->getSharingId());

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
	solver = new ParallelSolver(*(other.solver), this->getSharingId());

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
	Lit res;

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
		vec<Lit> gcls;
		makeGlueVec(tmp[i], gcls);

		if (solver->addClause(gcls) == false) {
			printf("unsat when adding cls\n");
			return SatResult::UNSAT;
		}
	}

	vec<Lit> gAssumptions;
	for (unsigned int i = 0; i < cube.size(); i++) {
		Lit l = GLUE_LIT(cube[i]);
		if (!solver->isEliminated(var(l))) {
			gAssumptions.push(l);
		}
	}

	lbool res = solver->solveLimited(gAssumptions);

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
	for (size_t ind = 0; ind < clauses.size(); ind++) {
		vec<Lit> mcls;

		for (size_t i = 0; i < clauses[ind].size(); i++) {
			int lit = clauses[ind][i];
			int var = abs(lit);

			while (solver->nVars() < nbVars) {
				solver->newVar();
			}

			mcls.push(GLUE_LIT(lit));
		}

		if (!solver->addClause(mcls)) {
			printf("unsat when adding initial cls\n");
		}
	}
	LOG2("The Glucose Solver %d loaded all the %u clauses with %u variables",
		 this->getSolverId(),
		 clauses.size(),
		 nbVars);
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