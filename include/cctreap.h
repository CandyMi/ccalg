/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Intrusive treap (tree + heap).  Embed `cctreap_node_t` in your struct.
**  No internal allocation — caller owns all node memory.
**  Priority is internal: a random uint64_t is generated for each node on
**  insert via xorshift64 (overridable via CCTREAP_RAND).  Max-heap invariant
**  is enforced automatically — no user-facing priority API.
**
**  ── Comparison ──
**
**    Key (BST order):
**      Default:   pass a `cctreap_cmp_t` to cctreap_init().
**      Optional:  define CCTREAP_COMPARE(a, b) before #include.
**
**  ── RNG override ──
**
**    #define CCTREAP_RAND_T               my_rng_state_t     (default uint64_t)
**    #define CCTREAP_RAND_INIT(m, seed)   my_rng_init(m, seed)
**    #define CCTREAP_RAND_NEXT(state)     my_rng_next(state)
**    Default: uint64_t state + pointer seed + xorshift64 step.
**
**  ── Container-of ──
**
**    CCTREAP_CONTAINER(ptr, type, member) — offsetof-based cast.
*/
#ifndef CCTREAP_H
#define CCTREAP_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ── branch hint ──────────────────────────────────────────────────────── */

#ifndef cctreap_unlikely
  #if defined(__GNUC__) || defined(__clang__)
    #define cctreap_unlikely(x) __builtin_expect(!!(x), 0)
  #else
    #define cctreap_unlikely(x) (x)
  #endif
#endif

/* ── prefetch (opt-in, multi-platform/arch) ───────────────────────────── */
#ifdef CCTREAP_PREFETCH
  #if defined(__GNUC__) || defined(__clang__)
    #define CCTREAP_PREFETCH_R(addr)  __builtin_prefetch((addr), 0, 1)
    #define CCTREAP_PREFETCH_W(addr)  __builtin_prefetch((addr), 1, 1)
  #elif defined(_MSC_VER)
    #if defined(_M_X64) || defined(_M_IX86)
      #ifndef __XMMINTRIN_H
        #include <xmmintrin.h>
      #endif
      #define CCTREAP_PREFETCH_R(addr)  _mm_prefetch((const char *)(addr), _MM_HINT_T1)
      #define CCTREAP_PREFETCH_W(addr)  _mm_prefetch((const char *)(addr), _MM_HINT_T1)
    #elif defined(_M_ARM64) || defined(_M_ARM)
      #include <intrin.h>
      #define CCTREAP_PREFETCH_R(addr)  __prefetch((const void *)(addr))
      #define CCTREAP_PREFETCH_W(addr)  __prefetch((const void *)(addr))
    #else
      #define CCTREAP_PREFETCH_R(addr)  ((void)(addr))
      #define CCTREAP_PREFETCH_W(addr)  ((void)(addr))
    #endif
  #else
    #define CCTREAP_PREFETCH_R(addr)  ((void)(addr))
    #define CCTREAP_PREFETCH_W(addr)  ((void)(addr))
  #endif
#else
  #define CCTREAP_PREFETCH_R(addr)  ((void)(addr))
  #define CCTREAP_PREFETCH_W(addr)  ((void)(addr))
#endif

#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
  #define CCTREAP_INLINE static inline
#elif defined(_MSC_VER)
  #define CCTREAP_INLINE static __inline
#elif defined(__GNUC__)
  #define CCTREAP_INLINE static __inline__
#else
  #define CCTREAP_INLINE static
#endif

#ifndef offsetof
  #define offsetof(type, member) \
      ((size_t) &(((type *)0)->member))
#endif

#ifndef container_of
  #define container_of(ptr, type, member) \
    ((type *)((uint8_t *)(ptr) - offsetof(type, member)))
#endif

#define CCTREAP_CONTAINER container_of

#define CCTREAP_LEFT  0
#define CCTREAP_RIGHT 1

