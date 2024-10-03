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
#include "solvers/KissatGASPISolver.h"
#include "utils/BloomFilter.h"
#include "utils/ErrorCodes.h"
#include "utils/SatUtils.h"
#include "painless.h"

#include <cassert>

// loadFormula includes
#include <GASPIKISSAT/src/parse.h>

// Macros for minisat literal representation conversion
#define MIDX(LIT) (((unsigned)(LIT)) >> 1)
#define MNEGATED(LIT) (((LIT) & 1u))
#define MNOT(LIT) (((LIT) ^ 1u))

char KissatGaspiImportUnit(void *painless_interface, kissat *internal_solver)
{
    KissatGASPISolver *painless_kissat = (KissatGASPISolver *)painless_interface;

    std::shared_ptr<ClauseExchange> unitClause;

    gaspi::kissat_clear_pclause(internal_solver);

    /*unitsToimport to become a Buffer of Int and not ClauseExchange, then use getClauses */
    while (painless_kissat->unitsToImport.getClause(unitClause))
    {
        gaspi::kissat_push_plit(internal_solver, unitClause->lits[0]);
    }

    return gaspi::kissat_pclause_size(internal_solver) > 0;
}

char KissatGaspiImportClause(void *painless_interface, kissat *internal_solver)
{
    KissatGASPISolver *painless_kissat = (KissatGASPISolver *)painless_interface;
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
                LOGERROR("GaspiKissat %d tried to import a clause with a duplicated literal %d from %d !!", painless_kissat->id, clause->lits[i], clause->from);
                exit(1);
            }
        }
    }
#endif
    assert(clause->size > 0);

    if (!gaspi::kissat_push_lits(internal_solver, &(clause->lits[0]), clause->lits.size()))
        return false;

    return true;
}

char KissatGaspiExportClause(void *painless_interface, kissat *internal_solver)
{
    KissatGASPISolver *painless_kissat = (KissatGASPISolver *)painless_interface;

    unsigned lbd = gaspi::kissat_get_pglue(internal_solver);

    if (lbd > painless_kissat->lbdLimit)
    {
        LOG4("GaspiKissat %d tried to export a clause with lbd %d > %d", painless_kissat->id, lbd, painless_kissat->lbdLimit.load());
        return false;
    }

    unsigned size = gaspi::kissat_pclause_size(internal_solver);

    std::shared_ptr<ClauseExchange> new_clause = std::make_shared<ClauseExchange>(size);

    // TODO : checksum for large clauses ? size > 100 or lbd > 20

    for (unsigned i = 0; i < size; i++)
    {
#ifndef NDEBUG
        for (int lit : new_clause->lits)
        {
            if (lit == gaspi::kissat_peek_plit(internal_solver, i))
            {
                LOGERROR("GaspiKissat %d generated a clause with a duplicated literal %d !!", painless_kissat->id, lit);
                exit(1);
            }
        }
#endif

        new_clause->lits[i] = gaspi::kissat_peek_plit(internal_solver, i);
        new_clause->checksum ^= lookup3_hash(new_clause->lits[i]);
    }

    new_clause->lbd = lbd;
    new_clause->from = painless_kissat->id;

    painless_kissat->clausesToExport.addClause(new_clause);
    LOG4("GaspiKissat %d exported a clause with lbd %d and size %d ", painless_kissat->id, lbd, size);
    return true;
}

KissatGASPISolver::KissatGASPISolver(int id) : SolverCdclInterface(id, KISSAT)
{
    lbdLimit = 0;

    solver = gaspi::kissat_init();

    gaspi::kissat_set_export_call(solver, KissatGaspiExportClause);
    gaspi::kissat_set_import_call(solver, KissatGaspiImportClause);
    gaspi::kissat_set_import_unit_call(solver, KissatGaspiImportUnit);
    gaspi::kissat_set_painless(solver, this);
    gaspi::kissat_set_id(solver, id);

    initkissatGASPIOptions(); /* Must be called before reserve or initshuffle */
}

KissatGASPISolver::~KissatGASPISolver()
{
    // kissat_print_sharing_stats(this->solver);
    gaspi::kissat_release(solver);
}

