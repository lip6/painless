#include "painless.hpp"
#include "sharing/Filters/BloomFilter.hpp"
#include "utils/ErrorCodes.hpp"
#include "utils/Logger.hpp"
#include "utils/MpiUtils.hpp"
#include "utils/Parameters.hpp"
#include "utils/System.hpp"

#include <cassert>

#include "KissatMABSolver.hpp"

extern "C"
{
#include "kissat_mab/src/parse.h"
}

char
KissatMabImportClause(void* painless_interface, kissat* internal_solver)
{
	KissatMABSolver* painless_kissat = (KissatMABSolver*)painless_interface;
	/* Needed for the KISSAT macros to work */

	ClauseExchangePtr clause;

	if (!painless_kissat->m_clausesToImport->getOneClause(clause)) {
		painless_kissat->m_clausesToImport->shrinkDatabase();
		return false;
	}

	assert(clause->size > 0);

	if (!kissat_mab_push_lits(internal_solver, &(clause->lits[0]), clause->size))
		return false;

	return true;
}

char
KissatMabExportClause(void* painless_interface, kissat* internal_solver)
{
	KissatMABSolver* painless_kissat = (KissatMABSolver*)painless_interface;

	unsigned int lbd = kissat_mab_get_pglue(internal_solver);

	unsigned int size = kissat_mab_pclause_size(internal_solver);

	assert(size > 0);

	ClauseExchangePtr new_clause = ClauseExchange::create(size, lbd, painless_kissat->getSharingId());

	for (unsigned int i = 0; i < size; i++) {
		new_clause->lits[i] = kissat_mab_peek_plit(internal_solver, i);
	}

	/* filtering defined by a sharing strategy */
	if (painless_kissat->exportClause(new_clause)) {
		return true;
	} else
		return false;
}

KissatMABSolver::KissatMABSolver(int id, const std::shared_ptr<ClauseDatabase>& clauseDB)
	: SolverCdclInterface(id, clauseDB, SolverCdclType::KISSATMAB)
	, clausesToAdd(__globalParameters__.defaultClauseBufferSize)
{
	solver = kissat_mab_init();

	family = KissatFamily::MIXED_SWITCH;

	kissat_mab_set_export_call(solver, KissatMabExportClause);
	kissat_mab_set_import_call(solver, KissatMabImportClause);
	kissat_mab_set_import_unit_call(solver, nullptr);
	kissat_mab_set_painless(solver, this);
	kissat_mab_set_id(solver, id);

	initkissatMABOptions(); /* Must be called before reserve or initshuffle */

	initializeTypeId<KissatMABSolver>();
}

KissatMABSolver::~KissatMABSolver()
{
	// kissat_mab_print_sharing_stats(this->solver);
	kissat_mab_release(solver);
}

void
KissatMABSolver::loadFormula(const char* filename)
{
	strictness strict = NORMAL_PARSING;
	file in;
	uint64_t lineno;
	kissat_mab_open_to_read_file(&in, filename);

	int nbVars;

	kissat_mab_parse_dimacs(solver, strict, &in, &lineno, &nbVars);

	kissat_mab_close_file(&in);

	kissat_mab_set_maxVar(this->solver, nbVars);

	this->setInitialized(true);

	LOG2("The KissatMab Solver %d loaded all the formula with %u variables", this->getSolverId(), nbVars);
}

// Get the number of variables of the formula
unsigned int
KissatMABSolver::getVariablesCount()
{
	return kissat_mab_get_maxVar(this->solver);
}

// Get a variable suitable for search splitting
int
KissatMABSolver::getDivisionVariable()
{
	return (rand() % getVariablesCount()) + 1;
}

// Set initial phase for a given variable
void
KissatMABSolver::setPhase(const unsigned int var, const bool phase)
{
	/* set target, best and saved phases: best is used in walk */
	kissat_mab_set_phase(this->solver, var, (phase) ? 1 : -1);
}

