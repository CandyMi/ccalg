/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Intrusive red-black tree.  Embed `ccmap_node_t` in your struct.
**  No internal allocation — caller owns all node memory.
**  Color stored in parent pointer's low bit → 16 bytes per node (64-bit).
**
**  ── Comparison (choose one) ──
**
**    Default:   pass a `ccmap_cmp_t` function pointer to ccmap_init().
**    Optional:  define CCMAP_COMPARE(a, b) as a macro before #include
**               for inlined comparisons.
**
**  ── Container-of ──
**
**    CCMAP_CONTAINER(ptr, type, member) — offsetof-based cast.
*/
#ifndef CCMAP_H
#define CCMAP_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ── branch hint ──────────────────────────────────────────────────────── */

#ifndef ccmap_unlikely
  #if defined(__GNUC__) || defined(__clang__)
    #define ccmap_unlikely(x) __builtin_expect(!!(x), 0)
  #else
    #define ccmap_unlikely(x) (x)
  #endif
#endif

#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
  #define CCMAP_INLINE static inline
#elif defined(_MSC_VER)
  #define CCMAP_INLINE static __inline
#elif defined(__GNUC__)
  #define CCMAP_INLINE static __inline__
#else
  #define CCMAP_INLINE static
#endif

#ifndef offsetof
  #define offsetof(type, member) \
      ((size_t) &(((type *)0)->member))
#endif

#ifndef container_of
  #define container_of(ptr, type, member) \
    ((type *)((uint8_t *)(ptr) - offsetof(type, member)))
#endif

#define CCMAP_CONTAINER container_of

#define CCMAP_RED   0
#define CCMAP_BLACK 1
#define CCMAP_LEFT  0
#define CCMAP_RIGHT 1

#ifdef CCMAP_COMPARE
  #define CCMAP_CMP(a, b) CCMAP_COMPARE((a), (b))
#else
  #define CCMAP_CMP(a, b) (cmp_fn((a), (b)))
#endif

/* ── packed parent+color ──────────────────────────────────────────────── */

#ifndef CCMAP_NODE_T
typedef struct ccmap_node {
  struct ccmap_node *child[2];  /* [CCMAP_LEFT]=left, [CCMAP_RIGHT]=right */
  uintptr_t                pc;  /* parent pointer | color (low bit) */
} ccmap_node_t;
#define CCMAP_NODE_T
#endif

/* accessors as macros — zero-overhead, compiler sees through to pc field */
#define _rb_p(n)      ((ccmap_node_t *)((n)->pc & ~(uintptr_t)1))
#define _rb_c(n)      ((int)((n)->pc & 1))
#define _rb_red(n)    ((n) && _rb_c(n) == CCMAP_RED)
#define _rb_sp(n, p)  ((n)->pc = ((n)->pc & 1) | (uintptr_t)(p))
#define _rb_sc(n, c)  ((n)->pc = ((n)->pc & ~(uintptr_t)1) | (uintptr_t)(c))
#define _rb_spc(n, p, c) ((n)->pc = (uintptr_t)(p) | (uintptr_t)(c))

CCMAP_INLINE ccmap_node_t *_rb_min(ccmap_node_t *x) { while (x->child[CCMAP_LEFT]) x = x->child[CCMAP_LEFT]; return x; }

/* ── forward-declare map_t for internal helpers ───────────────────────── */

typedef int64_t (*ccmap_cmp_t)(const ccmap_node_t *a, const ccmap_node_t *b);

typedef struct map {
  ccmap_node_t    *root;
  ccmap_node_t    *first;
  size_t           size;
#ifndef CCMAP_COMPARE
  ccmap_cmp_t      cmp;
#endif
} ccmap_t;

/* ── rotations ────────────────────────────────────────────────────────── */

CCMAP_INLINE void _rb_rot_left(ccmap_t *m, ccmap_node_t *x) {
  ccmap_node_t *y  = x->child[CCMAP_RIGHT];
  ccmap_node_t *xp = _rb_p(x);
  x->child[CCMAP_RIGHT] = y->child[CCMAP_LEFT];
  if (y->child[CCMAP_LEFT]) _rb_sp(y->child[CCMAP_LEFT], x);
  _rb_sp(y, xp);
  if (!xp)      m->root = y;
  else if (x == xp->child[CCMAP_LEFT]) xp->child[CCMAP_LEFT] = y;
  else                                 xp->child[CCMAP_RIGHT] = y;
  y->child[CCMAP_LEFT] = x; _rb_sp(x, y);
}

