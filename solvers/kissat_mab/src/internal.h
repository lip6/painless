#ifndef _internal_h_INCLUDED
#define _internal_h_INCLUDED

#include "arena.h"
#include "assign.h"
#include "averages.h"
#include "ccnr.h"
#include "check.h"
#include "clause.h"
#include "clueue.h"
#include "cover.h"
#include "cvec.h"
#include "extend.h"
#include "flags.h"
#include "format.h"
#include "frames.h"
#include "heap.h"
#include "kissat.h"
#include "limits.h"
#include "literal.h"
#include "mode.h"
#include "options.h"
#include "phases.h"
#include "profile.h"
#include "proof.h"
#include "queue.h"
#include "random.h"
#include "reluctant.h"
#include "rephase.h"
#include "smooth.h"
#include "stack.h"
#include "statistics.h"
#include "value.h"
#include "vector.h"
#include "watch.h"
// Begin Painless
#include "painless.h"
// End Painless

typedef struct temporary temporary;

struct temporary
{
	bool satisfied;
	bool shrink;
	bool trivial;
	unsigneds lits;
};

typedef struct idxrank idxrank;

struct idxrank
{
	unsigned idx;
	unsigned rank;
};

typedef struct import import;

struct import
{
	unsigned lit : 30;
	bool imported : 1;
	bool eliminated : 1;
};

// *INDENT-OFF*

typedef STACK(value) eliminated;
typedef STACK(import) imports;
typedef STACK(idxrank) idxranks;
typedef STACK(watch) statches;
typedef STACK(watch*) patches;

// *INDENT-ON*

struct kissat
{
	int ls_call_num;
	int ls_best_unsat_num;
	int ls_bms;
	int restarts_gap;
	long long ls_mems_num;
	long long ls_mems_num_min;
	double ls_mems_inc;
	CCAnr* lssolver;
	int* phase_value;
	char* top_trail_soln;
	char* ls_mediation_soln;
	char* ls_best_soln;
	char* ls_active;
	// int *last_ls_init;
	int ccanr_has_constructed;
	int freeze_ls_restart_num;
	// int initshuffle;
	int ndecide;
	int bump_one;
	int verso;

	// Begin Painless

	// Stats
	unsigned nb_exported;
	unsigned nb_exported_filtered;
	unsigned nb_imported_units;
	unsigned nb_imported_bin;
	unsigned nb_imported_cls;

	int id_painless;
	painless_clause pclause; // only for export and unit imports
	void* painless;			 // used as the callback parameter

	char (*cbkImportUnit)(void*, kissat*);
	char (*cbkImportClause)(void*, kissat*);
	char (*cbkExportClause)(void*, kissat*); // callback for clause learning
	// End Painless

	int* init_phase;
	int max_var; // from cnf file: to be removed
#ifdef LOGGING
	bool compacting;
#endif
	bool extended;
	bool inconsistent;
	bool iterating;
	bool probing;
#ifndef QUIET
	bool sectioned;
#endif
	bool stable;
	bool watching;
#ifdef COVERAGE
	volatile unsigned terminate;
#else
	volatile bool terminate;
#endif

	unsigned vars;
	unsigned size;
	/* Number of active variables*/
	unsigned active;
	unsigned nconflict;
	unsigned nassign;

	ints exportk; /* Painless: renamed since its is a keyword in C++ */
	ints units;
	imports import;
	extensions extend;
	unsigneds witness;

	/**
	 * An array of struct assigned, one for each iidx.
	 * Meaning: stocks information about the assignement of a variable, without the value assigned.
	 * struct assigned
	  {
		unsigned level:LD_MAX_LEVEL;
		unsigned analyzed:2;
		bool redundant:1;
		bool binary:1;
		unsigned reason; // must be a clause index or something like that
	  };
	*/
	assigned* assigned;

	/**
	 * An array of struct flag, one for each iidx
	 * Meaning: status of each variable
	 * struct flags
	  {
		bool active:1;
		bool eliminate:1;
		bool eliminated:1;
		bool fixed:1;
		bool probe:1;
		bool subsume:1;
	  };
	*/
	flags* flags;

	/**
	 * Mystery: where is it allocated ? kisst_init ?
	 * An array of signed char, one for each ilit (size  = LITS)
	 * Meaning: if everything is initialized to zero, in kissat_mab_add, it marks an already seen unassigned literal by
	 * 1, and its negation by -1 It is useful to detect tautologies, and it is reset to zero after the end of an
	 * original clause
	 */
	mark* marks;

