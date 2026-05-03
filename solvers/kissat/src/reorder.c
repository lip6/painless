#include "reorder.h"
#include "backtrack.h"
#include "bump.h"
#include "inline.h"
#include "inlineheap.h"
#include "inlinequeue.h"
#include "inlinevector.h"
#include "internal.h"
#include "logging.h"
#include "print.h"
#include "report.h"
#include "sort.h"

bool kissat_reordering (kissat *solver) {
  if (!GET_OPTION (reorder))
    return false;
  if (!solver->stable && GET_OPTION (reorder) < 2)
    return false;
  if (!GET_OPTION (reorderusetrail) && solver->level)
    return false;

  const int cond = GET_OPTION (reordercond);
  if (cond == 0)
    return CONFLICTS >= solver->limits.reorder.conflicts;
  if (cond == 1)
    return solver->statistics.units > solver->limits.reorder.units;
  return false;
}

static double *compute_jw_weights (kissat *solver) {
  double *weights = kissat_calloc (solver, LITS, sizeof *weights);
  const unsigned max_size = GET_OPTION (reordermaxsize);
  LOG ("limiting weight computation to maximum clause size %u", max_size);
  assert (2 <= max_size);
  double *table = kissat_nalloc (solver, max_size + 1, sizeof *table);
  {
    double weight = 1;
    for (unsigned size = 2; size <= max_size; size++) {
      LOG ("score table[%u] = %g", size, weight);
      table[size] = weight, weight /= 2.0;
    }
  }
  {
    assert (!solver->level);
    const signed char *const values = solver->values;
    const clause *last = kissat_last_irredundant_clause (solver);
    for (all_clauses (c)) {
      if (last && c > last)
        break;
      if (c->redundant)
        continue;
      if (c->garbage)
        continue;
      unsigned size = 0;
      for (all_literals_in_clause (lit, c)) {
        const signed char value = values[lit];
        if (value > 0)
          goto CONTINUE_WITH_NEXT_CLAUSE;
        if (!value && size < max_size && ++size == max_size)
          break;
      }
      const double weight = table[size];
      for (all_literals_in_clause (lit, c))
        weights[lit] += weight;
    CONTINUE_WITH_NEXT_CLAUSE:;
    }
  }
  assert (solver->watching);
  {
    double weight = table[2];
    kissat_dealloc (solver, table, max_size + 1, sizeof *table);
    for (all_literals (lit)) {
      const unsigned idx = IDX (lit);
      // value check required for reorderusetrail mode
      if (!ACTIVE (idx) || solver->values[lit])
        continue;
      watches *watches = &WATCHES (lit);
      for (all_binary_blocking_watches (watch, *watches)) {
        if (!watch.type.binary)
          continue;
        const unsigned other = watch.type.lit;
        if (other < lit)
          continue;
        const unsigned other_idx = IDX (other);
        // value check required for reorderusetrail mode
        if (!ACTIVE (other_idx) || solver->values[other])
          continue;

        weights[lit] += weight;
        weights[other] += weight;
      }
    }
  }
  for (all_variables (idx)) {
    if (!ACTIVE (idx))
      continue;
    unsigned lit = LIT (idx), not_lit = NOT (lit);
    double pos = weights[lit], neg = weights[not_lit];
    double max_pos_neg = MAX (pos, neg);
    double min_pos_neg = MIN (pos, neg);
    double scaled_min_pos_neg = 2 * min_pos_neg;
    double weight = max_pos_neg + scaled_min_pos_neg;
    LOG ("computed weight %g "
         "= %g + %g = max (%g, %g) + 2 * min (%g, %g) of %s",
         weight, max_pos_neg, scaled_min_pos_neg, pos, neg, pos, neg,
         LOGVAR (idx));
    weights[idx] = weight;
  }
  return weights;
}