/* ── comparison dispatch ──────────────────────────────────────────────── */

#ifdef CCTREAP_COMPARE
  #define CCTREAP_CMP(a, b) CCTREAP_COMPARE((a), (b))
#else
  #define CCTREAP_CMP(a, b) (key_cmp((a), (b)))
#endif

/* ── node ─────────────────────────────────────────────────────────────── */

#ifndef CCTREAP_NODE_T
typedef struct cctreap_node {
  struct cctreap_node *child[2];  /* [CCTREAP_LEFT]=left, [CCTREAP_RIGHT]=right */
  uintptr_t                pc;    /* parent pointer (low bit unused) */
  size_t                   size;  /* subtree node count */
  uint64_t                 priority; /* internal random priority (max-heap) */
} cctreap_node_t;
#define CCTREAP_NODE_T
#endif

/* parent accessors — same encoding as ccmap */
#define _tp_p(n)      ((cctreap_node_t *)((n)->pc & ~(uintptr_t)1))
#define _tp_sp(n, p)  ((n)->pc = ((n)->pc & 1) | (uintptr_t)(p))
#define _tp_spc(n, p, c) ((n)->pc = (uintptr_t)(p) | (uintptr_t)(c))
#define _tp_sz(n)     ((n) ? (n)->size : 0)

CCTREAP_INLINE void _tp_upd_sz(cctreap_node_t *n) {
  n->size = 1 + _tp_sz(n->child[CCTREAP_LEFT]) + _tp_sz(n->child[CCTREAP_RIGHT]);
}
#define _tp_upd_path(from)                                    \
  do { cctreap_node_t *u_ = (from);                           \
       while (u_) { _tp_upd_sz(u_); u_ = _tp_p(u_); }        \
  } while(0)

CCTREAP_INLINE cctreap_node_t *_tp_min(cctreap_node_t *x) {
  if (!x) return NULL;
  while (x->child[CCTREAP_LEFT]) {
    CCTREAP_PREFETCH_R(x->child[CCTREAP_LEFT]->child[CCTREAP_LEFT]);
    x = x->child[CCTREAP_LEFT];
  }
  return x;
}

CCTREAP_INLINE cctreap_node_t *_tp_max(cctreap_node_t *x) {
  if (!x) return NULL;
  while (x->child[CCTREAP_RIGHT]) {
    CCTREAP_PREFETCH_R(x->child[CCTREAP_RIGHT]->child[CCTREAP_RIGHT]);
    x = x->child[CCTREAP_RIGHT];
  }
  return x;
}


/* ── internal xorshift64 (overridable via CCTREAP_RAND_NEXT) ────────── */
CCTREAP_INLINE uint64_t _tp_xorshift64(uint64_t *state) {
  uint64_t x = *state;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  return *state = x;
}

/* ── RNG state type (overridable) ────────────────────────────────────── */

#ifndef CCTREAP_RAND_T
  #define CCTREAP_RAND_T uint64_t
#endif

#ifndef CCTREAP_RAND_INIT
  #define CCTREAP_RAND_INIT(m, seed)  ((m)->state = (seed))
#endif

#ifndef CCTREAP_RAND_NEXT
  #define CCTREAP_RAND_NEXT(state) _tp_xorshift64(state)
#endif

/* backward-compat alias */
#ifndef CCTREAP_RAND
  #define CCTREAP_RAND(state) CCTREAP_RAND_NEXT(state)
#endif

/* ── types ────────────────────────────────────────────────────────────── */

typedef int64_t (*cctreap_cmp_t) (const cctreap_node_t *a, const cctreap_node_t *b);

typedef struct cctreap {
  cctreap_node_t    *root;
  cctreap_node_t    *first;
  cctreap_node_t    *last;
  size_t             size;
#ifndef CCTREAP_COMPARE
  cctreap_cmp_t      key_cmp;
#endif
  CCTREAP_RAND_T     state;   /* RNG state (default uint64_t, overridable) */
} cctreap_t;

