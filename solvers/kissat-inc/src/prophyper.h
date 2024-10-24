#ifndef _prophyper_h_INCLUDED
#define _prophyper_h_INCLUDED

struct kissat;
struct clause;

struct clause *kissat_inc_hyper_propagate (struct kissat *,
				       const struct clause *);

#endif
