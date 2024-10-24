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
kissat_inc_signal_handler (int sig)
{
  assert (solver);
  kissat_inc_signal (solver, "caught", sig);
  kissat_inc_print_statistics (solver);
  kissat_inc_signal (solver, "raising", sig);
#ifdef QUIET
  (void) sig;
#endif
  FLUSH_COVERAGE (); } // Keep this '}' in the same line!

// *INDENT-ON*

static volatile bool ignore_alarm = false;

static void
kissat_inc_alarm_handler (void)
{
  if (ignore_alarm)
    return;
  assert (solver);
  kissat_inc_terminate (solver);
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
  solver = kissat_inc_init ();
  kissat_inc_init_alarm (kissat_inc_alarm_handler);
  kissat_inc_init_signal_handler (kissat_inc_signal_handler);
  res = kissat_inc_application (solver, argc, argv);
  kissat_inc_reset_signal_handler ();
  ignore_alarm = true;
  kissat_inc_reset_alarm ();
  kissat_inc_release (solver);
#ifndef NDEBUG
  if (!res)
    return dump (0);
#endif
  return res;
}
