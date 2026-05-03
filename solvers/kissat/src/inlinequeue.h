#ifndef _inlinequeue_h_INCLUDED
#define _inlinequeue_h_INCLUDED

#include "internal.h"
#include "logging.h"

/* Set the decision-search cache to start from idx on the next decision */
static inline void kissat_update_queue (kissat *solver, const links *links,
                                        unsigned idx) {
  assert (!DISCONNECTED (idx));
  const unsigned stamp = links[idx].stamp;
  LOG ("queue updated to %s stamped %u", LOGVAR (idx), stamp);
  solver->queue.search.idx = idx;
  solver->queue.search.stamp = stamp;
}

static inline void kissat_enqueue_links (kissat *solver, unsigned i,
                                         links *links, queue *queue) {
  /* Update links to add i as last element in the list */
  struct links *p = links + i;
  assert (DISCONNECTED (p->prev));
  assert (DISCONNECTED (p->next));
  const unsigned j = p->prev = queue->last;
  queue->last = i;
  if (DISCONNECTED (j))
    queue->first = i;
  else {
    struct links *l = links + j;
    assert (DISCONNECTED (l->next));
    l->next = i;
  }
  /* New element receives ++stamp */
  if (queue->stamp == UINT_MAX) {
    /* to manage overflows the queue stamp is reset to 0 */
    kissat_reassign_queue_stamps (solver);
    assert (p->stamp == queue->stamp);
  } else
    p->stamp = ++queue->stamp;
}

static inline void kissat_dequeue_links (unsigned i, links *links,
                                         queue *queue) {
  /* Update links to remove index 'i' from the linked list queue */
  struct links *l = links + i;
  const unsigned j = l->prev, k = l->next;
  l->prev = l->next = DISCONNECT;
  if (DISCONNECTED (j)) {
    assert (queue->first == i);
    queue->first = k;
  } else {
    struct links *p = links + j;
    assert (p->next == i);
    p->next = k;
  }
  if (DISCONNECTED (k)) {
    assert (queue->last == i);
    queue->last = j;
  } else {
    struct links *n = links + k;
    assert (n->prev == i);
    n->prev = j;
  }
}

/* Called only by activate_literal() in flags.c at variable discovery */
static inline void kissat_enqueue (kissat *solver, unsigned idx) {
  assert (idx < solver->vars);
  links *links = solver->links, *l = links + idx;
  l->prev = l->next = DISCONNECT;
  kissat_enqueue_links (solver, idx, links, &solver->queue);
  LOG ("enqueued %s stamped %u", LOGVAR (idx), l->stamp);
  if (!VALUE (LIT (idx)))
    kissat_update_queue (solver, links, idx);
  kissat_check_queue (solver);
}

/* Called by deactivate_variable() in flags.c and
 * adjust_scores_and_phases_of_fresh_variables() in factor.c */
static inline void kissat_dequeue (kissat *solver, unsigned idx) {
  assert (idx < solver->vars);
  LOG ("dequeued %s", LOGVAR (idx));
  links *links = solver->links;
  /* If idx is the cached one, we update the cache with next, or prev*/
  /* why do we prefer next here, and prev in move to front ?*/
  if (solver->queue.search.idx == idx) {
    struct links *l = links + idx;
    unsigned search = l->next;
    if (search == DISCONNECT)
      search = l->prev;
    if (search == DISCONNECT) {
      solver->queue.search.idx = DISCONNECT;
      solver->queue.search.stamp = 0;
    } else
      kissat_update_queue (solver, links, search);
  }
  kissat_dequeue_links (idx, links, &solver->queue);
  kissat_check_queue (solver);
}

static inline void kissat_move_to_front (kissat *solver, unsigned idx) {
  queue *queue = &solver->queue;
  links *links = solver->links;

  /* If variable is already at last (front) return */
  if (idx == queue->last) {
    assert (DISCONNECTED (links[idx].next));
    return;
  }
  assert (idx < solver->vars);
  const value tmp = VALUE (LIT (idx));
  /* if variable is assigned, and is the cached variable */
  if (tmp && queue->search.idx == idx) {
    unsigned prev = links[idx].prev;
    /* Prefer prev: lower stamp, consistent with the backward */
    if (!DISCONNECTED (prev))
      kissat_update_queue (solver, links, prev);
    else {
      /* no predecessor. Fall back to next. We know that idx is not last */
      unsigned next = links[idx].next;
      assert (!DISCONNECTED (next));
      kissat_update_queue (solver, links, next);
    }
  }
  kissat_dequeue_links (idx, links, queue);
  kissat_enqueue_links (solver, idx, links, queue);
  LOG ("moved-to-front %s stamped %u", LOGVAR (idx), LINK (idx).stamp);
  if (!tmp)
    kissat_update_queue (solver, links, idx);
  kissat_check_queue (solver);
}

#endif
