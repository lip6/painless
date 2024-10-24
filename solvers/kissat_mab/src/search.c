#include "analyze.h"
#include "decide.h"
#include "eliminate.h"
#include "internal.h"
#include "ls.h"
#include "logging.h"
#include "print.h"
#include "probe.h"
#include "propsearch.h"
#include "search.h"
#include "reduce.h"
#include "reluctant.h"
#include "report.h"
#include "restart.h"
#include "terminate.h"
#include "trail.h"
#include "walk.h"
#include "bump.h"

#include <inttypes.h>

// Begin Painless
#include "learn.h"
// End Painless

static void
start_search(kissat *solver)
{
  START(search);
  INC(searches);

  REPORT(0, '*');

  bool stable = (GET_OPTION(stable) == 2);

  solver->stable = stable;
  kissat_mab_phase(solver, "search", GET(searches),
               "initializing %s search after %" PRIu64 " conflicts",
               (stable ? "stable" : "focus"), CONFLICTS);

  kissat_mab_init_averages(solver, &AVERAGES);

  if (solver->stable)
    kissat_mab_init_reluctant(solver);

  kissat_mab_init_limits(solver);

  unsigned seed = GET_OPTION(seed);
  solver->random = seed;
  LOG("initialized random number generator with seed %u", seed);

  kissat_mab_reset_rephased(solver);

  const unsigned eagersubsume = GET_OPTION(eagersubsume);
  if (eagersubsume && !solver->clueue.elements)
    kissat_mab_init_clueue(solver, &solver->clueue, eagersubsume);
#ifndef QUIET
  limits *limits = &solver->limits;
  limited *limited = &solver->limited;
  if (!limited->conflicts && !limited->decisions)
    kissat_mab_very_verbose(solver, "starting unlimited search");
  else if (limited->conflicts && !limited->decisions)
    kissat_mab_very_verbose(solver,
                        "starting search with conflicts limited to %" PRIu64,
                        limits->conflicts);
  else if (!limited->conflicts && limited->decisions)
    kissat_mab_very_verbose(solver,
                        "starting search with decisions limited to %" PRIu64,
                        limits->decisions);
  else
    kissat_mab_very_verbose(solver,
                        "starting search with decisions limited to %" PRIu64
                        " and conflicts limited to %" PRIu64,
                        limits->decisions, limits->conflicts);
  if (stable)
  {
    START(stable);
    REPORT(0, '[');
  }
  else
  {
    START(focused);
    REPORT(0, '{');
  }
#endif
}

static void
stop_search(kissat *solver, int res)
{
  if (solver->limited.conflicts)
  {
    LOG("reset conflict limit");
    solver->limited.conflicts = false;
  }

  if (solver->limited.decisions)
  {
    LOG("reset decision limit");
    solver->limited.decisions = false;
  }

  if (solver->terminate)
  {
    kissat_mab_very_verbose(solver, "termination forced externally");
    solver->terminate = 0;
  }

#ifndef QUIET
  LOG("search result %d", res);
  if (solver->stable)
  {
    REPORT(0, ']');
    STOP(stable);
    solver->stable = false;
  }
  else
  {
    REPORT(0, '}');
    STOP(focused);
  }
  char type = (res == 10 ? '1' : res == 20 ? '0'
                                           : '?');
  REPORT(0, type);
#else
  (void)res;
#endif

  STOP(search);
}

static void
iterate(kissat *solver)
{
  assert(solver->iterating);
  solver->iterating = false;
  REPORT(0, 'i');
}

static bool
conflict_limit_hit(kissat *solver)
{
  if (!solver->limited.conflicts)
    return false;
  if (solver->limits.conflicts > solver->statistics.conflicts)
    return false;
  kissat_mab_very_verbose(solver, "conflict limit %" PRIu64 " hit after %" PRIu64 " conflicts",
                      solver->limits.conflicts,
                      solver->statistics.conflicts);
  return true;
}

static bool
decision_limit_hit(kissat *solver)
{
  if (!solver->limited.decisions)
    return false;
  if (solver->limits.decisions > solver->statistics.decisions)
    return false;
  kissat_mab_very_verbose(solver, "decision limit %" PRIu64 " hit after %" PRIu64 " decisions",
                      solver->limits.decisions,
                      solver->statistics.decisions);
  return true;
}

int kissat_mab_search(kissat *solver)
{
  start_search(solver);
  kissat_mab_bump_one(solver, solver->bump_one);
  int res = kissat_mab_walk_initially(solver);
  while (!res)
  {
    // Begin Painless
    if (0 == solver->level)
    {
      if (false == kissat_mab_import_unit_from_painless(solver))
        return 20;
      if (false == kissat_mab_import_from_painless(solver))
        return 20;
    }
    // End Painless
    clause *conflict = kissat_mab_search_propagate(solver);
    if (conflict)
    {
      res = kissat_mab_analyze(solver, conflict);
      solver->nconflict += 1;
    }
    else if (solver->iterating)
      iterate(solver);
    else if (!solver->unassigned)
      res = 10;
    else if (TERMINATED(11))
      break;
    else if (conflict_limit_hit(solver)) // app option
      break;
    else if (kissat_mab_reducing(solver)) // OPTION 'reduce': learned clause reduction
      res = kissat_mab_reduce(solver);
    else if (kissat_mab_restarting(solver)) // OPTION 'restart'
    {
      kissat_mab_restart(solver);
      kissat_mab_bump_one(solver, solver->bump_one);
    }
    else if (kissat_mab_rephasing(solver)) // OPTION 'rephase': reinitialization of decision phases
      kissat_mab_rephase(solver);
    else if (kissat_mab_eliminating(solver)) // OPTION 'simplify' && 'eliminate'
      res = kissat_mab_eliminate(solver);
    else if (kissat_mab_probing(solver)) // OPTION 'simplify' && 'probing' && ('substitute' || 'failed' || 'transitive' || 'vivify')
      res = kissat_mab_probe(solver);
    else if (!solver->level && solver->unflushed)
      kissat_mab_flush_trail(solver);
    else if (decision_limit_hit(solver)) // app option
      break;
    /*
      solver->restarts_gap = GET_OPTION(ccanr_gap_inc);
      kissat_mab_ccanr:
        ...
        solver->freeze_ls_restart_num = solver->restarts_gap; // the end of kissat_mab_ccanr
      kissat_mab_search_propagate:
        ...
        if (conflict) {
          INC (conflicts);
          solver->freeze_ls_restart_num--;
        }
  }
    */
    else if (kissat_mab_ccanring(solver)) // GET_OPTION(ccanr) && solver->freeze_ls_restart_num < 1
      res = kissat_mab_ccanr(solver);     // Launched when the number of conflicts reaches ccanr_gap_inc
    else
    {
      kissat_mab_decide(solver);
      solver->nassign += 1;
    }
  }

  stop_search(solver, res);

  return res;
}
