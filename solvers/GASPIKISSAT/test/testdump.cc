#include "test.h"

static void
test_dump (void)
{
  printf ("First generating and solving simple CNF:\n\n");
  kissat *solver = gaspi::kissat_init ();
  gaspi::kissat_add (solver, 1);
  gaspi::kissat_add (solver, -2);
  gaspi::kissat_add (solver, 0);
  gaspi::kissat_add (solver, -1);
  gaspi::kissat_add (solver, 2);
  gaspi::kissat_add (solver, 0);
  gaspi::kissat_add (solver, 1);
  gaspi::kissat_add (solver, 2);
  gaspi::kissat_add (solver, 3);
  gaspi::kissat_add (solver, 0);
  gaspi::kissat_add (solver, 1);
  gaspi::kissat_add (solver, 2);
  gaspi::kissat_add (solver, -3);
  gaspi::kissat_add (solver, 0);
  int res = gaspi::kissat_solve (solver);
  assert (res == 10);
  int a = gaspi::kissat_value(solver, 1);
  int b = gaspi::kissat_value(solver, 2);
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
  gaspi::kissat_release(solver);
}

void
tissat_schedule_dump (void)
{
  SCHEDULE_FUNCTION (test_dump);
}
