/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Intrusive singly-linked list with head + tail pointers.
**  Embed `cclink_node_t` in your struct.  No internal allocation.
**
**  ── Container-of ──
**
**    CCLINK_CONTAINER(ptr, type, member) — offsetof-based cast.
**
**  ── Usage ──
**
**    struct entry { int key; cclink_node_t node; };
**    cclink_t list; cclink_init(&list);
**    struct entry e = {42};
**    cclink_push(&list, &e.node);          // O(1) push front
**    cclink_push_back(&list, &e.node);     // O(1) push back (tail)
**    cclink_remove(&list, &e.node);        // O(n) removal
**
**    for (cclink_node_t *n = cclink_begin(&list); n != cclink_end(&list);
**         n = cclink_next(n)) {
**        struct entry *e = CCLINK_CONTAINER(n, struct entry, node);
**    }
*/
#ifndef CCLINK_H
#define CCLINK_H

#include <stddef.h>
#include <stdint.h>

/* ── branch hint ──────────────────────────────────────────────────────── */

#ifndef cclink_unlikely
  #if defined(__GNUC__) || defined(__clang__)
    #define cclink_unlikely(x) __builtin_expect(!!(x), 0)
  #else
    #define cclink_unlikely(x) (x)
  #endif
#endif

/* ── C89 bool shim ───────────────────────────────────────────────────── */

#if defined(__cplusplus)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
  #include <stdbool.h>
#else
  typedef int  bool;
  #define true  1
  #define false 0
#endif

/* ── portable inline ─────────────────────────────────────────────────── */

#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
  #define CCLINK_INLINE static inline
#elif defined(_MSC_VER)
  #define CCLINK_INLINE static __inline
#elif defined(__GNUC__)
  #define CCLINK_INLINE static __inline__
#else
  #define CCLINK_INLINE static
#endif

#if defined(__cplusplus) && __cplusplus >= 201703L
  #define CCLINK_NOEXCEPT noexcept
#else
  #define CCLINK_NOEXCEPT
#endif

#ifndef offsetof
  #define offsetof(type, member) \
      ((size_t) &(((type *)0)->member))
#endif

#ifndef container_of
  #define container_of(ptr, type, member) \
    ((type *)((uint8_t *)(ptr) - offsetof(type, member)))
#endif

#define CCLINK_CONTAINER container_of

