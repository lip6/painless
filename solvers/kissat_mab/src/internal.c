#include "allocate.h"
#include "backtrack.h"
#include "error.h"
#include "search.h"
#include "import.h"
#include "inline.h"
#include "print.h"
#include "propsearch.h"
#include "require.h"
#include "resize.h"
#include "resources.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

kissat *
kissat_mab_init(void)
{
  kissat *solver = kissat_mab_calloc(0, 1, sizeof *solver);
#ifndef NOPTIONS
  kissat_mab_init_options(&solver->options);
#else
  kissat_mab_init_options();
#endif
#ifndef QUIET
  kissat_mab_init_profiles(&solver->profiles);
#endif
  START(total);
  kissat_mab_init_queue(&solver->queue);
  kissat_mab_push_frame(solver, INVALID_LIT);
  solver->ls_call_num = 0;
  solver->ls_bms = 20;
  solver->ls_best_unsat_num = INT32_MAX;
  solver->restarts_gap = GET_OPTION(ccanr_gap_inc);
  solver->ls_mems_num = 100 * 1000 * 1000;
  solver->ls_mems_num_min = 1 * 1000 * 1000;
  solver->ls_mems_inc = 0.9;
  solver->ccanr_has_constructed = false;
  solver->lssolver = (CCAnr *)malloc(sizeof(CCAnr) * 1);
  solver->verso = 0;
  solver->bump_one = -1;
  // Begin Painless
  INIT_STACK(solver->pclause.lits);
  // End Painless
  solver->watching = true;
  solver->conflict.size = 2;
  solver->conflict.keep = true;
  solver->scinc = 1.0;
  solver->first_reducible = INVALID_REF;
  solver->last_irredundant = INVALID_REF;
  solver->nconflict = 0;
  solver->nassign = 0;
  // CHB
  solver->step_dec_chb = 0.000001;
  solver->step_min_chb = 0.06;
  // MAB
  solver->mab_heuristics = 2;
  solver->mab_decisions = 0;
  solver->mab_chosen_tot = 0;
#ifndef NDEBUG
  kissat_mab_init_checker(solver);
  // added code for debug
  // INIT_STACK(solver->exportk);
  // INIT_STACK(solver->import);
  //--------------------------
#endif
  return solver;
}

#define DEALLOC_GENERIC(NAME, ELEMENTS_PER_BLOCK)                        \
  do                                                                     \
  {                                                                      \
    const size_t block_size = ELEMENTS_PER_BLOCK * sizeof *solver->NAME; \
    kissat_mab_dealloc(solver, solver->NAME, solver->size, block_size);      \
    solver->NAME = 0;                                                    \
  } while (0)

#define DEALLOC_VARIABLE_INDEXED(NAME) \
  DEALLOC_GENERIC(NAME, 1)

#define DEALLOC_LITERAL_INDEXED(NAME) \
  DEALLOC_GENERIC(NAME, 2)

#define RELEASE_LITERAL_INDEXED_STACKS(NAME, ACCESS)     \
  do                                                     \
  {                                                      \
    for (all_stack(unsigned, IDX_RILIS, solver->active)) \
    {                                                    \
      const unsigned LIT_RILIS = LIT(IDX_RILIS);         \
      const unsigned NOT_LIT_RILIS = NOT(LIT_RILIS);     \
      RELEASE_STACK(ACCESS(LIT_RILIS));                  \
      RELEASE_STACK(ACCESS(NOT_LIT_RILIS));              \
    }                                                    \
    DEALLOC_LITERAL_INDEXED(NAME);                       \
  } while (0)

