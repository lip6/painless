#ifndef _substitute_h_INCLUDED
#define _substitute_h_INCLUDED

#include <stdbool.h>

struct kissat;

void
kissat_mab_substitute(struct kissat*, bool first);

#endif
