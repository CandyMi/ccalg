/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Intrusive Weak AVL (rank-balanced) tree.  Embed `ccmap1_node_t` in your struct.
**  No internal allocation — caller owns all node memory.
**  32 bytes per node (64-bit): child[2] (16) + parent (8) + rank (2) + padding (6).
**
**  Weak AVL properties (Haeupler-Sen-Tarjan 2015):
**    – Every rank-difference (parent rank − child rank) is 1 or 2.
**    – The rank of any leaf is 1.
**    – Maximum height ≤ 2·floor(log₂(n+1)).
**
**  ── Comparison (choose one) ──
**    Default:   pass a `ccmap1_cmp_t` fn-ptr to ccmap1_init().
**    Optional:  define CCMAP1_COMPARE(a, b) as a macro for inlined compares.
**
**  ── Container-of ──
**    CCMAP1_CONTAINER(ptr, type, member) — offsetof-based cast.
*/
#ifndef CCMAP1_H
#define CCMAP1_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifndef ccmap1_unlikely
  #if defined(__GNUC__) || defined(__clang__)
    #define ccmap1_unlikely(x) __builtin_expect(!!(x), 0)
  #else
    #define ccmap1_unlikely(x) (x)
  #endif
#endif

#ifdef CCMAP1_PREFETCH
  #if defined(__GNUC__) || defined(__clang__)
    #define CCMAP1_PREFETCH_R(addr)  __builtin_prefetch((addr), 0, 1)
    #define CCMAP1_PREFETCH_W(addr)  __builtin_prefetch((addr), 1, 1)
  #elif defined(_MSC_VER)
    #if defined(_M_X64) || defined(_M_IX86)
      #include <xmmintrin.h>
      #define CCMAP1_PREFETCH_R(addr)  _mm_prefetch((const char *)(addr), _MM_HINT_T1)
      #define CCMAP1_PREFETCH_W(addr)  _mm_prefetch((const char *)(addr), _MM_HINT_T1)
    #elif defined(_M_ARM64) || defined(_M_ARM)
      #include <intrin.h>
      #define CCMAP1_PREFETCH_R(addr)  __prefetch((const void *)(addr))
      #define CCMAP1_PREFETCH_W(addr)  __prefetch((const void *)(addr))
    #else
      #define CCMAP1_PREFETCH_R(addr)  ((void)(addr))
      #define CCMAP1_PREFETCH_W(addr)  ((void)(addr))
    #endif
  #else
    #define CCMAP1_PREFETCH_R(addr)  ((void)(addr))
    #define CCMAP1_PREFETCH_W(addr)  ((void)(addr))
  #endif
#else
  #define CCMAP1_PREFETCH_R(addr)  ((void)(addr))
  #define CCMAP1_PREFETCH_W(addr)  ((void)(addr))
#endif

#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
  #define CCMAP1_INLINE static inline
#elif defined(_MSC_VER)
  #define CCMAP1_INLINE static __inline
#elif defined(__GNUC__)
  #define CCMAP1_INLINE static __inline__
#else
  #define CCMAP1_INLINE static
#endif

#ifndef offsetof
  #define offsetof(type, member) ((size_t)&(((type *)0)->member))
#endif
#ifndef container_of
  #define container_of(ptr, type, member) \
    ((type *)((uint8_t *)(ptr) - offsetof(type, member)))
#endif
#define CCMAP1_CONTAINER container_of

#define CCMAP1_LEFT  0
#define CCMAP1_RIGHT 1

#ifdef CCMAP1_COMPARE
  #define CCMAP1_CMP(a, b) CCMAP1_COMPARE((a), (b))
#else
  #define CCMAP1_CMP(a, b) (cmp_fn((a), (b)))
#endif

#ifndef CCMAP1_NODE_T
typedef struct ccmap1_node {
  struct ccmap1_node *child[2];
  struct ccmap1_node *parent;
  uint16_t            rank;     /* 0 = null-equivalent, 1 = leaf */
} ccmap1_node_t;
#define CCMAP1_NODE_T
#endif