void kissat_mab_release(kissat *solver)
{
  kissat_mab_require_initialized(solver);

  kissat_mab_release_heap(solver, &solver->scores);

  // CHB
  kissat_mab_release_heap(solver, &solver->scores_chb);
  DEALLOC_VARIABLE_INDEXED(conflicted_chb);

  kissat_mab_release_heap(solver, &solver->schedule);

  kissat_mab_release_clueue(solver, &solver->clueue);

  RELEASE_STACK(solver->exportk);
  RELEASE_STACK(solver->import);

  //Begin Painless
  RELEASE_STACK(solver->pclause.lits);
  //End Painless

  DEALLOC_VARIABLE_INDEXED(assigned);
  DEALLOC_VARIABLE_INDEXED(flags);
  DEALLOC_VARIABLE_INDEXED(links);
  DEALLOC_VARIABLE_INDEXED(phases);
  DEALLOC_VARIABLE_INDEXED(init_phase);

  DEALLOC_LITERAL_INDEXED(marks);
  DEALLOC_LITERAL_INDEXED(values);
  DEALLOC_LITERAL_INDEXED(watches);

  RELEASE_STACK(solver->import);
  RELEASE_STACK(solver->eliminated);
  RELEASE_STACK(solver->extend);
  RELEASE_STACK(solver->witness);
  RELEASE_STACK(solver->etrail);

  RELEASE_STACK(solver->vectors.stack);
  RELEASE_STACK(solver->delayed);

  RELEASE_STACK(solver->clause.lits);
#if defined(LOGGING) || !defined(NDEBUG)
  RELEASE_STACK(solver->resolvent_lits);
#endif

  RELEASE_STACK(solver->arena);

  RELEASE_STACK(solver->units);
  RELEASE_STACK(solver->frames);
  RELEASE_STACK(solver->sorter);
  RELEASE_STACK(solver->trail);

  RELEASE_STACK(solver->analyzed);
  RELEASE_STACK(solver->levels);
  RELEASE_STACK(solver->minimize);
  RELEASE_STACK(solver->poisoned);
  RELEASE_STACK(solver->promote);
  RELEASE_STACK(solver->removable);
  RELEASE_STACK(solver->xorted[0]);
  RELEASE_STACK(solver->xorted[1]);

  RELEASE_STACK(solver->bump);

  RELEASE_STACK(solver->antecedents[0]);
  RELEASE_STACK(solver->antecedents[1]);
  RELEASE_STACK(solver->gates[0]);
  RELEASE_STACK(solver->gates[1]);
  RELEASE_STACK(solver->resolvents);

#if !defined(NDEBUG) || !defined(NPROOFS)
  RELEASE_STACK(solver->added);
  RELEASE_STACK(solver->removed);
#endif

#if !defined(NDEBUG) || !defined(NPROOFS) || defined(LOGGING)
  RELEASE_STACK(solver->original);
#endif

#ifndef QUIET
  RELEASE_STACK(solver->profiles.stack);
#endif

#ifndef NDEBUG
  kissat_mab_release_checker(solver);
#endif
#if !defined(NDEBUG) && !defined(NMETRICS)
  uint64_t leaked = solver->statistics.allocated_current;
  if (leaked)
    if (!getenv("LEAK"))
    {
      printf("d Internally leaking %" PRIu64 " bytes\n", leaked);
      // kissat_mab_fatal("internally leaking %" PRIu64 " bytes", leaked);
    }
#endif

  kissat_mab_free(0, solver, sizeof *solver);
}

// Begin Painless
// TODO : check the effectivness of this shuffle
static inline void kissat_mab_init_shuffle(kissat *solver, int max_var)
{
  if (GET_OPTION(initshuffle))
  {
    // seed is set using seed option of kissat
    srand(GET_OPTION(seed));
    int *id;
    id = (int *)malloc(sizeof(int) * (max_var + 1));
    for (int i = 1; i <= max_var; i++)
      id[i] = i;
    for (int i = 1; i <= max_var; i++)
    {
      int j = (rand() % max_var) + 1;
      int x = id[i];
      id[i] = id[j];
      id[j] = x;
    }
    for (int i = 1; i <= max_var; i++)
      solver->init_phase[kissat_mab_import_literal(solver, i) >> 1] = 0;
    for (int i = 1; i <= max_var; i++)
      kissat_mab_activate_literal(solver, kissat_mab_import_literal(solver, id[i]));
    free(id);
  }
  else
  {
    for (int i = 1; i <= max_var; i++)
      solver->init_phase[kissat_mab_import_literal(solver, i) >> 1] = 0;
  }
}
// End Painless

