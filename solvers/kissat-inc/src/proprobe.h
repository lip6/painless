#ifndef _proprobe_h_INCLUDED
#define _proprobe_h_INCLUDED

struct kissat;
struct clause;

struct clause *kissat_inc_probing_propagate (struct kissat *, struct clause *);

#endif
