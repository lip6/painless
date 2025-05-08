/*-------------------------------------------------------------------------*/
/* TaSSAT is a SLS SAT solver that implements a weight transferring algorithm.
It is based on Yalsat (by Armin Biere)
Copyright (C) 2023  Md Solimul Chowdhury, Cayden Codel, and Marijn Heule, Carnegie Mellon University, Pittsburgh, PA,
USA. */
/*-------------------------------------------------------------------------*/

#ifndef LIBYALS_H_INCLUDED
#define LIBYALS_H_INCLUDED

/*------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>

/*------------------------------------------------------------------------*/

typedef struct Yals Yals;

/*------------------------------------------------------------------------*/

Yals*
tass_new();
void
tass_del(Yals*);

/*------------------------------------------------------------------------*/

typedef void* (*YalsMalloc)(void*, size_t);
typedef void* (*YalsRealloc)(void*, void*, size_t, size_t);
typedef void (*YalsFree)(void*, void*, size_t);

Yals*
tass_new_with_mem_mgr(void*, YalsMalloc, YalsRealloc, YalsFree);

/*------------------------------------------------------------------------*/

void
tass_srand(Yals*, unsigned long long seed);
int
tass_setopt(Yals*, const char* name, int val);
void
tass_setprefix(Yals*, const char*);
void
tass_setout(Yals*, FILE*);
void
tass_setphase(Yals*, int lit);
void
tass_setflipslimit(Yals*, long long);
void
tass_setmemslimit(Yals*, long long);

int
tass_getopt(Yals*, const char* name);
void
tass_usage(Yals*);
void
tass_showopts(Yals*);

// Painless Extension

int
tass_init(Yals*, char);

int
tass_get_res(Yals*);

void
tass_init_outer_restart_interval(Yals*);

void
tass_init_one_outer_iteration(Yals*);

char
tass_need_to_run_max_tries(Yals*);

void
tass_init_inner_restart_interval(Yals*);

int
tass_done(Yals*);

int
tass_need_to_restart_outer(Yals*);

int
tass_need_to_restart_inner(Yals*);

void
tass_restart_inner(Yals*);

void
tass_restart_outer(Yals*);

void
tass_enable_liwet_if_needed(Yals*);

void
tass_flip(Yals*);

void
tass_flip_liwet(Yals*, int);

void
tass_disable_liwet(Yals*);

void
tass_enable_liwet(Yals*);

int
tass_is_liwet_active(Yals*);

int
tass_needs_liwet(Yals*);

void
tass_liwet_transfer_weights(Yals*);

int
tass_pick_literal_liwet(Yals*);

int
tass_liwet_get_uwrvs_size(Yals*);

double
tass_liwet_get_lit_gain(Yals*, int, int*);

int
tass_liwet_get_positive_lit(Yals*, int);

void
tass_liwet_compute_uwrvs(Yals*);

void
tass_liwet_compute_uwrvs_top_n(Yals*, int*, int);

unsigned
tass_satcnt(Yals*, int);

double*
tass_get_clause_weights_pointer(Yals*);

double
tass_get_clause_weight(Yals*, int);

int*
tass_liwet_satcnt_pointer(Yals*);

double*
tass_get_picked_gains_pointer(Yals*);

int*
tass_get_picked_lits_pointer(Yals*);

int
tass_get_var_count(Yals*);

int
tass_get_clauses_count(Yals*);

int*
tass_lits(Yals* yals, int cidx);

// End

/*------------------------------------------------------------------------*/

void
tass_add(Yals*, int lit);

int
tass_sat(Yals*);

/*------------------------------------------------------------------------*/

long long
tass_flips(Yals*);
long long
tass_mems(Yals*);

int
tass_minimum(Yals*);
int
tass_lkhd(Yals*);
int
tass_deref(Yals*, int lit);

const int*
tass_minlits(Yals*);

int
tass_flip_count(Yals* yals);

int
tass_nunsat_external(Yals* yals);

/*------------------------------------------------------------------------*/

void
tass_stats(Yals*);
int
tass_sat_palsat(Yals*, int);

/*------------------------------------------------------------------------*/

void
tass_seterm(Yals*, int (*term)(void*), void*);

void
tass_setime(Yals*, double (*time)(void));

void
tass_setmsglock(Yals*, void (*lock)(void*), void (*unlock)(void*), void*);

/*------------------------------------------------------------------------*/

int
init_done(Yals* yals);
int*
cdb_top(Yals* yals);
int*
cdb_start(Yals* yals);
int*
cdb_end(Yals* yals);
int*
occs(Yals* yals);
int
noccs(Yals* yals);
int*
refs(Yals* yals);
int*
lits(Yals* yals);
int
num_vars(Yals* yals);
void
set_tid(Yals* yals, int tid);
int*
preprocessed_trail(Yals* yals);
int
preprocessed_trail_size(Yals* yals);

void
tass_fnpointers(Yals* yals,
				int* (*get_cdb_start)(),
				int* (*get_cdb_end)(),
				int* (*get_cdb_top)(),
				int* (*get_occs)(),
				int (*get_noccs)(),
				int* (*get_refs)(),
				int* (*get_lits)(),
				int (*get_numvars)(),
				int* (*get_preprocessed_trail)(),
				int (*get_preprocessed_trail_size)(),
				void (*set_preprocessed_trail)());
void
set_shared_structures(Yals* yals);

#endif
