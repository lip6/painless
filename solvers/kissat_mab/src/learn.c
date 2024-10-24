#include "backtrack.h"
#include "inline.h"
#include "learn.h"
#include "reluctant.h"
#include "error.h"

#include <inttypes.h>

static unsigned
determine_new_level(kissat *solver, unsigned jump)
{
  assert(solver->level);
  const unsigned back = solver->level - 1;
  assert(jump <= back);

  const unsigned delta = back - jump;
  const unsigned limit =
      GET_OPTION(chrono) ? (unsigned)GET_OPTION(chronolevels) : UINT_MAX;

  unsigned res;

  if (!delta)
  {
    res = jump;
    LOG("using identical backtrack and jump level %u", res);
  }
  else if (delta > limit)
  {
    res = back;
    LOG("backjumping over %u levels (%u - %u) considered inefficient",
        delta, back, jump);
    LOG("backtracking chronologically to backtrack level %u", res);
    INC(chronological);
  }
  else
  {
    res = jump;
    LOG("backjumping over %u levels (%u - %u) considered efficient",
        delta, back, jump);
    LOG("backjumping non-chronologically to jump level %u", res);
  }

  return res;
}

static void
learn_unit(kissat *solver)
{
  const unsigned *lits = BEGIN_STACK(solver->clause.lits);
  const unsigned not_uip = lits[0];
  LOG("learned unit clause %s triggers iteration", LOGLIT(not_uip));
  const unsigned new_level = determine_new_level(solver, 0);
  kissat_mab_backtrack(solver, new_level);
  kissat_mab_assign_unit(solver, not_uip);
  solver->iterating = true;
  CHECK_AND_ADD_UNIT(not_uip);
  ADD_UNIT_TO_PROOF(not_uip);
}

static void
learn_binary(kissat *solver)
{
  const unsigned *lits = BEGIN_STACK(solver->clause.lits);
  const unsigned not_uip = lits[0];
  const unsigned other = lits[1];
  const unsigned jump_level = LEVEL(other);
  const unsigned new_level = determine_new_level(solver, jump_level);
  kissat_mab_backtrack(solver, new_level);
  const unsigned glue = SIZE_STACK(solver->levels);
  assert(glue == 1);
#ifndef NDEBUG
  const reference ref =
#endif
      kissat_mab_new_redundant_clause(solver, glue);
  assert(ref == INVALID_REF);
  kissat_mab_assign_binary(solver, true, not_uip, other);
  kissat_mab_eager_subsume(solver);
}

