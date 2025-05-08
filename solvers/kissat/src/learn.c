#include "learn.h"
#include "backtrack.h"
#include "inline.h"
#include "reluctant.h"

#include <inttypes.h>

static unsigned backjump_limit (struct kissat *solver) {
#ifdef NOPTIONS
  (void) solver;
#endif
  return GET_OPTION (chrono) ? (unsigned) GET_OPTION (chronolevels)
                             : UINT_MAX;
}

unsigned kissat_determine_new_level (kissat *solver, unsigned jump) {
  assert (solver->level);
  const unsigned back = solver->level - 1;
  assert (jump <= back);

  const unsigned delta = back - jump;
  const unsigned limit = backjump_limit (solver);

  unsigned res;

  if (!delta) {
    res = jump;
    LOG ("using identical backtrack and jump level %u", res);
  } else if (delta > limit) {
    res = back;
    LOG ("backjumping over %u levels (%u - %u) considered inefficient",
         delta, back, jump);
    LOG ("backtracking chronologically to backtrack level %u", res);
    INC (chronological);
  } else {
    res = jump;
    LOG ("backjumping over %u levels (%u - %u) considered efficient", delta,
         back, jump);
    LOG ("backjumping non-chronologically to jump level %u", res);
  }
  return res;
}

static void learn_unit (kissat *solver, unsigned not_uip) {
  assert (not_uip == PEEK_STACK (solver->clause, 0));
  LOG ("learned unit clause %s triggers iteration", LOGLIT (not_uip));
  const unsigned new_level = kissat_determine_new_level (solver, 0);
  kissat_backtrack_after_conflict (solver, new_level);
  kissat_learned_unit (solver, not_uip);
  if (!solver->probing) {
    solver->iterating = true;
    INC (iterations);
  }
}

static void learn_binary (kissat *solver, unsigned not_uip) {
  const unsigned other = PEEK_STACK (solver->clause, 1);
  const unsigned jump_level = LEVEL (other);
  const unsigned new_level =
      kissat_determine_new_level (solver, jump_level);
  kissat_backtrack_after_conflict (solver, new_level);
#ifndef NDEBUG
  const reference ref =
#endif
      kissat_new_redundant_clause (solver, 1);
  assert (ref == INVALID_REF);
  kissat_assign_binary (solver, not_uip, other);
}

static void insert_last_learned (kissat *solver, reference ref) {
  const reference *const end =
      solver->last_learned + GET_OPTION (eagersubsume);
  reference prev = ref;
  for (reference *p = solver->last_learned; p != end; p++) {
    reference tmp = *p;
    *p = prev;
    prev = tmp;
  }
}

static reference learn_reference (kissat *solver, unsigned not_uip,
                                  unsigned glue) {
  assert (solver->level > 1);
  assert (SIZE_STACK (solver->clause) > 2);
  unsigned *lits = BEGIN_STACK (solver->clause);
  assert (lits[0] == not_uip);
  unsigned *q = lits + 1;
  unsigned jump_lit = *q;
  unsigned jump_level = LEVEL (jump_lit);
  const unsigned *const end = END_STACK (solver->clause);
  const unsigned backtrack_level = solver->level - 1;
  assigned *all_assigned = solver->assigned;
  for (unsigned *p = lits + 2; p != end; p++) {
    const unsigned lit = *p;
    const unsigned idx = IDX (lit);
    const unsigned level = all_assigned[idx].level;
    if (jump_level >= level)
      continue;
    jump_level = level;
    jump_lit = lit;
    q = p;
    if (level == backtrack_level)
      break;
  }
  *q = lits[1];
  lits[1] = jump_lit;
  const reference ref = kissat_new_redundant_clause (solver, glue);
  assert (ref != INVALID_REF);
  clause *c = kissat_dereference_clause (solver, ref);
  c->used = MAX_USED;
  const unsigned new_level =
      kissat_determine_new_level (solver, jump_level);
  kissat_backtrack_after_conflict (solver, new_level);
  kissat_assign_reference (solver, not_uip, ref, c);
  return ref;
}

void kissat_update_learned (kissat *solver, unsigned glue, unsigned size) {
  assert (!solver->probing);
  INC (clauses_learned);
  LOG ("learned[%" PRIu64 "] clause glue %u size %u", GET (clauses_learned),
       glue, size);
  if (solver->stable)
    kissat_tick_reluctant (&solver->reluctant);
  ADD (literals_learned, size);
#ifndef QUIET
  UPDATE_AVERAGE (size, size);
#endif
  UPDATE_AVERAGE (fast_glue, glue);
  UPDATE_AVERAGE (slow_glue, glue);
}

static void flush_last_learned (kissat *solver) {
  reference *q = solver->last_learned;
  const reference *const end = q + GET_OPTION (eagersubsume), *p = q;
  while (p != end) {
    reference ref = *p++;
    if (ref != INVALID_REF)
      *q++ = ref;
  }
  while (q != end)
    *q++ = INVALID_REF;
}

static void eagerly_subsume_last_learned (kissat *solver) {
  value *marks = solver->marks;
  for (all_stack (unsigned, lit, solver->clause)) {
    assert (!marks[lit]);
    marks[lit] = 1;
  }
  unsigned clause_size = SIZE_STACK (solver->clause);
  unsigned subsumed = 0;
  reference *p = solver->last_learned;
  const reference *const end = p + GET_OPTION (eagersubsume);
  while (p != end) {
    reference ref = *p++;
    if (ref == INVALID_REF)
      continue;
    clause *c = kissat_dereference_clause (solver, ref);
    if (c->garbage)
      continue;
    if (!c->redundant)
      continue;
    unsigned c_size = c->size;
    if (c_size <= clause_size)
      continue;
    LOGCLS2 (c, "trying to eagerly subsume");
    unsigned needed = clause_size;
    unsigned remain = c_size;
    for (all_literals_in_clause (lit, c)) {
      if (marks[lit] && !--needed)
        break;
      else if (--remain < needed)
        break;
    }
    if (needed)
      continue;
    LOGCLS (c, "eagerly subsumed");
    kissat_mark_clause_as_garbage (solver, c);
    p[-1] = INVALID_REF;
    subsumed++;
    INC (eagerly_subsumed);
  }
  for (all_stack (unsigned, lit, solver->clause))
    marks[lit] = 0;
  if (subsumed)
    flush_last_learned (solver);
}