/* near propagation literals */
static double *compute_np_weights (kissat *solver) {
  size_t *occurrences = kissat_calloc (solver, LITS, sizeof *occurrences);
  const unsigned max_size = GET_OPTION (reordermaxsize);
  LOG ("limiting weight computation to maximum clause size %u", max_size);
  assert (2 <= max_size);
  {
    assert (!solver->level);
    const signed char *const values = solver->values;
    const clause *last = kissat_last_irredundant_clause (solver);
    for (all_clauses (c)) {
      if (last && c > last)
        break;
      if (c->redundant)
        continue;
      if (c->garbage)
        continue;
      unsigned size = 0;
      for (all_literals_in_clause (lit, c)) {
        const signed char value = values[lit];
        if (value > 0 || (!value && size < max_size && ++size == max_size))
          goto CONTINUE_WITH_NEXT_CLAUSE;
      }
      for (all_literals_in_clause (lit, c))
        occurrences[lit]++;
    CONTINUE_WITH_NEXT_CLAUSE:;
    }
  }
  assert (solver->watching);
  {
    for (all_literals (lit)) {
      const unsigned idx = IDX (lit);
      if (!ACTIVE (idx) || solver->values[lit])
        continue;
      watches *watches = &WATCHES (lit);
      for (all_binary_blocking_watches (watch, *watches)) {
        if (!watch.type.binary)
          continue;
        const unsigned other = watch.type.lit;
        if (other < lit)
          continue;
        const unsigned other_idx = IDX (other);
        if (!ACTIVE (other_idx) || solver->values[other])
          continue;
        occurrences[lit]++;
        occurrences[other]++;
      }
    }
  }
  double *weights = kissat_calloc (solver, LITS, sizeof *weights);
  for (all_variables (idx)) {
    if (!ACTIVE (idx))
      continue;
    unsigned lit = LIT (idx), not_lit = NOT (lit);
    size_t pos = occurrences[lit], neg = occurrences[not_lit];
    size_t max_pos_neg = MAX (pos, neg);

    if (GET_OPTION (phasesaving)) {
      value phase = (pos > neg) * 2 - 1;
      solver->phases.saved[idx] = phase; /* for saved */
    }

    weights[idx] = (double) max_pos_neg;
  }

  kissat_dealloc (solver, occurrences, LITS, sizeof *occurrences);
  return weights;
}

static double *compute_moms_weights (kissat *solver) {
  size_t *occurrences = kissat_calloc (solver, LITS, sizeof *occurrences);
  {
    assert (!solver->level);
    const signed char *const values = solver->values;
    const clause *last = kissat_last_irredundant_clause (solver);
    assert (solver->watching);

    unsigned int binaries = 0;

    for (all_literals (lit)) {
      const unsigned idx = IDX (lit);
      if (!ACTIVE (idx))
        continue;
      watches *watches = &WATCHES (lit);
      for (all_binary_blocking_watches (watch, *watches)) {
        if (!watch.type.binary)
          continue;
        const unsigned other = watch.type.lit;
        if (other < lit)
          continue;
        const unsigned other_idx = IDX (other);
        if (!ACTIVE (other_idx))
          continue;

        occurrences[lit]++;
        occurrences[other]++;

        binaries++;
      }
    }

    if (!binaries) {
      unsigned int min_size = UINT32_MAX;
      STACK (clause *) clauses;
      INIT_STACK (clauses);

      // Find minimum
      for (all_clauses (c)) {
        if (last && c > last)
          break;
        if (c->redundant)
          continue;
        if (c->garbage)
          continue;

        unsigned size = 0;
        for (all_literals_in_clause (lit, c)) {
          const signed char value = values[lit];
          if (value > 0)
            goto SKIP_SATISFIED;
          size++;
        }
        if (min_size > size) {
          min_size = size;
          CLEAR_STACK (clauses);
        }
        if (min_size == size)
          PUSH_STACK (clauses, c);

      SKIP_SATISFIED:;
      }

      // Second pass to count
      for (clause *c, **c_PTR = (clauses).begin,
                      **const c_END = (clauses).end;
           c_PTR != c_END && (c = *c_PTR, 1); ++c_PTR) {

        for (all_literals_in_clause (lit, c))
          occurrences[lit]++;
      }

      RELEASE_STACK (clauses);
    }
  }
  double *weights = kissat_calloc (solver, LITS, sizeof *weights);
  for (all_variables (idx)) {
    if (!ACTIVE (idx))
      continue;
    unsigned lit = LIT (idx), not_lit = NOT (lit);
    size_t pos = occurrences[lit], neg = occurrences[not_lit];
    size_t weight = (pos + neg) + pos * neg * 2;

    weights[idx] = (double) weight;
  }

  kissat_dealloc (solver, occurrences, LITS, sizeof *occurrences);
  return weights;
}

static bool less_focused_order (unsigned a, unsigned b, links *links,
                                double *weights) {
  double u = weights[a], v = weights[b];
  if (u < v)
    return true;
  if (u > v)
    return false;
  unsigned s = links[a].stamp, t = links[b].stamp;
  return s < t;
}

static bool less_stable_order (unsigned a, unsigned b, heap *scores,
                               double *weights) {
  double u = weights[a], v = weights[b];
  if (u < v)
    return true;
  if (u > v)
    return false;
  double s = kissat_get_heap_score (scores, a);
  double t = kissat_get_heap_score (scores, b);
  if (s < t)
    return true;
  if (s > t)
    return false;
  return b < a;
}

#define LESS_FOCUSED_ORDER(A, B) less_focused_order (A, B, links, weights)

#define LESS_STABLE_ORDER(A, B) less_stable_order (A, B, scores, weights)

