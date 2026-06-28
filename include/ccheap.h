/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  A generic D-ary heap with an optional inlined comparator macro
**  and inline pointer array.
**
**  ccheap_node_t is an 8-byte union — the heap never accesses its
**  members.  The comparison callback defines what the field means:
**
**    node.priority  →  priority queue (scheduling, Dijkstra, …)
**    node.timeout   →  timer wheel  (event loop, network I/O, …)
**    node.cost      →  floating-point cost (A*, pathfinding, …)
**
**  ── Comparison ──
**
**    Return type is int64_t.  Convention:
**      f(a, b) > 0  →  a has higher priority (closer to root)
**      f(a, b) < 0  →  b has higher priority
**
**    Default:                ccheap_compare_t function pointer via ccheap_init().
**    CCHEAP_COMPARE(a, b):   macro, inlined at every comparison site —
**                            eliminates indirect call overhead.  ccheap_init()'s
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
**    ccheap_t h;  ccheap_init(&h, NULL);
**    ccheap_node_t jobs[] = {{.priority=5}, {.priority=1}, {.priority=3}};
**    for (int i = 0; i < 3; i++) ccheap_push(&h, &jobs[i]);
**    while (ccheap_size(&h)) printf("%lu\n", ccheap_pop(&h)->priority); // 1,3,5
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
**    ccheap_t h;  ccheap_init(&h, cmp);
**    struct task t = {{.priority = 42}, my_callback, NULL};
**    ccheap_push(&h, &t.node);
**    struct task *done = CCHEAP_CONTAINER(ccheap_pop(&h), struct task, node);
**    done->cb(done->arg);
**
**  ── Bulk construction (Floyd) ──
**
**    ccheap_build(&h, nodes, count) replaces all contents and builds
**    a heap from an arbitrary array of node pointers in O(count) via
**    Floyd's bottom-up sift-down — avoids the O(n log n) cost of n
**    individual ccheap_insert() calls.
**
**    Example:
**      ccheap_node_t *pool[1000];
**      for (int i = 0; i < 1000; i++) pool[i] = &entries[i].node;
**      ccheap_build(&h, pool, 1000);            // O(1000)
**      // vs. for (...) ccheap_insert(&h, ...)   // O(1000·log 1000)
**
**    ccheap_heapify(&h) re-heapifies current contents in place.
**    Used after bulk priority changes:
**      for (size_t i = 0; i < ccheap_size(&h); i++)
**          CCHEAP_CONTAINER(h.data[i], struct task, node)->cost *= 0.9;
**      ccheap_heapify(&h);
*/
#ifndef CCHEAP_H
#define CCHEAP_H

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

/* ── branch hint ──────────────────────────────────────────────────────── */

#ifndef ccheap_unlikely
  #if defined(__GNUC__) || defined(__clang__)
    #define ccheap_unlikely(x) __builtin_expect(!!(x), 0)
  #else
    #define ccheap_unlikely(x) (x)
  #endif
#endif

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

#if defined(__cplusplus) && __cplusplus >= 201703L
  #define CCHEAP_NOEXCEPT noexcept
#else
  #define CCHEAP_NOEXCEPT
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
  #define CCHEAP_CMP(h, a, b) CCHEAP_COMPARE((a), (b))
#else
  #define CCHEAP_CMP(h, a, b) ((h)->f((a), (b)))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ── types ────────────────────────────────────────────────────────────── */

#ifndef CCHEAP_NODE_T
#define CCHEAP_NODE_T ccheap_node_t
typedef struct ccheap_node {
  union {
    uint64_t  priority;   /* integer priority (default) */
    uint64_t  timeout;    /* timer expiry                */
    double    cost;       /* floating-point cost (A*, …) */
  };
#ifdef CCHEAP_NODE_INDEX
  size_t  CCHEAP_NODE_INDEX;  /* heap array position (update/decrease-key) */
#endif
} ccheap_node_t;
#endif

typedef int64_t (*ccheap_compare_t)(const CCHEAP_NODE_T *, const CCHEAP_NODE_T *) CCHEAP_NOEXCEPT;

typedef struct ccheap {
  ccheap_node_t   **data;   /* pointer array     */
  size_t            len;    /* element count     */
  size_t            cap;    /* capacity          */
#ifndef CCHEAP_COMPARE
  ccheap_compare_t f;
#endif
} ccheap_t;

