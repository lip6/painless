#ifndef _trail_h_INLCUDED
#define _trail_h_INLCUDED

#include "reference.h"

#include <stdbool.h>

struct kissat;

void kissat_inc_flush_trail (struct kissat *);
void kissat_inc_restart_and_flush_trail (struct kissat *);
bool kissat_inc_flush_and_mark_reason_clauses (struct kissat *, reference start);
void kissat_inc_unmark_reason_clauses (struct kissat *, reference start);
void kissat_inc_mark_reason_clauses (struct kissat *, reference start);

#endif