/* ── internal priority (max-heap, non-overridable) ──────────────────── */
CCTREAP_INLINE int64_t _tp_prio_cmp(const cctreap_node_t *a,
                                    const cctreap_node_t *b) {
  if (a->priority > b->priority) return 1;
  if (a->priority < b->priority) return -1;
  return 0;
}
#define _TP_PRIO_CMP(a, b) _tp_prio_cmp((a), (b))

/* ── transplant ───────────────────────────────────────────────────────── */

CCTREAP_INLINE void _tp_transplant(cctreap_t *m, cctreap_node_t *z, cctreap_node_t *c) {
  cctreap_node_t *zp = _tp_p(z);
  if (c) _tp_sp(c, zp);
  if (!zp) { m->root = c; return; }
  zp->child[zp->child[CCTREAP_RIGHT] == z] = c;
}

/* ── rotations (with size update) ─────────────────────────────────────── */

CCTREAP_INLINE void _tp_rot_left(cctreap_t *m, cctreap_node_t *x) {
  cctreap_node_t *y  = x->child[CCTREAP_RIGHT];
  cctreap_node_t *xp = _tp_p(x);
  cctreap_node_t *yl = y->child[CCTREAP_LEFT];
  x->child[CCTREAP_RIGHT] = yl;
  if (yl) _tp_sp(yl, x);
  _tp_sp(y, xp);
  if (!xp) m->root = y;
  else     xp->child[xp->child[CCTREAP_RIGHT] == x] = y;
  y->child[CCTREAP_LEFT] = x; _tp_sp(x, y);
  _tp_upd_sz(x);
  _tp_upd_sz(y);
}

CCTREAP_INLINE void _tp_rot_right(cctreap_t *m, cctreap_node_t *x) {
  cctreap_node_t *y  = x->child[CCTREAP_LEFT];
  cctreap_node_t *xp = _tp_p(x);
  cctreap_node_t *yr = y->child[CCTREAP_RIGHT];
  x->child[CCTREAP_LEFT] = yr;
  if (yr) _tp_sp(yr, x);
  _tp_sp(y, xp);
  if (!xp) m->root = y;
  else     xp->child[xp->child[CCTREAP_RIGHT] == x] = y;
  y->child[CCTREAP_RIGHT] = x; _tp_sp(x, y);
  _tp_upd_sz(x);
  _tp_upd_sz(y);
}

/* ── insert fixup (bubble up by priority) ─────────────────────────────── */

CCTREAP_INLINE void _tp_ins_fix(cctreap_t *m, cctreap_node_t *z) {
  cctreap_node_t *p = _tp_p(z);
  while (p && _TP_PRIO_CMP(z, p) > 0) {
    if (z == p->child[CCTREAP_LEFT]) _tp_rot_right(m, p);
    else                             _tp_rot_left(m, p);
    p = _tp_p(z);
  }
  (void)m;
}

/* ── delete fixup (bubble down to leaf) ───────────────────────────────── */

CCTREAP_INLINE void _tp_del_fix(cctreap_t *m, cctreap_node_t *z) {
  while (z->child[CCTREAP_LEFT] || z->child[CCTREAP_RIGHT]) {
    if (!z->child[CCTREAP_LEFT])
      _tp_rot_left(m, z);
    else if (!z->child[CCTREAP_RIGHT])
      _tp_rot_right(m, z);
    else if (_TP_PRIO_CMP(z->child[CCTREAP_LEFT], z->child[CCTREAP_RIGHT]) > 0)
      _tp_rot_right(m, z);
    else
      _tp_rot_left(m, z);
  }
  (void)m;
}

/* ── public API ───────────────────────────────────────────────────────── */

