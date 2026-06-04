/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Intrusive chained hash map.  Internal only manages the bucket array.
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
**    #include "ccmap1.h"
**    struct entry { int key; cchashmap_node_t node; };
**
**    static uint64_t hash_fn(cchashmap_node_t *n) {
**        return CCHASHMAP_CONTAINER(n, struct entry, node)->key;
**    }
**    static bool equal_fn(cchashmap_node_t *a, cchashmap_node_t *b) {
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

#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
  #define CCHASHMAP_INLINE static inline
#elif defined(_MSC_VER)
  #define CCHASHMAP_INLINE static __inline
#elif defined(__GNUC__)
  #define CCHASHMAP_INLINE static __inline__
#else
  #define CCHASHMAP_INLINE static
#endif

#ifndef offsetof
  #define offsetof(type, member) \
      ((size_t) &(((type *)0)->member))
#endif

#ifndef container_of
  #define container_of(ptr, type, member) \
    ((type *)((uint8_t *)(ptr) - offsetof(type, member)))
#endif

#define CCHASHMAP_CONTAINER container_of

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
#define CCHASHMAP_DEFAULT_SLOT 8

/* ── comparison dispatch ──────────────────────────────────────────────── */

#ifdef CCHASHMAP_HASH
  #define CCHASHMAP_HASH_CALL(n, seed)    CCHASHMAP_HASH((n), (seed))
  #define CCHASHMAP_EQUAL_CALL(a,b)       CCHASHMAP_EQUAL((a),(b))
#else
  #define CCHASHMAP_HASH_CALL(n, seed)    (hash_fn((n), (seed)))
  #define CCHASHMAP_EQUAL_CALL(a,b) (equal_fn((a),(b)))
#endif

/* ── types ────────────────────────────────────────────────────────────── */

#ifndef CCHASHMAP_NODE_T
typedef struct cchashmap_node {
  struct cchashmap_node *next;
  uint64_t               hash;
} cchashmap_node_t;
#define CCHASHMAP_NODE_T
#endif

typedef uint64_t (*cchashmap_hash_t) (const cchashmap_node_t *n, size_t seed);
typedef bool     (*cchashmap_equal_t)(const cchashmap_node_t *a, const cchashmap_node_t *b);

typedef struct cchashmap {
  cchashmap_node_t **buckets;
  size_t          size;
  size_t          cap;
  size_t          seed;
#ifndef CCHASHMAP_HASH
  cchashmap_hash_t   hash_fn;
  cchashmap_equal_t  equal_fn;
#endif
} cchashmap_t;

/* ── API ──────────────────────────────────────────────────────────────── */

CCHASHMAP_INLINE void cchashmap_init(cchashmap_t *m, cchashmap_hash_t hfn, cchashmap_equal_t efn) {
  if (!m) return;
  m->buckets = NULL;
  m->size = m->cap = 0;
  m->seed = (size_t)(uintptr_t)m;
#ifndef CCHASHMAP_HASH
  m->hash_fn  = hfn;
  m->equal_fn = efn;
#else
  (void)hfn; (void)efn;
#endif
}

CCHASHMAP_INLINE void cchashmap_destroy(cchashmap_t *m) {
  if (!m) return;
  (void)CCHASHMAP_FREE(m->buckets);
  m->buckets = NULL; m->size = m->cap = 0;
}

CCHASHMAP_INLINE void _cchashmap_resize(cchashmap_t *m) {
  size_t nc = m->cap ? m->cap * 2 : CCHASHMAP_DEFAULT_SLOT;
  cchashmap_node_t **nb = (cchashmap_node_t **)CCHASHMAP_MALLOC(nc * sizeof(cchashmap_node_t *));
  if (!nb) return;
  memset(nb, 0, nc * sizeof(cchashmap_node_t *));
  for (size_t i = 0; i < m->cap; i++) {
    cchashmap_node_t *n = m->buckets[i];
    while (n) { cchashmap_node_t *nx = n->next; size_t idx = n->hash & (nc - 1);
      n->next = nb[idx]; nb[idx] = n; n = nx; }
  }
  (void)CCHASHMAP_FREE(m->buckets);
  m->buckets = nb; m->cap = nc;
}

#define cchashmap_insert(m, n, o) cchashmap_set((m), (n), (o))
CCHASHMAP_INLINE bool cchashmap_set(cchashmap_t *m, cchashmap_node_t *node, cchashmap_node_t **old) {
  if (!m || !node) return false;
  if (!m->buckets) _cchashmap_resize(m);
#ifndef CCHASHMAP_HASH
  cchashmap_hash_t hash_fn;
  cchashmap_equal_t equal_fn;
#endif
  node->hash = CCHASHMAP_HASH_CALL(node, m->seed);
  size_t idx = node->hash & (m->cap - 1);
  cchashmap_node_t *cur = m->buckets[idx];
  while (cur) { if (node->hash == cur->hash && CCHASHMAP_EQUAL_CALL(node, cur)) { if (old) *old = cur; return false; } cur = cur->next; }
  node->next = m->buckets[idx]; m->buckets[idx] = node; m->size++;
  if ((double)m->size / m->cap >= CCHASHMAP_MAX_LOAD) _cchashmap_resize(m);
  if (old) *old = NULL;
  return true;
}

#define cchashmap_find(m, n) cchashmap_get((m), (n))
CCHASHMAP_INLINE cchashmap_node_t *cchashmap_get(cchashmap_t *m, cchashmap_node_t *probe) {
  if (!m || !probe || !m->buckets) return NULL;
#ifndef CCHASHMAP_HASH
  cchashmap_hash_t hash_fn;
  cchashmap_equal_t equal_fn;
#endif
  probe->hash = CCHASHMAP_HASH_CALL(probe, m->seed);
  size_t idx = probe->hash & (m->cap - 1);
  cchashmap_node_t *cur = m->buckets[idx];
  while (cur) { if (probe->hash == cur->hash && CCHASHMAP_EQUAL_CALL(probe, cur)) return cur; cur = cur->next; }
  return NULL;
}

#define cchashmap_erase(m, n) cchashmap_del((m), (n))
CCHASHMAP_INLINE void cchashmap_del(cchashmap_t *m, cchashmap_node_t *node) {
  if (!m || !node || !m->buckets) return;
  size_t idx = node->hash & (m->cap - 1);
  cchashmap_node_t **pp = &m->buckets[idx];
  while (*pp) { if (*pp == node) { *pp = node->next; m->size--; return; } pp = &(*pp)->next; }
}

CCHASHMAP_INLINE size_t cchashmap_size(cchashmap_t *m) { return m ? m->size : 0; }
CCHASHMAP_INLINE void cchashmap_clear(cchashmap_t *m) {
  if (m && m->buckets) { memset(m->buckets, 0, m->cap * sizeof(cchashmap_node_t *)); m->size = 0; }
}

#endif /* CCHASHMAP_H */
