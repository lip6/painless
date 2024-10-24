#include "analyze.h"
#include "decide.h"
#include "eliminate.h"
#include "internal.h"
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

// Begin Painless
#include "learn.h"
// End Painless

#include <inttypes.h>

static void
start_search(kissat *solver)
{
  START(search);
  INC(searches);

  REPORT(0, '*');

  bool stable = (GET_OPTION(stable) == 2);

  solver->stable = stable;
  kissat_inc_phase(solver, "search", GET(searches),
               "initializing %s search after %" PRIu64 " conflicts",
               (stable ? "stable" : "focus"), CONFLICTS);

  kissat_inc_init_averages(solver, &AVERAGES);

  if (solver->stable)
    kissat_inc_init_reluctant(solver);

  kissat_inc_init_limits(solver);

  unsigned seed = GET_OPTION(seed);
  solver->random = seed;
  LOG("initialized random number generator with seed %u", seed);

  kissat_inc_reset_rephased(solver);

  const unsigned eagersubsume = GET_OPTION(eagersubsume);
  if (eagersubsume && !solver->clueue.elements)
    kissat_inc_init_clueue(solver, &solver->clueue, eagersubsume);
#ifndef QUIET
  limits *limits = &solver->limits;
  limited *limited = &solver->limited;
  if (!limited->conflicts && !limited->decisions)
    kissat_inc_very_verbose(solver, "starting unlimited search");
  else if (limited->conflicts && !limited->decisions)
    kissat_inc_very_verbose(solver,
                        "starting search with conflicts limited to %" PRIu64,
                        limits->conflicts);
  else if (!limited->conflicts && limited->decisions)
    kissat_inc_very_verbose(solver,
                        "starting search with decisions limited to %" PRIu64,
                        limits->decisions);
  else
    kissat_inc_very_verbose(solver,
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
    kissat_inc_very_verbose(solver, "termination forced externally");
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
  kissat_inc_very_verbose(solver, "conflict limit %" PRIu64 " hit after %" PRIu64 " conflicts",
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
  kissat_inc_very_verbose(solver, "decision limit %" PRIu64 " hit after %" PRIu64 " decisions",
                      solver->limits.decisions,
                      solver->statistics.decisions);
  return true;
}

int kissat_inc_search(kissat *solver)
{
  start_search(solver);

  int res = kissat_inc_walk_initially(solver);

  while (!res)
  {
    if (!solver->level && solver->reseting)
    {
      kissat_inc_shuffle_score(solver);
      solver->reseting = 0;
    }
    // Begin Painless
    if (0 == solver->level)
    {
      if (false == kissat_inc_import_unit_from_painless(solver))
        return 20;
      if (false == kissat_inc_import_from_painless(solver))
        return 20;
    }
    // End Painless
    clause *conflict = kissat_inc_search_propagate(solver);
    if (conflict)
      res = kissat_inc_analyze(solver, conflict);
    else if (solver->iterating)
      iterate(solver);
    else if (!solver->unassigned)
      res = 10;
    else if (TERMINATED(11))
      break;
    else if (conflict_limit_hit(solver))
      break;
    else if (kissat_inc_reducing(solver))
      res = kissat_inc_reduce(solver);
    else if (kissat_inc_restarting(solver))
      kissat_inc_restart(solver);
    else if (kissat_inc_rephasing(solver))
      kissat_inc_rephase(solver);
    else if (kissat_inc_eliminating(solver))
      res = kissat_inc_eliminate(solver);
    else if (kissat_inc_probing(solver))
      res = kissat_inc_probe(solver);
    else if (!solver->level && solver->unflushed)
      kissat_inc_flush_trail(solver);
    else if (decision_limit_hit(solver))
      break;
    else
      kissat_inc_decide(solver);
  }

  stop_search(solver, res);

  return res;
}
