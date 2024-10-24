#include "print.h"
#include "terminate.h"

#ifndef QUIET

void
kissat_inc_report_termination (kissat * solver, int bit,
			   const char *file, long lineno, const char *fun)
{
  kissat_inc_very_verbose (solver, "%s:%ld: %s: TERMINATED (%d)",
		       file, lineno, fun, bit);
}

#else
int kissat_inc_terminate_dummy_to_avoid_warning;
#endif
