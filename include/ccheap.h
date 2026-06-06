/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  A generic D-ary heap with an optional inlined comparator macro.
**
**  ccheap_node_t is an 8-byte union — the heap never accesses its
**  members.  The comparison callback defines what the field means:
**
**    node.priority  →  priority queue (scheduling, Dijkstra, …)
**    node.timeout   →  timer wheel  (event loop, network I/O, …)
**
**  ── Comparison ──
**
**    Return type is int64_t.  Convention:
**      f(a, b) > 0  →  a has higher priority (closer to root)
**      f(a, b) < 0  →  b has higher priority
**
**    Default:                heap_compare_t function pointer via heap_init().
**    CCHEAP_COMPARE(a, b):   macro, inlined at every comparison site —
**                            eliminates indirect call overhead.  heap_init()'s
**                            second argument is ignored when this is defined.
**
**    Function-pointer example (min-heap by timeout):
**      int64_t min_cmp(const ccheap_node_t *a, const ccheap_node_t *b) {
**          return (int64_t)b->timeout - (int64_t)a->timeout;
**      }
**
**    Subtraction is branchless and safe with int64_t for uint64_t keys.
**      Macro example (equivalent):
**        #define CCHEAP_COMPARE(a, b) \
**            ((int64_t)(b)->timeout - (int64_t)(a)->timeout)
**
**  ── Arity ──
**
**    CCHEAP_ARITY 2 (default) / 4 / 8.
**
**  ── Quick start: priority queue ──
**
**    #define CCHEAP_COMPARE(a, b) \
**        ((int64_t)(b)->priority - (int64_t)(a)->priority)
**    #include "ccheap.h"
**
**    ccheap_t h;  heap_init(&h, NULL);
**    ccheap_node_t jobs[] = {{.priority=5}, {.priority=1}, {.priority=3}};
**    for (int i = 0; i < 3; i++) heap_push(&h, &jobs[i]);
**    while (heap_size(&h)) printf("%lu\n", heap_pop(&h)->priority); // 1,3,5
**
**  ── Quick start: timer ──
**
**    #define CCHEAP_COMPARE(a, b) \
**        ((int64_t)(b)->timeout - (int64_t)(a)->timeout)
**    #include "ccheap.h"
**
**    ccheap_t h;  heap_init(&h, NULL);
**    ccheap_node_t ev = {.timeout = now() + 5000};
**    heap_push(&h, &ev);
**
**  ── Embedding for extra payload (container_of) ──
**
**    struct task {
**        ccheap_node_t node;      // embed the heap node
**        void (*cb)(void *);      // your payload
**        void *arg;
**    };
**
**    static int64_t cmp(const ccheap_node_t *a, const ccheap_node_t *b) {
**        struct task *ta = CCHEAP_CONTAINER(a, struct task, node);
**        struct task *tb = CCHEAP_CONTAINER(b, struct task, node);
**        return (int64_t)tb->node.priority - (int64_t)ta->node.priority;
**    }
**
**    ccheap_t h;  heap_init(&h, cmp);
**    struct task t = {{.priority = 42}, my_callback, NULL};
**    heap_push(&h, &t.node);
**    struct task *done = CCHEAP_CONTAINER(heap_pop(&h), struct task, node);
**    done->cb(done->arg);
**
**  The comparison defines priority order:
**    f(a, b) > 0  →  a has higher priority than b (closer to root)
**    f(a, b) < 0  →  b has higher priority than a
**    f(a, b) == 0 →  equal priority
*/
#ifndef CCHEAP_H
#define CCHEAP_H

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
/* ── portable inline (C89 / MSVC compat) ─────────────────────────────── */

#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
  #define CCHEAP_INLINE static inline
#elif defined(_MSC_VER)
  #define CCHEAP_INLINE static __inline
#elif defined(__GNUC__)
  #define CCHEAP_INLINE static __inline__
#else
  #define CCHEAP_INLINE static
#endif

/* ── D-ary heap configuration ─────────────────────────────────────────── */

#ifndef CCHEAP_ARITY
  #define CCHEAP_ARITY 2
#endif

#if CCHEAP_ARITY == 2
  #define CCHEAP_ARITY_N 2
