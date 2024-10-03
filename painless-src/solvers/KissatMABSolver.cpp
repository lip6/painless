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

#include "utils/Logger.h"
#include "utils/System.h"
#include "utils/Parameters.h"
#include "solvers/KissatMABSolver.h"
#include "utils/BloomFilter.h"
#include "utils/ErrorCodes.h"
#include "utils/SatUtils.h"
#include "painless.h"

#include <cassert>

// loadFormula includes
extern "C"
{
#include <kissat_mab/src/parse.h>
}

// Macros for minisat literal representation conversion
#define MIDX(LIT) (((unsigned)(LIT)) >> 1)
#define MNEGATED(LIT) (((LIT) & 1u))
#define MNOT(LIT) (((LIT) ^ 1u))

char KissatMabImportUnit(void *painless_interface, kissat *internal_solver)
{
    KissatMABSolver *painless_kissat = (KissatMABSolver *)painless_interface;

    std::shared_ptr<ClauseExchange> unitClause;

    kissat_clear_pclause(internal_solver);

    /*unitsToimport to become a Buffer of Int and not ClauseExchange, then use getClauses */
    while (painless_kissat->unitsToImport.getClause(unitClause))
    {
        kissat_push_plit(internal_solver, unitClause->lits[0]);
    }

    return kissat_pclause_size(internal_solver) > 0;
}

char KissatMabImportClause(void *painless_interface, kissat *internal_solver)
{
    KissatMABSolver *painless_kissat = (KissatMABSolver *)painless_interface;
    /* Needed for the KISSAT macros to work */

    std::shared_ptr<ClauseExchange> clause;

    if (!painless_kissat->clausesToImport.getClause(clause))
        return false;

#ifndef NDEBUG
    for (int i = 0; i < clause->lits.size(); i++)
    {
        for (int j = i + 1; j < clause->lits.size(); j++)
        {
            if (clause->lits[i] == clause->lits[j])
            {
                LOGERROR("Kissat %d tried to import a clause with a duplicated literal %d from %d !!", painless_kissat->id, clause->lits[i], clause->from);
                exit(1);
            }
        }
    }
#endif
    assert(clause->size > 0);

    if (!kissat_push_lits(internal_solver, &(clause->lits[0]), clause->lits.size()))
        return false;

    return true;
}

char KissatMabExportClause(void *painless_interface, kissat *internal_solver)
{
    KissatMABSolver *painless_kissat = (KissatMABSolver *)painless_interface;

    unsigned lbd = kissat_get_pglue(internal_solver);

    if (lbd > painless_kissat->lbdLimit)
    {
        LOG4("Solver %d tried to export a clause with lbd %d > %d", painless_kissat->id, lbd, painless_kissat->lbdLimit.load());
        return false;
    }

    unsigned size = kissat_pclause_size(internal_solver);

    std::shared_ptr<ClauseExchange> new_clause = std::make_shared<ClauseExchange>(size);

    // TODO : checksum for large clauses ? size > 100 or lbd > 20

    for (unsigned i = 0; i < size; i++)
    {
#ifndef NDEBUG
        for (int lit : new_clause->lits)
        {
            if (lit == kissat_peek_plit(internal_solver, i))
            {
                LOGERROR("Kissat %d generated a clause with a duplicated literal %d !!", painless_kissat->id, lit);
                exit(1);
            }
        }
#endif

        new_clause->lits[i] = kissat_peek_plit(internal_solver, i);
        new_clause->checksum ^= lookup3_hash(new_clause->lits[i]);
    }

    new_clause->lbd = lbd;
    new_clause->from = painless_kissat->id;

    painless_kissat->clausesToExport.addClause(new_clause);
    LOG4("Solver %d exported a clause with lbd %d and size %d ", painless_kissat->id, lbd, size);
    return true;
}