/* ── container_of ─────────────────────────────────────────────────────── */

#define CCHEAP_CONTAINER(ptr, type, member) \
    ((type *)((uint8_t *)(ptr) - offsetof(type, member)))

/* ── internal: direct array access (no bounds check, for hot path) ───── */

#define CCHEAP_AT(h, i) ((h)->data[i])
#define CCHEAP_PEEK(h)   CCHEAP_AT(h, 0)

CCHEAP_INLINE void _hswap(ccheap_t *h, size_t a, size_t b) {
  ccheap_node_t *tmp = CCHEAP_AT(h, a);
  CCHEAP_AT(h, a) = CCHEAP_AT(h, b);
  CCHEAP_AT(h, b) = tmp;
#ifdef CCHEAP_NODE_INDEX
  CCHEAP_AT(h, a)->CCHEAP_NODE_INDEX = a;
  CCHEAP_AT(h, b)->CCHEAP_NODE_INDEX = b;
#endif
}

/* internal: sift down from index i (pop / update) */
CCHEAP_INLINE void
_hsift_down(ccheap_t *h, size_t sz, size_t i) {
  for (;;) {
    size_t best  = i;
    size_t child = CCHEAP_CHILD(i, 0);

#if CCHEAP_ARITY_N > 0
    if (child + 0 < sz && CCHEAP_CMP(h, CCHEAP_AT(h, best), CCHEAP_AT(h, child + 0)) < 0) best = child + 0;
#endif
#if CCHEAP_ARITY_N > 1
    if (child + 1 < sz && CCHEAP_CMP(h, CCHEAP_AT(h, best), CCHEAP_AT(h, child + 1)) < 0) best = child + 1;
#endif
#if CCHEAP_ARITY_N > 2
    if (child + 2 < sz && CCHEAP_CMP(h, CCHEAP_AT(h, best), CCHEAP_AT(h, child + 2)) < 0) best = child + 2;
#endif
#if CCHEAP_ARITY_N > 3
    if (child + 3 < sz && CCHEAP_CMP(h, CCHEAP_AT(h, best), CCHEAP_AT(h, child + 3)) < 0) best = child + 3;
#endif
#if CCHEAP_ARITY_N > 4
    if (child + 4 < sz && CCHEAP_CMP(h, CCHEAP_AT(h, best), CCHEAP_AT(h, child + 4)) < 0) best = child + 4;
#endif
#if CCHEAP_ARITY_N > 5
    if (child + 5 < sz && CCHEAP_CMP(h, CCHEAP_AT(h, best), CCHEAP_AT(h, child + 5)) < 0) best = child + 5;
#endif
#if CCHEAP_ARITY_N > 6
    if (child + 6 < sz && CCHEAP_CMP(h, CCHEAP_AT(h, best), CCHEAP_AT(h, child + 6)) < 0) best = child + 6;
#endif
#if CCHEAP_ARITY_N > 7
    if (child + 7 < sz && CCHEAP_CMP(h, CCHEAP_AT(h, best), CCHEAP_AT(h, child + 7)) < 0) best = child + 7;
#endif

    if (best == i) break;
    _hswap(h, i, best);
    i = best;
  }
}

#ifdef CCHEAP_NODE_INDEX
/* ── decrease-key / update ──────────────────────────────────────────── */
CCHEAP_INLINE int
ccheap_update(ccheap_t *heap, CCHEAP_NODE_T *n) CCHEAP_NOEXCEPT
{
  if (ccheap_unlikely(!heap || !n)) return -1;

  size_t i = n->CCHEAP_NODE_INDEX;
  if (ccheap_unlikely(i >= heap->len || CCHEAP_AT(heap, i) != n))
    return -1;

  /* bubble up if priority increased (or cost decreased) */
  while (i > 0) {
    size_t p = CCHEAP_PARENT(i);
    if (CCHEAP_CMP(heap, CCHEAP_AT(heap, p), CCHEAP_AT(heap, i)) >= 0)
      break;
    _hswap(heap, p, i);
    i = p;
  }

  /* sift down if no bubble-up happened (priority decreased) */
  _hsift_down(heap, heap->len, i);
  return 0;
}
#endif /* CCHEAP_NODE_INDEX */

