#ifndef _ls_h_INCLUDED
#define _ls_h_INCLUDED
#include <stdbool.h>

struct kissat;

bool kissat_ccanring (struct kissat *);
int kissat_ccanr (struct kissat *);

#endif