void kissat_mab_reserve(kissat *solver, int max_var)
{
  kissat_mab_require_initialized(solver);
  kissat_mab_require(0 <= max_var,
                 "negative maximum variable argument '%d'", max_var);
  kissat_mab_require(max_var <= EXTERNAL_MAX_VAR,
                 "invalid maximum variable argument '%d'", max_var);
  kissat_mab_increase_size(solver, (unsigned)max_var);
  // Begin Painless
  kissat_mab_init_shuffle(solver, max_var);
  // End Painless
}

int kissat_mab_get_option(kissat *solver, const char *name)
{
  kissat_mab_require_initialized(solver);
  kissat_mab_require(name, "name zero pointer");
#ifndef NOPTIONS
  return kissat_mab_options_get(&solver->options, name);
#else
  (void)solver;
  return kissat_mab_options_get(name);
#endif
}

int kissat_mab_set_option(kissat *solver, const char *name, int new_value)
{
#ifndef NOPTIONS
  kissat_mab_require_initialized(solver);
  kissat_mab_require(name, "name zero pointer");
#ifndef NOPTIONS
  return kissat_mab_options_set(&solver->options, name, new_value);
#else
  return kissat_mab_options_set(name, new_value);
#endif
#else
  (void)solver, (void)new_value;
  return kissat_mab_options_get(name);
#endif
}

void kissat_mab_set_decision_limit(kissat *solver, unsigned limit)
{
  kissat_mab_require_initialized(solver);
  limits *limits = &solver->limits;
  limited *limited = &solver->limited;
  statistics *statistics = &solver->statistics;
  limited->decisions = true;
  assert(UINT64_MAX - limit >= statistics->decisions);
  limits->decisions = statistics->decisions + limit;
  LOG("set decision limit to %" PRIu64 " after %u decisions",
      limits->decisions, limit);
}

void kissat_mab_set_conflict_limit(kissat *solver, unsigned limit)
{
  kissat_mab_require_initialized(solver);
  limits *limits = &solver->limits;
  limited *limited = &solver->limited;
  statistics *statistics = &solver->statistics;
  limited->conflicts = true;
  assert(UINT64_MAX - limit >= statistics->conflicts);
  limits->conflicts = statistics->conflicts + limit;
  LOG("set conflict limit to %" PRIu64 " after %u conflicts",
      limits->conflicts, limit);
}

void kissat_mab_print_statistics(kissat *solver)
{
#ifndef QUIET
  kissat_mab_require_initialized(solver);
  const int verbosity = kissat_mab_verbosity(solver);
  if (verbosity < 0)
    return;
  if (GET_OPTION(profile))
  {
    kissat_mab_section(solver, "profiling");
    kissat_mab_profiles_print(solver);
  }
  const bool complete = GET_OPTION(statistics);
  kissat_mab_section(solver, "statistics");
  const bool verbose = (complete || verbosity > 0);
  kissat_mab_statistics_print(solver, verbose);
  if (solver->mab)
  {
    printf("c MAB stats : ");
    for (unsigned i = 0; i < solver->mab_heuristics; i++)
      printf("%d ", solver->mab_select[i]);
    printf("\n");
  }
#ifndef NPROOFS
  if (solver->proof)
  {
    kissat_mab_section(solver, "proof");
    kissat_mab_print_proof_statistics(solver, verbose);
  }
#endif
#ifndef NDEBUG
  if (GET_OPTION(check) > 1)
  {
    kissat_mab_section(solver, "checker");
    kissat_mab_print_checker_statistics(solver, verbose);
  }
#endif
  kissat_mab_section(solver, "resources");
  kissat_mab_print_resources(solver);
#endif
  (void)solver;
}