KissatMABSolver::KissatMABSolver(int id) : SolverCdclInterface(id, KISSAT)
{
    lbdLimit = 0;

    solver = kissat_init();

    kissat_set_export_call(solver, KissatMabExportClause);
    kissat_set_import_call(solver, KissatMabImportClause);
    kissat_set_import_unit_call(solver, KissatMabImportUnit);
    kissat_set_painless(solver, this);
    kissat_set_id(solver, id);

    initkissatMABOptions(); /* Must be called before reserve or initshuffle */
}

KissatMABSolver::~KissatMABSolver()
{
    // kissat_print_sharing_stats(this->solver);
    kissat_release(solver);
}

void KissatMABSolver::loadFormula(const char *filename)
{
    strictness strict = NORMAL_PARSING;
    file in;
    uint64_t lineno;
    kissat_open_to_read_file(&in, filename);

    int nbVars;

    kissat_parse_dimacs(solver, strict, &in, &lineno, &nbVars);

    kissat_close_file(&in);

    kissat_set_maxVar(this->solver, nbVars);

    this->initialized = true;

    LOG1("The Kissat Solver %d loaded all the formula with %u variables", this->id, nbVars);
}

// Get the number of variables of the formula
int KissatMABSolver::getVariablesCount()
{
    return kissat_get_maxVar(this->solver);
}

// Get a variable suitable for search splitting
int KissatMABSolver::getDivisionVariable()
{
    return (rand() % getVariablesCount()) + 1;
}

// Set initial phase for a given variable
void KissatMABSolver::setPhase(const unsigned var, const bool phase)
{
    /* set target, best and saved phases: best is used in walk */
    kissat_set_phase(this->solver, var, (phase) ? 1 : -1);
}

