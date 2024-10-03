#ifndef _learn_h_INCLUDED
#define _learn_h_INCLUDED

struct kissat;

void kissat_learn_clause (struct kissat *);

// Begin Painless
#include<stdbool.h>

bool kissat_import_from_painless(struct kissat *);
bool kissat_import_unit_from_painless(struct kissat *);
// End Painless
#endif
