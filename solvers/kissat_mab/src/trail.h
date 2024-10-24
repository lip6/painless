#ifndef _trail_h_INLCUDED
#define _trail_h_INLCUDED

#include "reference.h"

#include <stdbool.h>

struct kissat;

void
kissat_mab_flush_trail(struct kissat*);
void
kissat_mab_restart_and_flush_trail(struct kissat*);
bool
kissat_mab_flush_and_mark_reason_clauses(struct kissat*, reference start);
void
kissat_mab_unmark_reason_clauses(struct kissat*, reference start);
void
kissat_mab_mark_reason_clauses(struct kissat*, reference start);

#endif
