#ifndef _backtrack_h_INCLUDED
#define _backtrack_h_INCLUDED

struct kissat;

void kissat_inc_backtrack (struct kissat *, unsigned level);
void kissat_inc_backtrack_propagate_and_flush_trail (struct kissat *);

#endif
