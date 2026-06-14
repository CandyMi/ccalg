/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Intrusive doubly-linked list.  Embed `cclist_node_t` in your struct.
**  No internal allocation — caller owns all node memory.
**
**  ── Container-of ──
**
**    CCLIST_CONTAINER(ptr, type, member) — offsetof-based cast.
*/
#ifndef CCLIST_H
#define CCLIST_H

#include <stddef.h>
#include <stdint.h>

/* ── branch hint ──────────────────────────────────────────────────────── */

#ifndef cclist_unlikely
  #if defined(__GNUC__) || defined(__clang__)
    #define cclist_unlikely(x) __builtin_expect(!!(x), 0)
  #else
    #define cclist_unlikely(x) (x)
  #endif
#endif

#if defined(__cplusplus)
  /* C++ has built-in bool */
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
  #include <stdbool.h>
#else
  typedef int  bool;
  #define true  1
  #define false 0
#endif

#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
  #define CCLIST_INLINE static inline
#elif defined(_MSC_VER)
  #define CCLIST_INLINE static __inline
#elif defined(__GNUC__)
  #define CCLIST_INLINE static __inline__
#else
  #define CCLIST_INLINE static
#endif

#ifndef offsetof
  #define offsetof(type, member) \
      ((size_t) &(((type *)0)->member))
#endif

#ifndef container_of
  #define container_of(ptr, type, member) \
    ((type *)((uint8_t *)(ptr) - offsetof(type, member)))
#endif

#define CCLIST_CONTAINER container_of

/* ── types ────────────────────────────────────────────────────────────── */

typedef struct cclist_node {
  struct cclist_node *next;
  struct cclist_node *prev;
} cclist_node_t;

typedef struct cclist {
  cclist_node_t *head;
  cclist_node_t *tail;
  size_t         size;
} cclist_t;

/* ── internals (not part of public API) ────────────────────────────────── */

/* Link n between prev and next. prev/next may be NULL (end of list). */
CCLIST_INLINE void _cclist_link(cclist_t *l, cclist_node_t *n,
                                 cclist_node_t *prev, cclist_node_t *next) {
  n->prev = prev;
  n->next = next;
  if (prev) prev->next = n; else l->head = n;
  if (next) next->prev = n; else l->tail = n;
  l->size++;
}

/* Unlink n from list and clear its pointers. */
CCLIST_INLINE void _cclist_unlink(cclist_t *l, cclist_node_t *n) {
  if (n->prev) n->prev->next = n->next; else l->head = n->next;
  if (n->next) n->next->prev = n->prev; else l->tail = n->prev;
  l->size--;
  n->prev = n->next = NULL;
}

/* ── read-only API ────────────────────────────────────────────────────── */

CCLIST_INLINE void cclist_init(cclist_t *l) {
  if (!l) return;
  l->head = l->tail = NULL;
  l->size = 0;
}

CCLIST_INLINE size_t cclist_size(const cclist_t *l) { return l ? l->size : 0; }
CCLIST_INLINE bool   cclist_empty(const cclist_t *l) { return !l || l->size == 0; }

CCLIST_INLINE cclist_node_t *cclist_begin(const cclist_t *l)  { return l ? l->head : NULL; }
CCLIST_INLINE cclist_node_t *cclist_end(const cclist_t *l)    { (void)l; return NULL; }
CCLIST_INLINE cclist_node_t *cclist_rbegin(const cclist_t *l) { return l ? l->tail : NULL; }

CCLIST_INLINE cclist_node_t *cclist_next(const cclist_node_t *n) { return n ? n->next : NULL; }
CCLIST_INLINE cclist_node_t *cclist_prev(const cclist_node_t *n) { return n ? n->prev : NULL; }

CCLIST_INLINE cclist_node_t *cclist_front(const cclist_t *l) { return l ? l->head : NULL; }
CCLIST_INLINE cclist_node_t *cclist_back(const cclist_t *l)  { return l ? l->tail : NULL; }

/* ── push / pop ───────────────────────────────────────────────────────── */

CCLIST_INLINE void cclist_push_front(cclist_t *l, cclist_node_t *n) {
  if (cclist_unlikely(!l || !n)) return;
  _cclist_link(l, n, NULL, l->head);
}

CCLIST_INLINE void cclist_push_back(cclist_t *l, cclist_node_t *n) {
  if (cclist_unlikely(!l || !n)) return;
  _cclist_link(l, n, l->tail, NULL);
}

CCLIST_INLINE cclist_node_t *cclist_pop_front(cclist_t *l) {
  if (cclist_unlikely(!l || !l->head)) return NULL;
  cclist_node_t *n = l->head;
  _cclist_unlink(l, n);
  return n;
}

CCLIST_INLINE cclist_node_t *cclist_pop_back(cclist_t *l) {
  if (cclist_unlikely(!l || !l->tail)) return NULL;
  cclist_node_t *n = l->tail;
  _cclist_unlink(l, n);
  return n;
}

