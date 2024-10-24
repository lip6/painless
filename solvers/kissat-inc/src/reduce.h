#ifndef _reduce_h_INCLUDED
#define _reduce_h_INCLUDED

#include <stdbool.h>

struct kissat;

bool kissat_inc_reducing (struct kissat *);
int kissat_inc_reduce (struct kissat *);

#endif
