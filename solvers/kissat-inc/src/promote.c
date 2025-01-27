#include "internal.h"
#include "logging.h"
#include "promote.h"

unsigned
kissat_inc_recompute_glue (kissat * solver, clause * c)
{
  assert (EMPTY_STACK (solver->promote));
  for (all_literals_in_clause (lit, c))
    {
      assert (VALUE (lit));
      const unsigned level = LEVEL (lit);
      frame *frame = &FRAME (level);
      if (frame->promote)
	continue;
      frame->promote = true;
      PUSH_STACK (solver->promote, level);
    }
  for (all_stack (unsigned, level, solver->promote))
    {
      frame *frame = &FRAME (level);
      assert (frame->promote);
      frame->promote = false;
    }
  unsigned res = SIZE_STACK (solver->promote);
  CLEAR_STACK (solver->promote);
  return res;
}

void
kissat_inc_promote_clause (kissat * solver, clause * c, unsigned new_glue)
{
  assert (!c->keep);
  assert (c->redundant);
  const unsigned old_glue = c->glue;
  assert (new_glue < old_glue);
  const unsigned tier1 = GET_OPTION (tier1);
  const unsigned tier2 = MAX (GET_OPTION (tier2), GET_OPTION (tier1));
  if (c->hyper)
    LOGCLS (c, "promoting to new glue %u", new_glue);
  else if (new_glue <= tier1)
    {
      assert (tier1 < old_glue);
      assert (new_glue <= tier1);
      LOGCLS (c, "promoting to new glue %u to tier1", new_glue);
      INC (clauses_promoted1);
      c->keep = true;
    }
  else if (old_glue > tier2 && new_glue <= tier2)
    {
      assert (tier2 < old_glue);
      assert (tier1 < new_glue && new_glue <= tier2);
      LOGCLS (c, "promoting to new glue %u to tier2", new_glue);
      INC (clauses_promoted2);
      c->used = 2;
    }
  else if (old_glue <= tier2)
    {
      INC (clauses_kept2);
      assert (tier1 < old_glue && old_glue <= tier2);
      assert (tier1 < new_glue && new_glue <= tier2);
      LOGCLS (c, "keeping to new glue %u in tier2", new_glue);
    }
  else
    {
      INC (clauses_kept3);
      assert (tier2 < old_glue);
      assert (tier2 < new_glue);
      LOGCLS (c, "keeping to new glue %u in tier3", new_glue);
    }
  INC (clauses_improved);
  c->glue = new_glue;
#ifndef LOGGING
  (void) solver;
#endif
}