/* ── insert / remove ──────────────────────────────────────────────────── */

CCLIST_INLINE void cclist_insert_before(cclist_t *l, cclist_node_t *pos, cclist_node_t *n) {
  if (cclist_unlikely(!l || !pos || !n)) return;
  _cclist_link(l, n, pos->prev, pos);
}

CCLIST_INLINE void cclist_insert_after(cclist_t *l, cclist_node_t *pos, cclist_node_t *n) {
  if (cclist_unlikely(!l || !pos || !n)) return;
  _cclist_link(l, n, pos, pos->next);
}

CCLIST_INLINE void cclist_remove(cclist_t *l, cclist_node_t *n) {
  if (cclist_unlikely(!l || !n)) return;
  _cclist_unlink(l, n);
}

/* ── splice / move ────────────────────────────────────────────────────── */

/* Move all nodes from src to the end of dst. src becomes empty. */
CCLIST_INLINE void cclist_splice_back(cclist_t *dst, cclist_t *src) {
  if (!dst || !src || cclist_empty(src)) return;
  if (cclist_empty(dst)) {
    dst->head = src->head;
    dst->tail = src->tail;
  } else {
    dst->tail->next = src->head;
    src->head->prev = dst->tail;
    dst->tail = src->tail;
  }
  dst->size += src->size;
  cclist_init(src);
}

/* Transfer all nodes from `from` to the end of `to`, then reset `from`.
   Returns 0 on success, -1 if either list is NULL, -2 if `from` is empty,
   -3 if `to == from` (self-move). */
CCLIST_INLINE int cclist_move(cclist_t *to, cclist_t *from) {
  if (!to || !from)       return -1;
  if (to == from)         return -3;
  if (cclist_empty(from)) return -2;
  cclist_splice_back(to, from);
  return 0;
}

/* ── clear ────────────────────────────────────────────────────────────── */

/* Reset list to empty. Does NOT free nodes — caller owns memory. */
CCLIST_INLINE void cclist_clear(cclist_t *l) { cclist_init(l); }

/* ── debug ────────────────────────────────────────────────────────────── */

/* Floyd's algorithm — O(N) time, O(1) space. */
CCLIST_INLINE bool cclist_has_cycle(const cclist_t *l) {
  if (!l || !l->head) return false;
  cclist_node_t *slow = l->head;
  cclist_node_t *fast = l->head;
  while (fast && fast->next) {
    slow = slow->next;
    fast = fast->next->next;
    if (slow == fast) return true;
  }
  return false;
}

typedef enum {
  CCLIST_UNKNOWN = -1,
  CCLIST_NOERROR,
  CCLIST_ISNULL,
  CCLIST_NOHEAD,
  CCLIST_NONEXT,
  CCLIST_HASCYCLE,
  CCLIST_MISSPREV,
  CCLIST_SIZEERROR,
  /* ERROR MAX VALUE */
  CCLIST_ERRMAX,
} cclist_ecode_t ;

typedef struct cclist_error {
  cclist_ecode_t code;
  const char *err;
} cclist_error_t;

static const cclist_error_t cclist_errors[] = {
  {CCLIST_NOERROR,     "No Error."},
  {CCLIST_ISNULL,      "it's nullptr."},
  {CCLIST_NOHEAD,      "head->prev != NULL"},
  {CCLIST_NONEXT,      "tail->next != NULL"},
  {CCLIST_HASCYCLE,    "cycle detected"},
  {CCLIST_MISSPREV,    "prev-link mismatch (n->prev != previous node)"},
  {CCLIST_SIZEERROR,   "size != actual count"},
};

CCLIST_INLINE const cclist_error_t *cclist_get_error(cclist_ecode_t code) {
  if (code <= CCLIST_UNKNOWN || code >= CCLIST_ERRMAX) return NULL;
  return &cclist_errors[code];
}

/* Verify doubly-linked invariants. Returns 0 on success, or an
   error code for the first violation:
     1 — NULL l, or head/tail mismatch (one set, the other not)
     2 — head->prev != NULL
     3 — tail->next != NULL
     4 — cycle detected
     5 — prev-link mismatch (n->prev != previous node)
     6 — size != actual count                          */
CCLIST_INLINE cclist_ecode_t cclist_verify(const cclist_t *l) {
  if (!l) return CCLIST_ISNULL;
  if (!l->head != !l->tail) return CCLIST_ISNULL;  /* one NULL, the other not */
  if (l->head && l->head->prev) return CCLIST_NOHEAD;
  if (l->tail && l->tail->next) return CCLIST_NONEXT;
  if (cclist_has_cycle(l))     return CCLIST_HASCYCLE;
  size_t count = 0;
  const cclist_node_t *prev = NULL;
  for (const cclist_node_t *n = l->head; n; prev = n, n = n->next) {
    if (n->prev != prev) return CCLIST_MISSPREV;
    count++;
  }
  if (count != l->size) return CCLIST_SIZEERROR;
  return CCLIST_NOERROR;
}

#endif /* CCLIST_H */
