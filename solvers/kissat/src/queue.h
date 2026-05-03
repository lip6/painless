#ifndef _queue_h_INCLUDED
#define _queue_h_INCLUDED

#define DISCONNECT UINT_MAX
#define DISCONNECTED(IDX) ((int) (IDX) < 0)

struct kissat;

typedef struct links links;
typedef struct queue queue;

/* Per-variable doubly-linked-list node indexed by variable idx.
 * `prev` / `next` are variable indices (DISCONNECT terminates).
 * `stamp` is a monotonically increasing timestamp set when the variable
 * is enqueued or moved to the front via kissat_update_queue; higher
 * stamp == closer to `last` (the front of the queue). */
struct links {
  unsigned prev, next;
  unsigned stamp;
};

/* VMTF (Variable Move-To-Front) queue.
 *   first  : back of the queue  (lowest stamp, oldest touched)
 *   last   : front of the queue (highest stamp, most recently touched,
 *            and the starting point for decision search)
 *   stamp  : highest stamp currently in use; next enqueue uses ++stamp
 *   search : cache for decision picking. `idx` is the variable to start
 *            the next decision scan from; `stamp` mirrors links[idx].stamp
 *            so staleness can be detected without a deref. */
struct queue {
  unsigned first, last, stamp;
  struct {
    unsigned idx, stamp;
  } search;
};

/* Invariant checked by kissat_check_queue:
 *   - stamps are strictly increasing along first -> last
 *   - in focused mode, every variable with a stamp strictly greater than
 *     search.idx's stamp (i.e. visited after search.idx in the first->last
 *     walk) is assigned. Variables at or before search.idx may be either
 *     assigned or unassigned.
 *   - if search.idx is connected, search.stamp == links[search.idx].stamp.
*/

void kissat_init_queue (struct kissat *);
void kissat_reset_search_of_queue (struct kissat *);
void kissat_reassign_queue_stamps (struct kissat *);

#define LINK(IDX) (solver->links[assert ((IDX) < VARS), (IDX)])

#if defined(CHECK_QUEUE) && !defined(NDEBUG)
void kissat_check_queue (struct kissat *);
#else
#define kissat_check_queue(...) \
  do { \
  } while (0)
#endif

#endif

/**
 *    queue.first                               search.idx                 queue.last
        (back)                                   (decision cursor)           (front)
            │                                           │                        │
            v                                           v                        v
       ┌───────┐  ┌───────┐  ┌───────┐  ┌───────┐  ┌───────┐  ┌───────┐  ┌───────┐
  DISC◄│  v7   │◄►│  v2   │◄►│  v9   │◄►│  v4   │◄►│  v1   │◄►│  v8   │◄►│  v3   │►DISC
       │ s=1 U │  │ s=2 A │  │ s=3 U │  │ s=4 U │  │ s=5 A │  │ s=6 A │  │ s=7 A │
       └───────┘  └───────┘  └───────┘  └───────┘  └───────┘  └───────┘  └───────┘
                                            ▲         ▲           ▲         ▲
                                            │         └───────────┴─────────┘
                                            │         all assigned (enforced invariant)
                                            │
                              start here, walk ◄── backward if assigned
                                                  (here v4 is unassigned -> return v4,
                                                   and update search.idx := v4)

       U = unassigned    A = assigned

  After this pick, the stronger post-condition holds:
      search.idx (= v4) is unassigned, and everything ahead of it is assigned.
  Between picks, enqueue/dequeue/move_to_front may temporarily put the cache
  on an assigned variable; the next call to last_enqueued_unassigned_variable
  repairs it by walking backward.
 */