static void sort_active_variables_by_weight (kissat *solver,
                                             unsigneds *sorted,
                                             double *weights) {
  INIT_STACK (*sorted);
  for (all_variables (idx))
    if (ACTIVE (idx))
      PUSH_STACK (*sorted, idx);
  if (solver->stable) {
    heap *scores = SCORES;
    SORT_STACK (unsigned, *sorted, LESS_STABLE_ORDER);
#ifdef LOGGING
    for (all_stack (unsigned, idx, *sorted))
      if (ACTIVE (idx))
        LOG ("reordered %s with weight %g score %g", LOGVAR (idx),
             weights[idx], kissat_get_heap_score (scores, idx));
#endif
  } else {
    struct links *links = solver->links;
    SORT_STACK (unsigned, *sorted, LESS_FOCUSED_ORDER);
#ifdef LOGGING
    for (all_stack (unsigned, idx, *sorted))
      if (ACTIVE (idx))
        LOG ("reordered %s with weight %g stamp %u", LOGVAR (idx),
             weights[idx], links[idx].stamp);
#endif
  }
}

double *compute_weights (kissat *solver) {
  double *weights = NULL;
  if (0 == GET_OPTION (reorderalgo))
    weights = compute_jw_weights (solver);
  else if (1 == GET_OPTION (reorderalgo))
    weights = compute_moms_weights (solver);
  else if (2 == GET_OPTION (reorderalgo))
    weights = compute_np_weights (solver);

  return weights;
}

static void reorder_focused (kissat *solver) {
  INC (reordered_focused);
  assert (!solver->stable);
  double *weights = compute_weights (solver);
  unsigneds sorted;
  sort_active_variables_by_weight (solver, &sorted, weights);
  kissat_dealloc (solver, weights, LITS, sizeof *weights);
  for (all_stack (unsigned, idx, sorted)) {
    assert (ACTIVE (idx));
    kissat_move_to_front (solver, idx);
  }
  RELEASE_STACK (sorted);
}

static void reorder_stable (kissat *solver) {
  INC (reordered_stable);
  assert (solver->stable);
  double *weights = compute_weights (solver);
  kissat_rescale_scores (solver);
  unsigneds sorted;
  sort_active_variables_by_weight (solver, &sorted, weights);
  heap *scores = SCORES;
  while (!EMPTY_STACK (sorted)) {
    unsigned idx = POP_STACK (sorted);
    assert (ACTIVE (idx));
    const double old_score = kissat_get_heap_score (scores, idx);
    const double weight = weights[idx];
    const double new_score = old_score + weight;
    LOG ("updating score of %s to %g = %g (old score) + %g (weight)",
         LOGVAR (idx), new_score, old_score, weight);
    kissat_update_heap (solver, scores, idx, new_score);
  }
  kissat_dealloc (solver, weights, LITS, sizeof *weights);
  RELEASE_STACK (sorted);
}

void kissat_reorder (kissat *solver) {
  START (reorder);
  INC (reordered);
  assert (!solver->level);
  kissat_phase (solver, "reorder", GET (reordered),
                "reorder limit %" PRIu64 " hit a after %" PRIu64
                " conflicts in %s mode ",
                solver->limits.reorder.conflicts, CONFLICTS,
                solver->stable ? "stable" : "focused");
  if (solver->stable)
    reorder_stable (solver);
  else
    reorder_focused (solver);
  kissat_phase (solver, "reorder", GET (reordered),
                "reordered decisions in %s search mode",
                solver->stable ? "stable" : "focused");

  // Check if we should stop reordering:
  // 0: never
  bool stop = false;
  const double threshold = 1.0;
  if (GET_OPTION (reorderstop)) {
    // check if units number increased
    const unsigned new_unit_count =
        solver->statistics.units - solver->limits.reorder.units;
    // if (solver->statistics.reordered > 1) {
    UPDATE_CAVERAGE (units, new_unit_count);
    const double units_avg = CAVERAGE (units);
    // printf("reorder#%lu units(+%u): %lf <
    // %lf\n",solver->statistics.reordered, new_unit_count, units_avg,
    // threshold);
    stop = (units_avg < threshold);
    // }
  }

  if (stop) {
    printf ("c Stopping reorder at#%lu via mode %d at %lf\n",
            solver->statistics.reordered, GET_OPTION (reorderstop),
            kissat_time (solver));
    solver->options.reorder = 0;
  } else {
    if (GET_OPTION (reorderinitscale))
      UPDATE_CONFLICT_LIMIT (reorder, reordered, LINEAR, false);
    else
      solver->limits.reorder.conflicts =
          (solver->statistics.conflicts) + solver->options.reorderint;
  }

  solver->limits.reorder.units = solver->statistics.units;

  REPORT (0, 'o');
  STOP (reorder);
}
