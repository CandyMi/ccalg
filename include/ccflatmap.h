/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Sorted-array map (flat map).  Stores key-value pairs by value in a
**  contiguous, sorted array — binary-search find, memmove-based insert.
**
**  ── Comparison (choose one) ──
**
**    Default:   pass a `ccflatmap_cmp_t` function pointer to init().
**    Optional:  define CCFLATMAP_COMPARE(a, b) as a macro before #include
**               for inlined comparisons.
**
**  ── Custom element type ──
**
**    #define CCFLATMAP_NODE_T struct my_pair
**    struct my_pair { int id; double score; };
**    #define CCFLATMAP_COMPARE(a, b) ((int64_t)(a)->id - (int64_t)(b)->id)
**    #include "ccflatmap.h"
**
**    ccflatmap_t m; ccflatmap_init(&m, NULL);
**    struct my_pair e = {42, 3.14};
**    ccflatmap_insert(&m, e, NULL);
**    struct my_pair *f = ccflatmap_find(&m, &e);
**
**  ── Iteration ──
**
**    for (size_t i = 0; i < ccflatmap_size(&m); i++) {
**        CCFLATMAP_NODE_T *p = ccflatmap_at(&m, i);
**        // ...
**    }
**
**    // or pointer-based:
**    for (CCFLATMAP_NODE_T *p = ccflatmap_begin(&m);
**         p != ccflatmap_end(&m);
**         p = ccflatmap_next(&m, p)) { ... }
*/
#ifndef CCFLATMAP_H
#define CCFLATMAP_H

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

/* ── branch hint ──────────────────────────────────────────────────────── */

#ifndef ccflatmap_unlikely
  #if defined(__GNUC__) || defined(__clang__)
    #define ccflatmap_unlikely(x) __builtin_expect(!!(x), 0)
  #else
    #define ccflatmap_unlikely(x) (x)
  #endif
#endif

/* ── portable inline ──────────────────────────────────────────────────── */

#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
  #define CCFLATMAP_INLINE static inline
#elif defined(_MSC_VER)
  #define CCFLATMAP_INLINE static __inline
#elif defined(__GNUC__)
  #define CCFLATMAP_INLINE static __inline__
#else
  #define CCFLATMAP_INLINE static
#endif

/* ── allocator hooks ──────────────────────────────────────────────────── */

#ifndef CCFLATMAP_REALLOC
  #define CCFLATMAP_REALLOC realloc
#endif
#ifndef CCFLATMAP_MALLOC
  #define CCFLATMAP_MALLOC(sz) CCFLATMAP_REALLOC(NULL, (sz))
#endif
#ifndef CCFLATMAP_FREE
  #define CCFLATMAP_FREE(ptr) free(ptr)
#endif
#ifndef CCFLATMAP_DEFAULT_CAP
  #define CCFLATMAP_DEFAULT_CAP 8
#endif

/* ── types ────────────────────────────────────────────────────────────── */

#ifndef CCFLATMAP_NODE_T
  #define CCFLATMAP_NODE_T ccflatmap_node_t
  typedef struct ccflatmap_node {
    int64_t  key;
    uint64_t value;
  } ccflatmap_node_t;
#endif

typedef int64_t (*ccflatmap_cmp_t)(const CCFLATMAP_NODE_T *a,
                                   const CCFLATMAP_NODE_T *b);

typedef struct ccflatmap {
  CCFLATMAP_NODE_T *buckets;
  size_t            len;
  size_t            cap;
#ifndef CCFLATMAP_COMPARE
  ccflatmap_cmp_t   cmp;
#endif
} ccflatmap_t;

/* ── comparison dispatch ──────────────────────────────────────────────── */

#ifdef CCFLATMAP_COMPARE
  #define CCFLATMAP_CMP(a, b) CCFLATMAP_COMPARE((a), (b))
#else
  #define CCFLATMAP_CMP(a, b) (cmp_fn((a), (b)))
#endif

/* ── internal: binary search (single-branch lower_bound) ──────────────── */

