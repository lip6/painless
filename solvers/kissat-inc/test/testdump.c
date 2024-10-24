#include "test.h"

static void
test_dump (void)
{
  printf ("First generating and solving simple CNF:\n\n");
  kissat *solver = kissat_inc_init ();
  kissat_inc_add (solver, 1);
  kissat_inc_add (solver, -2);
  kissat_inc_add (solver, 0);
  kissat_inc_add (solver, -1);
  kissat_inc_add (solver, 2);
  kissat_inc_add (solver, 0);
  kissat_inc_add (solver, 1);
  kissat_inc_add (solver, 2);
  kissat_inc_add (solver, 3);
  kissat_inc_add (solver, 0);
  kissat_inc_add (solver, 1);
  kissat_inc_add (solver, 2);
  kissat_inc_add (solver, -3);
  kissat_inc_add (solver, 0);
  int res = kissat_inc_solve (solver);
  assert (res == 10);
  int a = kissat_inc_value (solver, 1);
  int b = kissat_inc_value (solver, 2);
  assert (a > 0);
  assert (b > 0);
#ifndef NDEBUG
  void dump (kissat *);
  printf ("\nCompletely dumping solver:\n\n");
  dump (solver);
  printf ("\nDumping also vectors:\n\n");
  void dump_vectors (kissat *);
  dump_vectors (solver);
#endif
  kissat_inc_release (solver);
}

void
tissat_schedule_dump (void)
{
  SCHEDULE_FUNCTION (test_dump);
}
