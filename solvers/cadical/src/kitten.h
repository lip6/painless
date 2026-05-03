#ifndef _kitten_h_INCLUDED
#define _kitten_h_INCLUDED

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define KITTEN_VISIBILITY __attribute__((visibility("hidden")))

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kitten kitten;

KITTEN_VISIBILITY kitten *kitten_init (void);
KITTEN_VISIBILITY void kitten_clear (kitten *);
KITTEN_VISIBILITY void kitten_release (kitten *);

#ifdef LOGGING
KITTEN_VISIBILITY void kitten_set_logging (kitten *kitten);
#endif

KITTEN_VISIBILITY void kitten_track_antecedents (kitten *);

KITTEN_VISIBILITY void kitten_shuffle_clauses (kitten *);
KITTEN_VISIBILITY void kitten_flip_phases (kitten *);
KITTEN_VISIBILITY void kitten_randomize_phases (kitten *);

KITTEN_VISIBILITY void kitten_assume (kitten *, unsigned lit);
KITTEN_VISIBILITY void kitten_assume_signed (kitten *, int lit);

KITTEN_VISIBILITY void kitten_clause (kitten *, size_t size, unsigned *);
void citten_clause_with_id (kitten *, unsigned id, size_t size, int *);
KITTEN_VISIBILITY void kitten_unit (kitten *, unsigned);
KITTEN_VISIBILITY void kitten_binary (kitten *, unsigned, unsigned);

KITTEN_VISIBILITY void kitten_clause_with_id_and_exception (kitten *, unsigned id,
                                          size_t size, const unsigned *,
                                          unsigned except);

void citten_clause_with_id_and_exception (kitten *, unsigned id,
                                          size_t size, const int *,
                                          unsigned except);
void citten_clause_with_id_and_equivalence (kitten *, unsigned id,
                                            size_t size, const int *,
                                            unsigned, unsigned);
KITTEN_VISIBILITY void kitten_no_ticks_limit (kitten *);
KITTEN_VISIBILITY void kitten_set_ticks_limit (kitten *, uint64_t);
KITTEN_VISIBILITY uint64_t kitten_current_ticks (kitten *);

KITTEN_VISIBILITY void kitten_no_terminator (kitten *);
KITTEN_VISIBILITY void kitten_set_terminator (kitten *, void *, int (*) (void *));

KITTEN_VISIBILITY int kitten_solve (kitten *);
KITTEN_VISIBILITY int kitten_status (kitten *);

KITTEN_VISIBILITY signed char kitten_value (kitten *, unsigned);
KITTEN_VISIBILITY signed char kitten_signed_value (kitten *, int); // converts second argument
KITTEN_VISIBILITY signed char kitten_fixed (kitten *, unsigned);
KITTEN_VISIBILITY signed char kitten_fixed_signed (kitten *, int); // converts
KITTEN_VISIBILITY bool kitten_failed (kitten *, unsigned);
KITTEN_VISIBILITY bool kitten_flip_literal (kitten *, unsigned);
KITTEN_VISIBILITY bool kitten_flip_signed_literal (kitten *, int);

KITTEN_VISIBILITY unsigned kitten_compute_clausal_core (kitten *, uint64_t *learned);
KITTEN_VISIBILITY void kitten_shrink_to_clausal_core (kitten *);

KITTEN_VISIBILITY void kitten_traverse_core_ids (kitten *, void *state,
                               void (*traverse) (void *state, unsigned id));

KITTEN_VISIBILITY void kitten_traverse_core_clauses (kitten *, void *state,
                                   void (*traverse) (void *state,
                                                     bool learned, size_t,
                                                     const unsigned *));
KITTEN_VISIBILITY void kitten_traverse_core_clauses_with_id (
    kitten *, void *state,
    void (*traverse) (void *state, unsigned, bool learned, size_t,
                      const unsigned *));
KITTEN_VISIBILITY void kitten_trace_core (kitten *, void *state,
                        void (*trace) (void *, unsigned, unsigned, bool,
                                       size_t, const unsigned *, size_t,
                                       const unsigned *));

KITTEN_VISIBILITY int kitten_compute_prime_implicant (kitten *kitten, void *state,
                                    bool (*ignore) (void *, unsigned));

KITTEN_VISIBILITY void kitten_add_prime_implicant (kitten *kitten, void *state, int side,
                                 void (*add_implicant) (void *, int, size_t,
                                                        const unsigned *));

KITTEN_VISIBILITY int kitten_flip_and_implicant_for_signed_literal (kitten *kitten, int elit);

#ifdef __cplusplus
}
#endif

#endif