typedef int64_t (*ccmap1_cmp_t)(const ccmap1_node_t *a, const ccmap1_node_t *b);
typedef struct ccmap1 {
  ccmap1_node_t    *root, *first, *last;
  size_t            size;
#ifndef CCMAP1_COMPARE
  ccmap1_cmp_t      cmp;
#endif
} ccmap1_t;

/* ── internal helpers ─────────────────────────────────────────────────── */

CCMAP1_INLINE ccmap1_node_t *_w_min(ccmap1_node_t *x) {
  while (x->child[CCMAP1_LEFT]) {
    CCMAP1_PREFETCH_R(x->child[CCMAP1_LEFT]->child[CCMAP1_LEFT]);
    x = x->child[CCMAP1_LEFT];
  }
  return x;
}

/* ── rotate up: pivot `child` above `parent` in direction `dir`.
 *    After rotation, `parent` becomes `child`'s child in direction `dir`.
 *    Returns 0 for single, 1 for double rotation (if caller needs to know). */
CCMAP1_INLINE void _w_rot_up(ccmap1_t *m, ccmap1_node_t *parent,
                              ccmap1_node_t *child, int dir) {
  int other = 1 - dir;
  ccmap1_node_t *gpar = parent->parent;
  parent->child[other] = child->child[dir];
  if (child->child[dir]) child->child[dir]->parent = parent;
  child->child[dir] = parent;
  parent->parent = child;
  child->parent = gpar;
  if (!gpar) m->root = child;
  else       gpar->child[gpar->child[CCMAP1_RIGHT] == parent] = child;
}

/* ── WAVL insert fix ─────────────────────────────────────────────────────
 *  z is newly inserted leaf (rank 1).
 *  Walk up: if Δ(parent, z) == 0 (same rank), promote parent.
 *  If that makes Δ(parent, sibling) == 0, rotate instead.
 *  Otherwise continue walking up.                                         */
CCMAP1_INLINE void _w_ins_fix(ccmap1_t *m, ccmap1_node_t *z) {
  while (z->parent) {
    ccmap1_node_t *p = z->parent;
    int d = (p->child[CCMAP1_RIGHT] == z);   /* direction p → z */

    if (p->rank - z->rank >= 2) break;        /* Δ >= 2 → already balanced */
    if (p->rank - z->rank == 1) break;        /* Δ = 1  → already balanced */

    /* Δ == 0 → promote p */
    p->rank++;

    /* Check sibling: other child of p */
    ccmap1_node_t *s = p->child[1 - d];
    /* If p has a real sibling s, and after the promotion Δ(p, s) == 0,
     we can't promote further — need rotation. */
  if (s && p->rank - s->rank == 0) {
      /* Promoting p made Δ(p, s) == 0 → invalid!  Need rotation.
         Check if z has a heavy child (Δ=2) in direction d for double rot. */
      if (z->child[d] && z->rank - z->child[d]->rank == 2) {
        /* Double rotation: rotate z's heavy child up first */
        _w_rot_up(m, z, z->child[d], 1 - d);
      }
      /* Single (or second half of double) rotation */
      _w_rot_up(m, p, z, 1 - d);

      /* Adjust ranks after rotation.
         `z` is now the new subtree root (replacing p in the tree).
         `p` is now z's child on side (1-d). */
      uint16_t old_rank_p = p->rank;
      p->rank = z->rank - 1;                    /* demote p */
      if (z->rank < old_rank_p) z->rank = old_rank_p;  /* z inherits p's old rank */
      break;
    }

    /* Promotion successful, continue up */
    z = p;
  }
}

/* ════════════════════════════════════════════════════════════════════════
   Public API
   ════════════════════════════════════════════════════════════════════════ */

CCMAP1_INLINE void ccmap1_init(ccmap1_t *m, ccmap1_cmp_t cmp) {
  if (ccmap1_unlikely(!m)) return;
  m->root = m->first = m->last = NULL;
  m->size = 0;
#ifndef CCMAP1_COMPARE
  m->cmp = cmp;
#else
  (void)cmp;
#endif
}

