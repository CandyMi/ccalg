/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Intrusive chained hash map.  Internal bucket array backed by ccvector.
**
**  ── Required (one of) ──
**
**    Either define CCHASHMAP_HASH + CCHASHMAP_EQUAL macros, OR pass
**    cchashmap_hash_t + cchashmap_equal_t function pointers to cchashmap_init().
**
**  ── Container-of ──
**
**    CCHASHMAP_CONTAINER(ptr, type, member) — offsetof-based cast.
**
**  ── Usage ──
**
**    struct entry { int key; cchashmap_node_t node; };
**
**    static uint64_t hash_fn(const cchashmap_node_t *n, size_t seed) {
**        (void)seed;
**        return CCHASHMAP_CONTAINER(n, struct entry, node)->key;
**    }
**    static bool equal_fn(const cchashmap_node_t *a, const cchashmap_node_t *b) {
**        return CCHASHMAP_CONTAINER(a,struct entry,node)->key ==
**               CCHASHMAP_CONTAINER(b,struct entry,node)->key;
**    }
**
**    cchashmap_t m; cchashmap_init(&m, hash_fn, equal_fn);
**    struct entry e = { .key = 42 };
**    cchashmap_set(&m, &e.node, NULL);
**    cchashmap_del(&m, &e.node);
**    cchashmap_destroy(&m);
*/
#ifndef CCHASHMAP_H
#define CCHASHMAP_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

/* ── branch hint ──────────────────────────────────────────────────────── */

#ifndef cchashmap_unlikely
  #if defined(__GNUC__) || defined(__clang__)
    #define cchashmap_unlikely(x) __builtin_expect(!!(x), 0)
  #else
    #define cchashmap_unlikely(x) (x)
  #endif
#endif

/* ── portable inline ──────────────────────────────────────────────────── */

#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
  #define CCHASHMAP_INLINE static inline
#elif defined(_MSC_VER)
  #define CCHASHMAP_INLINE static __inline
#elif defined(__GNUC__)
  #define CCHASHMAP_INLINE static __inline__
#else
  #define CCHASHMAP_INLINE static
#endif

/* ── container-of ─────────────────────────────────────────────────────── */

#ifndef offsetof
  #define offsetof(type, member) \
      ((size_t) &(((type *)0)->member))
#endif
#ifndef container_of
  #define container_of(ptr, type, member) \
    ((type *)((uint8_t *)(ptr) - offsetof(type, member)))
#endif
#define CCHASHMAP_CONTAINER container_of

/* ── allocator hooks ──────────────────────────────────────────────────── */

#ifndef CCHASHMAP_REALLOC
  #define CCHASHMAP_REALLOC realloc
#endif
#ifndef CCHASHMAP_MALLOC
  #define CCHASHMAP_MALLOC(sz) CCHASHMAP_REALLOC(NULL, (sz))
#endif
#ifndef CCHASHMAP_FREE
  #define CCHASHMAP_FREE(ptr) free(ptr)
#endif
#ifndef CCHASHMAP_MAX_LOAD
  #define CCHASHMAP_MAX_LOAD 1.25
#endif
#ifndef CCHASHMAP_DEFAULT_SLOT
  #define CCHASHMAP_DEFAULT_SLOT 64
#endif

/* ── forward allocators to ccvector ───────────────────────────────────── */

#ifndef CCVECTOR_REALLOC
  #define CCVECTOR_REALLOC  CCHASHMAP_REALLOC
#endif
#ifndef CCVECTOR_MALLOC
  #define CCVECTOR_MALLOC(sz) CCHASHMAP_MALLOC(sz)
#endif
#ifndef CCVECTOR_FREE
  #define CCVECTOR_FREE(ptr)  CCHASHMAP_FREE(ptr)
#endif
#ifndef CCVECTOR_DEFAULT_CAP
  #define CCVECTOR_DEFAULT_CAP CCHASHMAP_DEFAULT_SLOT
#endif

/* ── types (nodes defined first — ccvector needs them) ────────────────── */

