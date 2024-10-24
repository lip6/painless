#ifndef _print_h_INCLUDED
#define _print_h_INCLUDED

#ifndef QUIET

#include <stdint.h>

struct kissat;

int kissat_inc_verbosity (struct kissat *);

void kissat_inc_signal (struct kissat *, const char *type, int sig);
void kissat_inc_message (struct kissat *, const char *fmt, ...);
void kissat_inc_section (struct kissat *, const char *name);
void kissat_inc_verbose (struct kissat *, const char *fmt, ...);
void kissat_inc_very_verbose (struct kissat *, const char *fmt, ...);
void kissat_inc_extremely_verbose (struct kissat *, const char *fmt, ...);
void kissat_inc_warning (struct kissat *, const char *fmt, ...);

void kissat_inc_phase (struct kissat *, const char *name, uint64_t,
		   const char *fmt, ...);
#else

#define kissat_inc_message(...) do { } while (0)
#define kissat_inc_phase(...) do { } while (0)
#define kissat_inc_section(...) do { } while (0)
#define kissat_inc_signal(...) do { } while (0)
#define kissat_inc_verbose(...) do { } while (0)
#define kissat_inc_very_verbose(...) do { } while (0)
#define kissat_inc_extremely_verbose(...) do { } while (0)
#define kissat_inc_warning(...) do { } while (0)

#endif

#endif