#elif CCHEAP_ARITY == 4
  #define CCHEAP_ARITY_N 4
#elif CCHEAP_ARITY == 8
  #define CCHEAP_ARITY_N 8
#else
  #error "CCHEAP_ARITY must be 2, 4, or 8"
#endif

/* ── index helpers (D-ary) ────────────────────────────────────────────── */

#define CCHEAP_PARENT(i)    (((i) - 1) / CCHEAP_ARITY_N)
#define CCHEAP_CHILD(i, k)  (CCHEAP_ARITY_N * (i) + (k) + 1)

#if CCHEAP_ARITY == 2
  #define CCHEAP_LEFT(i)   CCHEAP_CHILD((i), 0)
  #define CCHEAP_RIGHT(i)  CCHEAP_CHILD((i), 1)
#endif

/* ── memory allocation hooks ──────────────────────────────────────────── */

#ifndef CCHEAP_REALLOC
  #define CCHEAP_REALLOC realloc
#endif
#ifndef CCHEAP_MALLOC
  #define CCHEAP_MALLOC(sz) CCHEAP_REALLOC(NULL, (sz))
#endif
#ifndef CCHEAP_FREE
  #define CCHEAP_FREE(ptr) free(ptr)
#endif
#ifndef CCHEAP_DEFAULT_CAP
  #define CCHEAP_DEFAULT_CAP 32
#endif

/* ── comparison dispatch ──────────────────────────────────────────────── */

#ifdef CCHEAP_COMPARE
  /* Macro-based, inlined at every call site — no function-pointer overhead */
  #define CCHEAP_CMP(h, a, b) CCHEAP_COMPARE((a), (b))
#else
  /* Function-pointer based (default) */
  #define CCHEAP_CMP(h, a, b) ((h)->f((a), (b)))
#endif

/* ── types ────────────────────────────────────────────────────────────── */

/* define the default node before the function-pointer typedef so the
** types are fully visible (silences -Wvisibility warnings) */
#define CCHEAP_NODE_T ccheap_node_t
typedef struct ccheap_node {
  union {
    uint64_t  priority; // maybe priority queue.
    uint64_t  timeout;  // maybe timer.
  };
} ccheap_node_t;

typedef int64_t (*ccheap_compare_t)(const CCHEAP_NODE_T *, const CCHEAP_NODE_T *);

typedef struct ccheap {
  CCHEAP_NODE_T   **data;
  size_t          size;
  size_t          cap;
#ifndef CCHEAP_COMPARE
  ccheap_compare_t  f;
#endif
} ccheap_t;

/* ── container_of ─────────────────────────────────────────────────────── */

#define CCHEAP_CONTAINER(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/* ── element helpers ──────────────────────────────────────────────────── */

#define CCHEAP_SWAP(h, a, b)  do {              \
    CCHEAP_NODE_T *_tmp = (h)->data[a];         \
    (h)->data[a] = (h)->data[b];                \
    (h)->data[b] = _tmp;                        \
} while(0)
#define CCHEAP_PEEK(h)  ((h)->data[0])
#define CCHEAP_AT(h, i) ((h)->data[i])

/* ── public API ───────────────────────────────────────────────────────── */

CCHEAP_INLINE int
heap_init(ccheap_t *heap, ccheap_compare_t f)
{
  if (!heap) return -1;
  heap->data = (CCHEAP_NODE_T **)CCHEAP_MALLOC(sizeof(CCHEAP_NODE_T *) * CCHEAP_DEFAULT_CAP);
  if (!heap->data) return -1;
  heap->size = 0;
  heap->cap  = CCHEAP_DEFAULT_CAP;
#ifndef CCHEAP_COMPARE
  heap->f    = f;
#else
  (void)f;
#endif
  return 0;
}