static void
learn_reference(kissat *solver)
{
  assert(solver->level > 1);
  unsigned *lits = BEGIN_STACK(solver->clause.lits);
  const unsigned not_uip = lits[0];
  unsigned *q = lits + 1;
  unsigned jump_lit = *q;
  unsigned jump_level = LEVEL(jump_lit);
  const unsigned *end = END_STACK(solver->clause.lits);
  const unsigned backtrack_level = solver->level - 1;
  assigned *all_assigned = solver->assigned;
  for (unsigned *p = lits + 2; p != end; p++)
  {
    const unsigned lit = *p;
    const unsigned idx = IDX(lit);
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
  const unsigned glue = SIZE_STACK(solver->levels);
  const reference ref = kissat_mab_new_redundant_clause(solver, glue);
  assert(ref != INVALID_REF);
  clause *c = kissat_mab_dereference_clause(solver, ref);
  c->used = 1 + (glue <= (unsigned)GET_OPTION(tier2));
  const unsigned new_level = determine_new_level(solver, jump_level);
  kissat_mab_backtrack(solver, new_level);
  kissat_mab_assign_reference(solver, not_uip, ref, c);
  kissat_mab_eager_subsume(solver);
  kissat_mab_push_clueue(&solver->clueue, ref);
}

// Begin Painless
static inline void kissat_mab_export_to_painless(kissat *solver, unsigned glue, unsigned size)
{
  solver->nb_exported++;

  if (NULL == solver->cbkExportClause)
  {
    LOGP("The function pointer export_clause_to_painless is NULL in solver %d!", solver->id_painless);
    return;
  }
  else
  {
    CLEAR_STACK(solver->pclause.lits);

    unsigned internal_lit;
    int external_lit;

    // Convert to external literals and copy them
    for (unsigned i = 0; i < size; i++)
    {
      internal_lit = PEEK_STACK(solver->clause.lits, i);
      /* External var */
      external_lit = PEEK_STACK(solver->exportk, IDX(internal_lit)); /* IDX(internal_lit) = internal_var */
      external_lit = (NEGATED(internal_lit)) ? -external_lit : external_lit;
      PUSH_STACK(solver->pclause.lits, external_lit);
    }
    assert(SIZE_STACK(solver->clause.lits) == SIZE_STACK(solver->pclause.lits));

    // Copy lbd
    solver->pclause.glue = glue;

    if (!solver->cbkExportClause(solver->painless, solver))
      solver->nb_exported_filtered++;
  }
}
// End Painless

void kissat_mab_learn_clause(kissat *solver)
{
  if (!solver->probing)
    INC(learned);
  if (solver->stable)
    kissat_mab_tick_reluctant(&solver->reluctant);
  const unsigned glue = SIZE_STACK(solver->levels);
  const unsigned size = SIZE_STACK(solver->clause.lits);
  LOG("learned[%" PRIu64 "] clause glue %u size %u",
      GET(learned), glue, size);
  if (!solver->probing)
  {
    ADD(literals_learned, size);
    UPDATE(size, size);
    UPDATE(fast_glue, glue);
    UPDATE(slow_glue, glue);
  }
  assert(size > 0);
  if (size == 1)
    learn_unit(solver);
  else if (size == 2)
    learn_binary(solver);
  else
    learn_reference(solver);

  // Begin Painless
  kissat_mab_export_to_painless(solver, glue, size);
  // End Painless
}

// Begin Painless

/**
 * The callback doesn't check anything, it fills solver->pclause.lits with external lits !!
 * This function must check:
 *  - If literal is eliminated
 *  - If literal was already assigned, and if conflict return UNSAT
 */
bool kissat_mab_import_unit_from_painless(kissat *solver)
{
  if (NULL == solver->cbkImportUnit)
  {
    LOGP("The function pointer cbkImportUnit is NULL in solver %d!", solver->id_painless);
    return true;
  }

  if (false == solver->cbkImportUnit(solver->painless, solver))
  {
    // LOGP("Function cbkImportUnit in solver %d didn't import any unit.", solver->id_painless);
    return true;
  }

  // else there are units to assign

  unsigned internal_lit = 0;
  unsigned external_var;
  import *import_lit = NULL;
  for (all_stack(int, external_lit, solver->pclause.lits))
  {
    /* imports stack is indexed by external variable value */
    external_var = ABS(external_lit);
    if (external_var >= SIZE_STACK(solver->import))
    {
      LOGP(" The variable %u is unknown !", external_var);
      continue;
    }
    import_lit = &PEEK_STACK(solver->import, external_var);
    if (import_lit->eliminated)
    {
      LOGP("The literal %d is eliminated in solver %d", external_lit, solver->id_painless);
      continue;
    }

    internal_lit = (external_lit > 0) ? import_lit->lit : NOT(import_lit->lit);

    switch (VALUE(internal_lit))
    {
    case 0:
      LOGP("The literal %d is now assigned in solver %d at root level.", external_lit, solver->id_painless);
      solver->nb_imported_units++;
      kissat_mab_assign_unit(solver, internal_lit);
      break;
    case -1:
      LOGP("The literal %d was falsified in solver %d. Returning false at root level.", external_lit, solver->id_painless);
      return false;
      break;
    case 1:
      // do nothing same root assignement
      break;
    default:
      kissat_mab_fatal("ERROR, unexpected value %d for literal %d in solver %d!!", VALUE(internal_lit), external_lit, solver->id_painless);
      return false;
    }
  }
  return true;
}

/**
 * Callback checks before copying internal literals to solver->clause:
 *  - if the clause contains an eliminated variable (ignores it)
 *  - if the clause is already satisfied by root affectation (no need for it)
 * The callback copies only unassigned literals inside clause.lits
 */
bool kissat_mab_import_from_painless(kissat *solver)
{
  if (NULL == solver->cbkImportClause)
  {
    LOGP("The function pointer import_clause_from_painless is NULL in solver %d!", solver->id_painless);
    return true;
  }

  unsigned size;
  reference new_clause_ref;

  while (solver->cbkImportClause(solver->painless, solver))
  {
    int internal_lit;
    /* In case contains an eliminated literal or is already satisfied */
    if (solver->clause.satisfied)
    {
      continue;
    }

    size = SIZE_STACK(solver->clause.lits);

    switch (size)
    {
    /* All literals are falsified */
    case 0:
      LOGP("The solver %d received an empty clause. Returns UNSAT!", solver->id_painless);
      return false;
      break;
    /* Only one unassigned */
    case 1:
      internal_lit = PEEK_STACK(solver->clause.lits, 0);
      // LOGP("The solver %d received a clause with only one unassigned internal literal %d.", solver->id_painless, internal_lit);
      solver->nb_imported_units++;
      kissat_mab_assign_unit(solver, internal_lit);
      break;
    /* size is used as the glue value */
    case 2:
      /* Inspired by learn_binary */
      /* Unfortunaetly new_binary is a static function */
      // LOGP("The solver %d received a clause with only two unassigned internal literals.", solver->id_painless);
      new_clause_ref = kissat_mab_new_redundant_clause(solver, size);
      assert(new_clause_ref == INVALID_REF); /*Since binaries are stored directly in watch lists, i.e no struct clause allocation */
      solver->nb_imported_bin++;
      kissat_mab_eager_subsume(solver);
      break;
    /* Else: size > 2*/
    default:
      /*Inspired by learn_reference*/
      // LOGP("The solver %d received a clause with %d unassigned internal literal.", solver->id_painless, size);
      new_clause_ref = kissat_mab_new_redundant_clause(solver, size);
      assert(new_clause_ref != INVALID_REF);
      solver->nb_imported_cls++;

      clause *new_clause = kissat_mab_dereference_clause(solver, new_clause_ref);
      new_clause->used = 1 + (size <= (unsigned)GET_OPTION(tier2)); // not understood, yet
      kissat_mab_eager_subsume(solver);
      kissat_mab_push_clueue(&solver->clueue, new_clause_ref); // not understood, yet
    }
  }

  CLEAR_STACK(solver->clause.lits);

  return true;
}
// End Painless