#ifndef _probe_h_INCLUDED
#define _probe_h_INCLUDED

struct kissat;

bool
kissat_mab_probing(struct kissat*);
int
kissat_mab_probe(struct kissat*);

#endif