// Bump activity for a given variable
void
KissatMABSolver::bumpVariableActivity(const int var, const int times)
{
	LOGERROR("Not Implemented, yet");
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void
KissatMABSolver::setSolverInterrupt()
{
	stopSolver = true;
	kissat_mab_terminate(this->solver);
	LOGDEBUG1("Asking KissatMAB (%d, %u) to end", this->getSolverId(), this->getSolverTypeId());
}

void
KissatMABSolver::unsetSolverInterrupt()
{
	stopSolver = false;
}

void
KissatMABSolver::setBumpVar(int v)
{
	bump_var = v;
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatResult
KissatMABSolver::solve(const std::vector<int>& cube)
{
	if (!this->isInitialized()) {
		LOGWARN("KissatMab %d was not initialized to be launched!", this->getSolverId());
		return SatResult::UNKNOWN;
	}
	unsetSolverInterrupt();

	// Ugly fix, to be enhanced.
	if (kissat_mab_check_searches(this->solver)) {
		LOGERROR("KissatMab solver %d was asked to solve more than once !!", this->getSolverId());
		exit(PERR_NOT_SUPPORTED);
	}

	for (int lit : cube) {
		// kissat_mab_set_value(this->solver, lit);
		this->setPhase(std::abs(lit), (lit > 0));
	}

	int res = kissat_mab_solve(solver);

	if (res == 10) {
		LOG2("KissatMab %d responded with SAT", this->getSolverId());
		kissat_mab_check_model(solver);
		return SatResult::SAT;
	}
	if (res == 20) {
		LOG2("KissatMab %d responded with UNSAT", this->getSolverId());
		return SatResult::UNSAT;
	}
	LOGDEBUG2("KissatMab %d responded with %d (UNKNOWN)", this->getSolverId(), res);
	return SatResult::UNKNOWN;
}

void
KissatMABSolver::addClause(ClauseExchangePtr clause)
{
	unsigned int maxVar = kissat_mab_get_maxVar(this->solver);
	for (int lit : clause->lits) {
		if (std::abs(lit) > maxVar) {
			LOGERROR("[KissatMab %d] literal %d is out of bound, maxVar is %d", this->getSolverId(), lit, maxVar);
			return;
		}
	}

	for (int lit : clause->lits)
		kissat_mab_add(this->solver, lit);
	kissat_mab_add(this->solver, 0);
}

bool
KissatMABSolver::importClause(const ClauseExchangePtr& clause)
{
	m_clausesToImport->addClause(clause);
	return true;
}

void
KissatMABSolver::importClauses(const std::vector<ClauseExchangePtr>& clauses)
{
	for (auto cls : clauses) {
		importClause(cls);
	}
}

void
KissatMABSolver::addClauses(const std::vector<ClauseExchangePtr>& clauses)
{
	for (auto clause : clauses)
		this->addClause(clause);
}

void
KissatMABSolver::addInitialClauses(const std::vector<simpleClause>& clauses, unsigned int nbVars)
{
	kissat_mab_set_maxVar(this->solver, nbVars);
	kissat_mab_reserve(this->solver, nbVars);

	for (auto& clause : clauses) {
		for (int lit : clause)
			kissat_mab_add(this->solver, lit);
		kissat_mab_add(this->solver, 0);
	}
	this->setInitialized(true);
	LOG2("The KissatMab Solver %d loaded all the %u clauses with %u variables",
		 this->getSolverId(),
		 clauses.size(),
		 nbVars);
}

void
KissatMABSolver::printStatistics()
{
	LOGERROR("Not implemented yet !!");
}

std::vector<int>
KissatMABSolver::getModel()
{
	std::vector<int> model;
	unsigned int maxVar = kissat_mab_get_maxVar(this->solver);

	for (unsigned int i = 1; i <= maxVar; i++) {
		int tmp = kissat_mab_value(solver, i);
		if (!tmp)
			tmp = i;
		model.push_back(tmp);
	}

	return model;
}

std::vector<int>
KissatMABSolver::getFinalAnalysis()
{
	std::vector<int> outCls;
	return outCls;
}

std::vector<int>
KissatMABSolver::getSatAssumptions()
{
	std::vector<int> outCls;
	return outCls;
};

void
KissatMABSolver::initkissatMABOptions()
{
	LOG3("Initializing KissatMab configuration ..");
	// initial configuration (mostly defaults of kissat_mab_mab)

	// Not Needed in practice
#ifndef NDEBUG
	kissatMABOptions.insert({ "quiet", 0 });
	kissatMABOptions.insert({ "check", 1 }); // (only on debug) 1: check model
											 // only, 2: check redundant clauses
#else
	kissatMABOptions.insert({ "quiet", 1 });
	kissatMABOptions.insert({ "check", 0 });
#endif
	kissatMABOptions.insert({ "log", 0 });
	kissatMABOptions.insert({ "verbose", 0 }); // verbosity level

	/*
	 * stable = 0: always focused mode (more restarts) good for UNSAT
	 * stable = 1: switching between stable and focused
	 * stable = 2: always in stable mode
	 */
	kissatMABOptions.insert({ "stable", 1 });
	/*
	 * Used in decide_phase: If solver in stable mode or target > 1 it takes the target
	 * phase
	 */
	kissatMABOptions.insert({ "target", 1 });

	kissatMABOptions.insert({ "initshuffle", 0 }); /* Shuffle variables in queue before kissat_mab_add */

	// Garbage Collection
	kissatMABOptions.insert({ "compact", 1 });	   // enable compacting garbage collection
	kissatMABOptions.insert({ "compactlim", 10 }); // compact inactive limit (in percent)

	/*----------------------------------------------------------------------------*/

	// Local search
	kissatMABOptions.insert({ "walkinitially", 0 }); // the other suboptions are not used
	kissatMABOptions.insert({ "walkstrategy", 3 });	 // walk (local search) strategy. 0: default, 1: no walk, 2: 5
													 // times walk, 3: select by decision tree
	kissatMABOptions.insert({ "walkbits", 16 });	 // number of enumerated variables per walk step
													 // (commented in kissat, not used!)
	kissatMABOptions.insert({ "walkrounds", 1 });	 // rounds per walking phase (max is 100000), in
													 // initial walk and phase walk (during rephasing)

	// CCANR in backtacking
	kissatMABOptions.insert({ "ccanr", 0 });
	kissatMABOptions.insert({ "ccanr_dynamic_bms", 20 }); // bms*10 ??
	kissatMABOptions.insert({ "ccanr_gap_inc", 1024 });	  // reducing it make the use of ccanr more frequent

	/*----------------------------------------------------------------------------*/

	// (Pre/In)processing Simplifications
	kissatMABOptions.insert({ "hyper", 1 });   // on-the-fly hyper binary resolution
	kissatMABOptions.insert({ "trenary", 1 }); // hyper Trenary Resolution: maybe its too expensive ??
	kissatMABOptions.insert({ "failed", 1 });  // failed literal probing, many suboptions
	kissatMABOptions.insert({ "reduce", 1 });  // learned clause reduction

	// Subsumption options: many other suboptions
	kissatMABOptions.insert({ "subsumeclslim", 1e3 }); // subsumption clause size limit,
	kissatMABOptions.insert({ "eagersubsume", 20 });   // eagerly subsume recently learned clauses (max 100)
	kissatMABOptions.insert({ "vivify", 1 });		   // vivify clauses, many suboptions
	kissatMABOptions.insert({ "otfs", 1 });			   // on-the-fly strengthening when deducing learned clause

	/*-- Equisatisfiable Simplifications --*/
	kissatMABOptions.insert({ "substitute", 1 }); // equivalent literal substitution, many subsumptions
	kissatMABOptions.insert({ "autarky", 1 });	  // autarky reasoning
	kissatMABOptions.insert({ "eliminate", 1 });  // bounded variable elimination (BVE), many suboptions

	// Gates detection
	kissatMABOptions.insert({ "and", 1 });			// and gates detection
	kissatMABOptions.insert({ "equivalences", 1 }); // extract and eliminate equivalence gates
	kissatMABOptions.insert({ "ifthenelse", 1 });	// extract and eliminate if-then-else gates

	// Xor gates
	kissatMABOptions.insert({ "xors", 0 });
	kissatMABOptions.insert({ "xorsbound", 1 });   // minimum elimination bound
	kissatMABOptions.insert({ "xorsclsslim", 5 }); // xor extraction clause size limit

	// Enhancements for variable elimination
	kissatMABOptions.insert({ "backward", 1 }); // backward subsumption in BVE
	kissatMABOptions.insert({ "forward", 1 });	// forward subsumption in BVE
	kissatMABOptions.insert({ "extract", 1 });	// extract gates in variable elimination

	// Delayed versions
	// kissatMABOptions.insert({"delay", 2}); // maximum delay (autarky, failed,
	// ...)
	kissatMABOptions.insert({ "autarkydelay", 1 });
	kissatMABOptions.insert({ "trenarydelay", 1 });

	/*----------------------------------------------------------------------------*/

	// Search Configuration
	kissatMABOptions.insert({ "chrono", 1 });		  // chronological backtracking, according the original
													  // paper, it is benefical for UNSAT
	kissatMABOptions.insert({ "chronolevels", 100 }); // if conflict analysis want to jump more than this amount of
													  // levels, chronological will be used

	// Restart Management
	kissatMABOptions.insert({ "restart", 1 });
	kissatMABOptions.insert({ "restartint", 1 });	  // base restart interval
	kissatMABOptions.insert({ "restartmargin", 10 }); // fast/slow margin in percent
	kissatMABOptions.insert({ "reluctant", 1 });	  // stable reluctant doubling restarting
	kissatMABOptions.insert({ "reducerestart", 0 });  // restart at reduce (1=stable , 2=always)

	// Clause and Literal Related Heuristic
	kissatMABOptions.insert({ "heuristic", 0 }); // 0: VSIDS, 1: CHB
	kissatMABOptions.insert({ "stepchb", 4 });	 // CHB step paramater
	kissatMABOptions.insert({ "tier1", 2 });	 // glue limit for tier1
	kissatMABOptions.insert({ "tier2", 6 });	 // glue limit for tier2

	// Mab Options
	kissatMABOptions.insert({ "mab", 1 }); // enable Multi Armed Bandit for
										   // VSIDS and CHB switching at restart

	// Phase
	kissatMABOptions.insert({ "phase", 1 }); /* initial phase: set the macro INITIAL_PHASE */
	kissatMABOptions.insert({ "phasesaving", 1 });
	kissatMABOptions.insert({ "rephase", 1 });	  // reinitialization of decision phases, have two
												  // suboptions that are never accessed outside of tests
	kissatMABOptions.insert({ "forcephase", 0 }); // force initial phase, forces the target option to false

	/*----------------------------------------------------------------------------*/

	// not understood, yet
	kissatMABOptions.insert({ "probedelay", 0 });
	kissatMABOptions.insert({ "targetinc", 0 });

	// random seed
	kissatMABOptions.insert({ "seed", this->getSolverId() }); // used in walk and rephase

	// Init Mab options
	kissat_mab_mabvars_init(solver);
}

void
KissatMABSolver::diversify(const SeedGenerator& getSeed)
{
	if (this->isInitialized()) {
		LOGERROR("Diversification must be done before adding clauses because "
				 "of kissat_mab_reserve()");
		exit(PERR_NOT_SUPPORTED);
	}
	this->computeFamily();
	int seed = getSeed(this);

	// Initialize random number generator
	std::mt19937 rng(seed);
	std::uniform_real_distribution<double> dist(0.0, 1.0);

	kissatMABOptions.at("seed") = this->getSolverId();

	// Half init as all false
	if (dist(rng) < 0.5) {
		kissatMABOptions.at("phase") = 0;
	}

	// Across all the solvers in case of dist mode (each mpi process may have a
	// different set of solvers)
	switch (this->family) {
		// 1/3 Focus on UNSAT
		case KissatFamily::UNSAT_FOCUSED:
			kissatMABOptions.at("stable") = 0;
			kissatMABOptions.at("restartmargin") = 10 + static_cast<int>(dist(rng) * 5);
			kissatMABOptions.at("restartint") = 1;
			// Experimental
			kissatMABOptions.at("chronolevels") = 100 - static_cast<int>(dist(rng) * 20);
			if (dist(rng) < 0.25) {
				kissatMABOptions.at("initshuffle") = 1;
			}
			break;

		// Focus on SAT ; target at 2 to enable target phase
		case KissatFamily::SAT_STABLE:
			kissatMABOptions.at("target") = 2;
			kissatMABOptions.at("restartint") = 50 + static_cast<int>(dist(rng) * 100);
			kissatMABOptions.at("restartmargin") = static_cast<int>(dist(rng) * 25) + 10;

			// Some solvers (50% chance) do initial walk and walk further in
			// rephasing (beneficial for SAT formulae)
			if (dist(rng) < 0.5) {
				kissatMABOptions.at("ccanr") = 1;
				kissatMABOptions.at("stable") = 2;
				kissatMABOptions.at("walkinitially") = 1;
				kissatMABOptions.at("walkrounds") = static_cast<int>(dist(rng) * (1 << 4));
				kissatMABOptions.at("tier1") = 2;
				kissatMABOptions.at("tier2") = 3;

				// (25% chance) a solver will be on stable mode only with more
				// walkrounds, ccanr and no chronological backtracking
				if (dist(rng) < 0.25) {
					kissatMABOptions.at("chrono") = 0;
					kissatMABOptions.at("walkrounds") = static_cast<int>(dist(rng) * (1 << 12));
					kissatMABOptions.at("mab") = 0;
					kissatMABOptions.at("heuristic") = (dist(rng) < 0.5) ? 0 : 1; // VSIDS vs CHB
					kissatMABOptions.at("stepchb") = (dist(rng) < 0.5) ? 4 : (4 + static_cast<int>(dist(rng) * 5));
					kissatMABOptions.at("reducerestart") = 1;
				}
			}
			break;

		// Switch mode without target phase : TODO better diversification needed
		// here
		default:
			kissatMABOptions.at("walkinitially") = 1;
			kissatMABOptions.at("walkrounds") = static_cast<int>(dist(rng) * (1 << 3));
			kissatMABOptions.at("initshuffle") = 1;
			break;
	}

	// Setting the options
	for (auto& opt : kissatMABOptions) {
		kissat_mab_set_option(this->solver, opt.first.c_str(), opt.second);
	}

	// ReInit Mab options
	kissat_mab_mabvars_init(solver);
	LOG1("Diversification of KissatMab (%d,%u) of family %d with seed %d",
		 this->getSolverId(),
		 this->getSolverTypeId(),
		 this->family,
		 seed);
}

void
KissatMABSolver::printWinningLog()
{
	this->SolverCdclInterface::printWinningLog();
	int family = static_cast<int>(this->family);
	LOGSTAT("The winner is KissatMab(%d, %u) of family %s",
			this->getSolverId(),
			this->getSolverTypeId(),
			(family) ? (family == 1) ? "MIXED_SWITCH" : "UNSAT_FOCUSED" : "SAT_STABLE");
}

void
KissatMABSolver::computeFamily()
{
	/* + mpi_rank enable better distributed fairness */
	this->family = static_cast<KissatFamily>((this->getSolverTypeId() + mpi_rank) % 3);
}