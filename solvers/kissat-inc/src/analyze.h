#ifndef _analyze_h_INCLUDED
#define _analyze_h_INCLUDED

#include <stdbool.h>

struct clause;
struct kissat;

int kissat_inc_analyze (struct kissat *, struct clause *);

#endif
