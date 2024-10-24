#ifndef _reduce_h_INCLUDED
#define _reduce_h_INCLUDED

#include <stdbool.h>

struct kissat;

bool
kissat_mab_reducing(struct kissat*);
int
kissat_mab_reduce(struct kissat*);

#endif
