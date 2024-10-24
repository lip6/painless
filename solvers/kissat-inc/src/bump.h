#ifndef _bump_h_INCLUDED
#define _bump_h_INCLUDED

struct kissat;

void kissat_inc_bump_variables (struct kissat *);
void kissat_inc_bump_chb (struct kissat *, unsigned idx, double multiplier);
void kissat_inc_init_shuffle (struct kissat *, int maxvar);
void kissat_inc_decay_chb (struct kissat *);
void kissat_inc_shuffle_score(struct kissat *);
void kissat_inc_update_conflicted_chb (struct kissat *);

#define MAX_SCORE 1e150

#endif
