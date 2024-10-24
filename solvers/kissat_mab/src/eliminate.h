#ifndef _eliminate_hpp_INCLUDED
#define _eliminate_hpp_INCLUDED

#include <stdbool.h>

struct kissat;
struct clause;

void
kissat_mab_update_after_removing_variable(struct kissat*, unsigned);
void
kissat_mab_update_after_removing_clause(struct kissat*, struct clause*, unsigned except);

void
kissat_mab_flush_units_while_connected(struct kissat*);

bool
kissat_mab_eliminating(struct kissat*);
int
kissat_mab_eliminate(struct kissat*);

void
kissat_mab_eliminate_binary(struct kissat*, unsigned, unsigned);
void
kissat_mab_eliminate_clause(struct kissat*, struct clause*, unsigned);

#endif
