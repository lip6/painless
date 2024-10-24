#ifndef _eliminate_hpp_INCLUDED
#define _eliminate_hpp_INCLUDED

#include <stdbool.h>

struct kissat;
struct clause;

void kissat_inc_update_after_removing_variable (struct kissat *, unsigned);
void kissat_inc_update_after_removing_clause (struct kissat *, struct clause *,
					  unsigned except);

void kissat_inc_flush_units_while_connected (struct kissat *);

bool kissat_inc_eliminating (struct kissat *);
int kissat_inc_eliminate (struct kissat *);

void kissat_inc_eliminate_binary (struct kissat *, unsigned, unsigned);
void kissat_inc_eliminate_clause (struct kissat *, struct clause *, unsigned);

#endif
