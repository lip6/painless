#ifndef _restart_h_INCLUDED
#define _restart_h_INCLUDED

struct kissat;

bool kissat_inc_restarting (struct kissat *);
void kissat_inc_restart (struct kissat *);
void kissat_inc_new_focused_restart_limit (struct kissat *);

#endif
