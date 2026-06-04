/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Intrusive singly-linked list.  Embed `cclink_node_t` in your struct.
**  No internal allocation — caller owns all node memory.
**
**  Designed as a lightweight building block for chained data structures
**  (e.g. cchashmap bucket chains) but also usable standalone.
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
**    cclink_push(&list, &e.node);          // insert at head
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

#ifndef offsetof
  #define offsetof(type, member) \
      ((size_t) &(((type *)0)->member))
#endif

#ifndef container_of
  #define container_of(ptr, type, member) \
    ((type *)((uint8_t *)(ptr) - offsetof(type, member)))
#endif

#define CCLINK_CONTAINER container_of

/* ── types ────────────────────────────────────────────────────────────── */

#ifndef CCLINK_NODE_T
typedef struct cclink_node {
  struct cclink_node *next;
} cclink_node_t;
#define CCLINK_NODE_T
#endif

typedef struct cclink {
  cclink_node_t *head;
  size_t         size;
} cclink_t;

/* ── internal ─────────────────────────────────────────────────────────── */

/* unlink node after prev (prev may be NULL -> head) */
CCLINK_INLINE void _cclink_unlink(cclink_t *l, cclink_node_t *prev,
                                   cclink_node_t *n) {
  if (prev) prev->next = n->next;
  else      l->head     = n->next;
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

CCLINK_INLINE void cclink_init(cclink_t *l) {
  if (!l) return;
  l->head = NULL;
  l->size = 0;
}

CCLINK_INLINE void cclink_clear(cclink_t *l) { cclink_init(l); }

/* ── push (O(1) at head) ──────────────────────────────────────────────── */

CCLINK_INLINE void cclink_push(cclink_t *l, cclink_node_t *n) {
  if (!l || !n) return;
  n->next = l->head;
  l->head = n;
  l->size++;
}

/* ── push_back (O(n)) ─────────────────────────────────────────────────── */

CCLINK_INLINE void cclink_push_back(cclink_t *l, cclink_node_t *n) {
  if (!l || !n) return;
  n->next = NULL;
  if (!l->head) { l->head = n; l->size++; return; }
  cclink_node_t *p = l->head;
  while (p->next) p = p->next;
  p->next = n;
  l->size++;
}

/* ── remove (O(n)) ────────────────────────────────────────────────────── */

CCLINK_INLINE void cclink_remove(cclink_t *l, cclink_node_t *n) {
  if (!l || !n) return;
  cclink_node_t *prev = _cclink_prev(l, n);
  if (!prev && l->head != n) return;  /* not in list */
  _cclink_unlink(l, prev, n);
}

/* remove and return head (O(1)).  Returns NULL if list is empty. */
CCLINK_INLINE cclink_node_t *cclink_pop_front(cclink_t *l) {
  if (!l || !l->head) return NULL;
  cclink_node_t *n = l->head;
  _cclink_unlink(l, NULL, n);
  return n;
}

/* ── iteration ────────────────────────────────────────────────────────── */

CCLINK_INLINE cclink_node_t *cclink_begin(const cclink_t *l) {
  return l ? l->head : NULL;
}
CCLINK_INLINE cclink_node_t *cclink_end(const cclink_t *l) {
  (void)l; return NULL;
}
CCLINK_INLINE cclink_node_t *cclink_next(const cclink_node_t *n) {
  return n ? n->next : NULL;
}
CCLINK_INLINE cclink_node_t *cclink_front(const cclink_t *l) {
  return l ? l->head : NULL;
}

/* ── query ────────────────────────────────────────────────────────────── */

CCLINK_INLINE size_t cclink_size(const cclink_t *l) {
  return l ? l->size : 0;
}
CCLINK_INLINE bool cclink_empty(const cclink_t *l) {
  return !l || l->size == 0;
}

/* ── debug ────────────────────────────────────────────────────────────── */

CCLINK_INLINE bool cclink_has_cycle(const cclink_t *l) {
  if (!l || !l->head) return false;
  const cclink_node_t *slow = l->head;
  const cclink_node_t *fast = l->head;
  while (fast && fast->next) {
    slow = slow->next;
    fast = fast->next->next;
    if (slow == fast) return true;
  }
  return false;
}

typedef enum {
  CLINK_UNKNOWN    = -1,
  CLINK_NOERROR    = 0,
  CLINK_ISNULL     = 1,
  CLINK_HASCYCLE   = 2,
  CLINK_SIZEERROR  = 3,
  CLINK_ERRMAX     = 4,
} cclink_ecode_t;

CCLINK_INLINE cclink_ecode_t cclink_verify(const cclink_t *l) {
  if (!l)                    return CLINK_ISNULL;
  if (cclink_has_cycle(l))   return CLINK_HASCYCLE;
  size_t count = 0;
  for (const cclink_node_t *n = l->head; n; n = n->next) count++;
  if (count != l->size)      return CLINK_SIZEERROR;
  return CLINK_NOERROR;
}

#endif /* CCLINK_H */