#ifndef CCHASHMAP_NODE_T
typedef struct cchashmap_node {
  struct cchashmap_node *next;
  uint64_t               hash;
} cchashmap_node_t;
#define CCHASHMAP_NODE_T
#endif

/* ccvector stores bucket pointers by value */
#ifndef CCVECTOR_NODE_T
  #define CCVECTOR_NODE_T cchashmap_node_t *
#endif
#include "ccvector.h"

/* ── comparison dispatch ──────────────────────────────────────────────── */

#ifdef CCHASHMAP_HASH
  #define CCHASHMAP_HASH_CALL(n, seed)    CCHASHMAP_HASH((n), (seed))
  #define CCHASHMAP_EQUAL_CALL(a,b)       CCHASHMAP_EQUAL((a),(b))
#else
  #define CCHASHMAP_HASH_CALL(n, seed)    (hash_fn((n), (seed)))
  #define CCHASHMAP_EQUAL_CALL(a,b) (equal_fn((a),(b)))
#endif

/* ── callback types ───────────────────────────────────────────────────── */

typedef uint64_t (*cchashmap_hash_t) (const cchashmap_node_t *n, size_t seed);
typedef bool     (*cchashmap_equal_t)(const cchashmap_node_t *a, const cchashmap_node_t *b);

/* ── container ────────────────────────────────────────────────────────── */

typedef struct cchashmap {
  ccvector_t buckets;   /* ccvector<cchashmap_node_t *>  */
  size_t     size;      /* total node count               */
  size_t     seed;      /* hash seed                      */
#ifndef CCHASHMAP_HASH
  cchashmap_hash_t   hash_fn;
  cchashmap_equal_t  equal_fn;
#endif
} cchashmap_t;

/* ── bucket access helpers ────────────────────────────────────────────── */

/* ccvector stores cchashmap_node_t* by value.
** ccvector_at returns a pointer to the stored element (cchashmap_node_t **).
** Dereference to get/set the bucket head pointer. */
CCHASHMAP_INLINE cchashmap_node_t *_bget(ccvector_t *v, size_t i) {
  cchashmap_node_t **p = (cchashmap_node_t **)ccvector_at(v, i);
  return p ? *p : NULL;
}
CCHASHMAP_INLINE void _bset(ccvector_t *v, size_t i, cchashmap_node_t *n) {
  cchashmap_node_t **p = (cchashmap_node_t **)ccvector_at(v, i);
  if (p) *p = n;
}

/* ── internal: resize ─────────────────────────────────────────────────── */

CCHASHMAP_INLINE void _cchashmap_resize(cchashmap_t *m) {
  size_t nc = ccvector_empty(&m->buckets)
                ? CCHASHMAP_DEFAULT_SLOT
                : ccvector_capacity(&m->buckets) * 2;

  /* new bucket vector, pre-filled with NULL */
  ccvector_t nb;
  ccvector_init_cap(&nb, nc);
  for (size_t i = 0; i < nc; i++)
    ccvector_push_back(&nb, NULL);

  /* rehash old chains into new buckets */
  size_t old_len = ccvector_size(&m->buckets);
  for (size_t i = 0; i < old_len; i++) {
    cchashmap_node_t *n = _bget(&m->buckets, i);
    while (n) {
      cchashmap_node_t *nx = n->next;
      size_t idx = n->hash & (nc - 1);
      n->next = _bget(&nb, idx);
      _bset(&nb, idx, n);
      n = nx;
    }
  }

  ccvector_destroy(&m->buckets);
  m->buckets = nb;
}

/* ── lifecycle ────────────────────────────────────────────────────────── */

CCHASHMAP_INLINE void cchashmap_init(cchashmap_t *m, cchashmap_hash_t hfn,
                                     cchashmap_equal_t efn) {
  if (cchashmap_unlikely(!m)) return;
  ccvector_init_cap(&m->buckets, CCHASHMAP_DEFAULT_SLOT);
  m->size = 0;
  m->seed = (size_t)(uintptr_t)m;
#ifndef CCHASHMAP_HASH
  m->hash_fn  = hfn;
  m->equal_fn = efn;
#else
  (void)hfn; (void)efn;
#endif
}

