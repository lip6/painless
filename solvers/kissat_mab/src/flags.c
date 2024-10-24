#include "inline.h"

static void
activate_literal(kissat *solver, unsigned lit)
{
  const unsigned idx = IDX(lit);
  flags *f = FLAGS(idx);
  if (f->active)
    return;
  lit = STRIP(lit); // sure to get the positive literal
  LOG("activating variable %u", idx);
  f->active = true;
  assert(!f->fixed);
  assert(!f->eliminated);
  solver->active++;
  kissat_mab_enqueue(solver, idx); // add the variable of lit to the solver->queue
  // MAB
  if (solver->mab)
  {
    kissat_mab_push_heap(solver, &solver->scores, idx);
    kissat_mab_push_heap(solver, &solver->scores_chb, idx);
  }
  else
    kissat_mab_push_heap(solver, solver->heuristic == 0 ? &solver->scores : &solver->scores_chb, idx);

  assert(solver->unassigned < UINT_MAX);
  solver->unassigned++;
  kissat_mab_mark_removed_literal(solver, lit); // activate the eliminate flag for the variable idx
  kissat_mab_mark_added_literal(solver, lit);   // activate the subsume flag for the variable idx
  assert(!VALUE(lit));                      // idx is not assigned
  assert(!VALUE(NOT(lit)));                 // idx is not assigned
  assert(!SAVED(idx));
  assert(!TARGET(idx));
  assert(!BEST(idx));
}

static void
deactivate_variable(kissat *solver, flags *f, unsigned idx)
{
  assert(solver->flags + idx == f);
  LOG("deactivating variable %u", idx);
  assert(f->active);
  assert(f->eliminated || f->fixed);
  f->active = false;
  assert(solver->active > 0);
  solver->active--;
  kissat_mab_dequeue(solver, idx);
  // MAB
  if (solver->mab)
  {
    if (kissat_mab_heap_contains(&solver->scores, idx))
      kissat_mab_pop_heap(solver, &solver->scores, idx);
    if (kissat_mab_heap_contains(&solver->scores_chb, idx))
      kissat_mab_pop_heap(solver, &solver->scores_chb, idx);
  }
  else
  {
    if (kissat_mab_heap_contains(solver->heuristic == 0 ? &solver->scores : &solver->scores_chb, idx))
      kissat_mab_pop_heap(solver, solver->heuristic == 0 ? &solver->scores : &solver->scores_chb, idx);
  }
}

void kissat_mab_activate_literal(kissat *solver, unsigned lit)
{
  activate_literal(solver, lit);
}

void kissat_mab_activate_literals(kissat *solver, unsigned size, unsigned *lits)
{
  for (unsigned i = 0; i < size; i++)
    activate_literal(solver, lits[i]);
}

void kissat_mab_mark_fixed_literal(kissat *solver, unsigned lit)
{
  assert(VALUE(lit) > 0);
  const unsigned idx = IDX(lit);
  LOG("marking internal variable %u as fixed", idx);
  flags *f = FLAGS(idx);
  assert(f->active);
  assert(!f->eliminated);
  assert(!f->fixed);
  f->fixed = true;
  deactivate_variable(solver, f, idx);
  INC(units);
  int elit = kissat_mab_export_literal(solver, lit);
  assert(elit);
  PUSH_STACK(solver->units, elit);
  LOG("pushed external unit literal %d (internal %u)", elit, lit);
}

void kissat_mab_mark_eliminated_variable(kissat *solver, unsigned idx)
{
  const unsigned lit = LIT(idx);
  assert(!VALUE(lit));
  LOG("marking internal variable %u as eliminated", idx);
  flags *f = FLAGS(idx);
  assert(f->active);
  assert(!f->eliminated);
  assert(!f->fixed);
  f->eliminated = true;
  deactivate_variable(solver, f, idx);
  int elit = kissat_mab_export_literal(solver, lit);
  assert(elit);
  assert(elit != INT_MIN);
  unsigned eidx = ABS(elit);
  import *import = &PEEK_STACK(solver->import, eidx);
  assert(!import->eliminated);
  assert(import->imported);
  assert(STRIP(import->lit) == STRIP(lit));
  size_t pos = SIZE_STACK(solver->eliminated);
  assert(pos < (1u << 30));
  import->lit = pos;
  import->eliminated = true;
  PUSH_STACK(solver->eliminated, (value)0);
  LOG("marked external variable %u as eliminated", eidx);
  assert(solver->unassigned > 0);
  solver->unassigned--;
}

void kissat_mab_mark_removed_literals(kissat *solver, unsigned size, unsigned *lits)
{
  for (unsigned i = 0; i < size; i++)
    kissat_mab_mark_removed_literal(solver, lits[i]);
}

void kissat_mab_mark_added_literals(kissat *solver, unsigned size, unsigned *lits)
{
  for (unsigned i = 0; i < size; i++)
    kissat_mab_mark_added_literal(solver, lits[i]);
}
