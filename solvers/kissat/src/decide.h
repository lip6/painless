#ifndef _decide_h_INCLUDED
#define _decide_h_INCLUDED

struct kissat;

void
kissat_decide(struct kissat*);
void
kissat_start_random_sequence(struct kissat*);
void
kissat_internal_assume(struct kissat*, unsigned lit);

#define INITIAL_PHASE (GET_OPTION(phase) ? 1 : -1)

#endif