CCHASHMAP_INLINE void cchashmap_destroy(cchashmap_t *m) {
  if (cchashmap_unlikely(!m)) return;
  ccvector_destroy(&m->buckets);
  m->size = 0;
}

CCHASHMAP_INLINE void cchashmap_clear(cchashmap_t *m) {
  if (!m) return;
  size_t len = ccvector_size(&m->buckets);
  for (size_t i = 0; i < len; i++)
    _bset(&m->buckets, i, NULL);
  m->size = 0;
}

/* ── insert ───────────────────────────────────────────────────────────── */

#define cchashmap_insert(m, n, o) cchashmap_set((m), (n), (o))
CCHASHMAP_INLINE bool cchashmap_set(cchashmap_t *m, cchashmap_node_t *node,
                                    cchashmap_node_t **old) {
  if (cchashmap_unlikely(!m || !node)) return false;
  if (ccvector_empty(&m->buckets)) _cchashmap_resize(m);
#ifndef CCHASHMAP_HASH
  cchashmap_hash_t   hash_fn  = m->hash_fn;
  cchashmap_equal_t  equal_fn = m->equal_fn;
#endif
  node->hash = CCHASHMAP_HASH_CALL(node, m->seed);
  size_t idx  = node->hash & (ccvector_capacity(&m->buckets) - 1);
  cchashmap_node_t *cur = _bget(&m->buckets, idx);
  while (cur) {
    if (node->hash == cur->hash && CCHASHMAP_EQUAL_CALL(node, cur)) {
      if (old) *old = cur;
      return false;
    }
    cur = cur->next;
  }
  node->next = _bget(&m->buckets, idx);
  _bset(&m->buckets, idx, node);
  m->size++;
  if ((double)m->size / ccvector_capacity(&m->buckets) >= CCHASHMAP_MAX_LOAD)
    _cchashmap_resize(m);
  if (old) *old = NULL;
  return true;
}

/* ── find ─────────────────────────────────────────────────────────────── */

#define cchashmap_find(m, n) cchashmap_get((m), (n))
CCHASHMAP_INLINE cchashmap_node_t *cchashmap_get(cchashmap_t *m,
                                                  cchashmap_node_t *probe) {
  if (cchashmap_unlikely(!m || !probe || ccvector_empty(&m->buckets)))
    return NULL;
#ifndef CCHASHMAP_HASH
  cchashmap_hash_t   hash_fn  = m->hash_fn;
  cchashmap_equal_t  equal_fn = m->equal_fn;
#endif
  probe->hash = CCHASHMAP_HASH_CALL(probe, m->seed);
  size_t idx = probe->hash & (ccvector_capacity(&m->buckets) - 1);
  cchashmap_node_t *cur = _bget(&m->buckets, idx);
  while (cur) {
    if (probe->hash == cur->hash && CCHASHMAP_EQUAL_CALL(probe, cur))
      return cur;
    cur = cur->next;
  }
  return NULL;
}

/* ── erase ────────────────────────────────────────────────────────────── */

#define cchashmap_erase(m, n) cchashmap_del((m), (n))
CCHASHMAP_INLINE void cchashmap_del(cchashmap_t *m, cchashmap_node_t *node) {
  if (cchashmap_unlikely(!m || !node || ccvector_empty(&m->buckets)))
    return;
  size_t idx = node->hash & (ccvector_capacity(&m->buckets) - 1);
  cchashmap_node_t **pp = (cchashmap_node_t **)ccvector_at(&m->buckets, idx);
  if (!pp) return;
  while (*pp) {
    if (*pp == node) { *pp = node->next; m->size--; return; }
    pp = &(*pp)->next;
  }
}

/* ── size ─────────────────────────────────────────────────────────────── */

CCHASHMAP_INLINE size_t cchashmap_size(cchashmap_t *m) {
  return m ? m->size : 0;
}

#endif /* CCHASHMAP_H */