void KissatGASPISolver::loadFormula(const char *filename)
{
    strictness strict = NORMAL_PARSING;
    file in;
    uint64_t lineno;
    kissat_open_to_read_file(&in, filename);

    int nbVars;

    gaspi::kissat_parse_dimacs(solver, strict, &in, &lineno, &nbVars);

    kissat_close_file(&in);

    gaspi::kissat_set_maxVar(this->solver, nbVars);

    this->initialized = true;

    LOG1("The GaspiKissat %d loaded all the formula with %u variables", this->id, nbVars);
}

// Get the number of variables of the formula
int KissatGASPISolver::getVariablesCount()
{
    return gaspi::kissat_get_maxVar(this->solver);
}

// Get a variable suitable for search splitting
int KissatGASPISolver::getDivisionVariable()
{
    return (rand() % getVariablesCount()) + 1;
}

// Set initial phase for a given variable
void KissatGASPISolver::setPhase(const unsigned var, const bool phase)
{
    /* set target, best and saved phases: best is used in walk */
    gaspi::kissat_set_phase(this->solver, var, (phase) ? 1 : -1);
}

// Bump activity for a given variable
void KissatGASPISolver::bumpVariableActivity(const int var, const int times)
{
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void KissatGASPISolver::setSolverInterrupt()
{
    stopSolver = true;
    gaspi::kissat_terminate(solver);
    LOG2("Asked GaspiKissat %d to terminate", this->id);
}

void KissatGASPISolver::unsetSolverInterrupt()
{
    stopSolver = false;
}

void KissatGASPISolver::setBumpVar(int v)
{
    bump_var = v;
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatResult
KissatGASPISolver::solve(const std::vector<int> &cube)
{
    if (!this->initialized)
    {
        LOGWARN("GaspiKissat %d was not initialized to be launched!", this->id);
        return SatResult::UNKNOWN;
    }
    unsetSolverInterrupt();

    // Ugly fix, to be enhanced.
    if (gaspi::kissat_check_searches(this->solver))
    {
        LOGERROR("GaspiKissat solver %d was asked to solve more than once !!", this->id);
        exit(PERR_NOT_SUPPORTED);
    }

    for (int lit : cube)
    {
        // kissat_set_value(this->solver, lit);
        this->setPhase(std::abs(lit), (lit > 0));
    }

    int res = gaspi::kissat_solve(solver);

    if (res == 10)
    {
        LOG1("GaspiKissat %d responded with SAT.", this->id);
        gaspi::kissat_check_model(solver);
        return SAT;
    }
    if (res == 20)
    {
        LOG1("GaspiKissat %d responded with UNSAT.", this->id);
        return UNSAT;
    }
    LOG1("GaspiKissat %d responded with %d (UNKNOWN).", this->id, res);
    return UNKNOWN;
}

void KissatGASPISolver::addClause(std::shared_ptr<ClauseExchange> clause)
{
    unsigned maxVar = gaspi::kissat_get_maxVar(this->solver);
    for (int lit : clause->lits)
    {
        if (std::abs(lit) > maxVar)
        {
            LOGERROR("[GaspiKissat %d] literal %d is out of bound, maxVar is %d", this->id, lit, maxVar);
            return;
        }
    }

    for (int lit : clause->lits)
        gaspi::kissat_add(this->solver, lit);
    gaspi::kissat_add(this->solver, 0);
}

bool KissatGASPISolver::importClause(std::shared_ptr<ClauseExchange> clause)
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

void KissatGASPISolver::addClauses(const std::vector<std::shared_ptr<ClauseExchange>> &clauses)
{
    for (auto clause : clauses)
        this->addClause(clause);
}

void KissatGASPISolver::addInitialClauses(const std::vector<simpleClause> &clauses, unsigned nbVars)
{
    /* USE formula instead of max_var */
    gaspi::kissat_set_maxVar(this->solver, nbVars);
    gaspi::kissat_reserve(this->solver, nbVars);

    std::vector<unsigned> sortedVars;

    /* TODO: get the occurences from PRS or SBVA ?? */
    for(int i=0; i<=nbVars; i++)
    {
        sortedVars.push_back(i);
    }

    gaspi::kissat_set_formula(this->solver, clauses.size(), nbVars, sortedVars);

    for (auto &clause : clauses)
    {
        for (int lit : clause)
            gaspi::kissat_add(this->solver, lit);
        gaspi::kissat_add(this->solver, 0);
    }

    this->initialized = true;
    LOG1("The GaspiKissat Solver %d loaded all the %u clauses with %u variables", this->id, clauses.size(), nbVars);
}

void KissatGASPISolver::importClauses(const std::vector<std::shared_ptr<ClauseExchange>> &clauses)
{
    for (auto cls : clauses)
    {
        importClause(cls);
    }
}

void KissatGASPISolver::exportClauses(std::vector<std::shared_ptr<ClauseExchange>> &clauses)
{
    clausesToExport.getClauses(clauses);
}

void KissatGASPISolver::increaseClauseProduction()
{
    lbdLimit++;
}

void KissatGASPISolver::decreaseClauseProduction()
{
    if (lbdLimit > 2)
    {
        lbdLimit--;
    }
}

void KissatGASPISolver::printStatistics()
{
    LOGERROR("Not implemented yet !!");

    gaspi::kissat_print_statistics(this->solver);
}

std::vector<int>
KissatGASPISolver::getModel()
{
    std::vector<int> model;
    unsigned maxVar = gaspi::kissat_get_maxVar(this->solver);

    for (int i = 1; i <= maxVar; i++)
    {
        int tmp = gaspi::kissat_value(solver, i);
        if (!tmp)
            tmp = i;
        model.push_back(tmp);
    }

    return model;
}

std::vector<int>
KissatGASPISolver::getFinalAnalysis()
{
    std::vector<int> outCls;
    return outCls;
}

std::vector<int>
KissatGASPISolver::getSatAssumptions()
{
    std::vector<int> outCls;
    return outCls;
};

void KissatGASPISolver::initkissatGASPIOptions()
{
    LOG3("Initializing Kissat configuration ..");
    // initial configuration (mostly defaults of kissat_mab)

    // Not Needed in practice
#ifndef NDEBUG
    kissatGASPIOptions.insert({"quiet", 0});
    kissatGASPIOptions.insert({"check", 1}); // (only on debug) 1: check model only, 2: check redundant clauses
#else
    kissatGASPIOptions.insert({"quiet", 1});
    kissatGASPIOptions.insert({"check", 0});
#endif
    kissatGASPIOptions.insert({"log", 0});
    kissatGASPIOptions.insert({"verbose", 0}); // verbosity level

    /*
     * stable = 0: always focused mode (more restarts) good for UNSAT
     * stable = 1: switching between stable and focused
     * stable = 2: always in stable mode
     */
    kissatGASPIOptions.insert({"stable", 1});
    /*
     * Used in decide_phase: (kissatMabProp made several modifications in decide_phase):
     * If solver in stable mode or target > 1 it takes the target phase
     */
    kissatGASPIOptions.insert({"target", 1});

    kissatGASPIOptions.insert({"initshuffle", 0}); /* Shuffle variables in queue before kissat_add */

    // Garbage Collection
    kissatGASPIOptions.insert({"compact", 1});     // enable compacting garbage collection
    kissatGASPIOptions.insert({"compactlim", 10}); // compact inactive limit (in percent)

    /*----------------------------------------------------------------------------*/

    // Local search
    kissatGASPIOptions.insert({"walkinitially", 0}); // the other suboptions are not used
    kissatGASPIOptions.insert({"walkstrategy", 3});  // walk (local search) strategy. 0: default, 1: no walk, 2: 5 times walk, 3: select by decision tree
    kissatGASPIOptions.insert({"walkbits", 16});     // number of enumerated variables per walk step (commented in kissat, not used!)
    kissatGASPIOptions.insert({"walkrounds", 1});    // rounds per walking phase (max is 100000), in initial walk and phase walk (during rephasing)

    // Genetic Initialization
    kissatGASPIOptions.insert({"mutation", 60});
    kissatGASPIOptions.insert({"crossover", 50});
    kissatGASPIOptions.insert({"generations", 40});
    kissatGASPIOptions.insert({"population", 20});

    /*----------------------------------------------------------------------------*/

    // (Pre/In)processing Simplifications
    kissatGASPIOptions.insert({"hyper", 1});   // on-the-fly hyper binary resolution
    kissatGASPIOptions.insert({"trenary", 1}); // hyper Trenary Resolution: maybe its too expensive ??
    kissatGASPIOptions.insert({"failed", 1});  // failed literal probing, many suboptions
    kissatGASPIOptions.insert({"reduce", 1});  // learned clause reduction

    // Subsumption options: many other suboptions
    kissatGASPIOptions.insert({"subsumeclslim", 1e3}); // subsumption clause size limit,
    kissatGASPIOptions.insert({"eagersubsume", 20});   // eagerly subsume recently learned clauses (max 100)
    kissatGASPIOptions.insert({"vivify", 1});          // vivify clauses, many suboptions
    kissatGASPIOptions.insert({"otfs", 1});            // on-the-fly strengthening when deducing learned clause

    /*-- Equisatisfiable Simplifications --*/
    kissatGASPIOptions.insert({"substitute", 1}); // equivalent literal substitution, many subsumptions
    kissatGASPIOptions.insert({"autarky", 1});    // autarky reasoning
    kissatGASPIOptions.insert({"eliminate", 1});  // bounded variable elimination (BVE), many suboptions

    // Gates detection
    kissatGASPIOptions.insert({"and", 1});          // and gates detection
    kissatGASPIOptions.insert({"equivalences", 1}); // extract and eliminate equivalence gates
    kissatGASPIOptions.insert({"ifthenelse", 1});   // extract and eliminate if-then-else gates

    // Xor gates
    kissatGASPIOptions.insert({"xors", 0});
    kissatGASPIOptions.insert({"xorsbound", 1});   // minimum elimination bound
    kissatGASPIOptions.insert({"xorsclsslim", 5}); // xor extraction clause size limit

    // Enhancements for variable elimination
    kissatGASPIOptions.insert({"backward", 1}); // backward subsumption in BVE
    kissatGASPIOptions.insert({"forward", 1});  // forward subsumption in BVE
    kissatGASPIOptions.insert({"extract", 1});  // extract gates in variable elimination

    // Delayed versions
    // kissatGASPIOptions.insert({"delay", 2}); // maximum delay (autarky, failed, ...)
    kissatGASPIOptions.insert({"autarkydelay", 1});
    kissatGASPIOptions.insert({"trenarydelay", 1});

    /*----------------------------------------------------------------------------*/

    // Search Configuration
    kissatGASPIOptions.insert({"chrono", 1});         // chronological backtracking, according the original paper, it is benefical for UNSAT
    kissatGASPIOptions.insert({"chronolevels", 100}); // if conflict analysis want to jump more than this amount of levels, chronological will be used

    // Restart Management
    kissatGASPIOptions.insert({"restart", 1});
    kissatGASPIOptions.insert({"restartint", 1});     // base restart interval
    kissatGASPIOptions.insert({"restartmargin", 10}); // fast/slow margin in percent
    kissatGASPIOptions.insert({"reluctant", 1});      // stable reluctant doubling restarting
    kissatGASPIOptions.insert({"reducerestart", 0});  // restart at reduce (1=stable , 2=always)

    // Clause and Literal Related Heuristic
    kissatGASPIOptions.insert({"heuristic", 0}); // 0: VSIDS, 1: CHB
    kissatGASPIOptions.insert({"stepchb", 4});   // CHB step paramater
    kissatGASPIOptions.insert({"tier1", 2});     // glue limit for tier1
    kissatGASPIOptions.insert({"tier2", 6});     // glue limit for tier2

    // Mab Options
    kissatGASPIOptions.insert({"mab", 1}); // enable Multi Armed Bandit for VSIDS and CHB switching at restart

    // Phase
    kissatGASPIOptions.insert({"phase", 1}); /* initial phase: set the macro INITIAL_PHASE */
    kissatGASPIOptions.insert({"phasesaving", 1});
    kissatGASPIOptions.insert({"rephase", 1});    // reinitialization of decision phases, have two suboptions that are never accessed outside of tests
    kissatGASPIOptions.insert({"forcephase", 0}); // force initial phase, forces the target option to false

    /*----------------------------------------------------------------------------*/

    // not understood, yet
    kissatGASPIOptions.insert({"probedelay", 0});
    kissatGASPIOptions.insert({"targetinc", 0});

    // random seed
    kissatGASPIOptions.insert({"seed", this->id}); // used in walk and rephase
}

// Diversify the solver
void KissatGASPISolver::diversify(std::mt19937 &rng_engine, std::uniform_int_distribution<int> &uniform_dist)
{
    if (this->initialized)
    {
        LOGERROR("Diversification must be done before adding clauses because of kissat_reserve()");
        exit(PERR_NOT_SUPPORTED);
    }

    int noise = uniform_dist(rng_engine);

    unsigned maxNoise = Parameters::getIntParam("max-div-noise", 100);

    kissatGASPIOptions.at("seed") = this->id; /* changed if dist */

    /* Half init as all false */
    if (this->id % 2) // == 1
    {
        kissatGASPIOptions.at("phase") = 0;
    }

    // Across all the solvers in case of dist mode (each mpi process may have a different set of solvers)
    switch (this->family)
    {
    // 1/3 Focus on UNSAT
    case SolverDivFamily::UNSAT_FOCUSED:
        kissatGASPIOptions.at("stable") = 0;
        kissatGASPIOptions.at("restartmargin") = 10 + (noise % 5);
        kissatGASPIOptions.at("restartint") = 1; // bigger values are of interest ?
        // Exprimental
        kissatGASPIOptions.at("chronolevels") = 100 - (noise % 20); // more frequent chronological

        /* TODO : test this */
        if (noise <= maxNoise / 4)
        {
            kissatGASPIOptions.at("tier1") = 2;
            kissatGASPIOptions.at("tier2") = 8;
        }
        break;

    // Focus on SAT ; target at 2 to enable target phase
    case SolverDivFamily::SAT_STABLE:
        kissatGASPIOptions.at("target") = 2;              /* checks target phase first */
        kissatGASPIOptions.at("restartint") = 50 + noise; /* less restarts when in focused mode */
        kissatGASPIOptions.at("restartmargin") = noise % 25 + 10;

        // Some solvers (50% chance) do initial walk and walk further in rephasing ( benifical for SAT formulae)
        if (noise <= maxNoise / 2)
        {
            LOG3("Solver %d in the half", this->id);
            kissatGASPIOptions.at("stable") = 2; /* to start at stable and to not switch to focused*/
            kissatGASPIOptions.at("walkinitially") = 1;
            kissatGASPIOptions.at("walkrounds") = (this->id + noise) % (1 << 4); /* TODO: enhance this*/
            /* Oh's expriment showed that learned clauses are not that important for SAT*/
            kissatGASPIOptions.at("tier1") = 2;
            kissatGASPIOptions.at("tier2") = 3;

            // (25% chance) a solver will be on stable mode only with more walkrounds, ccanr and no chronological backtracking
            if (noise <= maxNoise / 4)
            {
                int heuristicNoise = uniform_dist(rng_engine);
                LOG3("Solver %d in the quarter", this->id);
                kissatGASPIOptions.at("chrono") = 0;
                kissatGASPIOptions.at("walkrounds") = (this->id + noise * 10) % (1 << 12); /* TODO: enhance this*/

                // TODO: look more onto the effect of mab on sat and unsat
                kissatGASPIOptions.at("mab") = 0;
                kissatGASPIOptions.at("heuristic") = (heuristicNoise < (maxNoise / 2)); // VSIDS vs CHB
                kissatGASPIOptions.at("stepchb") = (heuristicNoise < (maxNoise / 2)) ? (4 + heuristicNoise) % 9 : 4;
                kissatGASPIOptions.at("reducerestart") = 1;
            }
        }
        break;

    // Switch mode without target phase : TODO better diversification needed here
    default:
        kissatGASPIOptions.at("walkinitially") = 1;
        kissatGASPIOptions.at("walkrounds") = (this->id + noise) % (1 << 3);
        /* Find other diversification parameters : restarts ?*/
        kissatGASPIOptions.at("initshuffle") = 1; /* takes some time */
    }

set_options:
    // Setting the options
    for (auto &opt : kissatGASPIOptions)
    {
        gaspi::kissat_set_option(this->solver, opt.first.c_str(), opt.second);
    }

    // Init options
    gaspi::kissat_mab_init(solver);
    gaspi::kissat_gaspi_init(solver);
    LOG1("Diversification of gaspiKissat %d of family %d with noise %d", this->id, this->family, noise);
}