#define heap_push(h, n) heap_insert((h), (n))
CCHEAP_INLINE int
heap_insert(ccheap_t *heap, CCHEAP_NODE_T *n)
{
  if (!heap || !heap->data || !n) return -1;

  if (heap->size == heap->cap) {
    if (heap->cap > SIZE_MAX / 2 / sizeof(CCHEAP_NODE_T *)) return -1;
    size_t new_cap = heap->cap * 2;
    CCHEAP_NODE_T **nd = (CCHEAP_NODE_T **)CCHEAP_REALLOC(heap->data, sizeof(CCHEAP_NODE_T *) * new_cap);
    if (!nd) return -1;
    heap->data = nd;
    heap->cap  = new_cap;
  }

  size_t i = heap->size++;
  heap->data[i] = n;

  while (i > 0) {
    size_t p = CCHEAP_PARENT(i);
    if (CCHEAP_CMP(heap, CCHEAP_AT(heap, p), CCHEAP_AT(heap, i)) >= 0)
      break;
    CCHEAP_SWAP(heap, p, i);
    i = p;
  }

  return 0;
}

CCHEAP_INLINE CCHEAP_NODE_T *
heap_pop(ccheap_t *heap)
{
  if (!heap || !heap->data || heap->size == 0) return NULL;

  CCHEAP_NODE_T *result = heap->data[0];

  heap->size--;
  if (heap->size == 0) return result;

  /* move last to root */
  heap->data[0] = heap->data[heap->size];

  const size_t sz = heap->size;
  size_t i = 0;
  for (;;) {
    size_t best  = i;
    size_t child = CCHEAP_CHILD(i, 0);    /* leftmost child */

    /* unrolled D-child comparison — compile-time constant */
#if CCHEAP_ARITY_N > 0
    if (child + 0 < sz && CCHEAP_CMP(heap, CCHEAP_AT(heap, best), CCHEAP_AT(heap, child + 0)) < 0) best = child + 0;
#endif
#if CCHEAP_ARITY_N > 1
    if (child + 1 < sz && CCHEAP_CMP(heap, CCHEAP_AT(heap, best), CCHEAP_AT(heap, child + 1)) < 0) best = child + 1;
#endif
#if CCHEAP_ARITY_N > 2
    if (child + 2 < sz && CCHEAP_CMP(heap, CCHEAP_AT(heap, best), CCHEAP_AT(heap, child + 2)) < 0) best = child + 2;
#endif
#if CCHEAP_ARITY_N > 3
    if (child + 3 < sz && CCHEAP_CMP(heap, CCHEAP_AT(heap, best), CCHEAP_AT(heap, child + 3)) < 0) best = child + 3;
#endif
#if CCHEAP_ARITY_N > 4
    if (child + 4 < sz && CCHEAP_CMP(heap, CCHEAP_AT(heap, best), CCHEAP_AT(heap, child + 4)) < 0) best = child + 4;
#endif
#if CCHEAP_ARITY_N > 5
    if (child + 5 < sz && CCHEAP_CMP(heap, CCHEAP_AT(heap, best), CCHEAP_AT(heap, child + 5)) < 0) best = child + 5;
#endif
#if CCHEAP_ARITY_N > 6
    if (child + 6 < sz && CCHEAP_CMP(heap, CCHEAP_AT(heap, best), CCHEAP_AT(heap, child + 6)) < 0) best = child + 6;
#endif
#if CCHEAP_ARITY_N > 7
    if (child + 7 < sz && CCHEAP_CMP(heap, CCHEAP_AT(heap, best), CCHEAP_AT(heap, child + 7)) < 0) best = child + 7;
#endif

    if (best == i) break;
    CCHEAP_SWAP(heap, i, best);
    i = best;
  }

  return result;
}

CCHEAP_INLINE CCHEAP_NODE_T *
heap_peek(ccheap_t *heap)
{
  if (!heap || !heap->data || heap->size == 0) return NULL;
  return CCHEAP_PEEK(heap);
}

CCHEAP_INLINE size_t
heap_size(ccheap_t *heap)
{
  if (!heap || !heap->data) return 0;
  return heap->size;
}

CCHEAP_INLINE void
heap_destroy(ccheap_t *heap)
{
  if (!heap) return;
  (void)CCHEAP_FREE(heap->data);
  heap->data = NULL;
  heap->size = 0;
  heap->cap  = 0;
#ifndef CCHEAP_COMPARE
  heap->f = NULL;
#endif
}

#endif /* CCHEAP_H */
