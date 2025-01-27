#include "backtrack.h"
#include "inline.h"
#include "propsearch.h"
#include "trail.h"

void
kissat_inc_flush_trail (kissat * solver)
{
  assert (!solver->level);
  assert (solver->unflushed);
  assert (!solver->inconsistent);
  assert (kissat_inc_propagated (solver));
  assert (SIZE_STACK (solver->trail) == solver->unflushed);
  LOG ("flushed %zu units from trail", SIZE_STACK (solver->trail));
  CLEAR_STACK (solver->trail);
  solver->unflushed = 0;
  solver->propagated = 0;
}

void
kissat_inc_mark_reason_clauses (kissat * solver, reference start)
{
  LOG ("starting marking reason clauses at clause[%zu]", start);
  assert (!solver->unflushed);
#ifdef LOGGING
  unsigned reasons = 0;
#endif 
  word *arena = BEGIN_STACK (solver->arena); 
  for (all_stack (unsigned, lit, solver->trail))
    { 
      assigned *a = ASSIGNED (lit);
      assert (a->level > 0);
      if (a->binary)
	continue;
      const reference ref = a->reason;
      assert (ref != UNIT);
      if (ref == DECISION)
	continue;
      if (ref < start)
	continue;
      clause *c = (clause *) (arena + ref);
      assert (kissat_inc_clause_in_arena (solver, c)); 
      c->reason = true;
#ifdef LOGGING
      reasons++;
#endif
    }
  LOG ("marked %u reason clauses", reasons);
}

void
kissat_inc_restart_and_flush_trail (kissat * solver)
{
  if (solver->level)
    {
      LOG ("forced restart");
      kissat_inc_backtrack (solver, 0);
    }
#ifndef NDEBUG
  clause *conflict =
#endif
    kissat_inc_search_propagate (solver);   
  assert (!conflict);
  if (solver->unflushed)
    kissat_inc_flush_trail (solver);   
}

bool
kissat_inc_flush_and_mark_reason_clauses (kissat * solver, reference start)
{
  assert (solver->watching);
  assert (!solver->inconsistent);
  assert (kissat_inc_propagated (solver));

  if (solver->unflushed)
    {
      LOG ("need to flush %zu units from trail", solver->unflushed);
      kissat_inc_restart_and_flush_trail (solver);
    }
  else
    {
      LOG ("no need to flush units from trail (all units already flushed)");
      kissat_inc_mark_reason_clauses (solver, start);
    }

  return true;
}

void
kissat_inc_unmark_reason_clauses (kissat * solver, reference start)
{
  LOG ("starting unmarking reason clauses at clause[%zu]", start);
  assert (!solver->unflushed);
#ifdef LOGGING
  unsigned reasons = 0;
#endif
  word *arena = BEGIN_STACK (solver->arena);
  for (all_stack (unsigned, lit, solver->trail))
    {
      assigned *a = ASSIGNED (lit);
      assert (a->level > 0);
      if (a->binary)
	continue;
      const reference ref = a->reason;
      assert (ref != UNIT);
      if (ref == DECISION)
	continue;
      if (ref < start)
	continue;
      clause *c = (clause *) (arena + ref);
      assert (kissat_inc_clause_in_arena (solver, c));
      assert (c->reason);
      c->reason = false;
#ifdef LOGGING
      reasons++;
#endif
    }
  LOG ("unmarked %u reason clauses", reasons);
}
