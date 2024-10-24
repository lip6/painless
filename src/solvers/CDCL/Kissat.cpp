#include "painless.hpp"
#include "sharing/Filters/BloomFilter.hpp"
#include "utils/ErrorCodes.hpp"
#include "utils/Logger.hpp"
#include "utils/MpiUtils.hpp"
#include "utils/Parameters.hpp"
#include "utils/Parsers.hpp"
#include "utils/System.hpp"

#include <cassert>
#include <iomanip>

#include "solvers/CDCL/Kissat.hpp"

// loadFormula includes
extern "C"
{
#include "kissat/src/parse.h"
}

int
kissatTerminate(void* solverPtr)
{
	Kissat* pkissat = (Kissat*)solverPtr;
	return pkissat->stopSolver;
}

char
kissatImportClause(void* painless_interface, kissat* internal_solver)
{
	Kissat* painless_kissat = (Kissat*)painless_interface;

	ClauseExchangePtr clause;

	if (!painless_kissat->m_clausesToImport->getOneClause(clause)) {
		painless_kissat->m_clausesToImport->shrinkDatabase();
		return false;
	}

	assert((clause->size > 1 && clause->lbd > 0) || (clause->size == 1 && clause->lbd >= 0));

	kissat_set_pglue(internal_solver, clause->lbd);

	return kissat_import_pclause(internal_solver, clause->lits, clause->size);
}

char
kissatExportClause(void* painless_interface, kissat* internal_solver)
{
	Kissat* painless_kissat = (Kissat*)painless_interface;

	unsigned int lbd = kissat_get_pglue(internal_solver);

	unsigned int size = kissat_pclause_size(internal_solver);

	assert(size > 0);

	ClauseExchangePtr new_clause = ClauseExchange::create(size, lbd, painless_kissat->getSharingId());

	for (unsigned int i = 0; i < size; i++) {
		new_clause->lits[i] = kissat_peek_plit(internal_solver, i);
	}

	/* filtering defined by a sharing strategy */
	if (painless_kissat->exportClause(new_clause)) {
		return true;
	} else
		return false;
}

Kissat::Kissat(int id, const std::shared_ptr<ClauseDatabase>& clauseDB)
	: SolverCdclInterface(id, clauseDB, SolverCdclType::KISSAT)
	, clausesToAdd(__globalParameters__.defaultClauseBufferSize)
{
	solver = kissat_init();

	family = KissatFamily::MIXED_SWITCH;

	// kissat_set_terminate(solver, this, kissatTerminate);
	kissat_set_export_call(solver, kissatExportClause);
	kissat_set_import_call(solver, kissatImportClause);
	kissat_set_import_unit_call(solver, nullptr); // kissatImportClause is enough
	kissat_set_painless(solver, this);
	kissat_set_id(solver, id);

	initKissatOptions(); /* Must be called before reserve or initshuffle */

	initializeTypeId<Kissat>();
}

Kissat::~Kissat()
{
	// kissat_print_sharing_stats(this->solver);
	kissat_release(solver);
}

void
Kissat::loadFormula(const char* filename)
{
	strictness strict = NORMAL_PARSING;
	file in;
	uint64_t lineno;
	kissat_open_to_read_file(&in, filename);

	kissat_parse_dimacs(solver, strict, &in, &lineno, (int*)&this->originalVars);

	kissat_close_file(&in);

	this->setInitialized(true);

	LOG2("The Kissat Solver %d loaded all the formula with %u variables", this->getSolverId(), originalVars);
}

// Get the number of variables of the formula
unsigned int
Kissat::getVariablesCount()
{
	return kissat_get_var_count(this->solver);
}

// Get a variable suitable for search splitting
int
Kissat::getDivisionVariable()
{
	return (rand() % getVariablesCount()) + 1;
}

// Set initial phase for a given variable
void
Kissat::setPhase(const unsigned int var, const bool phase)
{
	/* set target, best and saved phases: best is used in walk */
	kissat_set_phase(this->solver, var, (phase) ? 1 : -1);
}