/* Returns the first index where buckets[pos] >= probe.
** Caller checks for exact match with CMP(probe, &buckets[pos]) == 0. */
/* Branchless binary search — compiler emits cmov instead of unpredictable
** branches.  The ternary `gt ? x : y` pattern is recognized by clang/gcc
** at -O2 and converted to conditional-move instructions. */
CCFLATMAP_INLINE size_t
_ccflatmap_lower_bound(const ccflatmap_t *m,
                       const CCFLATMAP_NODE_T *probe) {
  CCFLATMAP_NODE_T *b = m->buckets;
  size_t lo = 0, n = m->len;
#ifndef CCFLATMAP_COMPARE
  ccflatmap_cmp_t cmp_fn = m->cmp;
#endif
  while (n > 0) {
    size_t half = n / 2;
    size_t mid = lo + half;
    int gt = CCFLATMAP_CMP(probe, &b[mid]) > 0;
    lo = gt ? (mid + 1) : lo;
    n  = gt ? (n - half - 1) : half;
  }
  return lo;
}

/* ── internal: grow ──────────────────────────────────────────────────── */

CCFLATMAP_INLINE int
_ccflatmap_grow(ccflatmap_t *m) {
  if (m->cap > SIZE_MAX / 2 / sizeof(CCFLATMAP_NODE_T)) return -1;
  size_t new_cap = m->cap * 2;
  CCFLATMAP_NODE_T *nd = (CCFLATMAP_NODE_T *)CCFLATMAP_REALLOC(
      m->buckets, sizeof(CCFLATMAP_NODE_T) * new_cap);
  if (!nd) return -1;
  m->buckets = nd;
  m->cap = new_cap;
  return 0;
}

/* ── lifecycle ────────────────────────────────────────────────────────── */

CCFLATMAP_INLINE int
ccflatmap_init(ccflatmap_t *m, ccflatmap_cmp_t cmp) {
  if (ccflatmap_unlikely(!m)) return -1;
  m->buckets = (CCFLATMAP_NODE_T *)CCFLATMAP_MALLOC(
      sizeof(CCFLATMAP_NODE_T) * CCFLATMAP_DEFAULT_CAP);
  if (!m->buckets) return -1;
  m->len = 0;
  m->cap = CCFLATMAP_DEFAULT_CAP;
#ifndef CCFLATMAP_COMPARE
  m->cmp = cmp;
#else
  (void)cmp;
#endif
  return 0;
}

CCFLATMAP_INLINE void
ccflatmap_destroy(ccflatmap_t *m) {
  if (!m) return;
  CCFLATMAP_FREE(m->buckets);
  m->buckets = NULL;
  m->len = m->cap = 0;
}

CCFLATMAP_INLINE void
ccflatmap_clear(ccflatmap_t *m) {
  if (m) m->len = 0;
}

/* ── insert ───────────────────────────────────────────────────────────── */

CCFLATMAP_INLINE int
ccflatmap_insert(ccflatmap_t *m, CCFLATMAP_NODE_T elem,
                 CCFLATMAP_NODE_T **out) {
  if (ccflatmap_unlikely(!m || !m->buckets)) return -1;

#ifndef CCFLATMAP_COMPARE
  ccflatmap_cmp_t cmp_fn = m->cmp;
#endif

  size_t pos = _ccflatmap_lower_bound(m, &elem);

  /* duplicate check before grow — saves realloc on dup */
  if (pos < m->len && CCFLATMAP_CMP(&elem, &m->buckets[pos]) == 0) {
    if (out) *out = &m->buckets[pos];
    return -1;
  }

  if (ccflatmap_unlikely(m->len == m->cap)) {
    if (_ccflatmap_grow(m) != 0) return -1;
  }

  /* reload buckets — may have moved after grow */
  CCFLATMAP_NODE_T *b = m->buckets;
  if (pos < m->len)
    memmove(&b[pos + 1], &b[pos],
            (m->len - pos) * sizeof(CCFLATMAP_NODE_T));
  b[pos] = elem;
  m->len++;

  if (out) *out = &b[pos];
  return 0;
}

/* ── find ─────────────────────────────────────────────────────────────── */

CCFLATMAP_INLINE CCFLATMAP_NODE_T *
ccflatmap_find(ccflatmap_t *m, const CCFLATMAP_NODE_T *probe) {
  if (ccflatmap_unlikely(!m || !probe || !m->len)) return NULL;
#ifndef CCFLATMAP_COMPARE
  ccflatmap_cmp_t cmp_fn = m->cmp;
#endif
  size_t pos = _ccflatmap_lower_bound(m, probe);
  if (pos < m->len && CCFLATMAP_CMP(probe, &m->buckets[pos]) == 0)
    return &m->buckets[pos];
  return NULL;
}

/* ── erase ────────────────────────────────────────────────────────────── */

