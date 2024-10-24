#ifndef _heap_h_INCLUDED
#define _heap_h_INCLUDED

#include "stack.h"

#include <assert.h>
#include <limits.h>
#include <stdbool.h>

#define DISCONTAIN UINT_MAX
#define DISCONTAINED(IDX) ((int)(IDX) < 0)

typedef struct heap heap;

struct heap
{
  bool tainted;
  unsigned vars;
  unsigned size;
  unsigneds stack;
  double *score;
  unsigned *pos;
};

struct kissat;

void kissat_inc_resize_heap (struct kissat *, heap *, unsigned size);
void kissat_inc_release_heap (struct kissat *, heap *);

static inline bool
kissat_inc_heap_contains (heap * heap, unsigned idx)
{
  return idx < heap->vars && !DISCONTAINED (heap->pos[idx]);
}

static inline unsigned
kissat_inc_get_heap_pos (const heap * heap, unsigned idx)
{
  return idx < heap->vars ? heap->pos[idx] : DISCONTAIN;
}

static inline double
kissat_inc_get_heap_score (const heap * heap, unsigned idx)
{
  return idx < heap->vars ? heap->score[idx] : 0.0;
}

static inline bool
kissat_inc_empty_heap (heap * heap)
{
  return EMPTY_STACK (heap->stack);
}

static inline size_t
kissat_inc_size_heap (heap * heap)
{
  return SIZE_STACK (heap->stack);
}

void kissat_inc_push_heap (struct kissat *, heap *, unsigned);
void kissat_inc_pop_heap (struct kissat *, heap *, unsigned);
void kissat_inc_update_heap (struct kissat *, heap *, unsigned, double);

static inline unsigned
kissat_inc_max_heap (heap * heap)
{
  assert (!kissat_inc_empty_heap (heap));
  return PEEK_STACK (heap->stack, 0);
}

double kissat_inc_max_score_on_heap (heap *);
void kissat_inc_rescale_heap (struct kissat *, heap * heap, double factor);

#ifndef NDEBUG
void kissat_inc_dump_heap (heap *);
#endif

#ifndef NDEBUG
void kissat_inc_check_heap (heap *);
#else
#define kissat_inc_check_heap(...) do { } while (0)
#endif

#endif
