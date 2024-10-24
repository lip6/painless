#ifndef _promote_h_INCLUDED
#define _promote_h_INCLUDED

struct clause;
struct kissat;

unsigned kissat_inc_recompute_glue (struct kissat *, struct clause *c);
void kissat_inc_promote_clause (struct kissat *, clause *, unsigned new_glue);

#endif
