#ifndef _decide_h_INCLUDED
#define _decide_h_INCLUDED

struct kissat;

void
kissat_mab_decide(struct kissat*);
void
kissat_mab_internal_assume(struct kissat*, unsigned lit);
unsigned
kissat_mab_next_decision_variable(struct kissat*);

#define INITIAL_PHASE (GET_OPTION(phase) ? 1 : -1)

#endif
