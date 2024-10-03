#include "internal.h"
#include "logging.h"
#include "error.h"

// For Sharing
void gaspi::kissat_clear_pclause(kissat *solver)
{
    CLEAR_STACK(solver->pclause.lits);
    solver->pclause.glue = 0;
}

unsigned gaspi::kissat_pclause_size(kissat *solver)
{
    return SIZE_STACK(solver->pclause.lits);
}

void gaspi::kissat_push_plit(kissat *solver, int lit)
{
    PUSH_STACK(solver->pclause.lits, lit);
}

int gaspi::kissat_pop_plit(kissat *solver)
{
    return POP_STACK(solver->pclause.lits);
}

int gaspi::kissat_peek_plit(kissat *solver, unsigned idx)
{
    return PEEK_STACK(solver->pclause.lits, idx);
}

void gaspi::kissat_set_pglue(kissat *solver, unsigned new_glue)
{
    solver->pclause.glue = new_glue;
}

unsigned gaspi::kissat_get_pglue(kissat *solver)
{
    return solver->pclause.glue;
}

char gaspi::kissat_push_lits(kissat *solver, const int *external_lits, unsigned size)
{
    CLEAR_STACK(solver->clause.lits);
    solver->clause.satisfied = false; /* used to ignore statified clauses and ones with an eliminated variable */
    solver->clause.shrink = false;
    solver->clause.trivial = false;

    import *import_lit = NULL;
    unsigned internal_lit;
    unsigned external_var;

    for (unsigned i = 0; i < size; i++)
    {
        external_var = ABS(external_lits[i]);
        if (external_var >= SIZE_STACK(solver->import))
        {
            LOGP(" The kissat solver %d doesn't know about the variable %u", solver->id_painless, external_var);
            solver->clause.satisfied = true;
            return false;
        }

        import_lit = &PEEK_STACK(solver->import, external_var);
        if (import_lit->eliminated)
        {
            solver->clause.satisfied = true;
            return true;
        }
        else
        {
            internal_lit = (external_lits[i] > 0) ? import_lit->lit : NOT(import_lit->lit);
            /**
             * Since the function is used at root level we can :
             *     - check if the clause is already sat
             *     - copy only non falsified literals
             */
            switch (VALUE(internal_lit))
            {
            case 0:
                PUSH_STACK(solver->clause.lits, internal_lit);
                break;
            case 1:
                solver->clause.satisfied = true;
                LOGP("The clause at %p is already satisfied in solver %d with literal %d", external_lits, solver->id_painless, external_lits[i]);
                return true;
                break;
            case -1:
                LOGP("The literal %d is falsified in clause at %p in solver %d ", external_lits[i], external_lits, solver->id_painless);
                break;
            default:
                kissat_fatal("ERROR, unexpected value %d for literal %d in solver %d!!", VALUE(internal_lit), external_lits[i], solver->id_painless);
                return false;
            }
        }
    }
    return true;
}

void gaspi::kissat_print_sharing_stats(kissat *solver)
{
    printf("c----------[Kissat %d Stats]--------------\
    \nc General:\
    \nc --------\
    \nc \t* Conflicts: %lu\
    \nc \t* Propagations: %lu\
    \nc \t* Restarts: %lu\
    \nc \t* Decisions: %lu\
    \nc Sharing:\
    \nc --------\
    \nc \t* exported %u (kept: %u) clauses\
    \nc \t* imported %u units\
    \nc \t* imported %u binaries\
    \nc \t* imported %u clauses\n",
           solver->id_painless, solver->statistics.conflicts, solver->statistics.propagations, solver->statistics.restarts, solver->statistics.decisions,
           solver->nb_exported, solver->nb_exported - solver->nb_exported_filtered, solver->nb_imported_units, solver->nb_imported_bin, solver->nb_imported_cls);
}

// Kissat Init
void gaspi::kissat_set_import_unit_call(kissat *solver, char (*call)(void *, kissat *))
{
    solver->cbkImportUnit = call;
}

void gaspi::kissat_set_import_call(kissat *solver, char (*call)(void *, kissat *))
{
    solver->cbkImportClause = call;
}

void gaspi::kissat_set_export_call(kissat *solver, char (*call)(void *, kissat *))
{
    solver->cbkExportClause = call;
}

void gaspi::kissat_set_painless(kissat *solver, void *painless_gkissat)
{
    solver->painless = painless_gkissat;
}

void gaspi::kissat_set_id(kissat *solver, int id)
{
    solver->id_painless = id;
}

void gaspi::kissat_set_maxVar(kissat *solver, unsigned maxVar)
{
    solver->max_var = maxVar;
}

unsigned gaspi::kissat_get_maxVar(kissat *solver)
{
    return solver->max_var;
}

// Moved from application.c
void gaspi::kissat_mab_init(struct kissat *solver)
{
    solver->step_chb = 0.1 * GET_OPTION(stepchb);
    solver->heuristic = GET_OPTION(heuristic);
    solver->mab = GET_OPTION(mab);
    if (solver->mab)
    {
        for (unsigned i = 0; i < solver->mab_heuristics; i++)
        {
            solver->mab_reward[i] = 0;
            solver->mab_select[i] = 0;
        }
        solver->mabc = GET_OPTION(mabcint) + 0.1 * GET_OPTION(mabcdecimal);
        solver->mab_select[solver->heuristic]++;
    }
}

void gaspi::kissat_gaspi_init(struct kissat *solver)
{
    // Moved from application.c
    solver->crossover_rate = GET_OPTION(crossover) / 100.0;
    solver->mutation_rate = GET_OPTION(mutation) / 100.0;
    solver->max_generations = GET_OPTION(generations);
    solver->population_size = GET_OPTION(population);
}

void gaspi::kissat_set_formula(kissat *solver, int nbClauses, int nbVars , std::vector<unsigned> &sorted_variables)
{
    solver->formula.setNumClauses(nbClauses);
    solver->formula.setNumVariables(nbVars);
    solver->formula.set_degree_centrality_variables(sorted_variables);
}

// Interface some internal functions
void gaspi::kissat_set_phase(kissat *solver, unsigned external_var, int phase)
{
    if (external_var >= SIZE_STACK(solver->import))
    {
        LOGP(" The kissat solver %d doesn't know about the literal %u", solver->id_painless, external_var);
        solver->clause.satisfied = true;
        return;
    }
    import *import_lit = &PEEK_STACK(solver->import, external_var);
    if (import_lit->eliminated)
    {
        LOGP(" The kissat solver %d eliminated the variable %u", solver->id_painless, external_var);
        return;
    }
    unsigned internal_var = IDX(import_lit->lit);
    solver->phases[internal_var].best = phase;   /* for walk */
    solver->phases[internal_var].target = phase; /* for target */
    solver->phases[internal_var].saved = phase;  /* for saved */
}

char gaspi::kissat_check_searches(kissat *solver)
{
    return kissat_get_searches(&solver->statistics) > 0;
}

void gaspi::kissat_check_model(kissat *solver)
{
#ifndef NDEBUG
    kissat_check_satisfying_assignment(solver);
#else
    return;
#endif
}