CCMAP_INLINE void _rb_rot_right(ccmap_t *m, ccmap_node_t *x) {
  ccmap_node_t *y  = x->child[CCMAP_LEFT];
  ccmap_node_t *xp = _rb_p(x);
  x->child[CCMAP_LEFT] = y->child[CCMAP_RIGHT];
  if (y->child[CCMAP_RIGHT]) _rb_sp(y->child[CCMAP_RIGHT], x);
  _rb_sp(y, xp);
  if (!xp)      m->root = y;
  else if (x == xp->child[CCMAP_RIGHT]) xp->child[CCMAP_RIGHT] = y;
  else                                  xp->child[CCMAP_LEFT] = y;
  y->child[CCMAP_RIGHT] = x; _rb_sp(x, y);
}

/* ── insert fixup ─────────────────────────────────────────────────────── */

CCMAP_INLINE void _rb_ins_fix(ccmap_t *m, ccmap_node_t *z) {
  ccmap_node_t *p = _rb_p(z);
  while (_rb_red(p)) {
    ccmap_node_t *g = _rb_p(p);
    if (p == g->child[CCMAP_LEFT]) {
      ccmap_node_t *u = g->child[CCMAP_RIGHT];
      if (_rb_red(u)) {
        _rb_sc(p, CCMAP_BLACK); _rb_sc(u, CCMAP_BLACK); _rb_sc(g, CCMAP_RED);
        z = g; p = _rb_p(g);
      } else {
        if (z == p->child[CCMAP_RIGHT]) { _rb_rot_left(m, p); z = p; p = _rb_p(z); }
        _rb_sc(p, CCMAP_BLACK); _rb_sc(g, CCMAP_RED); _rb_rot_right(m, g);
        break;
      }
    } else {
      ccmap_node_t *u = g->child[CCMAP_LEFT];
      if (_rb_red(u)) {
        _rb_sc(p, CCMAP_BLACK); _rb_sc(u, CCMAP_BLACK); _rb_sc(g, CCMAP_RED);
        z = g; p = _rb_p(g);
      } else {
        if (z == p->child[CCMAP_LEFT]) { _rb_rot_right(m, p); z = p; p = _rb_p(z); }
        _rb_sc(p, CCMAP_BLACK); _rb_sc(g, CCMAP_RED); _rb_rot_left(m, g);
        break;
      }
    }
  }
  _rb_sc(m->root, CCMAP_BLACK);
}

/* ── delete fixup ─────────────────────────────────────────────────────── */

