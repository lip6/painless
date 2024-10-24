#ifndef _kissat_h_INCLUDED
#define _kissat_h_INCLUDED

typedef struct kissat kissat;

// Default (partial) IPASIR interface.

const char*
kissat_signature(void);
kissat*
kissat_init(void);
void
kissat_add(kissat* solver, int lit);
int
kissat_solve(kissat* solver);
int
kissat_value(kissat* solver, int lit);
void
kissat_release(kissat* solver);

void
kissat_set_terminate(kissat* solver, void* state, int (*terminate)(void* state));

// Additional API functions.

void
kissat_terminate(kissat* solver);
void
kissat_reserve(kissat* solver, int max_var);

const char*
kissat_id(void);
const char*
kissat_version(void);
const char*
kissat_compiler(void);

const char**
kissat_copyright(void);
void
kissat_build(const char* line_prefix);
void
kissat_banner(const char* line_prefix, const char* name_of_app);

int
kissat_get_option(kissat* solver, const char* name);
int
kissat_set_option(kissat* solver, const char* name, int new_value);

int
kissat_has_configuration(const char* name);
int
kissat_set_configuration(kissat* solver, const char* name);

void
kissat_set_conflict_limit(kissat* solver, unsigned);
void
kissat_set_decision_limit(kissat* solver, unsigned);

void
kissat_print_statistics(kissat* solver);

// Begin Painless

typedef struct
{
	unsigned long conflictsPerSec;
	unsigned long propagationsPerSec;
	unsigned long restarts;
	unsigned long decisionsPerConf;
} KissatMainStatistics;

void
kissat_get_main_statistics(kissat* solver, KissatMainStatistics*);

// Kissat init
void
kissat_set_import_unit_call(kissat*, char (*)(void*, kissat*));
void
kissat_set_import_call(kissat*, char (*)(void*, kissat*));
void
kissat_set_export_call(kissat*, char (*)(void*, kissat*));
void
kissat_set_painless(kissat*, void*);
void
kissat_set_id(kissat*, int);

// Interface inner functions
char
kissat_set_phase(kissat*, unsigned, int);
char
kissat_check_searches(kissat*);

// Sharing
char
kissat_import_pclause(kissat*, const int*, unsigned);
char
kissat_assign_punits(kissat*, int*, unsigned);

void
kissat_set_pglue(kissat*, unsigned);
unsigned
kissat_get_pglue(kissat*);

unsigned
kissat_get_var_count(kissat*);

/* Used the kissat struct for the STACK macros to work fine */
// Interface for solver->pclause
// void kissat_clear_pclause(kissat *);
unsigned
kissat_pclause_size(kissat*);
// void kissat_push_plit(kissat *, int);
// int kissat_pop_plit(kissat *);
int
kissat_peek_plit(kissat*, unsigned);

void
kissat_print_sharing_stats(kissat*);
// End Painless

#endif