void kissat_mab_add(kissat *solver, int elit)
{
  kissat_mab_require_initialized(solver);
  kissat_mab_require(!GET(searches), "incremental solving not supported");

#if !defined(NDEBUG) || !defined(NPROOFS) || defined(LOGGING)
  const int checking = kissat_mab_checking(solver);
  const bool logging = kissat_mab_logging(solver);
  const bool proving = kissat_mab_proving(solver);
#endif
  if (elit) // elit != 0
  {
    kissat_mab_require_valid_external_internal(elit);
#if !defined(NDEBUG) || !defined(NPROOFS) || defined(LOGGING)
    if (checking || logging || proving)
      PUSH_STACK(solver->original, elit);
#endif
    unsigned ilit = kissat_mab_import_literal(solver, elit);
    /* After kissat_mab_import_literal:
      exportk[iidx]=eidx
      import[eidx].lit=ilit
    */

    const mark mark = MARK(ilit); // if the literal was already seen and was not assigned in the current clause
    if (!mark)                    // literal or its negation were never seen in the current clause
    {
      const value value = kissat_mab_fixed(solver, ilit); // returns 0 if ilit is not assigned or is assigned at level > 0. If assigned at level 0 returns -1 or 1
      if (value > 0)                                  // ilit is assigned at root and is true
      {
        /**
         *  to remember : since solver is global, it must be initialized to false
         * Clearly at kissat_mab_init():
         * (gdb) print solver->clause
            $1 = {satisfied = false, shrink = false, trivial = false, lits = {begin = 0x0, end = 0x0, allocated = 0x0}}
          * It is the same values at kissat_mab_add(), but more checks are needed to be sure that solver->clause was not touched after kissat_mab_init()
        */
        if (!solver->clause.satisfied) // the current original clause is satisfied
        {
          LOG("adding root level satisfied literal %u(%d)@0=1",
              ilit, elit);
          solver->clause.satisfied = true;
        }
      }
      else if (value < 0) // ilit is assigned at root and is false
      {
        LOG("adding root level falsified literal %u(%d)@0=-1",
            ilit, elit);
        if (!solver->clause.shrink) // a false literal is to be removed, thus shrink clause
        {
          solver->clause.shrink = true;
          LOG("thus original clause needs shrinking");
        }
      }
      else // ilit is not assigned
      {
        MARK(ilit) = 1;
        MARK(NOT(ilit)) = -1;
        assert(SIZE_STACK(solver->clause.lits) < UINT_MAX);
        PUSH_STACK(solver->clause.lits, ilit); // adding the never seen literal to the clause
      }
    }
    else if (mark < 0) // already saw the negation of this literal in current clause and was unassigned
    {
      assert(mark < 0);
      if (!solver->clause.trivial) // not already tautological
      {
        LOG("adding dual literal %u(%d) and %u(%d)",
            NOT(ilit), -elit, ilit, elit);
        solver->clause.trivial = true; // tautological if seen a literal and its negation in the same clause
      }
    }
    else // already seen this literal in current clause
    {
      assert(mark > 0);
      LOG("adding duplicated literal %u(%d)", ilit, elit);
      if (!solver->clause.shrink)
      {
        solver->clause.shrink = true;
        LOG("thus original clause needs shrinking");
      }
    }
  }
  else // elit == 0
  {
#if !defined(NDEBUG) || !defined(NPROOFS) || defined(LOGGING)
    const size_t offset = solver->offset_of_last_original_clause;
    size_t esize = SIZE_STACK(solver->original) - offset;
    int *elits = BEGIN_STACK(solver->original) + offset;
    assert(esize <= UINT_MAX);
#endif
    ADD_UNCHECKED_EXTERNAL(esize, elits); // not understood yet, esize and elits are inside an if macro ??
    const size_t isize = SIZE_STACK(solver->clause.lits);
    unsigned *ilits = BEGIN_STACK(solver->clause.lits);
    assert(isize < (unsigned)INT_MAX);

    if (solver->inconsistent) // previous kissat_mab_add can make the solver inconsistent
      LOG("inconsistent thus skipping original clause");
    else if (solver->clause.satisfied)
      LOG("skipping satisfied original clause");
    else if (solver->clause.trivial)
      LOG("skipping trivial original clause");
    else // the clause is useful
    {
      /**
       * enqueue all variables and set the flags activate, eliminate and subsume to all the variables of the clause
       * If a variable is already active it will pass to the next without doing anything
       */
      kissat_mab_activate_literals(solver, isize, ilits);

      if (!isize) // no literal was added, all falsfied or none exists "0 0"
      {
        if (solver->clause.shrink)
          LOG("all original clause literals root level falsified");
        else
          LOG("found empty original clause");

        if (!solver->inconsistent)
        {
          LOG("thus solver becomes inconsistent");
          solver->inconsistent = true; // UNSAT
          CHECK_AND_ADD_EMPTY();
          ADD_EMPTY_TO_PROOF();
        }
      }
      else if (isize == 1) // unit clause
      {
        unsigned unit = TOP_STACK(solver->clause.lits);

        if (solver->clause.shrink) // if duplicates and falsified
          LOGUNARY(unit, "original clause shrinks to");
        else // originally unit
          LOGUNARY(unit, "found original");

        kissat_mab_assign_unit(solver, unit);

        if (!solver->level) // to be sure to be at root
        {
          clause *conflict = kissat_mab_search_propagate(solver);
          if (conflict) // Conflict at root : problem UNSAT
          {
            LOG("propagation of root level unit failed");
            solver->inconsistent = true;
            CHECK_AND_ADD_EMPTY();
            ADD_EMPTY_TO_PROOF();
          }
        }
      }
      else // isize >= 2
      {
        // the index of the original clause in the arena stack
        reference res = kissat_mab_new_original_clause(solver);

        // two watch initialization, kissat_mab_sort_literals in kissat_mab_new_original_clause iterates through the literals to choose the watchers
        const unsigned a = ilits[0]; // literal to propagate if b is false
        const unsigned b = ilits[1]; // sentinel literal if can only be false, propagate

        // 0 or -1 or 1
        const value u = VALUE(a);
        const value v = VALUE(b);

        // if assigned get level, otherwise UINT_MAX
        const unsigned k = u ? LEVEL(a) : UINT_MAX;
        const unsigned l = v ? LEVEL(b) : UINT_MAX;

        bool assign = false;

        // a must be assigned after or at the same level than b.
        if (!u && v < 0) // if first lit is unassigned and second lit is false
        {
          LOG("original clause immediately forcing");
          assign = true;
        }
        else if (u < 0 && k == l) // both falsified at same level
        {
          LOG("both watches falsified at level @%u", k);
          assert(v < 0);
          assert(k > 0); // otherwise unsat
          kissat_mab_backtrack(solver, k - 1);
        }
        else if (u < 0) // both falsified but at different levels (k > l)
        {
          LOG("watches falsified at levels @%u and @%u", k, l);
          assert(v < 0);
          assert(k > l);
          assert(l > 0);
          assign = true; // not understood , yet
        }
        else if (u > 0 && v < 0)
        {
          LOG("first watch satisfied at level @%u "
              "second falsified at level @%u",
              k, l);
          assert(k <= l);
        }
        // from cadical: // Get the value of an internal literal: -1=false, 0=unassigned, 1=true.
        // and according to the tests done on value, 1 means literal is true
        else if (!u && v > 0) // second is true !!
        {
          LOG("first watch unassigned "
              "second falsified at level @%u",
              l);
          assign = true;
        }
        else // none was assigned
        {
          assert(!u);
          assert(!v);
        }

        if (assign) // if a is to be assigned
        {
          assert(solver->level > 0);

          if (isize == 2)
          {
            assert(res == INVALID_REF);
            /**
             * void kissat_mab_assign_binary(kissat *solver,
                #ifdef INLINE_ASSIGN
                         value *values, assigned *assigned,
                #endif
                         bool redundant, unsigned lit, unsigned other)
            */
            kissat_mab_assign_binary(solver, false, a, b);
          }
          else // isize > 2
          {
            assert(res != INVALID_REF);
            clause *c = kissat_mab_dereference_clause(solver, res);
            kissat_mab_assign_reference(solver, a, res, c);
          }
        }
      }
    }

#if !defined(NDEBUG) || !defined(NPROOFS)
    if (solver->clause.satisfied || solver->clause.trivial)
    {
#ifndef NDEBUG
      if (checking > 1)
        kissat_mab_remove_checker_external(solver, esize, elits);
#endif
#ifndef NPROOFS
      if (proving)
        kissat_mab_delete_external_from_proof(solver, esize, elits);
#endif
    }
    else if (solver->clause.shrink)
    {
#ifndef NDEBUG
      if (checking > 1)
      {
        kissat_mab_check_and_add_internal(solver, isize, ilits);
        kissat_mab_remove_checker_external(solver, esize, elits);
      }
#endif
#ifndef NPROOFS
      if (proving)
      {
        kissat_mab_add_lits_to_proof(solver, isize, ilits);
        kissat_mab_delete_external_from_proof(solver, esize, elits);
      }
#endif
    }
#endif

#if !defined(NDEBUG) || !defined(NPROOFS) || defined(LOGGING)
    if (checking)
    {
      LOGINTS(esize, elits, "saved original");
      PUSH_STACK(solver->original, 0);
      solver->offset_of_last_original_clause =
          SIZE_STACK(solver->original);
    }
    else if (logging || proving)
    {
      LOGINTS(esize, elits, "reset original");
      CLEAR_STACK(solver->original);
      solver->offset_of_last_original_clause = 0;
    }
#endif
    for (all_stack(unsigned, lit, solver->clause.lits))
      MARK(lit) = MARK(NOT(lit)) = 0; // clause ended, so marks to detect tautologies are to be reset

    // prepare for next clause by resetting solver->clause
    CLEAR_STACK(solver->clause.lits);

    solver->clause.satisfied = false;
    solver->clause.trivial = false;
    solver->clause.shrink = 0;
  }
}