CCMAP_INLINE void _rb_del_fix(ccmap_t *m, ccmap_node_t *x, ccmap_node_t *xp) {
  while (x != m->root && !_rb_red(x)) {
    if (xp->child[CCMAP_LEFT] == x) {
      ccmap_node_t *w = xp->child[CCMAP_RIGHT];
      if (_rb_red(w)) {
        _rb_sc(w, CCMAP_BLACK); _rb_sc(xp, CCMAP_RED); _rb_rot_left(m, xp); w = xp->child[CCMAP_RIGHT];
      }
      if (!_rb_red(w->child[CCMAP_LEFT]) && !_rb_red(w->child[CCMAP_RIGHT])) {
        _rb_sc(w, CCMAP_RED); x = xp; xp = _rb_p(xp);
      } else {
        if (!_rb_red(w->child[CCMAP_RIGHT])) {
          if (w->child[CCMAP_LEFT]) _rb_sc(w->child[CCMAP_LEFT], CCMAP_BLACK);
          _rb_sc(w, CCMAP_RED); _rb_rot_right(m, w); w = xp->child[CCMAP_RIGHT];
        }
        _rb_sc(w, _rb_c(xp)); _rb_sc(xp, CCMAP_BLACK);
        if (w->child[CCMAP_RIGHT]) _rb_sc(w->child[CCMAP_RIGHT], CCMAP_BLACK);
        _rb_rot_left(m, xp); x = m->root;
      }
    } else {
      ccmap_node_t *w = xp->child[CCMAP_LEFT];
      if (_rb_red(w)) {
        _rb_sc(w, CCMAP_BLACK); _rb_sc(xp, CCMAP_RED); _rb_rot_right(m, xp); w = xp->child[CCMAP_LEFT];
      }
      if (!_rb_red(w->child[CCMAP_RIGHT]) && !_rb_red(w->child[CCMAP_LEFT])) {
        _rb_sc(w, CCMAP_RED); x = xp; xp = _rb_p(xp);
      } else {
        if (!_rb_red(w->child[CCMAP_LEFT])) {
          if (w->child[CCMAP_RIGHT]) _rb_sc(w->child[CCMAP_RIGHT], CCMAP_BLACK);
          _rb_sc(w, CCMAP_RED); _rb_rot_left(m, w); w = xp->child[CCMAP_LEFT];
        }
        _rb_sc(w, _rb_c(xp)); _rb_sc(xp, CCMAP_BLACK);
        if (w->child[CCMAP_LEFT]) _rb_sc(w->child[CCMAP_LEFT], CCMAP_BLACK);
        _rb_rot_right(m, xp); x = m->root;
      }
    }
  }
  if (x) _rb_sc(x, CCMAP_BLACK);
}

/* ── public API ───────────────────────────────────────────────────────── */

CCMAP_INLINE void ccmap_init(ccmap_t *m, ccmap_cmp_t cmp) {
  if (ccmap_unlikely(!m)) return;
  m->root  = NULL;
  m->first = NULL;
  m->size  = 0;
#ifndef CCMAP_COMPARE
  m->cmp = cmp;
#else
  (void)cmp;
#endif
}

CCMAP_INLINE int ccmap_insert(ccmap_t *m, ccmap_node_t *node, ccmap_node_t **out) {
  if (ccmap_unlikely(!m || !node)) return -1;
  node->child[CCMAP_LEFT] = node->child[CCMAP_RIGHT] = NULL;
  _rb_spc(node, NULL, CCMAP_RED);
#ifndef CCMAP_COMPARE
  ccmap_cmp_t cmp_fn = m->cmp;
#endif
  /* fast path: smaller than current minimum → insert as new first */
  if (m->first && CCMAP_CMP(node, m->first) < 0) {
    m->first->child[CCMAP_LEFT] = node;
    _rb_sp(node, m->first);
    _rb_ins_fix(m, node);
    m->first = node;
    m->size++;
    if (out) *out = node;
    return 0;
  }

  /* normal BST insert from root */
  ccmap_node_t *y = NULL, *x = m->root;
  while (x) {
    y = x;
    int64_t c = CCMAP_CMP(node, x);
    if (c == 0) { if (out) *out = x; return -1; }
    x = x->child[c > 0];
  }
  _rb_sp(node, y);
  if (!y) m->root = node;
  else {
    int64_t c = CCMAP_CMP(node, y);
    y->child[c > 0] = node;
  }

  _rb_ins_fix(m, node);
  m->size++;
  if (!m->first || CCMAP_CMP(node, m->first) < 0)
    m->first = node;
  if (out) *out = node;
  return 0;
}

CCMAP_INLINE ccmap_node_t *ccmap_find(ccmap_t *m, const ccmap_node_t *probe) {
  if (ccmap_unlikely(!m || !probe)) return NULL;
  ccmap_node_t *x = m->root;
#ifndef CCMAP_COMPARE
  ccmap_cmp_t cmp_fn = m->cmp;
#endif
  while (x) {
    int64_t c = CCMAP_CMP(probe, x);
    if (c == 0) return x;
    x = x->child[c > 0];
  }
  return NULL;
}

