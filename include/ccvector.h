/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Dynamic array (vector).  Stores elements by value in a contiguous
**  array — auto-grows on push_back.  Element type is controlled by
**  CCVECTOR_NODE_T (default: ccvector_node_t, a uint32_t wrapper).
**
**  ── Usage ──
**
**    ccvector_t v;  ccvector_init(&v);
**    ccvector_node_t n = {.value = 42};
**    ccvector_push_back(&v, n);
**    printf("%u\n", ccvector_at(&v, 0)->value);
**    ccvector_destroy(&v);
**
**  ── Custom element type ──
**
**    #define CCVECTOR_NODE_T struct my_elem
**    struct my_elem { int x; int y; };
**    #include "ccvector.h"
**
**    ccvector_t v; ccvector_init(&v);
**    struct my_elem e = {1, 2};
**    ccvector_push_back(&v, e);
**    struct my_elem *p = ccvector_at(&v, 0);
*/
#ifndef CCVECTOR_H
#define CCVECTOR_H

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

/* ── portable inline (C89 / MSVC compat) ─────────────────────────────── */

#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
  #define CCVECTOR_INLINE static inline
#elif defined(_MSC_VER)
  #define CCVECTOR_INLINE static __inline
#elif defined(__GNUC__)
  #define CCVECTOR_INLINE static __inline__
#else
  #define CCVECTOR_INLINE static
#endif

/* ── allocator hooks ──────────────────────────────────────────────────── */

#ifndef CCVECTOR_REALLOC
  #define CCVECTOR_REALLOC realloc
#endif
#ifndef CCVECTOR_MALLOC
  #define CCVECTOR_MALLOC(sz) CCVECTOR_REALLOC(NULL, (sz))
#endif
#ifndef CCVECTOR_FREE
  #define CCVECTOR_FREE(ptr) free(ptr)
#endif
#ifndef CCVECTOR_DEFAULT_CAP
  #define CCVECTOR_DEFAULT_CAP 8
#endif

/* ── types ────────────────────────────────────────────────────────────── */

#ifndef CCVECTOR_NODE_T
  #define CCVECTOR_NODE_T ccvector_node_t
  typedef struct ccvector_node {
    union { uint32_t value; };
  } ccvector_node_t;
#endif

typedef struct ccvector {
  CCVECTOR_NODE_T *buckets;
  size_t           len;
  size_t           cap;
} ccvector_t;

/* ── lifecycle ────────────────────────────────────────────────────────── */

CCVECTOR_INLINE int
ccvector_init(ccvector_t *v) {
  if (!v) return -1;
  v->buckets = (CCVECTOR_NODE_T *)CCVECTOR_MALLOC(
      sizeof(CCVECTOR_NODE_T) * CCVECTOR_DEFAULT_CAP);
  if (!v->buckets) return -1;
  v->len = 0;
  v->cap = CCVECTOR_DEFAULT_CAP;
  return 0;
}

CCVECTOR_INLINE void
ccvector_destroy(ccvector_t *v) {
  if (!v) return;
  (void)CCVECTOR_FREE(v->buckets);
  v->buckets = NULL;
  v->len = v->cap = 0;
}

CCVECTOR_INLINE void
ccvector_clear(ccvector_t *v) {
  if (!v) return;
  v->len = 0;
}

/* ── push / pop ───────────────────────────────────────────────────────── */

CCVECTOR_INLINE int
ccvector_push_back(ccvector_t *v, CCVECTOR_NODE_T elem) {
  if (!v || !v->buckets) return -1;

  if (v->len == v->cap) {
    if (v->cap > SIZE_MAX / 2 / sizeof(CCVECTOR_NODE_T)) return -1;
    size_t new_cap = v->cap * 2;
    CCVECTOR_NODE_T *nd = (CCVECTOR_NODE_T *)CCVECTOR_REALLOC(
        v->buckets, sizeof(CCVECTOR_NODE_T) * new_cap);
    if (!nd) return -1;
    v->buckets = nd;
    v->cap = new_cap;
  }

  v->buckets[v->len++] = elem;
  return 0;
}

CCVECTOR_INLINE int
ccvector_pop_back(ccvector_t *v) {
  if (!v || v->len == 0) return -1;
  v->len--;
  return 0;
}

/* ── access ───────────────────────────────────────────────────────────── */

CCVECTOR_INLINE CCVECTOR_NODE_T *
ccvector_at(ccvector_t *v, size_t i) {
  if (!v || i >= v->len) return NULL;
  return &v->buckets[i];
}

CCVECTOR_INLINE CCVECTOR_NODE_T *
ccvector_front(ccvector_t *v) {
  return ccvector_at(v, 0);
}

CCVECTOR_INLINE CCVECTOR_NODE_T *
ccvector_back(ccvector_t *v) {
  if (!v || v->len == 0) return NULL;
  return &v->buckets[v->len - 1];
}

/* ── query ────────────────────────────────────────────────────────────── */

CCVECTOR_INLINE size_t
ccvector_size(ccvector_t *v) {
  return v ? v->len : 0;
}

CCVECTOR_INLINE size_t
ccvector_capacity(ccvector_t *v) {
  return v ? v->cap : 0;
}

CCVECTOR_INLINE int
ccvector_empty(ccvector_t *v) {
  return !v || v->len == 0;
}

/* ── reserve ──────────────────────────────────────────────────────────── */

CCVECTOR_INLINE int
ccvector_reserve(ccvector_t *v, size_t new_cap) {
  if (!v || !v->buckets || new_cap <= v->cap) return -1;
  CCVECTOR_NODE_T *nd = (CCVECTOR_NODE_T *)CCVECTOR_REALLOC(
      v->buckets, sizeof(CCVECTOR_NODE_T) * new_cap);
  if (!nd) return -1;
  v->buckets = nd;
  v->cap = new_cap;
  return 0;
}

#endif /* CCVECTOR_H */