CCFLATMAP_INLINE void
ccflatmap_erase(ccflatmap_t *m, const CCFLATMAP_NODE_T *probe) {
  if (ccflatmap_unlikely(!m || !probe || !m->len)) return;
#ifndef CCFLATMAP_COMPARE
  ccflatmap_cmp_t cmp_fn = m->cmp;
#endif
  CCFLATMAP_NODE_T *b = m->buckets;
  size_t pos = _ccflatmap_lower_bound(m, probe);
  if (pos >= m->len || CCFLATMAP_CMP(probe, &b[pos]) != 0) return;
  if (pos < m->len - 1)
    memmove(&b[pos], &b[pos + 1],
            (m->len - pos - 1) * sizeof(CCFLATMAP_NODE_T));
  m->len--;
}

/* Erase by index — skips binary search when position is already known
** (e.g. from a prior find).  O(n) memmove, same as erase-by-key. */
CCFLATMAP_INLINE void
ccflatmap_erase_at(ccflatmap_t *m, size_t pos) {
  if (ccflatmap_unlikely(!m || pos >= m->len)) return;
  CCFLATMAP_NODE_T *b = m->buckets;
  if (pos < m->len - 1)
    memmove(&b[pos], &b[pos + 1],
            (m->len - pos - 1) * sizeof(CCFLATMAP_NODE_T));
  m->len--;
}

/* Unordered erase — swaps target with last element, then pops.
** O(log n) find + O(1) swap.  Breaks sorted order — call sort() after
** a batch of unordered erases to restore ordering.
**
** Pattern:
**   for (...) ccflatmap_erase_unordered(&m, &probes[i]);
**   ccflatmap_sort(&m);   // restore order, O(n log n) once   */
CCFLATMAP_INLINE void
ccflatmap_erase_unordered(ccflatmap_t *m,
                          const CCFLATMAP_NODE_T *probe) {
  if (ccflatmap_unlikely(!m || !probe || !m->len)) return;
#ifndef CCFLATMAP_COMPARE
  ccflatmap_cmp_t cmp_fn = m->cmp;
#endif
  CCFLATMAP_NODE_T *b = m->buckets;
  size_t pos = _ccflatmap_lower_bound(m, probe);
  if (pos >= m->len || CCFLATMAP_CMP(probe, &b[pos]) != 0) return;
  if (pos < m->len - 1)
    b[pos] = b[m->len - 1];
  m->len--;
}

/* ── access ───────────────────────────────────────────────────────────── */

CCFLATMAP_INLINE CCFLATMAP_NODE_T *
ccflatmap_at(ccflatmap_t *m, size_t i) {
  if (ccflatmap_unlikely(!m || i >= m->len)) return NULL;
  return &m->buckets[i];
}

/* ── iteration ────────────────────────────────────────────────────────── */

CCFLATMAP_INLINE CCFLATMAP_NODE_T *
ccflatmap_begin(ccflatmap_t *m) {
  return (m && m->len) ? &m->buckets[0] : NULL;
}

CCFLATMAP_INLINE CCFLATMAP_NODE_T *
ccflatmap_end(ccflatmap_t *m) {
  (void)m;
  return NULL;
}

CCFLATMAP_INLINE CCFLATMAP_NODE_T *
ccflatmap_rbegin(ccflatmap_t *m) {
  return (m && m->len) ? &m->buckets[m->len - 1] : NULL;
}

CCFLATMAP_INLINE CCFLATMAP_NODE_T *
ccflatmap_next(ccflatmap_t *m, CCFLATMAP_NODE_T *p) {
  if (!m || !p || (size_t)(p - m->buckets) >= m->len - 1) return NULL;
  return p + 1;
}

CCFLATMAP_INLINE CCFLATMAP_NODE_T *
ccflatmap_prev(ccflatmap_t *m, CCFLATMAP_NODE_T *p) {
  if (!m || !p || p <= m->buckets) return NULL;
  return p - 1;
}

/* ── query ────────────────────────────────────────────────────────────── */

CCFLATMAP_INLINE size_t
ccflatmap_size(const ccflatmap_t *m) {
  return m ? m->len : 0;
}

CCFLATMAP_INLINE int
ccflatmap_empty(ccflatmap_t *m) {
  return !m || m->len == 0;
}

CCFLATMAP_INLINE size_t
ccflatmap_capacity(ccflatmap_t *m) {
  return m ? m->cap : 0;
}

/* ── reserve ──────────────────────────────────────────────────────────── */

