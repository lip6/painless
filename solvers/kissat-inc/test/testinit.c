#include "test.h"

#include "../src/import.h"
#include "../src/resize.h"
#include "../src/resources.h"

static kissat *
new_solver (void)
{
  kissat *solver = kissat_inc_init ();
  tissat_init_solver (solver);
  return solver;
}

static void
test_init_zero (void)
{
#ifndef QUIET
  assert (kissat_inc_verbosity (0) < 0);
#endif
  kissat_inc_warning (0, "YOU SHOULD NOT SEE THIS WARNING!");
  kissat_inc_signal (0, "YOU SHOULD NOT SEE THIS SIGNAL MESSAGE FOR ", 0);
}

static void
test_init_release (void)
{
  kissat *solver = new_solver ();
  kissat_inc_release (solver);
}

static void
test_init_enlarge_release (void)
{
  format format;
  memset (&format, 0, sizeof format);
#ifndef QUIET
  printf ("before allocating solver %s\n",
	  kissat_inc_format_bytes (&format, kissat_inc_maximum_resident_set_size ()));
#endif
  kissat *solver = new_solver ();
  kissat_inc_enlarge_variables (solver, 17);
  for (int i = 1; i < (tissat_big ? 28 : 21); i++)
    {
      kissat_inc_enlarge_variables (solver, solver->vars + 1);
      kissat_inc_enlarge_variables (solver, 1 << i);
#ifndef QUIET
      printf ("after %d << 20 %s\n", i,
	      kissat_inc_format_bytes (&format,
				   kissat_inc_maximum_resident_set_size ()));
#endif
    }
  if (tissat_big)
    {
      kissat_inc_enlarge_variables (solver, EXTERNAL_MAX_VAR);
#ifndef QUIET
      printf ("after %d %s\n", EXTERNAL_MAX_VAR,
	      kissat_inc_format_bytes (&format,
				   kissat_inc_maximum_resident_set_size ()));
#endif
    }
  kissat_inc_release (solver);
#ifndef QUIET
  printf ("after deallocating solver %s\n",
	  kissat_inc_format_bytes (&format, kissat_inc_maximum_resident_set_size ()));
#endif
}

static void
test_init_import_release (void)
{
  kissat *solver = new_solver ();
  (void) kissat_inc_import_literal (solver, -1);
  (void) kissat_inc_import_literal (solver, 42);
  if (tissat_big)
    (void) kissat_inc_import_literal (solver, EXTERNAL_MAX_VAR);
  kissat_inc_release (solver);
}

static void
test_init_add_release (void)
{
#ifndef NDEBUG
  const int max_check = 1;
#else
  const int max_check = 0;
#endif
  for (int check = 0; check <= max_check; check++)
    {
      kissat *solver = new_solver ();
      kissat_inc_set_option (solver, "log", 4);
#ifndef NDEBUG
      kissat_inc_set_option (solver, "check", check);
#endif
      kissat_inc_add (solver, 2);
      kissat_inc_add (solver, -12);
      kissat_inc_add (solver, 0);
      kissat_inc_add (solver, -17);
      kissat_inc_add (solver, -2);
      kissat_inc_add (solver, 0);
      kissat_inc_add (solver, -1);
      kissat_inc_add (solver, 1);
      kissat_inc_add (solver, -1);
      kissat_inc_add (solver, 1);
      kissat_inc_add (solver, 0);
      kissat_inc_add (solver, 7);
      kissat_inc_add (solver, 8);
      kissat_inc_add (solver, 7);
      kissat_inc_add (solver, 8);
      kissat_inc_add (solver, 7);
      kissat_inc_add (solver, 0);
      kissat_inc_add (solver, -7);
      kissat_inc_add (solver, -7);
      kissat_inc_add (solver, -7);
      kissat_inc_add (solver, -7);
      kissat_inc_add (solver, -7);
      kissat_inc_add (solver, 0);
      kissat_inc_add (solver, 9);
      kissat_inc_add (solver, 7);
      kissat_inc_add (solver, 0);
      if (tissat_big)
	{
	  kissat_inc_add (solver, EXTERNAL_MAX_VAR);
	  kissat_inc_add (solver, 0);
	}
      kissat_inc_release (solver);
    }
}

void
tissat_schedule_init (void)
{
  SCHEDULE_FUNCTION (test_init_zero);
  SCHEDULE_FUNCTION (test_init_release);
  SCHEDULE_FUNCTION (test_init_enlarge_release);
  SCHEDULE_FUNCTION (test_init_import_release);
  SCHEDULE_FUNCTION (test_init_add_release);
}