/* ── public API ─────────────────────────────────────────────────────────
 *
 *  Lifecycle:
 *    ccheap_init       — allocate internal array (default cap 32)
 *    ccheap_destroy    — free internal array
 *    ccheap_clear      — reset to empty (no free, no realloc)
 *
 *  Insert / Remove:
 *    ccheap_push       — alias for ccheap_insert
 *    ccheap_insert     — push one node — O(log len)
 *    ccheap_build      — replace & build from pointer array — O(count)
 *    ccheap_pop        — remove and return the root — O(log len)
 *    ccheap_peek       — read root without removing — O(1)
 *
 *  Bulk rebuild:
 *    ccheap_heapify    — re-heapify current data[] in place — O(len)
 *
 *  Update (only with CCHEAP_NODE_INDEX):
 *    ccheap_update     — rebalance after priority change — O(log len)
 *
 *  Query:
 *    ccheap_size       — element count — O(1)
 */

#define ccheap_node_get(n, name) (n)->name
#define ccheap_node_set(n, name, val) ccheap_node_get(n, name) = (val)

CCHEAP_INLINE int
ccheap_init(ccheap_t *heap, ccheap_compare_t f) CCHEAP_NOEXCEPT
{
  if (ccheap_unlikely(!heap)) return -1;
#ifndef CCHEAP_COMPARE
  if (ccheap_unlikely(!f)) return -1;
  heap->f = f;
#else
  (void)f;
#endif
  heap->len = 0;
  heap->cap = CCHEAP_DEFAULT_CAP;
  heap->data = (ccheap_node_t **)CCHEAP_MALLOC(
      sizeof(ccheap_node_t *) * CCHEAP_DEFAULT_CAP);
  if (!heap->data) return -1;
  return 0;
}

#define ccheap_push(h, n) ccheap_insert((h), (n))
CCHEAP_INLINE int
ccheap_insert(ccheap_t *heap, CCHEAP_NODE_T *n) CCHEAP_NOEXCEPT
{
  if (ccheap_unlikely(!heap || !n)) return -1;

  /* grow if needed */
  if (heap->len == heap->cap) {
    if (heap->cap > SIZE_MAX / 2 / sizeof(ccheap_node_t *)) return -1;
    size_t new_cap = heap->cap * 2;
    ccheap_node_t **nd = (ccheap_node_t **)CCHEAP_REALLOC(
        heap->data, sizeof(ccheap_node_t *) * new_cap);
    if (!nd) return -1;
    heap->data = nd;
    heap->cap = new_cap;
  }

  heap->data[heap->len] = n;
#ifdef CCHEAP_NODE_INDEX
  n->CCHEAP_NODE_INDEX = heap->len;
#endif
  heap->len++;
  size_t i = heap->len - 1;

  while (i > 0) {
    size_t p = CCHEAP_PARENT(i);
    if (CCHEAP_CMP(heap, CCHEAP_AT(heap, p), CCHEAP_AT(heap, i)) >= 0)
      break;
    _hswap(heap, p, i);
    i = p;
  }

  return 0;
}

/* ── Floyd heap construction ────────────────────────────────────────── */

/*  Replace contents with nodes[0..count-1] and establish the heap
 *  property via Floyd's bottom-up sift-down — O(count).  Pointers are
 *  shallow-copied (no node memory is allocated or freed).
 *
 *  Equivalent to clearing the heap and calling ccheap_insert count
 *  times, but runs in O(count) instead of O(count · log count).
 *
 *  Returns 0 on success, -1 on invalid argument or allocation failure.
 *  When count == 0 the heap is emptied (no error).
 *
 *  Example:
 *    ccheap_t h;  ccheap_init(&h, cmp);
 *    ccheap_node_t *nodes[3] = {&a, &b, &c};
 *    ccheap_build(&h, nodes, 3);    // O(3) instead of O(3 log 3)
 *
 *  CCHEAP_NODE_INDEX note:  every node's index is set by this function;
 *  ccheap_update remains safe immediately after ccheap_build.
 */