int kissat_mab_solve(kissat *solver)
{
  kissat_mab_require(EMPTY_STACK(solver->clause.lits), "incomplete clause (terminating zero not added)");
  kissat_mab_require(!GET(searches), "incremental solving not supported");
  return kissat_mab_search(solver);
}

void kissat_mab_terminate(kissat *solver)
{
  kissat_mab_require_initialized(solver);
  solver->terminate = ~(unsigned)0;
  assert(solver->terminate);
}

int kissat_mab_value(kissat *solver, int elit)
{
  kissat_mab_require_initialized(solver);
  kissat_mab_require_valid_external_internal(elit);
  const unsigned eidx = ABS(elit);
  if (eidx >= SIZE_STACK(solver->import))
    return 0;
  const import *import = &PEEK_STACK(solver->import, eidx);
  if (!import->imported)
    return 0;
  value tmp;
  if (import->eliminated)
  {
    if (!solver->extended && !EMPTY_STACK(solver->extend))
      kissat_mab_extend(solver);
    const unsigned eliminated = import->lit;
    tmp = PEEK_STACK(solver->eliminated, eliminated);
  }
  else
  {
    const unsigned ilit = import->lit;
    tmp = VALUE(ilit);
  }
  if (!tmp)
    return 0;
  if (elit < 0)
    tmp = -tmp;
  return tmp < 0 ? -elit : elit;
}
