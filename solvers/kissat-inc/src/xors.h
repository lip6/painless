#ifndef _xors_h_INCLUDED
#define _xors_h_INCLUDED

#include <stdbool.h>

struct kissat;

bool kissat_inc_find_xor_gate (struct kissat *, unsigned lit, unsigned negative);

#endif
