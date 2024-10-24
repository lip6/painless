#include "application.h"
#include "cover.h"
#include "handle.h"
#include "kissat.h"
#include "print.h"

#include <assert.h>
#include <stdbool.h>

static kissat *solver;

// *INDENT-OFF*

static void
kissat_mab_signal_handler (int sig)
{
  assert (solver);
  kissat_mab_signal (solver, "caught", sig);
  kissat_mab_print_statistics (solver);
  kissat_mab_signal (solver, "raising", sig);
#ifdef QUIET
  (void) sig;
#endif
  FLUSH_COVERAGE (); } // Keep this '}' in the same line!

// *INDENT-ON*

static volatile bool ignore_alarm = false;

static void
kissat_mab_alarm_handler (void)
{
  if (ignore_alarm)
    return;
  assert (solver);
  kissat_mab_terminate (solver);
}

#ifndef NDEBUG
extern int dump (kissat *);
#endif

#include "random.h"
#include "error.h"
#include <strings.h>

int
main (int argc, char **argv)
{
  int res;
  solver = kissat_mab_init ();
  kissat_mab_init_alarm (kissat_mab_alarm_handler);
  kissat_mab_init_signal_handler (kissat_mab_signal_handler);
  res = kissat_mab_application (solver, argc, argv);
  kissat_mab_reset_signal_handler ();
  ignore_alarm = true;
  kissat_mab_reset_alarm ();
  kissat_mab_release (solver);
#ifndef NDEBUG
  if (!res)
    return dump (0);
#endif
  return res;
}