#ifdef __cplusplus
extern "C" {
#endif

/* ── types ────────────────────────────────────────────────────────────── */

typedef struct cclink_node {
  struct cclink_node *next;
} cclink_node_t;

typedef struct cclink {
  cclink_node_t *head;
  cclink_node_t *tail;
  size_t         size;
} cclink_t;

/* ── internal ─────────────────────────────────────────────────────────── */

/* unlink node after prev (prev may be NULL -> head) */
CCLINK_INLINE void _cclink_unlink(cclink_t *l, cclink_node_t *prev,
                                   cclink_node_t *n) {
  if (prev) prev->next = n->next;
  else      l->head     = n->next;
  if (!n->next) l->tail = prev;   /* n was tail */
  l->size--;
  n->next = NULL;
}

/* find predecessor of n (NULL if n is head or absent) */
CCLINK_INLINE cclink_node_t *_cclink_prev(const cclink_t *l,
                                           const cclink_node_t *n) {
  if (!l || !n || l->head == n) return NULL;
  cclink_node_t *p = l->head;
  while (p && p->next != n) p = p->next;
  return p;
}

/* ── lifecycle ────────────────────────────────────────────────────────── */

CCLINK_INLINE void cclink_init(cclink_t *l) CCLINK_NOEXCEPT {
  if (!l) return;
  l->head = l->tail = NULL;
  l->size = 0;
}

CCLINK_INLINE void cclink_clear(cclink_t *l) CCLINK_NOEXCEPT { cclink_init(l); }

/* ── push (O(1) at head) ──────────────────────────────────────────────── */

CCLINK_INLINE void cclink_push(cclink_t *l, cclink_node_t *n) CCLINK_NOEXCEPT {
  if (cclink_unlikely(!l || !n)) return;
  n->next = l->head;
  l->head = n;
  if (!l->tail) l->tail = n;   /* was empty */
  l->size++;
}

/* ── push_back (O(1)) ────────────────────────────────────────────────── */

CCLINK_INLINE void cclink_push_back(cclink_t *l, cclink_node_t *n) CCLINK_NOEXCEPT {
  if (cclink_unlikely(!l || !n)) return;
  n->next = NULL;
  if (l->tail) { l->tail->next = n; l->tail = n; }
  else         { l->head = l->tail = n; }
  l->size++;
}

/* ── remove (O(n)) ────────────────────────────────────────────────────── */

CCLINK_INLINE void cclink_remove(cclink_t *l, cclink_node_t *n) CCLINK_NOEXCEPT {
  if (cclink_unlikely(!l || !n)) return;
  cclink_node_t *prev = _cclink_prev(l, n);
  if (!prev && l->head != n) return;  /* not in list */
  _cclink_unlink(l, prev, n);
}

/* remove and return head (O(1)).  Returns NULL if list is empty. */
CCLINK_INLINE cclink_node_t *cclink_pop_front(cclink_t *l) CCLINK_NOEXCEPT {
  if (cclink_unlikely(!l || !l->head)) return NULL;
  cclink_node_t *n = l->head;
  _cclink_unlink(l, NULL, n);
  return n;
}

/* ── iteration ────────────────────────────────────────────────────────── */

CCLINK_INLINE cclink_node_t *cclink_begin(const cclink_t *l) CCLINK_NOEXCEPT {
  return l ? l->head : NULL;
}
CCLINK_INLINE cclink_node_t *cclink_end(const cclink_t *l) CCLINK_NOEXCEPT {
  (void)l; return NULL;
}
CCLINK_INLINE cclink_node_t *cclink_next(const cclink_node_t *n) CCLINK_NOEXCEPT {
  return n ? n->next : NULL;
}
CCLINK_INLINE cclink_node_t *cclink_front(const cclink_t *l) CCLINK_NOEXCEPT {
  return cclink_begin(l);
}
CCLINK_INLINE cclink_node_t *cclink_back(const cclink_t *l) CCLINK_NOEXCEPT {
  return l ? l->tail : NULL;
}

/* ── query ────────────────────────────────────────────────────────────── */

CCLINK_INLINE size_t cclink_size(const cclink_t *l) CCLINK_NOEXCEPT {
  return l ? l->size : 0;
}
CCLINK_INLINE bool cclink_empty(const cclink_t *l) CCLINK_NOEXCEPT {
  return !l || cclink_size(l) == 0;
}

/* ── debug ────────────────────────────────────────────────────────────── */

CCLINK_INLINE bool cclink_has_cycle(const cclink_t *l) CCLINK_NOEXCEPT {
  if (!l || !cclink_begin(l)) return false;
  const cclink_node_t *slow = cclink_begin(l);
  const cclink_node_t *fast = cclink_begin(l);
  while (fast && fast->next) {
    slow = slow->next;
    fast = fast->next->next;
    if (slow == fast) return true;
  }
  return false;
}

typedef enum {
  CCLINK_UNKNOWN    = -1,
  CCLINK_NOERROR    = 0,
  CCLINK_ISNULL     = 1,
  CCLINK_HASCYCLE   = 2,
  CCLINK_SIZEERROR  = 3,
  CCLINK_ERRMAX     = 4,
} cclink_ecode_t;

CCLINK_INLINE cclink_ecode_t cclink_verify(const cclink_t *l) CCLINK_NOEXCEPT {
  if (!l)                    return CCLINK_ISNULL;
  if (cclink_has_cycle(l))   return CCLINK_HASCYCLE;
  size_t count = 0;
  for (const cclink_node_t *n = l->head; n; n = n->next) count++;
  if (count != l->size)      return CCLINK_SIZEERROR;
  return CCLINK_NOERROR;
}

/* ── error → string ──────────────────────────────────────────────────── */

typedef struct cclink_error {
  cclink_ecode_t code;
  const char    *err;
} cclink_error_t;

static const cclink_error_t cclink_errors[] = {
  {CCLINK_UNKNOWN,   "Unknown error."},
  {CCLINK_NOERROR,   "No error."},
  {CCLINK_ISNULL,    "is nullptr."},
  {CCLINK_HASCYCLE,  "cycle detected."},
  {CCLINK_SIZEERROR, "size != actual count."},
};

CCLINK_INLINE const cclink_error_t *cclink_get_error(cclink_ecode_t code) CCLINK_NOEXCEPT {
  int idx = (int)code + 1;  /* CCLINK_UNKNOWN = -1 → index 0 */
  if (idx < 0 || (size_t)idx >= sizeof(cclink_errors) / sizeof(cclink_errors[0]))
    return NULL;
  return &cclink_errors[idx];
}

#ifdef __cplusplus
}
#endif

#endif /* CCLINK_H */
