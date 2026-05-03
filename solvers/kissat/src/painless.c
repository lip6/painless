// Begin Painless
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
  stats->conflicts = kissat_get_conflicts (&solver->statistics);
  stats->propagations = kissat_get_propagations (&solver->statistics);
  stats->restarts = kissat_get_restarts (&solver->statistics);
  stats->decisions = kissat_get_decisions (&solver->statistics);
  stats->memoryPeak =
      0; //((double)kissat_get_allocated_max(&solver->statistics)) / 1024;
  stats->importedUnits =
      kissat_get_clauses_imported_s1 (&solver->statistics);
  stats->importedBinaries =
      kissat_get_clauses_imported_s2 (&solver->statistics);
  stats->importedLarges =
      kissat_get_clauses_imported_sL (&solver->statistics);
  stats->exportedClauses =
      kissat_get_clauses_exported (&solver->statistics);
  stats->filteredExportedClauses =
      kissat_get_clauses_exported_filtered (&solver->statistics);
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

int kissat_assign_punits (kissat *solver, int *external_lits,
                          unsigned size) {
  // Assign a collection of unit literals
  LOGP ("Kissat %u Calling assign_punits", solver->id_painless);

  /*We need to convert all our external literals in to internal ones */
  for (unsigned i = 0; i < size; i++) {
    unsigned external_var = ABS (external_lits[i]);
    if (external_var >= SIZE_STACK (solver->import)) {
      LOGP (" The variable %u is unknown !", external_var);
      continue;
    }
    // Data structure that allows the external->internal translation
    import *import_lit = &PEEK_STACK (solver->import, external_var);
    if (import_lit->eliminated) {
      LOGP ("The literal %d is eliminated in solver %d", external_lits[i],
            solver->id_painless);
      continue;
    }

    // All internal lits are unsigned since they are indices
    unsigned internal_lit =
        (external_lits[i] > 0) ? import_lit->lit : NOT (import_lit->lit);

    /* 0: unassigned, 1: satisfying, -1: falsified */
    value val = VALUE (internal_lit);
    assert (val >= -1 && val <= 1);
    if (0 == val) {
      LOGP ("The literal %d is now assigned in solver %d at root level.",
            external_lits[i], solver->id_painless);
      INC (clauses_imported_s1);
      kissat_assign_unit (solver, internal_lit, "painless reason");
    } else if (-1 == val) {

      LOGP ("The literal %d was falsified in solver %d. Returning false at "
            "root level.",
            external_lits[i], solver->id_painless);
      return 20;
    } /* else if (1 == val) nothing to do since solver already have the same
         unit*/
  }
  return 0;
}

/* Todo a single external_lits with a size array */
unsigned char kissat_import_pclause (kissat *solver,
                                     const int *external_lits,
                                     unsigned size) {
  assert (size);

  LOGP ("Kissat %u Calling import_pclause", solver->id_painless);

  CLEAR_STACK (solver->clause);

  // Converting and copying only useful literals
  for (unsigned i = 0; i < size; i++) {

    unsigned external_var = ABS (external_lits[i]);
    if (external_var >= SIZE_STACK (solver->import)) {
      kissat_fatal (
          "The kissat solver %d doesn't know about the variable %u",
          solver->id_painless, external_var);
    }

    /* Conversion */
    import *import_lit = &PEEK_STACK (solver->import, external_var);
    if (import_lit->eliminated) {
      LOGP ("The kissat solver %d eliminated the variable %u",
            solver->id_painless, external_var);
      // Will not import the clause
      CLEAR_STACK (solver->clause);
      return false;
    }

    unsigned internal_lit =
        (external_lits[i] > 0) ? import_lit->lit : NOT (import_lit->lit);
    /**
     * Since the function is executed at root level we can :
     *     - check if the clause is already satisfied
     *     - copy only non falsified literals
     */
    value val = VALUE (internal_lit);

    if (0 == val) {
      PUSH_STACK (solver->clause, internal_lit);
    } else if (1 == val) {
      LOGP ("The clause at %p is already satisfied in solver %d with "
            "literal %d",
            external_lits, solver->id_painless, external_lits[i]);
      // Will not import the clause
      CLEAR_STACK (solver->clause);
      return false;
    } /* else if (-1 == val) {
      We won't push level 0 falsified literals
    }*/
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
    \nc \t* exported %lu (kept: %lu) clauses\
    \nc \t* imported %lu units\
    \nc \t* imported %lu binaries\
    \nc \t* imported %lu clauses\n",
          solver->id_painless, solver->statistics.conflicts,
          solver->statistics.propagations, solver->statistics.restarts,
          solver->statistics.decisions, solver->statistics.clauses_exported,
          solver->statistics.clauses_exported -
              solver->statistics.clauses_exported_filtered,
          solver->statistics.clauses_imported_s1,
          solver->statistics.clauses_imported_s2,
          solver->statistics.clauses_imported_sL);
}

// Kissat Init

void kissat_set_import_check (kissat *solver,
                              unsigned char (*call) (void *)) {
  solver->cbkHasClauseToImport = call;
}

void kissat_set_import_call (kissat *solver,
                             unsigned char (*call) (void *)) {
  solver->cbkImportClause = call;
}

void kissat_set_export_call (kissat *solver, char (*call) (void *)) {
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
  solver->phases.best[internal_var] = phase; /* for walk */
  if (GET_OPTION (target))
    solver->phases.target[internal_var] = phase; /* for target */
  if (GET_OPTION (phasesaving))
    solver->phases.saved[internal_var] = phase; /* for saved */

  return true;
}

char kissat_check_searches (kissat *solver) {
  return kissat_get_searches (&solver->statistics) > 0;
}