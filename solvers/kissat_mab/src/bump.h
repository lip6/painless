#ifndef _bump_h_INCLUDED
#define _bump_h_INCLUDED

struct kissat;

void
kissat_mab_bump_variables(struct kissat*);
void
kissat_mab_bump_chb(struct kissat*, unsigned idx, double multiplier);
void
kissat_mab_decay_chb(struct kissat*);
void
kissat_mab_update_conflicted_chb(struct kissat*);
void
kissat_mab_bump_one(struct kissat*, int idx);
#define MAX_SCORE 1e150

#endif