// Begin Painless
static inline void kissat_export_to_painless (kissat *solver, unsigned glue,
                                              unsigned size) {
  solver->nb_exported++;

  if (NULL == solver->cbkExportClause) {
    LOGP ("The function pointer export_clause_to_painless is NULL in "
          "solver %d!",
          solver->id_painless);
    return;
  }

  CLEAR_STACK (solver->pclause);

  unsigned internal_lit;
  int external_lit;

  // Convert to external literals and copy them
  for (unsigned i = 0; i < size; i++) {
    internal_lit = PEEK_STACK (solver->clause, i);
    /* External var */
    external_lit = PEEK_STACK (
        solver->export,
        IDX (internal_lit)); /* IDX(internal_lit) = internal_var */
    external_lit = (NEGATED (internal_lit)) ? -external_lit : external_lit;
    PUSH_STACK (solver->pclause, external_lit);
  }
  assert (SIZE_STACK (solver->clause) == SIZE_STACK (solver->pclause));

  // Copy lbd
  solver->pglue = glue;

  if (!solver->cbkExportClause (solver->painless, solver))
    solver->nb_exported_filtered++;
}
// End Painless

void kissat_learn_clause (kissat *solver) {
  const unsigned not_uip = PEEK_STACK (solver->clause, 0);
  const unsigned size = SIZE_STACK (solver->clause);
  const size_t glue = SIZE_STACK (solver->levels);
  assert (glue <= UINT_MAX);
  if (!solver->probing)
    kissat_update_learned (solver, glue, size);
  assert (size > 0);
  reference ref = INVALID_REF;
  if (size == 1)
    learn_unit (solver, not_uip);
  else if (size == 2)
    learn_binary (solver, not_uip);
  else
    ref = learn_reference (solver, not_uip, glue);
  if (GET_OPTION (eagersubsume)) {
    eagerly_subsume_last_learned (solver);
    if (ref != INVALID_REF)
      insert_last_learned (solver, ref);
  }

  // Begin Painless
  kissat_export_to_painless (solver, glue, size);
  // End Painless
}

/**
 * The callback does everything, it checks:
 *  - If literal is eliminated
 *  - If literal was already assigned, and if conflict return UNSAT
 * Before assigning any unit.
 */
bool kissat_import_unit_from_painless (kissat *solver) {
  if (NULL == solver->cbkImportUnit) {
    LOGP ("The function pointer cbkImportUnit is NULL in solver %d!",
          solver->id_painless);
    return true;
  }

  return solver->cbkImportUnit (solver->painless, solver);
}

/**
 * Callback checks before copying internal literals to solver->pclause:
 *  - if the clause contains an eliminated variable (ignores it)
 *  - if the clause is already satisfied by root affectation (no need for
 * it) The callback copies only unassigned literals inside clause.lits
 */
bool kissat_import_from_painless (kissat *solver) {
  if (NULL == solver->cbkImportClause) {
    LOGP ("The function pointer import_clause_from_painless is NULL in "
          "solver %d!",
          solver->id_painless);
    return true;
  }

  unsigned size;

  while ((solver->cbkImportClause (solver->painless, solver))) {
    // Adding the new clause loaded in pclause

    /* If already satisfied or containing eliminated/unknown literals */
    if (solver->do_not_import) {
      continue;
    }

    size = SIZE_STACK (solver->clause);

    assert(size);

#ifndef NDEBUG
    reference new_clause_ref;
#endif
    switch (size) {
    /* All literals are falsified */
    case 0:
      LOGP ("The solver %d received an empty clause. Returns UNSAT!",
            solver->id_painless);
      CLEAR_STACK (solver->clause);
      return false;
      break;
      /* Only one unassigned */
    case 1:
      LOGP ("The solver %d received a clause with only one unassigned "
            "internal literal %d.",
            solver->id_painless, PEEK_STACK (solver->clause, 0));
      solver->nb_imported_units++;
      kissat_assign_unit (solver, PEEK_STACK (solver->clause, 0),
                          "painless reason");
      break;
    case 2:
      /* Inspired by learn_binary */
      LOGP ("The solver %d received a clause with only two unassigned "
            "internal literals.",
            solver->id_painless);
#ifndef NDEBUG
      new_clause_ref =
#endif
          kissat_new_redundant_clause (solver, 1);
      assert (new_clause_ref ==
              INVALID_REF); /*Since binaries are stored directly in watch
                               lists, i.e no struct clause allocation */
      solver->nb_imported_bin++;
      break;
      /* Else: size > 2*/
      /* size is used as the glue value */
    default:
      /*Inspired by learn_reference*/
      LOGP ("The solver %d received a clause with %d unassigned internal "
            "literal.",
            solver->id_painless, size);
      assert(solver->pglue);
#ifndef NDEBUG
      new_clause_ref =
#endif
          kissat_new_redundant_clause (solver, solver->pglue);
      assert (new_clause_ref != INVALID_REF);
      solver->nb_imported_cls++;
    }
  }
  CLEAR_STACK (solver->clause);
  return true;
}
// End Painless