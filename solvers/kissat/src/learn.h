#ifndef _learn_h_INCLUDED
#define _learn_h_INCLUDED

struct kissat;

void
kissat_learn_clause(struct kissat*);
void
kissat_update_learned(struct kissat*, unsigned glue, unsigned size);
unsigned
kissat_determine_new_level(struct kissat*, unsigned jump);

// Begin Painless
#include <stdbool.h>

bool
kissat_import_from_painless(struct kissat*);
bool
kissat_import_unit_from_painless(struct kissat*);
// End Painless

#endif
