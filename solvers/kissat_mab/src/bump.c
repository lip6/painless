#include "bump.h"
#include "internal.h"
#include "logging.h"
#include "print.h"
#include "rank.h"
#include "sort.h"

static inline unsigned
rank_of_idxrank (idxrank ir)
{
  return ir.rank;
}

static inline bool
smaller_idxrank (idxrank ir, idxrank jr)
{
  return ir.rank < jr.rank;
}

#define RADIX_SORT_BUMP_LIMIT 800
#define RADIX_SORT_BUMP_LENGTH 8

static void
sort_bump (kissat * solver)
{
  const size_t size = SIZE_STACK (solver->analyzed);
  if (size < RADIX_SORT_BUMP_LIMIT)
    {
      LOG ("quick sorting %zu analyzed variables", size);
      SORT_STACK (idxrank, solver->bump, smaller_idxrank);
    }
  else
    {
      LOG ("radix sorting %zu analyzed variables", size);
      RADIX_STACK (RADIX_SORT_BUMP_LENGTH, idxrank,
		   unsigned, solver->bump, rank_of_idxrank);
    }
}

static void
rescale_scores (kissat * solver, heap * scores)
{
  INC (rescaled);
  const double max_score = kissat_mab_max_score_on_heap (scores);
  kissat_mab_phase (solver, "rescale", GET (rescaled),
		"maximum score %g increment %g", max_score, solver->scinc);
  const double rescale = MAX (max_score, solver->scinc);
  assert (rescale > 0);
  const double factor = 1.0 / rescale;
  kissat_mab_rescale_heap (solver, scores, factor);
  solver->scinc *= factor;
  kissat_mab_phase (solver, "rescale",
		GET (rescaled), "rescaled by factor %g", factor);
}

static void
bump_score_increment (kissat * solver, heap * scores)
{
  const double old_scinc = solver->scinc;
  const double decay = GET_OPTION (decay) * 1e-3;
  assert (0 <= decay), assert (decay <= 0.5);
  const double factor = 1.0 / (1.0 - decay);
  const double new_scinc = old_scinc * factor;
  LOG ("new score increment %g = %g * %g", new_scinc, factor, old_scinc);
  solver->scinc = new_scinc;
  if (new_scinc > MAX_SCORE)
    rescale_scores (solver, scores);
}

static inline void
bump_variable_score (kissat * solver, heap * scores, unsigned idx)
{
  const double old_score = kissat_mab_get_heap_score (scores, idx);
  const double new_score = old_score + solver->scinc;
  LOG ("new score[%u] = %g = %g + %g",
       idx, new_score, old_score, solver->scinc);
  kissat_mab_update_heap (solver, scores, idx, new_score);
  if (new_score > MAX_SCORE)
    rescale_scores (solver, scores);
}


void kissat_mab_bump_one(kissat * solver, int eidx) {
  if (eidx == -1) return;
  assert(eidx >= 0);
  assert(eidx < VARS);
  assert(!solver->probing);
  //printf("bump %d\n", idx);
  flags *flags = solver->flags;
  import *import = &PEEK_STACK (solver->import, eidx);
  assert(import->imported);
  if (import->eliminated) return;
  int idx = (import->lit) >> 1;
  //vsids 
  if (solver->stable && solver->heuristic == 0 && flags[idx].active && kissat_mab_heap_contains (&solver->scores, idx))
    kissat_mab_update_heap(solver, &solver->scores, idx, 1e100);

  //vmtf
  links *links = solver->links;
  if (!solver->stable && flags[idx].active && !VALUE(LIT(idx)) && links[idx].prev != DISCONNECT && links[idx].next != DISCONNECT) {
    kissat_mab_move_to_front(solver, idx);
  }

  //chb
  if (solver->stable && solver->heuristic == 1 && flags[idx].active && kissat_mab_heap_contains (&solver->scores_chb, idx))
    kissat_mab_update_heap (solver, &solver->scores_chb, idx, 1.0);
}

static void
bump_analyzed_variable_scores (kissat * solver)
{
  heap *scores = &solver->scores;
  flags *flags = solver->flags;

  for (all_stack (unsigned, idx, solver->analyzed))
    if (flags[idx].active)
        bump_variable_score (solver, scores, idx);

  bump_score_increment (solver, scores);
}

static void
move_analyzed_variables_to_front_of_queue (kissat * solver)
{
  assert (EMPTY_STACK (solver->bump));
  const links *links = solver->links;
  for (all_stack (unsigned, idx, solver->analyzed))
    {
// *INDENT-OFF*
    const idxrank idxrank = { .idx = idx, .rank = links[idx].stamp };
// *INDENT-ON*
      PUSH_STACK (solver->bump, idxrank);
    }

  sort_bump (solver);

  flags *flags = solver->flags;
  unsigned idx;

  for (all_stack (idxrank, idxrank, solver->bump))
    if (flags[idx = idxrank.idx].active)
      kissat_mab_move_to_front (solver, idx);

  CLEAR_STACK (solver->bump);
}

void
kissat_mab_bump_variables (kissat * solver)
{
  START (bump);
  assert (!solver->probing);
  if (solver->stable)
    bump_analyzed_variable_scores (solver);
  else
    move_analyzed_variables_to_front_of_queue (solver);
  STOP (bump);
}

// CHB

void kissat_mab_bump_chb(kissat * solver, unsigned v, double multiplier) {
    int64_t age = solver->statistics.conflicts - solver->conflicted_chb[v] + 1;
    double reward_chb = multiplier / age;
    double old_score = kissat_mab_get_heap_score (&solver->scores_chb, v);
    double new_score = solver->step_chb * reward_chb + (1 - solver->step_chb) * old_score;
    LOG ("new score[%u] = %g vs %g",
       v, new_score, old_score);
    kissat_mab_update_heap (solver, &solver->scores_chb, v, new_score);
}

void kissat_mab_decay_chb(kissat * solver){
    if (solver->step_chb > solver->step_min_chb) solver->step_chb -= solver->step_dec_chb;
}

void
kissat_mab_update_conflicted_chb (kissat * solver)
{
  flags *flags = solver->flags;

  for (all_stack (unsigned, idx, solver->analyzed))
    if (flags[idx].active)
        solver->conflicted_chb[idx]=solver->statistics.conflicts;
}