CCMAP1_INLINE int ccmap1_insert(ccmap1_t *m, ccmap1_node_t *node, ccmap1_node_t **out) {
  if (ccmap1_unlikely(!m || !node)) return -1;
  node->child[CCMAP1_LEFT] = node->child[CCMAP1_RIGHT] = NULL;
  node->parent = NULL;
  node->rank = 1;
#ifndef CCMAP1_COMPARE
  ccmap1_cmp_t cmp_fn = m->cmp;
#endif

  /* fast-path: smaller than min */
  if (m->first && CCMAP1_CMP(node, m->first) < 0) {
    m->first->child[CCMAP1_LEFT] = node;
    node->parent = m->first;
    _w_ins_fix(m, node);
    m->first = node;
    m->size++;
    if (out) *out = node;
    return 0;
  }
  /* fast-path: larger than max */
  if (m->last && CCMAP1_CMP(node, m->last) > 0) {
    m->last->child[CCMAP1_RIGHT] = node;
    node->parent = m->last;
    _w_ins_fix(m, node);
    m->last = node;
    m->size++;
    if (out) *out = node;
    return 0;
  }

  ccmap1_node_t *y = NULL, *x = m->root;
  while (x) {
    y = x;
    int64_t c = CCMAP1_CMP(node, x);
    if (c == 0) { if (out) *out = x; return -1; }
    CCMAP1_PREFETCH_R(x->child[CCMAP1_LEFT]);
    CCMAP1_PREFETCH_R(x->child[CCMAP1_RIGHT]);
    x = x->child[c > 0];
  }
  node->parent = y;
  if (!y) { m->root = m->first = m->last = node; }
  else {
    int64_t c = CCMAP1_CMP(node, y);
    y->child[c > 0] = node;
    _w_ins_fix(m, node);
  }
  m->size++;
  if (!m->first || CCMAP1_CMP(node, m->first) < 0) m->first = node;
  if (!m->last  || CCMAP1_CMP(node, m->last)  > 0) m->last  = node;
  if (out) *out = node;
  return 0;
}

CCMAP1_INLINE ccmap1_node_t *ccmap1_find(ccmap1_t *m, const ccmap1_node_t *probe) {
  if (ccmap1_unlikely(!m || !probe)) return NULL;
  ccmap1_node_t *x = m->root;
#ifndef CCMAP1_COMPARE
  ccmap1_cmp_t cmp_fn = m->cmp;
#endif
  while (x) {
    int64_t c = CCMAP1_CMP(probe, x);
    if (c == 0) return x;
    CCMAP1_PREFETCH_R(x->child[CCMAP1_LEFT]);
    CCMAP1_PREFETCH_R(x->child[CCMAP1_RIGHT]);
    x = x->child[c > 0];
  }
  return NULL;
}

