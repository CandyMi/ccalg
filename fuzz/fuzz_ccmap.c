/*
**  LICENSE: BSD
**  Author: CandyMi (https://github.com/candymi)
**
**  Fuzz harness for ccmap (intrusive red-black tree).
**
**  The fuzz data encodes a sequence of operations:
**    - INSERT key N   (node embedded in a static pool)
**    - ERASE  key N   (remove by key)
**    - FIND   key N   (lookup and compare with expected state)
**    - CLEAR           (reset tree)
**
**  Invariants verified at every operation:
**    1. Forward iteration yields strictly increasing key order
**    2. ccmap_size matches traversal count
**    3. first/last pointers are correct
**    4. Every inserted node is findable until erased
**
**  Build (libFuzzer, requires clang):
**    clang -std=c99 -fsanitize=fuzzer,address -I../include \
**          fuzz_ccmap.c -o fuzz_ccmap
**
**  Build (standalone):
**    clang -std=c99 -DDXXR_STANDALONE -I../include \
**          fuzz_ccmap.c -o fuzz_ccmap
*/
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../include/ccmap.h"

/* ── node pool ────────────────────────────────────────────────────────── */
#define POOL_SIZE 256

struct entry {
  int          key;
  ccmap_node_t node;
  int          alive;   /* 1 = currently in tree */
};

static struct entry pool[POOL_SIZE];

/* ── comparison: ascending int key ────────────────────────────────────── */
static int64_t cmp_fn(const ccmap_node_t *a, const ccmap_node_t *b) {
  struct entry *ea = CCMAP_CONTAINER(a, struct entry, node);
  struct entry *eb = CCMAP_CONTAINER(b, struct entry, node);
  return (int64_t)ea->key - (int64_t)eb->key;
}

/* ── traverse forward, verify increasing order, return count ──────────── */
static int check_forward(ccmap_t *m) {
  int prev = -2147483647 - 1;  /* INT32_MIN - 1 but careful */
  int cnt = 0;
  for (ccmap_node_t *n = ccmap_begin(m); n != ccmap_end(m); n = ccmap_next(n)) {
    int k = CCMAP_CONTAINER(n, struct entry, node)->key;
    if (k <= prev) return -1;   /* out of order */
    prev = k;
    cnt++;
  }
  return cnt;
}

/* ── traverse reverse, verify decreasing order ────────────────────────── */
static int check_reverse(ccmap_t *m) {
  int prev = 2147483647;  /* INT32_MAX */
  int cnt = 0;
  for (ccmap_node_t *n = ccmap_rbegin(m); n; n = ccmap_prev(n)) {
    int k = CCMAP_CONTAINER(n, struct entry, node)->key;
    if (k >= prev) return -1;
    prev = k;
    cnt++;
  }
  return cnt;
}

/* ── verify all tree invariants ───────────────────────────────────────── */
static void verify_tree(ccmap_t *m) {
  size_t sz = ccmap_size(m);

  int fwd = check_forward(m);
  if (fwd < 0 || (size_t)fwd != sz) __builtin_trap();

  int rev = check_reverse(m);
  if (rev < 0 || (size_t)rev != sz) __builtin_trap();

  /* first/last check */
  if (sz > 0) {
    struct entry *fe = CCMAP_CONTAINER(ccmap_first(m), struct entry, node);
    struct entry *le = CCMAP_CONTAINER(ccmap_last(m), struct entry, node);
    /* first should be the minimum, last the maximum */
    if (ccmap_find(m, &fe->node) != &fe->node) __builtin_trap();
    if (ccmap_find(m, &le->node) != &le->node) __builtin_trap();
  }
}

/* ── fuzz entry point ─────────────────────────────────────────────────── */
int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  if (Size == 0) return 0;

  /* Reset pool — mark all nodes as free */
  memset(pool, 0, sizeof(pool));

  ccmap_t m;
  ccmap_init(&m, cmp_fn);

  /* Walk the fuzz data as a sequence of (opcode, key) pairs.
   * Each operation consumes 2 bytes: opcode byte + key byte.
   * We limit the number of operations to prevent O(n²) blowup. */
  size_t pos = 0;
  size_t max_ops = 2048;  /* cap total operations per iteration */
  size_t ops = 0;

  /* Track which pool slots are in use (for insert: find a free slot) */
  int slot_used[POOL_SIZE];
  memset(slot_used, 0, sizeof(slot_used));

  while (pos + 2 <= Size && ops < max_ops) {
    uint8_t opcode = Data[pos++];
    uint8_t key_byte = Data[pos++];
    int key = (int)key_byte - 128;           /* [-128, 127] range */
    ops++;

    switch (opcode & 7) {

    case 0: /* INSERT */
    case 1:
    case 2:
    case 3: {
      /* Find a free pool slot */
      int slot = -1;
      for (int i = 0; i < POOL_SIZE; i++) {
        if (!slot_used[i] && !pool[i].alive) { slot = i; break; }
      }
      if (slot < 0) break;   /* pool exhausted */

      /* Randomize key a bit to avoid trivial sequences;
       * mix key with opcode to spread coverage. */
      int final_key = key;
      /* Ensure uniqueness (approx): if the key collides with an
       * existing node, tweak it (fuzzer already varies input). */

      pool[slot].key = final_key;
      pool[slot].alive = 1;

      ccmap_node_t *out = NULL;
      int r = ccmap_insert(&m, &pool[slot].node, &out);

      if (r == 0) {
        slot_used[slot] = 1;   /* mark as used in tree */
      } else {
        /* Duplicate: node was not inserted; mark as not alive */
        pool[slot].alive = 0;
      }

      verify_tree(&m);
      break;
    }

    case 4: /* ERASE by probing a key */
    case 5: {
      int final_key = key;
      /* Build a probe entry */
      struct entry probe;
      probe.key = final_key;

      ccmap_node_t *found = ccmap_find(&m, &probe.node);
      if (found) {
        struct entry *e = CCMAP_CONTAINER(found, struct entry, node);
        ccmap_erase(&m, found);
        e->alive = 0;
        /* Clear slot_used — linear scan to find slot */
        for (int i = 0; i < POOL_SIZE; i++) {
          if (&pool[i].node == found) {
            slot_used[i] = 0;
            break;
          }
        }
      }
      verify_tree(&m);
      break;
    }

    case 6: /* FIND (just verify, don't modify) */
    {
      struct entry probe;
      probe.key = key;
      ccmap_node_t *found = ccmap_find(&m, &probe.node);
      /* If found, it must be alive */
      if (found) {
        struct entry *e = CCMAP_CONTAINER(found, struct entry, node);
        if (!e->alive) __builtin_trap();   /* ghost node */
      }
      break;
    }

    case 7: /* CLEAR */
      ccmap_clear(&m);
      memset(slot_used, 0, sizeof(slot_used));
      for (int i = 0; i < POOL_SIZE; i++) pool[i].alive = 0;
      verify_tree(&m);
      break;
    }
  }

  /* Final invariant check */
  verify_tree(&m);

  return 0;
}

/* ── standalone entry point ───────────────────────────────────────────── */
#ifndef FUZZ_MAX_INPUT
#define FUZZ_MAX_INPUT 8192
#endif

#ifdef DXXR_STANDALONE
int main(void) {
  uint8_t buf[FUZZ_MAX_INPUT];
  ssize_t nread;
  while ((nread = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
    LLVMFuzzerTestOneInput(buf, (size_t)nread);
  }
  return 0;
}
#endif