// Bump activity for a given variable
void KissatMABSolver::bumpVariableActivity(const int var, const int times)
{
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void KissatMABSolver::setSolverInterrupt()
{
    stopSolver = true;
    kissat_terminate(solver);
    LOG2("Asked kissat %d to terminate", this->id);
}

void KissatMABSolver::unsetSolverInterrupt()
{
    stopSolver = false;
}

void KissatMABSolver::setBumpVar(int v)
{
    bump_var = v;
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatResult
KissatMABSolver::solve(const std::vector<int> &cube)
{
    if (!this->initialized)
    {
        LOGWARN("Kissat %d was not initialized to be launched!", this->id);
        return SatResult::UNKNOWN;
    }
    unsetSolverInterrupt();

    // Ugly fix, to be enhanced.
    if (kissat_check_searches(this->solver))
    {
        LOGERROR("Kissat solver %d was asked to solve more than once !!", this->id);
        exit(PERR_NOT_SUPPORTED);
    }

    for (int lit : cube)
    {
        // kissat_set_value(this->solver, lit);
        this->setPhase(std::abs(lit), (lit > 0));
    }

    int res = kissat_solve(solver);

    if (res == 10)
    {
        LOG1("Kissat %d responded with SAT", this->id);
        kissat_check_model(solver);
        return SAT;
    }
    if (res == 20)
    {
        LOG1("Kissat %d responded with UNSAT", this->id);
        return UNSAT;
    }
    LOG1("Kissat %d responded with %d (UNKNOWN)", this->id, res);
    return UNKNOWN;
}

void KissatMABSolver::addClause(std::shared_ptr<ClauseExchange> clause)
{
    unsigned maxVar = kissat_get_maxVar(this->solver);
    for (int lit : clause->lits)
    {
        if (std::abs(lit) > maxVar)
        {
            LOGERROR("[Kissat %d] literal %d is out of bound, maxVar is %d", this->id, lit, maxVar);
            return;
        }
    }

    for (int lit : clause->lits)
        kissat_add(this->solver, lit);
    kissat_add(this->solver, 0);
}

bool KissatMABSolver::importClause(std::shared_ptr<ClauseExchange> clause)
{
    if (clause->size == 1)
    {
        unitsToImport.addClause(clause);
    }
    else
    {
        clausesToImport.addClause(clause);
    }
    return true;
}

void KissatMABSolver::addClauses(const std::vector<std::shared_ptr<ClauseExchange>> &clauses)
{
    for (auto clause : clauses)
        this->addClause(clause);
}

void KissatMABSolver::addInitialClauses(const std::vector<simpleClause> &clauses, unsigned nbVars)
{
    kissat_set_maxVar(this->solver, nbVars);
    kissat_reserve(this->solver, nbVars);

    for (auto &clause : clauses)
    {
        for (int lit : clause)
            kissat_add(this->solver, lit);
        kissat_add(this->solver, 0);
    }
    this->initialized = true;
    LOG1("The Kissat Solver %d loaded all the %u clauses with %u variables", this->id, clauses.size(), nbVars);
}

void KissatMABSolver::importClauses(const std::vector<std::shared_ptr<ClauseExchange>> &clauses)
{
    for (auto cls : clauses)
    {
        importClause(cls);
    }
}

void KissatMABSolver::exportClauses(std::vector<std::shared_ptr<ClauseExchange>> &clauses)
{
    clausesToExport.getClauses(clauses);
}

void KissatMABSolver::increaseClauseProduction()
{
    lbdLimit++;
}

void KissatMABSolver::decreaseClauseProduction()
{
    if (lbdLimit > 2)
    {
        lbdLimit--;
    }
}

void KissatMABSolver::printStatistics()
{
    LOGERROR("Not implemented yet !!");

    kissat_print_statistics(this->solver);
}

std::vector<int>
KissatMABSolver::getModel()
{
    std::vector<int> model;
    unsigned maxVar = kissat_get_maxVar(this->solver);

    for (int i = 1; i <= maxVar; i++)
    {
        int tmp = kissat_value(solver, i);
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

void KissatMABSolver::initkissatMABOptions()
{
    LOG3("Initializing Kissat configuration ..");
    // initial configuration (mostly defaults of kissat_mab)

    // Not Needed in practice
#ifndef NDEBUG
    kissatMABOptions.insert({"quiet", 0});
    kissatMABOptions.insert({"check", 1}); // (only on debug) 1: check model only, 2: check redundant clauses
#else
    kissatMABOptions.insert({"quiet", 1});
    kissatMABOptions.insert({"check", 0});
#endif
    kissatMABOptions.insert({"log", 0});
    kissatMABOptions.insert({"verbose", 0}); // verbosity level

    /*
     * stable = 0: always focused mode (more restarts) good for UNSAT
     * stable = 1: switching between stable and focused
     * stable = 2: always in stable mode
     */
    kissatMABOptions.insert({"stable", 1});
    /*
     * Used in decide_phase: (kissatMabProp made several modifications in decide_phase):
     * If solver in stable mode or target > 1 it takes the target phase
     */
    kissatMABOptions.insert({"target", 1});

    kissatMABOptions.insert({"initshuffle", 0}); /* Shuffle variables in queue before kissat_add */

    // Garbage Collection
    kissatMABOptions.insert({"compact", 1});     // enable compacting garbage collection
    kissatMABOptions.insert({"compactlim", 10}); // compact inactive limit (in percent)

    /*----------------------------------------------------------------------------*/

    // Local search
    kissatMABOptions.insert({"walkinitially", 0}); // the other suboptions are not used
    kissatMABOptions.insert({"walkstrategy", 3});  // walk (local search) strategy. 0: default, 1: no walk, 2: 5 times walk, 3: select by decision tree
    kissatMABOptions.insert({"walkbits", 16});     // number of enumerated variables per walk step (commented in kissat, not used!)
    kissatMABOptions.insert({"walkrounds", 1});    // rounds per walking phase (max is 100000), in initial walk and phase walk (during rephasing)

    // CCANR in backtacking
    kissatMABOptions.insert({"ccanr", 0});
    kissatMABOptions.insert({"ccanr_dynamic_bms", 20}); // bms*10 ??
    kissatMABOptions.insert({"ccanr_gap_inc", 1024});   // reducing it make the use of ccanr more frequent

    /*----------------------------------------------------------------------------*/

    // (Pre/In)processing Simplifications
    kissatMABOptions.insert({"hyper", 1});   // on-the-fly hyper binary resolution
    kissatMABOptions.insert({"trenary", 1}); // hyper Trenary Resolution: maybe its too expensive ??
    kissatMABOptions.insert({"failed", 1});  // failed literal probing, many suboptions
    kissatMABOptions.insert({"reduce", 1});  // learned clause reduction

    // Subsumption options: many other suboptions
    kissatMABOptions.insert({"subsumeclslim", 1e3}); // subsumption clause size limit,
    kissatMABOptions.insert({"eagersubsume", 20});   // eagerly subsume recently learned clauses (max 100)
    kissatMABOptions.insert({"vivify", 1});          // vivify clauses, many suboptions
    kissatMABOptions.insert({"otfs", 1});            // on-the-fly strengthening when deducing learned clause

    /*-- Equisatisfiable Simplifications --*/
    kissatMABOptions.insert({"substitute", 1}); // equivalent literal substitution, many subsumptions
    kissatMABOptions.insert({"autarky", 1});    // autarky reasoning
    kissatMABOptions.insert({"eliminate", 1});  // bounded variable elimination (BVE), many suboptions

    // Gates detection
    kissatMABOptions.insert({"and", 1});          // and gates detection
    kissatMABOptions.insert({"equivalences", 1}); // extract and eliminate equivalence gates
    kissatMABOptions.insert({"ifthenelse", 1});   // extract and eliminate if-then-else gates

    // Xor gates
    kissatMABOptions.insert({"xors", 0});
    kissatMABOptions.insert({"xorsbound", 1});   // minimum elimination bound
    kissatMABOptions.insert({"xorsclsslim", 5}); // xor extraction clause size limit

    // Enhancements for variable elimination
    kissatMABOptions.insert({"backward", 1}); // backward subsumption in BVE
    kissatMABOptions.insert({"forward", 1});  // forward subsumption in BVE
    kissatMABOptions.insert({"extract", 1});  // extract gates in variable elimination

    // Delayed versions
    // kissatMABOptions.insert({"delay", 2}); // maximum delay (autarky, failed, ...)
    kissatMABOptions.insert({"autarkydelay", 1});
    kissatMABOptions.insert({"trenarydelay", 1});

    /*----------------------------------------------------------------------------*/

    // Search Configuration
    kissatMABOptions.insert({"chrono", 1});         // chronological backtracking, according the original paper, it is benefical for UNSAT
    kissatMABOptions.insert({"chronolevels", 100}); // if conflict analysis want to jump more than this amount of levels, chronological will be used

    // Restart Management
    kissatMABOptions.insert({"restart", 1});
    kissatMABOptions.insert({"restartint", 1});     // base restart interval
    kissatMABOptions.insert({"restartmargin", 10}); // fast/slow margin in percent
    kissatMABOptions.insert({"reluctant", 1});      // stable reluctant doubling restarting
    kissatMABOptions.insert({"reducerestart", 0});  // restart at reduce (1=stable , 2=always)

    // Clause and Literal Related Heuristic
    kissatMABOptions.insert({"heuristic", 0}); // 0: VSIDS, 1: CHB
    kissatMABOptions.insert({"stepchb", 4});   // CHB step paramater
    kissatMABOptions.insert({"tier1", 2});     // glue limit for tier1
    kissatMABOptions.insert({"tier2", 6});     // glue limit for tier2

    // Mab Options
    kissatMABOptions.insert({"mab", 1}); // enable Multi Armed Bandit for VSIDS and CHB switching at restart

    // Phase
    kissatMABOptions.insert({"phase", 1}); /* initial phase: set the macro INITIAL_PHASE */
    kissatMABOptions.insert({"phasesaving", 1});
    kissatMABOptions.insert({"rephase", 1});    // reinitialization of decision phases, have two suboptions that are never accessed outside of tests
    kissatMABOptions.insert({"forcephase", 0}); // force initial phase, forces the target option to false

    /*----------------------------------------------------------------------------*/

    // not understood, yet
    kissatMABOptions.insert({"probedelay", 0});
    kissatMABOptions.insert({"targetinc", 0});

    // random seed
    kissatMABOptions.insert({"seed", this->id}); // used in walk and rephase

    // Init Mab options
    kissat_mab_init(solver);
}

// Diversify the solver
void KissatMABSolver::diversify(std::mt19937 &rng_engine, std::uniform_int_distribution<int> &uniform_dist)
{
    if (this->initialized)
    {
        LOGERROR("Diversification must be done before adding clauses because of kissat_reserve()");
        exit(PERR_NOT_SUPPORTED);
    }

    int noise = uniform_dist(rng_engine);

    unsigned maxNoise = Parameters::getIntParam("max-div-noise", 100);

    kissatMABOptions.at("seed") = this->id; /* changed if dist */

    /* Half init as all false */
    if (this->id % 2) // == 1
    {
        kissatMABOptions.at("phase") = 0;
    }

    // Across all the solvers in case of dist mode (each mpi process may have a different set of solvers)
    switch (this->family)
    {
    // 1/3 Focus on UNSAT
    case SolverDivFamily::UNSAT_FOCUSED:
        kissatMABOptions.at("stable") = 0;
        kissatMABOptions.at("restartmargin") = 10 + (noise % 5);
        kissatMABOptions.at("restartint") = 1; // bigger values are of interest ?
        // Exprimental
        kissatMABOptions.at("chronolevels") = 100 - (noise % 20); // more frequent chronological

        /* TODO : test this */
        if (noise <= maxNoise / 4)
        {
            kissatMABOptions.at("initshuffle") = 1;
        }
        break;

    // Focus on SAT ; target at 2 to enable target phase
    case SolverDivFamily::SAT_STABLE:
        kissatMABOptions.at("target") = 2;              /* checks target phase first */
        kissatMABOptions.at("restartint") = 50 + noise; /* less restarts when in focused mode */
        kissatMABOptions.at("restartmargin") = noise % 25 + 10;

        // Some solvers (50% chance) do initial walk and walk further in rephasing ( benifical for SAT formulae)
        if (noise <= maxNoise / 2)
        {
            kissatMABOptions.at("ccanr") = 1;
            kissatMABOptions.at("stable") = 2; /* to start at stable and to not switch to focused*/
            kissatMABOptions.at("walkinitially") = 1;
            kissatMABOptions.at("walkrounds") = (this->id + noise) % (1 << 4); /* TODO: enhance this*/
            /* Oh's expriment showed that learned clauses are not that important for SAT*/
            kissatMABOptions.at("tier1") = 2;
            kissatMABOptions.at("tier2") = 3;

            // (25% chance) a solver will be on stable mode only with more walkrounds, ccanr and no chronological backtracking
            if (noise <= maxNoise / 4)
            {
                int heuristicNoise = uniform_dist(rng_engine);
                kissatMABOptions.at("chrono") = 0;
                kissatMABOptions.at("walkrounds") = (this->id + noise * 10) % (1 << 12); /* TODO: enhance this*/

                // TODO: look more onto the effect of mab on sat and unsat
                kissatMABOptions.at("mab") = 0;
                kissatMABOptions.at("heuristic") = (heuristicNoise < (maxNoise / 2)); // VSIDS vs CHB
                kissatMABOptions.at("stepchb") = (heuristicNoise < (maxNoise / 2)) ? (4 + heuristicNoise) % 9 : 4;
                kissatMABOptions.at("reducerestart") = 1;
            }
        }

        break;

    // Switch mode without target phase : TODO better diversification needed here
    default:
        kissatMABOptions.at("walkinitially") = 1;
        kissatMABOptions.at("walkrounds") = (this->id + noise) % (1 << 3);
        /* Find other diversification parameters : restarts ?*/
        kissatMABOptions.at("initshuffle") = 1; /* takes some time */
    }

set_options:
    // Setting the options
    for (auto &opt : kissatMABOptions)
    {
        kissat_set_option(this->solver, opt.first.c_str(), opt.second);
    }
    // ReInit Mab options
    kissat_mab_init(solver);
    LOG1("Diversification of solver %d of family %d with noise %d", this->id, this->family, noise);
}