CCMAP_INLINE void ccmap_erase(ccmap_t *m, ccmap_node_t *z) {
  if (ccmap_unlikely(!m || !z)) return;
  ccmap_node_t *y = z, *x = NULL, *xp = NULL;
  int yc = _rb_c(y);

  if (!z->child[CCMAP_LEFT]) {
    x = z->child[CCMAP_RIGHT]; xp = _rb_p(z);
    if (x) _rb_sp(x, xp);
    if (!xp) m->root = x; else if (xp->child[CCMAP_LEFT] == z) xp->child[CCMAP_LEFT] = x; else xp->child[CCMAP_RIGHT] = x;
  } else if (!z->child[CCMAP_RIGHT]) {
    x = z->child[CCMAP_LEFT]; xp = _rb_p(z);
    if (x) _rb_sp(x, xp);
    if (!xp) m->root = x; else if (xp->child[CCMAP_LEFT] == z) xp->child[CCMAP_LEFT] = x; else xp->child[CCMAP_RIGHT] = x;
  } else {
    y = _rb_min(z->child[CCMAP_RIGHT]); yc = _rb_c(y); x = y->child[CCMAP_RIGHT];
    if (_rb_p(y) == z) { xp = y; if (x) _rb_sp(x, y); }
    else { xp = _rb_p(y); if (x) _rb_sp(x, xp); xp->child[CCMAP_LEFT] = x;
           y->child[CCMAP_RIGHT] = z->child[CCMAP_RIGHT]; _rb_sp(z->child[CCMAP_RIGHT], y); }
    if (!_rb_p(z)) m->root = y;
    else if (_rb_p(z)->child[CCMAP_LEFT] == z) _rb_p(z)->child[CCMAP_LEFT] = y; else _rb_p(z)->child[CCMAP_RIGHT] = y;
    _rb_sp(y, _rb_p(z)); y->child[CCMAP_LEFT] = z->child[CCMAP_LEFT]; _rb_sp(z->child[CCMAP_LEFT], y); _rb_sc(y, _rb_c(z));
  }
  if (z == m->first) {
    if (z->child[CCMAP_RIGHT]) { m->first = z->child[CCMAP_RIGHT]; while (m->first->child[CCMAP_LEFT]) m->first = m->first->child[CCMAP_LEFT]; }
    else { ccmap_node_t *p = _rb_p(z); while (p && z == p->child[CCMAP_RIGHT]) { z = p; p = _rb_p(p); } m->first = p; }
  }
  m->size--;
  if (yc == CCMAP_BLACK) _rb_del_fix(m, x, xp);
}

CCMAP_INLINE ccmap_node_t *ccmap_first(ccmap_t *m)  { return m ? m->first : NULL; }
CCMAP_INLINE ccmap_node_t *ccmap_begin(ccmap_t *m)   { return m ? m->first : NULL; }
CCMAP_INLINE ccmap_node_t *ccmap_end(ccmap_t *m)     { (void)m; return NULL; }
CCMAP_INLINE ccmap_node_t *ccmap_rbegin(ccmap_t *m)  { if (!m || !m->root) return NULL; ccmap_node_t *x = m->root; while (x->child[CCMAP_RIGHT]) x = x->child[CCMAP_RIGHT]; return x; }

CCMAP_INLINE ccmap_node_t *ccmap_next(ccmap_node_t *x) {
  if (!x) return NULL;
  if (x->child[CCMAP_RIGHT]) return _rb_min(x->child[CCMAP_RIGHT]);
  ccmap_node_t *p = _rb_p(x); while (p && x == p->child[CCMAP_RIGHT]) { x = p; p = _rb_p(p); } return p;
}
CCMAP_INLINE ccmap_node_t *ccmap_prev(ccmap_node_t *x) {
  if (!x) return NULL;
  if (x->child[CCMAP_LEFT]) { x = x->child[CCMAP_LEFT]; while (x->child[CCMAP_RIGHT]) x = x->child[CCMAP_RIGHT]; return x; }
  ccmap_node_t *p = _rb_p(x); while (p && x == p->child[CCMAP_LEFT]) { x = p; p = _rb_p(p); } return p;
}

CCMAP_INLINE size_t ccmap_size(ccmap_t *m) { return m ? m->size : 0; }
CCMAP_INLINE void   ccmap_clear(ccmap_t *m) { if (m) { m->root = NULL; m->first = NULL; m->size = 0; } }

#endif /* CCMAP_H */