CCFLATMAP_INLINE int
ccflatmap_reserve(ccflatmap_t *m, size_t new_cap) {
  if (!m || !m->buckets || new_cap <= m->cap) return -1;
  CCFLATMAP_NODE_T *nd = (CCFLATMAP_NODE_T *)CCFLATMAP_REALLOC(
      m->buckets, sizeof(CCFLATMAP_NODE_T) * new_cap);
  if (!nd) return -1;
  m->buckets = nd;
  m->cap = new_cap;
  return 0;
}

/* ── bulk insert ──────────────────────────────────────────────────────── */

/* Append without maintaining sorted order — O(1) amortized.
** Use ccflatmap_sort() afterwards to restore order.
**
** Pattern for bulk insert:
**   ccflatmap_reserve(&m, N);
**   for (...) ccflatmap_push_back(&m, elem);
**   ccflatmap_sort(&m);   // O(N log N) instead of O(N²)      */
CCFLATMAP_INLINE int
ccflatmap_push_back(ccflatmap_t *m, CCFLATMAP_NODE_T elem) {
  if (ccflatmap_unlikely(!m || !m->buckets)) return -1;
  if (ccflatmap_unlikely(m->len == m->cap)) {
    if (_ccflatmap_grow(m) != 0) return -1;
  }
  m->buckets[m->len++] = elem;
  return 0;
}

/* ── internal: quicksort (in-place, C-portable) ──────────────────────── */

CCFLATMAP_INLINE void
_ccflatmap_swap(CCFLATMAP_NODE_T *a, CCFLATMAP_NODE_T *b) {
  CCFLATMAP_NODE_T t = *a; *a = *b; *b = t;
}

/* median-of-three pivot selection */
CCFLATMAP_INLINE CCFLATMAP_NODE_T *
_ccflatmap_med3(CCFLATMAP_NODE_T *a, CCFLATMAP_NODE_T *b, CCFLATMAP_NODE_T *c
#ifndef CCFLATMAP_COMPARE
                , ccflatmap_cmp_t cmp_fn
#endif
) {
  if (CCFLATMAP_CMP(a, b) > 0) { /* a < b */ _ccflatmap_swap(a, b); }
  if (CCFLATMAP_CMP(b, c) > 0) { /* b < c */
    _ccflatmap_swap(b, c);
    if (CCFLATMAP_CMP(a, b) > 0) _ccflatmap_swap(a, b);
  }
  return b;
}

CCFLATMAP_INLINE void
_ccflatmap_qsort(CCFLATMAP_NODE_T *lo, CCFLATMAP_NODE_T *hi
#ifndef CCFLATMAP_COMPARE
                 , ccflatmap_cmp_t cmp_fn
#endif
) {
  /* insertion sort for small partitions */
  ptrdiff_t n = hi - lo + 1;
  if (n <= 16) {
    for (CCFLATMAP_NODE_T *i = lo + 1; i <= hi; i++) {
      CCFLATMAP_NODE_T tmp = *i;
      CCFLATMAP_NODE_T *j = i;
      while (j > lo && CCFLATMAP_CMP(j - 1, &tmp) > 0) {
        *j = *(j - 1); j--;
      }
      *j = tmp;
    }
    return;
  }

  CCFLATMAP_NODE_T *piv = _ccflatmap_med3(lo, lo + n / 2, hi
#ifndef CCFLATMAP_COMPARE
    , cmp_fn
#endif
  );
  CCFLATMAP_NODE_T pv = *piv;

  /* Hoare partition */
  CCFLATMAP_NODE_T *i = lo - 1, *j = hi + 1;
  for (;;) {
    do { i++; } while (CCFLATMAP_CMP(i, &pv) < 0);
    do { j--; } while (CCFLATMAP_CMP(&pv, j) < 0);
    if (i >= j) break;
    _ccflatmap_swap(i, j);
  }

  _ccflatmap_qsort(lo, j
#ifndef CCFLATMAP_COMPARE
    , cmp_fn
#endif
  );
  _ccflatmap_qsort(j + 1, hi
#ifndef CCFLATMAP_COMPARE
    , cmp_fn
#endif
  );
}

/* ── sort ─────────────────────────────────────────────────────────────── */

CCFLATMAP_INLINE void
ccflatmap_sort(ccflatmap_t *m) {
  if (!m || ccflatmap_size(m) <= 1) return;
#ifndef CCFLATMAP_COMPARE
  ccflatmap_cmp_t cmp_fn = m->cmp;
#endif
  _ccflatmap_qsort(m->buckets, m->buckets + ccflatmap_size(m) - 1
#ifndef CCFLATMAP_COMPARE
    , cmp_fn
#endif
  );
}

#endif /* CCFLATMAP_H */
