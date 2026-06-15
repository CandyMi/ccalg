/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Intrusive chained hash map with inline bucket array.
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

#if defined(__cplusplus) && __cplusplus >= 201703L
  #define CCHASHMAP_NOEXCEPT noexcept
#else
  #define CCHASHMAP_NOEXCEPT
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

/* ── types ────────────────────────────────────────────────────────────── */

#ifndef CCHASHMAP_NODE_T
typedef struct cchashmap_node {
  struct cchashmap_node *next;
  uint64_t               hash;
} cchashmap_node_t;
#define CCHASHMAP_NODE_T
#endif

/* ── comparison dispatch ──────────────────────────────────────────────── */

#ifdef CCHASHMAP_HASH
  #define CCHASHMAP_HASH_CALL(n, seed)    CCHASHMAP_HASH((n), (seed))
  #define CCHASHMAP_EQUAL_CALL(a,b)       CCHASHMAP_EQUAL((a),(b))
#else
  #define CCHASHMAP_HASH_CALL(n, seed)    (hash_fn((n), (seed)))
  #define CCHASHMAP_EQUAL_CALL(a,b)       (equal_fn((a), (b)))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ── callback types ───────────────────────────────────────────────────── */

typedef uint64_t (*cchashmap_hash_t) (const cchashmap_node_t *n, size_t seed) CCHASHMAP_NOEXCEPT;
typedef bool     (*cchashmap_equal_t)(const cchashmap_node_t *a, const cchashmap_node_t *b) CCHASHMAP_NOEXCEPT;

/* ── container ────────────────────────────────────────────────────────── */

typedef struct cchashmap {
  cchashmap_node_t **buckets;   /* bucket pointer array            */
  size_t              cap;      /* bucket count (power of two)     */
  size_t              size;     /* total node count                */
  size_t              seed;     /* hash seed                       */
#ifndef CCHASHMAP_HASH
  cchashmap_hash_t   hash_fn;
  cchashmap_equal_t  equal_fn;
#endif
} cchashmap_t;

/* ── bucket access helpers ────────────────────────────────────────────── */

/* direct array access — no bounds check (hot path, bounds guaranteed) */
#define _bget(m, i)   ((m)->buckets[i])
#define _bset(m, i, n) ((m)->buckets[i] = (n))

/* ── internal: resize ─────────────────────────────────────────────────── */

CCHASHMAP_INLINE void _cchashmap_resize(cchashmap_t *m) {
  size_t nc = m->buckets ? m->cap * 2 : CCHASHMAP_DEFAULT_SLOT;

  /* new bucket array, zero-filled */
  cchashmap_node_t **nb = (cchashmap_node_t **)CCHASHMAP_MALLOC(
      sizeof(cchashmap_node_t *) * nc);
  if (!nb) return;
  memset(nb, 0, sizeof(cchashmap_node_t *) * nc);

  /* rehash old chains into new buckets */
  if (m->buckets) {
    for (size_t i = 0; i < m->cap; i++) {
      cchashmap_node_t *n = m->buckets[i];
      while (n) {
        cchashmap_node_t *nx = n->next;
        size_t idx = n->hash & (nc - 1);
        n->next = nb[idx];
        nb[idx] = n;
        n = nx;
      }
    }
    CCHASHMAP_FREE(m->buckets);
  }

  m->buckets = nb;
  m->cap = nc;
}

/* ── lifecycle ────────────────────────────────────────────────────────── */

CCHASHMAP_INLINE void cchashmap_init(cchashmap_t *m, cchashmap_hash_t hfn,
                                     cchashmap_equal_t efn) CCHASHMAP_NOEXCEPT {
  if (cchashmap_unlikely(!m)) return;
  m->buckets = NULL;
  m->cap = 0;
  m->size = 0;
  m->seed = (size_t)(uintptr_t)m;
#ifndef CCHASHMAP_HASH
  m->hash_fn  = hfn;
  m->equal_fn = efn;
#else
  (void)hfn; (void)efn;
#endif
}