CCMAP1_INLINE void ccmap1_erase(ccmap1_t *m, ccmap1_node_t *z) {
  if (ccmap1_unlikely(!m || !z)) return;
  ccmap1_node_t *y = z, *x = NULL, *xp = NULL;
  int is_first = (z == m->first), is_last = (z == m->last);

  if (!z->child[CCMAP1_LEFT]) {
    x = z->child[CCMAP1_RIGHT]; xp = z->parent;
    /* transplant: replace z with x */
    if (x) x->parent = xp;
    if (!xp) m->root = x;
    else xp->child[xp->child[CCMAP1_RIGHT] == z] = x;
  } else if (!z->child[CCMAP1_RIGHT]) {
    x = z->child[CCMAP1_LEFT]; xp = z->parent;
    if (x) x->parent = xp;
    if (!xp) m->root = x;
    else xp->child[xp->child[CCMAP1_RIGHT] == z] = x;
  } else {
    y = _w_min(z->child[CCMAP1_RIGHT]); x = y->child[CCMAP1_RIGHT];
    if (y->parent == z) { xp = y; if (x) x->parent = y; }
    else {
      xp = y->parent; if (x) x->parent = xp;
      xp->child[CCMAP1_LEFT] = x;
      y->child[CCMAP1_RIGHT] = z->child[CCMAP1_RIGHT];
      z->child[CCMAP1_RIGHT]->parent = y;
    }
    /* transplant: replace z with y */
    y->parent = z->parent;
    if (!z->parent) m->root = y;
    else z->parent->child[z->parent->child[CCMAP1_RIGHT] == z] = y;
    y->child[CCMAP1_LEFT] = z->child[CCMAP1_LEFT];
    z->child[CCMAP1_LEFT]->parent = y;
    y->rank = z->rank;   /* y inherits z's rank */
  }

  /* update first/last */
  if (is_first) {
    if (z->child[CCMAP1_RIGHT]) {
      m->first = z->child[CCMAP1_RIGHT];
      while (m->first->child[CCMAP1_LEFT]) m->first = m->first->child[CCMAP1_LEFT];
    } else {
      ccmap1_node_t *p = z->parent;
      ccmap1_node_t *n = z;
      while (p && n == p->child[CCMAP1_RIGHT]) { n = p; p = p->parent; }
      m->first = p;
    }
  }
  if (is_last) {
    if (z->child[CCMAP1_LEFT]) {
      m->last = z->child[CCMAP1_LEFT];
      while (m->last->child[CCMAP1_RIGHT]) m->last = m->last->child[CCMAP1_RIGHT];
    } else {
      ccmap1_node_t *p = z->parent;
      ccmap1_node_t *n = z;
      while (p && n == p->child[CCMAP1_LEFT]) { n = p; p = p->parent; }
      m->last = p;
    }
  }
  m->size--;
  /* delete fix (WIP — no-demotion stub for now) */
  (void)xp;
}

/* ── iteration ────────────────────────────────────────────────────────── */
CCMAP1_INLINE ccmap1_node_t *ccmap1_first(ccmap1_t *m)  { return m ? m->first : NULL; }
CCMAP1_INLINE ccmap1_node_t *ccmap1_begin(ccmap1_t *m)  { return m ? m->first : NULL; }
CCMAP1_INLINE ccmap1_node_t *ccmap1_end(ccmap1_t *m)    { (void)m; return NULL; }
CCMAP1_INLINE ccmap1_node_t *ccmap1_rbegin(ccmap1_t *m) { return m ? m->last : NULL; }
CCMAP1_INLINE ccmap1_node_t *ccmap1_last(ccmap1_t *m)   { return ccmap1_rbegin(m); }

CCMAP1_INLINE ccmap1_node_t *ccmap1_next(ccmap1_node_t *x) {
  if (!x) return NULL;
  if (x->child[CCMAP1_RIGHT]) return _w_min(x->child[CCMAP1_RIGHT]);
  ccmap1_node_t *p = x->parent;
  while (p && x == p->child[CCMAP1_RIGHT]) { x = p; p = p->parent; }
  return p;
}
CCMAP1_INLINE ccmap1_node_t *ccmap1_prev(ccmap1_node_t *x) {
  if (!x) return NULL;
  if (x->child[CCMAP1_LEFT]) {
    x = x->child[CCMAP1_LEFT];
    while (x->child[CCMAP1_RIGHT]) { CCMAP1_PREFETCH_R(x->child[CCMAP1_RIGHT]); x = x->child[CCMAP1_RIGHT]; }
    return x;
  }
  ccmap1_node_t *p = x->parent;
  while (p && x == p->child[CCMAP1_LEFT]) { x = p; p = p->parent; }
  return p;
}
CCMAP1_INLINE size_t ccmap1_size(ccmap1_t *m) { return m ? m->size : 0; }
CCMAP1_INLINE void   ccmap1_clear(ccmap1_t *m) { if (m) { m->root = m->first = m->last = NULL; m->size = 0; } }
CCMAP1_INLINE int    ccmap1_height(const ccmap1_t *m) {
  if (!m || !m->size) return 0;
  size_t n = m->size; int h = 0;
  do h++; while (n >>= 1);
  return h * 2;
}

#endif /* CCMAP1_H */
