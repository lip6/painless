/*-------------------------------------------------------------------------*/
/* TaSSAT is a SLS SAT solver that implements a weight transferring algorithm.
It is based on Yalsat (by Armin Biere)
Copyright (C) 2023  Md Solimul Chowdhury, Cayden Codel, and Marijn Heule, Carnegie Mellon University, Pittsburgh, PA, USA. */
/*-------------------------------------------------------------------------*/

#ifndef YILS_H_INCLUDED
#define YILS_H_INCLUDED

#ifndef YALSINTERNAL
#error "this file is internal to 'libyals'"
#endif

/*------------------------------------------------------------------------*/

#include "yals.h"

/*------------------------------------------------------------------------*/

#include <stdlib.h>

/*------------------------------------------------------------------------*/

#ifndef NDEBUG
void tass_logging (Yals *, int logging);
void tass_checking (Yals *, int checking);
#endif

/*------------------------------------------------------------------------*/

void tass_abort (Yals *, const char * fmt, ...);
void tass_exit (Yals *, int exit_code, const char * fmt, ...);
void tass_msg (Yals *, int level, const char * fmt, ...);

const char * tass_default_prefix (void);
const char * tass_version ();
void tass_banner (const char * prefix);

/*------------------------------------------------------------------------*/

double tass_process_time ();				// process time

double tass_sec (Yals *);				// time in 'tass_sat'
size_t tass_max_allocated (Yals *);		// max allocated bytes

/*------------------------------------------------------------------------*/

void * tass_malloc (Yals *, size_t);
void tass_free (Yals *, void*, size_t);
void * tass_realloc (Yals *, void*, size_t, size_t);

/*------------------------------------------------------------------------*/

void tass_srand (Yals *, unsigned long long);

/* liwet non-static methods */
int get_pos (int lit);
void tass_liwet_update_lit_weights_on_make (Yals * yals, int cidx, int lit);
void tass_liwet_update_lit_weights_at_start (Yals * yals, int cidx, int satcnt, int crit);
void tass_liwet_update_lit_weights_at_restart (Yals *yals);
void tass_liwet_compute_neighborhood_for_clause (Yals *yals, int cidx);
void tass_liwet_compute_uwrvs (Yals * yals);
int tass_pick_literal_liwet (Yals * yals);
int tass_pick_non_increasing (Yals * yals);
void tass_liwet_init_build (Yals *yals);
void compute_neighborhood_for_clause_init (Yals *yals, int cidx);
int tass_pick_literals_random (Yals * yals);
void tass_liwet_update_lit_weights_on_break (Yals * yals, int cidx, int lit);
void tass_add_vars_to_uvars (Yals* yals, int cidx);
int tass_var_in_unsat (Yals *yals, int v);
void tass_delete_vars_from_uvars (Yals* yals, int cidx);
void tass_liwet_update_var_unsat_count (Yals *yals, int cidx);
int tass_needs_liwet (Yals *yals); 
void tass_print_stats (Yals * yals); 
void tass_liwet_update_uvars (Yals *yals, int cidx);
void set_options (Yals * yals);
void tass_outer_loop_maxtries (Yals * yals);
void tass_set_wid (Yals * yals, int widx);
int tass_inner_loop_max_tries (Yals * yals);
double set_cspt (Yals * yals);
void tass_set_threadspecvals (Yals * yals, int widx, int nthreads);


#endif
