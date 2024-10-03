#define INLINE_ASSIGN

#include "inline.h"
#include "propsearch.h"
#include "bump.h"

// Keep this 'inlined' file separate:

#include "assign.c"

#define PROPAGATE_LITERAL search_propagate_literal
#define PROPAGATION_TYPE "search"

#include "proplit.h"

static inline void
update_search_propagation_statistics (kissat * solver,
				      unsigned previous_propagated_level)
{
  assert (previous_propagated_level <= solver->propagated);
  const unsigned propagated = solver->propagated - previous_propagated_level;

  LOG ("propagation took %" PRIu64 " propagations", propagated);
  LOG ("propagation took %" PRIu64 " ticks", solver->ticks);

  ADD (propagations, propagated);
  ADD (ticks, solver->ticks);

  ADD (search_propagations, propagated);
  ADD (search_ticks, solver->ticks);

  if (solver->stable)
    {
      ADD (stable_propagations, propagated);
      ADD (stable_ticks, solver->ticks);
    }
  else
    {
      ADD (focused_propagations, propagated);
      ADD (focused_ticks, solver->ticks);
    }
}

static inline void
update_consistently_assigned (kissat * solver)
{
  const unsigned assigned = kissat_assigned (solver);
  if (assigned != solver->consistently_assigned)
    {
      LOG ("updating consistently assigned from %u to %u",
	   solver->consistently_assigned, assigned);
      solver->consistently_assigned = assigned;
    }
  else
    LOG ("keeping consistently assigned %u", assigned);
}

static clause *
search_propagate (kissat * solver)
{
  clause *res = 0;
  while (!res && solver->propagated < SIZE_STACK (solver->trail))
    {
      const unsigned lit = PEEK_STACK (solver->trail, solver->propagated);
      res = search_propagate_literal (solver, lit);
      solver->propagated++;
    }
  return res;
}




clause *
kissat_search_propagate (kissat * solver)
{
  assert (!solver->probing);
  assert (solver->watching);
  assert (!solver->inconsistent);

  START (propagate);

  solver->ticks = 0;
  const unsigned propagated = solver->propagated;
  clause *conflict = search_propagate (solver);
  update_search_propagation_statistics (solver, propagated);
  update_consistently_assigned (solver);
  if (conflict) {
    INC (conflicts);
    solver->freeze_ls_restart_num--;
  }
  STOP (propagate);

  // CHB
  if(solver->stable && solver->heuristic==1){
      int i = SIZE_STACK (solver->trail) - 1;
      unsigned lit = i>=0?PEEK_STACK (solver->trail, i):0;  
      while(i>=0 && LEVEL(lit)==solver->level){
	    lit = PEEK_STACK (solver->trail, i);
            kissat_bump_chb(solver,IDX(lit), conflict? 1.0 : 0.9); 
	    i--;	    
      }
  }  
  if(solver->stable && solver->heuristic==1 && conflict) kissat_decay_chb(solver);

  return conflict;
}
