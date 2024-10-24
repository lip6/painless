#ifndef _kissat_mab_h_INCLUDED
#define _kissat_mab_h_INCLUDED

typedef struct kissat kissat;

// Default (partial) IPASIR interface.

const char*
kissat_mab_signature(void);
kissat*
kissat_mab_init(void);
void
kissat_mab_add(kissat* solver, int lit);
int
kissat_mab_solve(kissat* solver);
void
kissat_mab_terminate(kissat* solver);
int
kissat_mab_value(kissat* solver, int lit);
void
kissat_mab_release(kissat* solver);

// void kissat_mab_set_terminate(kissat *solver,
// 						  void *state, int (*terminate)(void *state));

// Additional API functions.

void
kissat_mab_reserve(kissat* solver, int max_var);

const char*
kissat_mab_id(void);
const char*
kissat_mab_version(void);
const char*
kissat_mab_compiler(void);

void
kissat_mab_banner(const char* line_prefix, const char* name_of_app);

int
kissat_mab_get_option(kissat* solver, const char* name);
int
kissat_mab_set_option(kissat* solver, const char* name, int new_value);

void
kissat_mab_set_configuration(kissat* solver, const char* name);

void
kissat_mab_set_conflict_limit(kissat* solver, unsigned);
void
kissat_mab_set_decision_limit(kissat* solver, unsigned);

void
kissat_mab_print_statistics(kissat* solver);

// Begin Painless
// Moved from application.h
void
kissat_mab_mabvars_init(struct kissat*);

// Kissat init
void
kissat_mab_set_import_unit_call(kissat*, char (*)(void*, kissat*));
void
kissat_mab_set_import_call(kissat*, char (*)(void*, kissat*));
void
kissat_mab_set_export_call(kissat*, char (*)(void*, kissat*));
void
kissat_mab_set_painless(kissat*, void*);
void
kissat_mab_set_id(kissat*, int);
void
kissat_mab_set_maxVar(kissat*, unsigned);
unsigned
kissat_mab_get_maxVar(kissat*);

// Interface inner functions
void
kissat_mab_set_phase(kissat*, unsigned, int);
char
kissat_mab_check_searches(kissat*);
void
kissat_mab_check_model(kissat*);

/* Used the kissat struct for the STACK macros to work fine */
// Interface for solver->pclause
void
kissat_mab_clear_pclause(kissat*);
unsigned
kissat_mab_pclause_size(kissat*);
void
kissat_mab_push_plit(kissat*, int);
int
kissat_mab_pop_plit(kissat*);
int
kissat_mab_peek_plit(kissat*, unsigned);
void
kissat_mab_set_pglue(kissat*, unsigned);
unsigned
kissat_mab_get_pglue(kissat*);

// Interface for solver->clause
char
kissat_mab_push_lits(kissat*, const int*, unsigned);
void
kissat_mab_print_sharing_stats(kissat*);
// End Painless

#endif
