#ifndef _restart_h_INCLUDED
#define _restart_h_INCLUDED

struct kissat;

bool
kissat_mab_restarting(struct kissat*);
void
kissat_mab_restart(struct kissat*);
void
kissat_mab_new_focused_restart_limit(struct kissat*);

#endif