CCHASHMAP_INLINE void cchashmap_destroy(cchashmap_t *m) CCHASHMAP_NOEXCEPT {
  if (cchashmap_unlikely(!m)) return;
  if (m->buckets) CCHASHMAP_FREE(m->buckets);
  m->buckets = NULL;
  m->cap = 0;
  m->size = 0;
}

CCHASHMAP_INLINE void cchashmap_clear(cchashmap_t *m) CCHASHMAP_NOEXCEPT {
  if (!m || !m->buckets) return;
  for (size_t i = 0; i < m->cap; i++)
    m->buckets[i] = NULL;
  m->size = 0;
}

/* ── insert ───────────────────────────────────────────────────────────── */

#define cchashmap_insert(m, n, o) cchashmap_set((m), (n), (o))
CCHASHMAP_INLINE bool cchashmap_set(cchashmap_t *m, cchashmap_node_t *node,
                                    cchashmap_node_t **old) CCHASHMAP_NOEXCEPT {
  if (cchashmap_unlikely(!m || !node)) return false;
  if (!m->buckets) _cchashmap_resize(m);
#ifndef CCHASHMAP_HASH
  cchashmap_hash_t   hash_fn  = m->hash_fn;
  cchashmap_equal_t  equal_fn = m->equal_fn;
#endif
  node->hash = CCHASHMAP_HASH_CALL(node, m->seed);
  size_t idx  = node->hash & (m->cap - 1);
  cchashmap_node_t *cur = m->buckets[idx];
  while (cur) {
    if (node->hash == cur->hash && CCHASHMAP_EQUAL_CALL(node, cur)) {
      if (old) *old = cur;
      return false;
    }
    cur = cur->next;
  }
  node->next = m->buckets[idx];
  m->buckets[idx] = node;
  m->size++;
  if ((double)m->size / m->cap >= CCHASHMAP_MAX_LOAD)
    _cchashmap_resize(m);
  if (old) *old = NULL;
  return true;
}

/* ── find ─────────────────────────────────────────────────────────────── */

#define cchashmap_find(m, n) cchashmap_get((m), (n))
CCHASHMAP_INLINE cchashmap_node_t *cchashmap_get(cchashmap_t *m,
                                                  cchashmap_node_t *probe) CCHASHMAP_NOEXCEPT {
  if (cchashmap_unlikely(!m || !probe || !m->buckets))
    return NULL;
#ifndef CCHASHMAP_HASH
  cchashmap_hash_t   hash_fn  = m->hash_fn;
  cchashmap_equal_t  equal_fn = m->equal_fn;
#endif
  probe->hash = CCHASHMAP_HASH_CALL(probe, m->seed);
  size_t idx = probe->hash & (m->cap - 1);
  cchashmap_node_t *cur = m->buckets[idx];
  while (cur) {
    if (probe->hash == cur->hash && CCHASHMAP_EQUAL_CALL(probe, cur))
      return cur;
    cur = cur->next;
  }
  return NULL;
}

/* ── erase ────────────────────────────────────────────────────────────── */

#define cchashmap_erase(m, n) cchashmap_del((m), (n))
CCHASHMAP_INLINE void cchashmap_del(cchashmap_t *m, cchashmap_node_t *node) CCHASHMAP_NOEXCEPT {
  if (cchashmap_unlikely(!m || !node || !m->buckets))
    return;
  size_t idx = node->hash & (m->cap - 1);
  cchashmap_node_t **pp = &m->buckets[idx];
  while (*pp) {
    if (*pp == node) { *pp = node->next; m->size--; return; }
    pp = &(*pp)->next;
  }
}

/* ── size ─────────────────────────────────────────────────────────────── */

CCHASHMAP_INLINE size_t cchashmap_size(cchashmap_t *m) CCHASHMAP_NOEXCEPT {
  return m ? m->size : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* CCHASHMAP_H */
