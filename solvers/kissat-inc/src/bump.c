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
  const double max_score = kissat_inc_max_score_on_heap (scores);
  kissat_inc_phase (solver, "rescale", GET (rescaled),
		"maximum score %g increment %g", max_score, solver->scinc);
  const double rescale = MAX (max_score, solver->scinc);
  assert (rescale > 0);
  const double factor = 1.0 / rescale;
  kissat_inc_rescale_heap (solver, scores, factor);
  solver->scinc *= factor;
  kissat_inc_phase (solver, "rescale",
		GET (rescaled), "rescaled by factor %g", factor);
}

void kissat_inc_init_shuffle(kissat * solver, int maxvar) {
  int seed = GET_OPTION(worker_seed);
  int index = GET_OPTION(worker_index);
  int number = GET_OPTION(worker_number);

  LOG2("c index:%d number:%d seed:%d\n", index, number, seed);

  if (seed != -1)
  {
    // printf("c init shuffle %d\n", seed);
    int *id;
    id = (int *)malloc(sizeof(int) * (maxvar + 1));
    for (int i = 1; i <= maxvar; i++)
      id[i] = i;
    
    if(seed != 0) {
          for (int i = 1; i <= maxvar; i++)
        {
          int j = (rand_r(&seed) % maxvar) + 1;
          int x = id[i];
          id[i] = id[j];
          id[j] = x;
        }
    }

    int block_size = maxvar / number;
    int cursor = index * block_size + 1;

    for(int i=cursor; i<=maxvar; i++) {
      kissat_inc_activate_literal(solver, kissat_inc_import_literal(solver, id[i]));
    }

    for (int i = 1; i < cursor && i <= maxvar ; i++) {
      kissat_inc_activate_literal(solver, kissat_inc_import_literal(solver, id[i]));
    }
      
    free(id);
  }
  else
  {
    for (int i = 1; i <= maxvar; i++)
      kissat_inc_import_literal(solver, i);
  }
}

void kissat_inc_shuffle_score(kissat * solver) {
  heap *scores = &solver->scores;
  heap *scores_chb = &solver->scores_chb;
  flags *flags = solver->flags;
  int *id = (int *)malloc(sizeof(int) * (VARS));
  for (int i = 0; i < VARS; i++) id[i] = i;
  for (int i = 0; i < VARS; i++)
  {
    int j = (rand() % VARS);
    int x = id[i];
    id[i] = id[j];
    id[j] = x;
  }
  for (int i = 0; i < VARS; i++) {
    int idx = id[i];
    if (flags[idx].active) {
      double new_score = 1.0 * i / VARS;
      kissat_inc_update_heap (solver, scores, idx, new_score);
      kissat_inc_update_heap (solver, scores_chb, idx, new_score);
    }
  }
  for (int i = 0; i < VARS; i++) {
    int idx = id[i];
    if (flags[idx].active)
      kissat_inc_move_to_front(solver, idx);
  }
  solver->scinc = 1.0;
  free(id);
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
  const double old_score = kissat_inc_get_heap_score (scores, idx);
  const double new_score = old_score + solver->scinc;
  LOG ("new score[%u] = %g = %g + %g",
       idx, new_score, old_score, solver->scinc);
  kissat_inc_update_heap (solver, scores, idx, new_score);
  if (new_score > MAX_SCORE)
    rescale_scores (solver, scores);
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
      kissat_inc_move_to_front (solver, idx);

  CLEAR_STACK (solver->bump);
}

void
kissat_inc_bump_variables (kissat * solver)
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

void kissat_inc_bump_chb(kissat * solver, unsigned v, double multiplier) {
    int64_t age = solver->statistics.conflicts - solver->conflicted_chb[v] + 1;
    double reward_chb = multiplier / age;
    double old_score = kissat_inc_get_heap_score (&solver->scores_chb, v);
    double new_score = solver->step_chb * reward_chb + (1 - solver->step_chb) * old_score;
    LOG ("new score[%u] = %g vs %g",
       v, new_score, old_score);
    kissat_inc_update_heap (solver, &solver->scores_chb, v, new_score);
}

void kissat_inc_decay_chb(kissat * solver){
    if (solver->step_chb > solver->step_min_chb) solver->step_chb -= solver->step_dec_chb;
}

void
kissat_inc_update_conflicted_chb (kissat * solver)
{
  flags *flags = solver->flags;

  for (all_stack (unsigned, idx, solver->analyzed))
    if (flags[idx].active)
        solver->conflicted_chb[idx]=solver->statistics.conflicts;
}
