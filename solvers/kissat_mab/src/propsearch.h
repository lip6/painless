#ifndef _propsearch_h_INCLUDED
#define _propsearch_h_INCLUDED

struct kissat;
struct clause;

struct clause*
kissat_mab_search_propagate(struct kissat*);

#endif
