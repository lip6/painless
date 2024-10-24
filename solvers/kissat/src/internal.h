#ifndef _internal_h_INCLUDED
#define _internal_h_INCLUDED

#include "arena.h"
#include "array.h"
#include "assign.h"
#include "averages.h"
#include "check.h"
#include "clause.h"
#include "cover.h"
#include "extend.h"
#include "flags.h"
#include "format.h"
#include "frames.h"
#include "heap.h"
#include "kimits.h"
#include "kissat.h"
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

typedef struct datarank datarank;

struct datarank
{
	unsigned data;
	unsigned rank;
};

typedef struct import import;

struct import
{
	unsigned lit;
	bool imported;
	bool eliminated;
};

typedef struct termination termination;

struct termination
{
#ifdef COVERAGE
	volatile uint64_t flagged;
#else
	volatile bool flagged;
#endif
	volatile void* state;
	int (*volatile terminate)(void*);
};

// clang-format off

typedef STACK (value) eliminated;
typedef STACK (import) imports;
typedef STACK (datarank) dataranks;
typedef STACK (watch) statches;
typedef STACK (watch *) patches;

// clang-format on

struct kitten;

struct kissat
{
#if !defined(NDEBUG) || defined(METRICS)
	bool backbone_computing;
#endif
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
#if !defined(NDEBUG) || defined(METRICS)
	bool transitive_reducing;
	bool vivifying;
#endif
	bool watching;

	bool large_clauses_watched_after_binary_clauses;

	termination termination;

	unsigned vars;
	unsigned size;
	unsigned active;
	unsigned randec;

	// Begin Painless

	// Stats
	unsigned nb_exported;
	unsigned nb_exported_filtered;
	unsigned nb_imported_units;
	unsigned nb_imported_bin;
	unsigned nb_imported_cls;

	ints pclause;	// for export only, filled with external literals didn't use clause for independency
	unsigned pglue; // glue value of pclause
	char do_not_import;

	int id_painless;
	void* painless; // used as the callback parameter

	char (*cbkImportUnit)(void*, kissat*);
	char (*cbkImportClause)(void*, kissat*);
	char (*cbkExportClause)(void*,
							kissat*); // callback for clause learning
	// End Painless

	ints export;
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
	 * Meaning: if everything is initialized to zero, in kissat_add, it marks an already seen unassigned literal by 1,
	 * and its negation by -1 It is useful to detect tautologies, and it is reset to zero after the end of an original
	 * clause
	 */
	mark* marks;

	/**
	 * An array of signed char, one for each ilit
	 * Meaning (not sure): -1 for falsified, 1 for truefied, 0 for unassigned
	 */
	value* values;
	phases phases;

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

	heap scores;
	double scinc;

	heap schedule;
	double scoreshift;

	unsigned level;
	frames frames;

	unsigned_array trail;
	unsigned* propagate;

	unsigned best_assigned;
	unsigned target_assigned;
	unsigned unflushed;
	unsigned unassigned;

	unsigneds delayed;

#if defined(LOGGING) || !defined(NDEBUG)
	unsigneds resolvent;
#endif
	unsigned resolvent_size;
	unsigned antecedent_size;

	dataranks ranks;

	unsigneds analyzed;
	unsigneds levels;
	unsigneds minimize;
	unsigneds poisoned;
	unsigneds promote;
	unsigneds removable;
	unsigneds shrinkable;

	clause conflict;

	bool clause_satisfied;
	bool clause_shrink;
	bool clause_trivial;

	unsigneds clause;
	unsigneds shadow;

	arena arena;
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
	effort last;
	limited limited;
	limits limits;
	waiting waiting;
	unsigned walked;

	statistics statistics;
	mode mode;

	uint64_t ticks;

	format format;

	statches antecedents[2];
	statches gates[2];
	patches xorted[2];
	unsigneds resolvents;
	bool resolve_gate;

	struct kitten* kitten;
#ifdef METRICS
	uint64_t* gate_eliminated;
#else
	bool gate_eliminated;
#endif
	unsigneds sweep;

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

#define SCORES (&solver->scores)

static inline unsigned
kissat_assigned(kissat* solver)
{
	assert(VARS >= solver->unassigned);
	return VARS - solver->unassigned;
}

#define all_variables(IDX)                                                                                             \
	unsigned IDX = 0, IDX##_END = solver->vars;                                                                        \
	IDX != IDX##_END;                                                                                                  \
	++IDX

#define all_literals(LIT)                                                                                              \
	unsigned LIT = 0, LIT##_END = LITS;                                                                                \
	LIT != LIT##_END;                                                                                                  \
	++LIT

#define all_clauses(C)                                                                                                 \
	clause *C = (clause*)BEGIN_STACK(solver->arena), *const C##_END = (clause*)END_STACK(solver->arena), *C##_NEXT;    \
	C != C##_END && (C##_NEXT = kissat_next_clause(C), true);                                                          \
	C = C##_NEXT

#endif