CCTREAP_INLINE void cctreap_init(cctreap_t *m, cctreap_cmp_t key_cmp) {
  if (cctreap_unlikely(!m)) return;
  m->root      = NULL;
  m->first     = NULL;
  m->last      = NULL;
  m->size      = 0;
#ifndef CCTREAP_COMPARE
  m->key_cmp   = key_cmp;
#else
  (void)key_cmp;
#endif
  CCTREAP_RAND_INIT(m, (uint64_t)(uintptr_t)m);
}

CCTREAP_INLINE int cctreap_insert(cctreap_t *m, cctreap_node_t *node, cctreap_node_t **out) {
  if (cctreap_unlikely(!m || !node)) return -1;
  node->child[CCTREAP_LEFT] = node->child[CCTREAP_RIGHT] = NULL;
  node->size     = 1;
  node->priority = CCTREAP_RAND_NEXT(&m->state);
  _tp_spc(node, NULL, 0);
#ifndef CCTREAP_COMPARE
  cctreap_cmp_t key_cmp = m->key_cmp;
#endif

  /* fast path: smaller than current minimum → insert as new first */
  if (m->first && CCTREAP_CMP(node, m->first) < 0) {
    m->first->child[CCTREAP_LEFT] = node;
    _tp_sp(node, m->first);
    _tp_upd_path(m->first);
    _tp_ins_fix(m, node);
    m->first = node;
    m->size++;
    if (out) *out = node;
    return 0;
  }
  /* fast path: larger than current maximum → insert as new last */
  if (m->last && CCTREAP_CMP(node, m->last) > 0) {
    m->last->child[CCTREAP_RIGHT] = node;
    _tp_sp(node, m->last);
    _tp_upd_path(m->last);
    _tp_ins_fix(m, node);
    m->last = node;
    m->size++;
    if (out) *out = node;
    return 0;
  }

  /* normal BST insert from root */
  cctreap_node_t *y = NULL, *x = m->root;
  while (x) {
    y = x;
    int64_t c = CCTREAP_CMP(node, x);
    if (c == 0) { if (out) *out = x; return -1; }
    CCTREAP_PREFETCH_R(x->child[CCTREAP_LEFT]);
    CCTREAP_PREFETCH_R(x->child[CCTREAP_RIGHT]);
    x = x->child[c > 0];
  }
  _tp_sp(node, y);
  if (!y) { m->root = m->first = m->last = node; }
  else {
    int64_t c = CCTREAP_CMP(node, y);
    y->child[c > 0] = node;
    _tp_upd_path(y);
  }

  _tp_ins_fix(m, node);
  m->size++;
  if (CCTREAP_CMP(node, m->first) < 0) m->first = node;
  if (CCTREAP_CMP(node, m->last) > 0)  m->last  = node;
  if (out) *out = node;
  return 0;
}

CCTREAP_INLINE cctreap_node_t *cctreap_find(cctreap_t *m, const cctreap_node_t *probe) {
  if (cctreap_unlikely(!m || !probe)) return NULL;
  cctreap_node_t *x = m->root;
#ifndef CCTREAP_COMPARE
  cctreap_cmp_t key_cmp = m->key_cmp;
#endif
  while (x) {
    int64_t c = CCTREAP_CMP(probe, x);
    if (c == 0) return x;
    CCTREAP_PREFETCH_R(x->child[CCTREAP_LEFT]);
    CCTREAP_PREFETCH_R(x->child[CCTREAP_RIGHT]);
    x = x->child[c > 0];
  }
  return NULL;
}

CCTREAP_INLINE void cctreap_erase(cctreap_t *m, cctreap_node_t *z) {
  if (cctreap_unlikely(!m || !z)) return;
  int is_first = (z == m->first);
  int is_last  = (z == m->last);

  /* bubble z down to a leaf */
  _tp_del_fix(m, z);

  /* remove the leaf */
  {
    cctreap_node_t *zp = _tp_p(z);
    _tp_transplant(m, z, NULL);
    _tp_upd_path(zp);
  }

  /* recompute cached boundaries */
  if (is_first) m->first = _tp_min(m->root);
  if (is_last)  m->last  = _tp_max(m->root);
  m->size--;
}