	/**
	 * An array of signed char, one for each ilit
	 * Meaning (not sure): -1 for falsified, 1 for truefied, 0 for unassigned
	 */
	value* values;
	phase* phases;

	eliminated eliminated;
	unsigneds etrail;

	/**
	 * Array of struct link, one for each variable
	 * Meaning: Used to have a linked list (queue) of unassigned variables ??
	 * struct links
	  {
		unsigned prev, next;
		unsigned stamp;
	  };
	*/
	links* links;

	/**
	 * first and last are iidx values.
	 * Meaning: queue of unassigned, or to be propagated ??
	 * struct queue
	  {
		unsigned first, last, stamp;
		struct
		{
		  unsigned idx, stamp;
		} search;
	  };
	*/
	queue queue;

	rephased rephased;

	heap scores;
	double scinc;

	// CHB
	heap scores_chb;
	unsigned* conflicted_chb;
	double step_chb;
	double step_dec_chb;
	double step_min_chb;

	// MAB
	unsigned heuristic;
	bool mab;
	double mabc;
	double mab_reward[2];
	unsigned mab_select[2];
	unsigned mab_heuristics;
	double mab_decisions;
	unsigned* mab_chosen;
	unsigned mab_chosen_tot;

	heap schedule;

	unsigned level;
	frames frames;

	unsigneds trail;
	unsigned propagated;

	unsigned best_assigned;
	unsigned consistently_assigned;
	unsigned target_assigned;
	unsigned unflushed;
	unsigned unassigned;

	unsigneds delayed;

#if defined(LOGGING) || !defined(NDEBUG)
	unsigneds resolvent_lits;
#endif
	unsigned resolvent_size;
	unsigned antecedent_size;

	unsigned transitive;

	unsigneds analyzed;
	idxranks bump;
	unsigneds levels;
	unsigneds minimize;
	unsigneds poisoned;
	unsigneds promote;
	unsigneds removable;

	clause conflict;

	/**
	 * Must be a structure for a clause used only temporarly as the name suggests.
	struct temporary
	* A clause is first initialize here before calling kissat_mab_new_original_clause or
	kissat_mab_new_redundant_clause, which will call new_clause using the data saved here
	{
	  bool satisfied;
	  bool shrink;
	  bool trivial;
	  unsigneds lits; // a stack of unsigned int
	};
	*/
	temporary clause;

	arena arena;
	clueue clueue;
	vectors vectors;
	reference first_reducible;
	reference last_irredundant;
	watches* watches;

	sizes sorter;

	generator random;
	averages averages[2];
	reluctant reluctant;

	bounds bounds;
	delays delays;
	enabled enabled;
	limited limited;
	limits limits;
	waiting waiting;

	statistics statistics;
	mode mode;

	uint64_t ticks;

	format format;

	statches antecedents[2];
	statches gates[2];
	patches xorted[2];
	unsigneds resolvents;
	bool resolve_gate;

#ifndef NMETRICS
	uint64_t* gate_eliminated;
#else
	bool gate_eliminated;
#endif

#if !defined(NDEBUG) || !defined(NPROOFS)
	unsigneds added;
	unsigneds removed;
#endif

#if !defined(NDEBUG) || !defined(NPROOFS) || defined(LOGGING)
	ints original;
	size_t offset_of_last_original_clause;
#endif

#ifndef QUIET
	profiles profiles;
#endif

#ifndef NOPTIONS
	options options;
#endif

#ifndef NDEBUG
	checker* checker;
#endif

#ifndef NPROOFS
	proof* proof;
#endif
};

#define VARS (solver->vars)
#define LITS (2 * solver->vars)

static inline unsigned
kissat_mab_assigned(kissat* solver)
{
	assert(VARS >= solver->unassigned);
	return VARS - solver->unassigned;
}

#define all_variables(IDX)                                                                                             \
	unsigned IDX = 0, IDX_END = solver->vars;                                                                          \
	IDX != IDX_END;                                                                                                    \
	++IDX

#define all_literals(LIT)                                                                                              \
	unsigned LIT = 0, LIT_END = LITS;                                                                                  \
	LIT != LIT_END;                                                                                                    \
	++LIT

#define all_clauses(C)                                                                                                 \
	clause *C = (clause*)BEGIN_STACK(solver->arena), *C_END = (clause*)END_STACK(solver->arena), *C_NEXT;              \
	C != C_END && (C_NEXT = kissat_mab_next_clause(C), true);                                                          \
	C = C_NEXT

#endif
