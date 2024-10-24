#include "error.h"
#include "internal.h"
#include "logging.h"
#include "statistics.h"

#include "inline.h"

// Core
unsigned kissat_get_var_count (kissat *solver) { return solver->vars; }

// Statistics
void kissat_get_main_statistics (kissat *solver,
                                 KissatMainStatistics *stats) {
  stats->conflictsPerSec = kissat_get_conflicts (&solver->statistics);
  stats->propagationsPerSec = kissat_get_propagations (&solver->statistics);
  stats->restarts = kissat_get_restarts (&solver->statistics);
  stats->decisionsPerConf = kissat_get_decisions (&solver->statistics);
}

// For Sharing

void kissat_set_pglue (kissat *solver, unsigned new_glue) {
  solver->pglue = new_glue;
}

unsigned kissat_get_pglue (kissat *solver) { return solver->pglue; }

// void kissat_clear_pclause(kissat *solver)
// {
//     CLEAR_STACK(solver->pclause);
//     solver->pglue = 0;
// }

unsigned kissat_pclause_size (kissat *solver) {
  return SIZE_STACK (solver->pclause);
}

// void kissat_push_plit(kissat *solver, int lit)
// {
//     PUSH_STACK(solver->pclause, lit);
// }

// int kissat_pop_plit(kissat *solver)
// {
//     return POP_STACK(solver->pclause);
// }

int kissat_peek_plit (kissat *solver, unsigned idx) {
  return PEEK_STACK (solver->pclause, idx);
}

char kissat_assign_punits (kissat *solver, int *external_lits,
                           unsigned size) {
  LOGP ("Kissat %u Calling assign_punits", solver->id_painless);
  unsigned internal_lit = 0;
  unsigned external_var;
  import *import_lit = NULL;
  for (unsigned i = 0; i < size; i++) {
    external_var = ABS (external_lits[i]);
    if (external_var >= SIZE_STACK (solver->import)) {
      LOGP (" The variable %u is unknown !", external_var);
      continue;
    }
    import_lit = &PEEK_STACK (solver->import, external_var);
    if (import_lit->eliminated) {
      LOGP ("The literal %d is eliminated in solver %d", external_lits[i],
            solver->id_painless);
      continue;
    }

    internal_lit =
        (external_lits[i] > 0) ? import_lit->lit : NOT (import_lit->lit);

    switch (VALUE (internal_lit)) {
    case 0:
      LOGP ("The literal %d is now assigned in solver %d at root level.",
            external_lits[i], solver->id_painless);
      solver->nb_imported_units++;
      kissat_assign_unit (solver, internal_lit, "painless reason");
      break;
    case -1:
      LOGP ("The literal %d was falsified in solver %d. Returning false at "
            "root level.",
            external_lits[i], solver->id_painless);
      return false;
      break;
    case 1:
      // do nothing same as root assignement
      break;
    default:
      kissat_fatal (
          "ERROR, unexpected value %d for literal %d in solver %d!!",
          VALUE (internal_lit), external_lits[i], solver->id_painless);
      return false;
    }
  }
  return true;
}

/* Todo a single external_lits with a size array */
char kissat_import_pclause (kissat *solver, const int *external_lits,
                            unsigned size) {
  import *import_lit = NULL;
  unsigned internal_lit;
  unsigned external_var;

  assert(size);

  LOGP ("Kissat %u Calling import_pclause", solver->id_painless);

  CLEAR_STACK (solver->clause);
  solver->do_not_import = 0;

  // Converting and copying only useful literals

  for (unsigned i = 0; i < size; i++) {
    external_var = ABS (external_lits[i]);
    if (external_var >= SIZE_STACK (solver->import)) {
      LOGP ("The kissat solver %d doesn't know about the variable %u",
            solver->id_painless, external_var);
      solver->do_not_import = 1;
      return true;
    }

    import_lit = &PEEK_STACK (solver->import, external_var);
    if (import_lit->eliminated) {
      LOGP ("The kissat solver %d eliminated the variable %u",
            solver->id_painless, external_var);
      solver->do_not_import = 1;
      return true;
    } else {
      internal_lit =
          (external_lits[i] > 0) ? import_lit->lit : NOT (import_lit->lit);
      /**
       * Since the function is executed at root level we can :
       *     - check if the clause is already sat
       *     - copy only non falsified literals
       */
      switch (VALUE (internal_lit)) {
      case 0:
        PUSH_STACK (solver->clause, internal_lit);
        break;
      case 1:
        LOGP ("The clause at %p is already satisfied in solver %d with "
              "literal %d",
              external_lits, solver->id_painless, external_lits[i]);
        solver->do_not_import = 1;
        return true;
        break;
      case -1:
        LOGP ("The literal %d is falsified pushing clause at %p in solver "
              "%d ",
              external_lits[i], external_lits, solver->id_painless);
        break;
      default:
        kissat_fatal (
            "ERROR, unexpected value %d for literal %d in solver %d!!",
            VALUE (internal_lit), external_lits[i], solver->id_painless);
        return false;
      }
    }
  }
  return true;
}

void kissat_print_sharing_stats (kissat *solver) {
  printf ("c----------[Kissat %d Stats]--------------\
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
          solver->id_painless, solver->statistics.conflicts,
          solver->statistics.propagations, solver->statistics.restarts,
          solver->statistics.decisions, solver->nb_exported,
          solver->nb_exported - solver->nb_exported_filtered,
          solver->nb_imported_units, solver->nb_imported_bin,
          solver->nb_imported_cls);
}

// Kissat Init
void kissat_set_import_unit_call (kissat *solver,
                                  char (*call) (void *, kissat *)) {
  solver->cbkImportUnit = call;
}

void kissat_set_import_call (kissat *solver,
                             char (*call) (void *, kissat *)) {
  solver->cbkImportClause = call;
}

void kissat_set_export_call (kissat *solver,
                             char (*call) (void *, kissat *)) {
  solver->cbkExportClause = call;
}

void kissat_set_painless (kissat *solver, void *painless_kissat) {
  solver->painless = painless_kissat;
}

void kissat_set_id (kissat *solver, int id) { solver->id_painless = id; }

// Interface some internal functions
char kissat_set_phase (kissat *solver, unsigned external_var, int phase) {
  if (external_var >= SIZE_STACK (solver->import)) {
    LOGP (" The kissat solver %d doesn't know about the literal %u",
          solver->id_painless, external_var);
    return false;
  }
  import *import_lit = &PEEK_STACK (solver->import, external_var);
  if (import_lit->eliminated) {
    LOGP (" The kissat solver %d eliminated the variable %u",
          solver->id_painless, external_var);
    return false;
  }
  unsigned internal_var = IDX (import_lit->lit);
  solver->phases.best[internal_var] = phase;   /* for walk */
  solver->phases.target[internal_var] = phase; /* for target */
  solver->phases.saved[internal_var] = phase;  /* for saved */

  return true;
}

char kissat_check_searches (kissat *solver) {
  return kissat_get_searches (&solver->statistics) > 0;
}