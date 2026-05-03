#ifndef _kitten_h_INCLUDED
#define _kitten_h_INCLUDED

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define KITTEN_VISIBILITY __attribute__((visibility("hidden")))

typedef struct kitten kitten;

KITTEN_VISIBILITY kitten *kitten_init (void);
KITTEN_VISIBILITY void kitten_clear (kitten *);
KITTEN_VISIBILITY void kitten_release (kitten *);

KITTEN_VISIBILITY void kitten_track_antecedents (kitten *);

KITTEN_VISIBILITY void kitten_shuffle_clauses (kitten *);
KITTEN_VISIBILITY void kitten_flip_phases (kitten *);
KITTEN_VISIBILITY void kitten_randomize_phases (kitten *);

KITTEN_VISIBILITY void kitten_assume (kitten *, unsigned lit);

KITTEN_VISIBILITY void kitten_clause (kitten *, size_t size, unsigned *);
KITTEN_VISIBILITY void kitten_unit (kitten *, unsigned);
KITTEN_VISIBILITY void kitten_binary (kitten *, unsigned, unsigned);

KITTEN_VISIBILITY void kitten_clause_with_id_and_exception (kitten *, unsigned id,
                                          size_t size, const unsigned *,
                                          unsigned except);

KITTEN_VISIBILITY void kitten_no_ticks_limit (kitten *);
KITTEN_VISIBILITY void kitten_set_ticks_limit (kitten *, uint64_t);

KITTEN_VISIBILITY int kitten_solve (kitten *);
KITTEN_VISIBILITY int kitten_status (kitten *);

KITTEN_VISIBILITY signed char kitten_value (kitten *, unsigned);
KITTEN_VISIBILITY signed char kitten_fixed (kitten *, unsigned);
KITTEN_VISIBILITY bool kitten_failed (kitten *, unsigned);
KITTEN_VISIBILITY bool kitten_flip_literal (kitten *, unsigned);

KITTEN_VISIBILITY unsigned kitten_compute_clausal_core (kitten *, uint64_t *learned);
KITTEN_VISIBILITY void kitten_shrink_to_clausal_core (kitten *);

KITTEN_VISIBILITY void kitten_traverse_core_ids (kitten *, void *state,
                               void (*traverse) (void *state, unsigned id));

KITTEN_VISIBILITY void kitten_traverse_core_clauses (kitten *, void *state,
                                   void (*traverse) (void *state,
                                                     bool learned, size_t,
                                                     const unsigned *));
struct kissat;
KITTEN_VISIBILITY kitten *kitten_embedded (struct kissat *);

#endif