CCTREAP_INLINE cctreap_node_t *cctreap_kth(cctreap_t *m, size_t k) {
  if (cctreap_unlikely(!m || !m->root || k >= m->size)) return NULL;
  cctreap_node_t *x = m->root;
  while (x) {
    size_t ls = _tp_sz(x->child[CCTREAP_LEFT]);
    if (k < ls)       x = x->child[CCTREAP_LEFT];
    else if (k == ls) return x;
    else              { k -= ls + 1; x = x->child[CCTREAP_RIGHT]; }
  }
  return NULL;
}

CCTREAP_INLINE int64_t cctreap_rank(cctreap_t *m, const cctreap_node_t *probe) {
  if (cctreap_unlikely(!m || !probe || !m->root)) return -1;
  cctreap_node_t *x = m->root;
#ifndef CCTREAP_COMPARE
  cctreap_cmp_t key_cmp = m->key_cmp;
#endif
  int64_t r = 0;
  while (x) {
    int64_t c = CCTREAP_CMP(probe, x);
    size_t ls = _tp_sz(x->child[CCTREAP_LEFT]);
    if (c == 0) return r + (int64_t)ls;
    if (c > 0) { r += (int64_t)ls + 1; x = x->child[CCTREAP_RIGHT]; }
    else       { x = x->child[CCTREAP_LEFT]; }
  }
  return -1;
}

CCTREAP_INLINE cctreap_node_t *cctreap_first(cctreap_t *m)  { return m ? m->first  : NULL; }
CCTREAP_INLINE cctreap_node_t *cctreap_begin(cctreap_t *m)  { return m ? m->first  : NULL; }
CCTREAP_INLINE cctreap_node_t *cctreap_end(cctreap_t *m)    { (void)m; return NULL; }
CCTREAP_INLINE cctreap_node_t *cctreap_last(cctreap_t *m)   { return m ? m->last   : NULL; }
CCTREAP_INLINE cctreap_node_t *cctreap_rbegin(cctreap_t *m) { return m ? m->last   : NULL; }

CCTREAP_INLINE cctreap_node_t *cctreap_next(cctreap_node_t *x) {
  if (!x) return NULL;
  if (x->child[CCTREAP_RIGHT]) return _tp_min(x->child[CCTREAP_RIGHT]);
  cctreap_node_t *p = _tp_p(x);
  while (p && x == p->child[CCTREAP_RIGHT]) {
    x = p; p = _tp_p(p);
    CCTREAP_PREFETCH_R(p);
  }
  return p;
}

CCTREAP_INLINE cctreap_node_t *cctreap_prev(cctreap_node_t *x) {
  if (!x) return NULL;
  if (x->child[CCTREAP_LEFT]) { x = x->child[CCTREAP_LEFT]; while (x->child[CCTREAP_RIGHT]) { CCTREAP_PREFETCH_R(x->child[CCTREAP_RIGHT]); x = x->child[CCTREAP_RIGHT]; } return x; }
  cctreap_node_t *p = _tp_p(x);
  while (p && x == p->child[CCTREAP_LEFT]) {
    x = p; p = _tp_p(p);
    CCTREAP_PREFETCH_R(p);
  }
  return p;
}

CCTREAP_INLINE size_t cctreap_size(cctreap_t *m) { return m ? m->size : 0; }
CCTREAP_INLINE void cctreap_clear(cctreap_t *m) {
  if (m) { m->root = NULL; m->first = NULL; m->last = NULL; m->size = 0; }
}

CCTREAP_INLINE int cctreap_height(const cctreap_t *m) {
  if (!m || !m->size) return 0;
  size_t n = m->size;
  int h = 0;
  do { h++; } while (n >>= 1);
  return h * 2;
}

#endif /* CCTREAP_H */
