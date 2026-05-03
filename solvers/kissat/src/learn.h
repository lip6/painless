#ifndef _learn_h_INCLUDED
#define _learn_h_INCLUDED

struct kissat;

void kissat_learn_clause (struct kissat *);
void kissat_update_learned (struct kissat *, unsigned glue, unsigned size);
unsigned kissat_determine_new_level (struct kissat *, unsigned jump);

// Begin Painless
#include <stdbool.h>
int kissat_external_learn_clauses (struct kissat *);
bool kissat_external_learning (struct kissat *);
// End Painless

#endif