// Bump activity for a given variable
void
Kissat::bumpVariableActivity(const int var, const int times)
{
	LOGERROR("Not Implemented, yet");
	exit(PERR_NOT_SUPPORTED);
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void
Kissat::setSolverInterrupt()
{
	stopSolver = true;
	kissat_terminate(this->solver);
	LOGDEBUG1("Asking Kissat (%d, %u) to end", this->getSolverId(), this->getSolverTypeId());
}

void
Kissat::unsetSolverInterrupt()
{
	stopSolver = false;
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatResult
Kissat::solve(const std::vector<int>& cube)
{
	if (!this->isInitialized()) {
		LOGWARN("Kissat %d was not initialized to be launched!", this->getSolverId());
		return SatResult::UNKNOWN;
	}
	unsetSolverInterrupt();

	if (kissat_check_searches(this->solver)) {
		LOGERROR("Kissat solver %d was asked to solve more than once !!", this->getSolverId());
		exit(PERR_NOT_SUPPORTED);
	}

	for (int lit : cube) {
		// kissat_set_value(this->solver, lit);
		this->setPhase(std::abs(lit), (lit > 0));
	}

	int res = kissat_solve(solver);

	if (res == 10) {
		LOG2("Kissat %d responded with SAT", this->getSolverId());
		return SatResult::SAT;
	}
	if (res == 20) {
		LOG2("Kissat %d responded with UNSAT", this->getSolverId());
		return SatResult::UNSAT;
	}
	LOGDEBUG2("Kissat %d responded with %d (UNKNOWN)", this->getSolverId(), res);
	return SatResult::UNKNOWN;
}

void
Kissat::addClause(ClauseExchangePtr clause)
{
	unsigned int maxVar = kissat_get_var_count(this->solver);
	for (int lit : clause->lits) {
		if (std::abs(lit) > maxVar) {
			LOGERROR("[Kissat %d] literal %d is out of bound, maxVar is %d", this->getSolverId(), lit, maxVar);
			return;
		}
	}

	for (int lit : clause->lits)
		kissat_add(this->solver, lit);
	kissat_add(this->solver, 0);
}

void
Kissat::addClauses(const std::vector<ClauseExchangePtr>& clauses)
{
	for (auto clause : clauses)
		this->addClause(clause);
}

void
Kissat::addInitialClauses(const std::vector<simpleClause>& clauses, unsigned int nbVars)
{
	kissat_reserve(this->solver, nbVars);

	this->originalVars = nbVars;

	for (auto& clause : clauses) {
		for (int lit : clause)
			kissat_add(this->solver, lit);
		kissat_add(this->solver, 0);
	}
	this->setInitialized(true);
	LOG2("The Kissat Solver %d loaded all the %u clauses with %u variables",
		 this->getSolverId(),
		 clauses.size(),
		 nbVars);
}

bool
Kissat::importClause(const ClauseExchangePtr& clause)
{
	assert(clause->size > 0);
	m_clausesToImport->addClause(clause);
	return true;
}

void
Kissat::importClauses(const std::vector<ClauseExchangePtr>& clauses)
{
	for (auto cls : clauses) {
		importClause(cls);
	}
}

void
Kissat::printStatistics()
{
	KissatMainStatistics kstats;

	kissat_get_main_statistics(this->solver, &kstats);

	std::cout << std::left << std::setw(15) << ("| K" + std::to_string(this->getSolverTypeId())) << std::setw(20)
			  << ("| " + std::to_string(kstats.conflictsPerSec)) << std::setw(20)
			  << ("| " + std::to_string(kstats.propagationsPerSec)) << std::setw(17)
			  << ("| " + std::to_string(kstats.restarts)) << std::setw(20)
			  << ("| " + std::to_string(kstats.decisionsPerConf)) << std::setw(20) << "|"
			  << "\n";

	// kissat_print_statistics(this->solver);
}

std::vector<int>
Kissat::getModel()
{
	std::vector<int> model;

	for (unsigned int i = 1; i <= originalVars; i++) {
		int tmp = kissat_value(solver, i);
		if (!tmp)
			tmp = i;
		model.push_back(tmp);
	}

	return model;
}

std::vector<int>
Kissat::getFinalAnalysis()
{
	std::vector<int> outCls;
	LOGERROR("NOT IMPLEMENTED");
	return outCls;
}

std::vector<int>
Kissat::getSatAssumptions()
{
	std::vector<int> outCls;
	LOGERROR("NOT IMPLEMENTED");
	return outCls;
};

void
Kissat::initKissatOptions()
{
	LOG3("Initializing Kissat configuration ..");
	// initial configuration (mostly defaults of kissat_mab)

	// Not Needed in practice
#ifndef NDEBUG
	kissatOptions.insert({ "check", 1 }); // (only on debug) 1: check model only, 2: check redundant clauses
	kissatOptions.insert({ "quiet", 0 });
#else
	kissatOptions.insert({ "quiet", 1 });
	kissatOptions.insert({ "check", 0 });
#endif

	kissatOptions.insert({ "log", 0 });
	kissatOptions.insert({ "verbose", 0 }); // verbosity level

	/*
	 * stable = 0: always focused mode (more restarts) good for UNSAT
	 * stable = 1: switching between stable and focused
	 * stable = 2: always in stable mode
	 */
	kissatOptions.insert({ "stable", 1 });
	/*
	 * If solver in stable mode or target > 1 it takes the target phase
	 */
	kissatOptions.insert({ "target", 1 });

	kissatOptions.insert({ "initshuffle", 0 }); /* Shuffle variables in queue before kissat_add */

	// Garbage Collection
	kissatOptions.insert({ "compact", 1 });		// enable compacting garbage collection
	kissatOptions.insert({ "compactlim", 10 }); // compact inactive limit (in percent)

	/*----------------------------------------------------------------------------*/

	// Initialization
	kissatOptions.insert({ "walkinitially", 0 });
	kissatOptions.insert({ "warmup", 1 }); // initialize phases by unit propagation

	/*----------------------------------------------------------------------------*/

	// (Pre/In)processing Simplifications
	kissatOptions.insert({ "hyper", 1 });	// on-the-fly hyper binary resolution
	kissatOptions.insert({ "trenary", 1 }); // hyper Trenary Resolution: maybe its too expensive ??
	kissatOptions.insert({ "failed", 1 });	// failed literal probing, many suboptions
	kissatOptions.insert({ "reduce", 1 });	// learned clause reduction

	// Subsumption options: many other suboptions
	kissatOptions.insert({ "subsumeclslim", 1e3 }); // subsumption clause size limit,
	kissatOptions.insert({ "eagersubsume", 20 });	// eagerly subsume recently learned clauses (max 100)
	kissatOptions.insert({ "vivify", 1 });			// vivify clauses, many suboptions
	kissatOptions.insert({ "otfs", 1 });			// on-the-fly strengthening when deducing learned clause

	/*-- Equisatisfiable Simplifications --*/
	kissatOptions.insert({ "substitute", 1 }); // equivalent literal substitution, many subsumptions
	kissatOptions.insert({ "autarky", 1 });	   // autarky reasoning
	kissatOptions.insert({ "eliminate", 1 });  // bounded variable elimination (BVE), many suboptions

	// Gates detection
	kissatOptions.insert({ "and", 1 });			 // and gates detection
	kissatOptions.insert({ "equivalences", 1 }); // extract and eliminate equivalence gates
	kissatOptions.insert({ "ifthenelse", 1 });	 // extract and eliminate if-then-else gates

	// Enhancements for variable elimination
	kissatOptions.insert({ "backward", 1 }); // backward subsumption in BVE
	kissatOptions.insert({ "forward", 1 });	 // forward subsumption in BVE
	kissatOptions.insert({ "extract", 1 });	 // extract gates in variable elimination

	// Delayed versions
	// kissatOptions.insert({"delay", 2}); // maximum delay (autarky, failed, ...)
	kissatOptions.insert({ "autarkydelay", 1 });
	kissatOptions.insert({ "trenarydelay", 1 });

	/*----------------------------------------------------------------------------*/

	// Search Configuration
	kissatOptions.insert(
		{ "chrono", 1 }); // chronological backtracking, according the original paper, it is benefical for UNSAT
	kissatOptions.insert(
		{ "chronolevels",
		  100 }); // if conflict analysis want to jump more than this amount of levels, chronological will be used

	// Restart Management
	kissatOptions.insert({ "restart", 1 });
	kissatOptions.insert({ "restartint", 1 });	   // base restart interval
	kissatOptions.insert({ "restartmargin", 10 }); // fast/slow margin in percent
	kissatOptions.insert({ "reluctant", 1 });	   // stable reluctant doubling restarting
	kissatOptions.insert({ "reducerestart", 0 });  // restart at reduce (1=stable , 2=always)

	// Clause and Literal Related Heuristic
	kissatOptions.insert({ "heuristic", 0 }); // 0: VSIDS, 1: CHB
	kissatOptions.insert({ "stepchb", 4 });	  // CHB step paramater
	kissatOptions.insert({ "tier1", 2 });	  // glue limit for tier1
	kissatOptions.insert({ "tier2", 6 });	  // glue limit for tier2

	// Phase
	kissatOptions.insert({ "phase", 1 }); /* initial phase: set the macro INITIAL_PHASE */
	kissatOptions.insert({ "phasesaving", 1 });
	kissatOptions.insert(
		{ "rephase",
		  1 }); // reinitialization of decision phases, have two suboptions that are never accessed outside of tests
	kissatOptions.insert({ "forcephase", 0 }); // force initial phase, forces the target option to false

	/*----------------------------------------------------------------------------*/

	// Diverse (used in mallob diversification)
	kissatOptions.insert({ "sweep", 1 });
	kissatOptions.insert({ "minimizedepth", 1e3 });
	kissatOptions.insert({ "reducefraction", 75 });
	kissatOptions.insert({ "vivifyeffort", 100 });
	kissatOptions.insert({ "probe", 1 });

	// random seed
	kissatOptions.insert({ "seed", this->getSolverId() }); // used in walk and rephase

	// Setting the options
	for (auto& opt : kissatOptions) {
		kissat_set_option(this->solver, opt.first.c_str(), opt.second);
	}
}

static std::mt19937 engine;
static std::uniform_int_distribution<unsigned> uniform(0, 100);

// Diversify the solver
void
Kissat::diversify(const SeedGenerator& getSeed)
{
	if (this->isInitialized()) {
		LOGERROR("Diversification must be done before adding clauses because of kissat_reserve()");
		exit(PERR_NOT_SUPPORTED);
	}

	int typeId = this->getSolverTypeId();
	int generalSeed = getSeed(this);

	/* Global Diversification (across all solvers)*/
	kissatOptions.at("seed") = generalSeed;
	kissatOptions.at("phase") = generalSeed % 2;

	/* Kissat Group Diversification */
	this->computeFamily();

	LOGDEBUG1("Kissat (%d,%d) is of type %u", this->getSolverId(), typeId, family);

	switch (this->family) {
		// 1/3 Focus on UNSAT
		case KissatFamily::UNSAT_FOCUSED:
			kissat_set_configuration(solver, "unsat");
			kissatOptions.at("restartint") = 1;
			kissatOptions.at("chrono") = 0;

			if (uniform(engine) < 25) // 1/4 chance
			{
				kissatOptions.at("initshuffle") = 1;
				kissatOptions.at("heuristic") = (typeId % 2); // VSIDS vs CHB
				kissatOptions.at("stepchb") = (typeId % 2) ? (4 + typeId) % 9 : 4;
			}
			break;

		// Focus on SAT ; target at 2 to enable target phase
		case KissatFamily::SAT_STABLE:
			kissat_set_configuration(solver, "sat");

			// Some solvers (~50%) do initial walk and walk further in rephasing ( benifical for SAT formulae)
			if (uniform(engine) < 50) {
				kissatOptions.at("restartint") = 50 + typeId % 100; /* less restarts when in focused mode */
				kissatOptions.at("restartmargin") = typeId % 25 + 10;
				kissatOptions.at("stable") = 2; /* to start at stable and to not switch to focused*/
				kissatOptions.at("walkinitially") = 1;
				/* Oh's expriment showed that learned clauses are not that important for SAT*/
				kissatOptions.at("tier1") = 2;
				kissatOptions.at("tier2") = 3;

				// (~25%)  more aggressive chronological backtracking
				if (uniform(engine) < 25) {
					kissatOptions.at("reducerestart") = 1;
					kissatOptions.at("chronolevels") = typeId % 200;
				}
			}

			break;

		// Switch mode
		default:
			kissatOptions.at("walkinitially") = 1;
			/* Find other diversification parameters : restarts ?*/
			kissatOptions.at("initshuffle") = 1; /* takes some time */
			// Mallob ISC24 diversfication
			switch (typeId % 9) {
				case 0:
					kissatOptions.at("eliminate") = 0;
					break;
				case 1:
					kissatOptions.at("restartint") = 10;
					break;
				case 2:
					kissatOptions.at("walkinitially") = 1;
					break;
				case 3:
					kissatOptions.at("restartint") = 0;
					break;
				case 4:
					kissatOptions.at("sweep") = 0;
					break;
				case 5:
					kissatOptions.at("probe") = 0;
					break;
				case 6:
					kissatOptions.at("minimizedepth") = 1e4;
					break;
				case 7:
					kissatOptions.at("reducefraction") = 90;
					break;
				case 8:
					kissatOptions.at("vivifyeffort") = 1000;
					break;
			}
	}

set_options:
	// Setting the options
	for (auto& opt : kissatOptions) {
		kissat_set_option(this->solver, opt.first.c_str(), opt.second);
	}
}

void
Kissat::printWinningLog()
{
	this->SolverCdclInterface::printWinningLog();
	int family = static_cast<int>(this->family);
	LOGSTAT("The winner is kissat(%d, %u) of family %s",
			this->getSolverId(),
			this->getSolverTypeId(),
			(family) ? (family == 1) ? "MIXED_SWITCH" : "UNSAT_FOCUSED" : "SAT_STABLE");
}

void
Kissat::computeFamily()
{
	this->family = static_cast<KissatFamily>((this->getSolverTypeId()) % 3);
}