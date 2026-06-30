/*
**  LICENSE: BSD
**  Author: CandyMi (https://github.com/candymi)
**
**  Fuzz harness for cchashmap (intrusive chained hash map).
**
**  The fuzz data encodes a sequence of operations:
**    - SET   key   (insert or update)
**    - GET   key   (lookup and verify identity)
**    - DEL   key   (remove)
**    - CLEAR       (reset all buckets)
**
**  Invariants verified:
**    1. After SET, GET returns the correct node
**    2. After DEL, GET returns NULL for that key
**    3. size matches actual count
**    4. Duplicate SETs return the previous node via the `old` output
**    5. Resize during SET preserves all existing entries
**
**  Build (libFuzzer, requires clang):
**    clang -std=c99 -fsanitize=fuzzer,address -I../include \
**          fuzz_cchashmap.c -o fuzz_cchashmap
**
**  Build (standalone):
**    clang -std=c99 -DDXXR_STANDALONE -I../include \
**          fuzz_cchashmap.c -o fuzz_cchashmap
*/
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── pre-define node type so we can use it in struct entry ────────────── */
typedef struct cchashmap_node {
  struct cchashmap_node *next;
  uint64_t               hash;
} cchashmap_node_t;
#define CCHASHMAP_NODE_T

struct entry {
  int               key;
  cchashmap_node_t  node;
};

/* Macro-mode: direct hash/equal from key field (same as test_cchashmap.c) */
#define CCHASHMAP_HASH(n, seed)  ((uint64_t)container_of((n), struct entry, node)->key)
#define CCHASHMAP_EQUAL(a, b)    (container_of((a), struct entry, node)->key == \
                                   container_of((b), struct entry, node)->key)

#include "../include/cchashmap.h"

/* ── node pool ────────────────────────────────────────────────────────── */
#define POOL_SIZE 512

static struct entry pool[POOL_SIZE];
static int pool_inuse[POOL_SIZE];   /* 1 = currently in hash map */

/* ── count actual entries by walking all buckets ──────────────────────── */
static size_t count_entries(cchashmap_t *m) {
  if (!m->buckets) return 0;
  size_t cnt = 0;
  for (size_t i = 0; i < m->cap; i++) {
    for (cchashmap_node_t *n = m->buckets[i]; n; n = n->next) {
      cnt++;
    }
  }
  return cnt;
}

/* ── verify all invariants ────────────────────────────────────────────── */
static void verify_hashmap(cchashmap_t *m) {
  size_t sz = cchashmap_size(m);
  size_t actual = count_entries(m);
  if (sz != actual) __builtin_trap();   /* size mismatch */

  /* Verify every entry in the pool that we think is alive is findable */
  for (int i = 0; i < POOL_SIZE; i++) {
    if (pool_inuse[i]) {
      struct entry probe;
      probe.key = pool[i].key;
      cchashmap_node_t *found = cchashmap_get(m, &probe.node);
      if (found != &pool[i].node) __builtin_trap();   /* missing or wrong */
    }
  }
}

/* ── fuzz entry point ─────────────────────────────────────────────────── */
int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  if (Size == 0) return 0;

  memset(pool, 0, sizeof(pool));
  memset(pool_inuse, 0, sizeof(pool_inuse));

  cchashmap_t m;
  cchashmap_init(&m, NULL, NULL);

  size_t pos = 0;
  size_t max_ops = 4096;
  size_t ops = 0;

  while (pos + 2 <= Size && ops < max_ops) {
    uint8_t opcode = Data[pos++];
    uint8_t key_byte = Data[pos++];
    int key = (int)key_byte;   /* 0..255 */
    ops++;

    switch (opcode & 7) {

    case 0: /* SET */
    case 1:
    case 2:
    case 3: {
      /* Find a free pool slot */
      int slot = -1;
      for (int i = 0; i < POOL_SIZE; i++) {
        if (!pool_inuse[i]) { slot = i; break; }
      }
      if (slot < 0) break;   /* pool exhausted */

      /* Check if this key is already in the map — if so, skip
       * (duplicate set is allowed but would return the existing node) */
      struct entry probe;
      probe.key = key;
      cchashmap_node_t *existing = cchashmap_get(&m, &probe.node);
      if (existing) {
        /* Key exists — the SET would be a no-op (duplicate).
         * We still call it to exercise the duplicate path,
         * but don't allocate a new slot. */
        cchashmap_node_t *old = NULL;
        (void)cchashmap_set(&m, &pool[slot].node, &old);
        /* The set should have failed (duplicate key),
         * and old should point to the existing node */
        if (old != existing) __builtin_trap();
        pool_inuse[slot] = 0;   /* slot not actually used */
        break;
      }

      /* Insert new key */
      pool[slot].key = key;
      pool_inuse[slot] = 1;

      cchashmap_node_t *old = NULL;
      bool inserted = cchashmap_set(&m, &pool[slot].node, &old);
      if (!inserted) {
        /* Set returned false — shouldn't happen since we checked above.
         * Could be OOM from resize.  Clean up. */
        pool_inuse[slot] = 0;
      } else if (old != NULL) {
        /* Should not happen: old should be NULL for a new key */
        __builtin_trap();
      }

      verify_hashmap(&m);
      break;
    }

    case 4: /* GET */
    case 5: {
      struct entry probe;
      probe.key = key;
      cchashmap_node_t *found = cchashmap_get(&m, &probe.node);

      if (found) {
        struct entry *e = (struct entry *)((uint8_t *)found -
            offsetof(struct entry, node));
        /* Check it's in the pool and marked alive */
        int idx = (int)(e - pool);
        if (idx < 0 || idx >= POOL_SIZE) __builtin_trap();
        if (!pool_inuse[idx]) __builtin_trap();      /* ghost entry */
      }
      break;
    }

    case 6: /* DEL */
    {
      struct entry probe;
      probe.key = key;
      cchashmap_node_t *found = cchashmap_get(&m, &probe.node);

      if (found) {
        struct entry *e = (struct entry *)((uint8_t *)found -
            offsetof(struct entry, node));
        int idx = (int)(e - pool);
        if (idx >= 0 && idx < POOL_SIZE) {
          cchashmap_del(&m, found);
          pool_inuse[idx] = 0;

          /* Verify it's gone */
          if (cchashmap_get(&m, &probe.node) != NULL) __builtin_trap();
        }
      }
      verify_hashmap(&m);
      break;
    }

    case 7: /* CLEAR */
      cchashmap_clear(&m);
      memset(pool_inuse, 0, sizeof(pool_inuse));
      verify_hashmap(&m);
      break;
    }
  }

  /* Final verify */
  verify_hashmap(&m);
  cchashmap_destroy(&m);

  return 0;
}

/* ── standalone entry point ───────────────────────────────────────────── */
#ifdef DXXR_STANDALONE
int main(void) {
  uint8_t buf[8192];
  ssize_t nread;
  while ((nread = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
    LLVMFuzzerTestOneInput(buf, (size_t)nread);
  }
  return 0;
}
#endif
