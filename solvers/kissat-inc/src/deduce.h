#ifndef _deduce_h_INCLUDED
#define _deduce_h_INCLUDED

struct clause;
struct kissat;

struct clause *kissat_inc_deduce_first_uip_clause (struct kissat *,
					       struct clause *);

#endif