CCHEAP_INLINE int
ccheap_build(ccheap_t *heap, CCHEAP_NODE_T **nodes, size_t count) CCHEAP_NOEXCEPT
{
  if (ccheap_unlikely(!heap || (!nodes && count))) return -1;

  if (count == 0) { heap->len = 0; return 0; }

  /* grow internal array if needed */
  if (heap->cap < count) {
    ccheap_node_t **nd = (ccheap_node_t **)CCHEAP_REALLOC(
        heap->data, sizeof(ccheap_node_t *) * count);
    if (!nd) return -1;
    heap->data = nd;
    heap->cap = count;
  }

  /* copy pointers */
  for (size_t i = 0; i < count; i++)
    heap->data[i] = nodes[i];
  heap->len = count;

#ifdef CCHEAP_NODE_INDEX
  for (size_t i = 0; i < count; i++)
    heap->data[i]->CCHEAP_NODE_INDEX = i;
#endif

  /* Floyd's algorithm: sift down from the last parent to 0 — O(n) */
  if (count > 1) {
    size_t i = CCHEAP_PARENT(count - 1) + 1;
    while (i-- > 0)
      _hsift_down(heap, count, i);
  }

  return 0;
}

/*  Re-heapify the current data[0..len-1] in place — O(len).  Does
 *  not allocate, shrink, or change len/cap.  Useful for O(n) recovery
 *  after bulk priority changes that would otherwise require O(n log n)
 *  individual ccheap_update calls.
 *
 *  Returns 0 on success, -1 on NULL heap.  An empty or single-element
 *  heap is a no-op (returns 0).
 *
 *  Correct use — bulk priority update, data[] pointers intact:
 *    for (size_t i = 0; i < ccheap_size(&h); i++)
 *        h.data[i]->priority += 1;
 *    ccheap_heapify(&h);
 *
 *  CCHEAP_NODE_INDEX caveat — nodes that are never sifted during Floyd
 *  retain their old index.  If data[] pointers were swapped by hand,
 *  some CCHEAP_NODE_INDEX values may be stale (ccheap_update detects
 *  this via CCHEAP_AT(heap, i) != n and returns -1).  When stale
 *  indices are unacceptable, use ccheap_build instead.
 */
CCHEAP_INLINE int
ccheap_heapify(ccheap_t *heap) CCHEAP_NOEXCEPT
{
  if (ccheap_unlikely(!heap)) return -1;
  if (heap->len <= 1) return 0;

  size_t i = CCHEAP_PARENT(heap->len - 1) + 1;
  while (i-- > 0)
    _hsift_down(heap, heap->len, i);

  return 0;
}

CCHEAP_INLINE CCHEAP_NODE_T *
ccheap_pop(ccheap_t *heap) CCHEAP_NOEXCEPT
{
  if (ccheap_unlikely(!heap || heap->len == 0)) return NULL;

  CCHEAP_NODE_T *result = CCHEAP_AT(heap, 0);

  if (heap->len == 1) {
    heap->len--;
    return result;
  }

  /* move last to root, then pop */
  CCHEAP_AT(heap, 0) = CCHEAP_AT(heap, heap->len - 1);
#ifdef CCHEAP_NODE_INDEX
  CCHEAP_AT(heap, 0)->CCHEAP_NODE_INDEX = 0;
#endif
  heap->len--;

  _hsift_down(heap, heap->len, 0);

  return result;
}

CCHEAP_INLINE CCHEAP_NODE_T *
ccheap_peek(ccheap_t *heap) CCHEAP_NOEXCEPT
{
  if (ccheap_unlikely(!heap || heap->len == 0)) return NULL;
  return CCHEAP_PEEK(heap);
}

CCHEAP_INLINE size_t
ccheap_size(const ccheap_t *heap) CCHEAP_NOEXCEPT
{
  if (ccheap_unlikely(!heap)) return 0;
  return heap->len;
}

CCHEAP_INLINE void
ccheap_clear(ccheap_t *heap) CCHEAP_NOEXCEPT
{
  if (ccheap_unlikely(!heap)) return;
  heap->len = 0;
}

CCHEAP_INLINE void
ccheap_destroy(ccheap_t *heap) CCHEAP_NOEXCEPT
{
  if (ccheap_unlikely(!heap)) return;
  if (heap->data) CCHEAP_FREE(heap->data);
  heap->data = NULL;
  heap->len = heap->cap = 0;
#ifndef CCHEAP_COMPARE
  heap->f = NULL;
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* CCHEAP_H */
