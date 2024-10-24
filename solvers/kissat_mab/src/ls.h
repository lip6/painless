#ifndef _ls_h_INCLUDED
#define _ls_h_INCLUDED
#include <stdbool.h>

struct kissat;

bool
kissat_mab_ccanring(struct kissat*);
int
kissat_mab_ccanr(struct kissat*);

#endif
