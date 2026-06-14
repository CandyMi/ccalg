/*
**  Test suite for ccmap1 (intrusive Weak AVL / rank-balanced tree)
**  Build:  gcc -std=c99 -Wall -Wextra -o test_ccmap1 test_ccmap1.c && ./test_ccmap1
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../include/ccmap1.h"

/* ── test entry ───────────────────────────────────────────────────────── */
struct entry {
  int           key;
  ccmap1_node_t node;
};
static struct entry *E(const ccmap1_node_t *n) {
  return CCMAP1_CONTAINER(n, struct entry, node);
}

/* ── comparison: sort by int key ascending ───────────────────────────── */
static int64_t cmp_fn(const ccmap1_node_t *a, const ccmap1_node_t *b) {
  return (int64_t)E(a)->key - (int64_t)E(b)->key;
}

/* ── test harness ─────────────────────────────────────────────────────── */
static int passed, failed;
#define TEST(name) static void test_##name(void)
#define ASSERT(cond) do { \
  if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failed++; } \
  else passed++; \
} while(0)
#define RUN(name) do { printf("  %s\n", #name); test_##name(); } while(0)

/* ── helpers ──────────────────────────────────────────────────────────── */
static int count_nodes(ccmap1_t *m) {
  int n = 0;
  for (ccmap1_node_t *c = ccmap1_begin(m); c != ccmap1_end(m); c = ccmap1_next(c)) n++;
  return n;
}

static int count_nodes_rev(ccmap1_t *m) {
  int n = 0;
  for (ccmap1_node_t *c = ccmap1_rbegin(m); c; c = ccmap1_prev(c)) n++;
  return n;
}

/* ── tests ────────────────────────────────────────────────────────────── */

TEST(init_empty) {
  ccmap1_t m;
  ccmap1_init(&m, cmp_fn);
  ASSERT(ccmap1_size(&m) == 0);
  ASSERT(ccmap1_begin(&m) == NULL);
  ASSERT(ccmap1_end(&m) == NULL);
  ASSERT(ccmap1_first(&m) == NULL);
  ASSERT(ccmap1_rbegin(&m) == NULL);
}

TEST(insert_and_size) {
  ccmap1_t m; ccmap1_init(&m, cmp_fn);
  struct entry e[5];
  for (int i = 0; i < 5; i++) { e[i].key = i * 10; ccmap1_insert(&m, &e[i].node, NULL); }
  ASSERT(ccmap1_size(&m) == 5);
  ASSERT(count_nodes(&m) == 5);
}

TEST(insert_duplicate) {
  ccmap1_t m; ccmap1_init(&m, cmp_fn);
  struct entry e1 = {.key = 42}; ccmap1_node_t *out = NULL;
  ASSERT(ccmap1_insert(&m, &e1.node, &out) == 0);
  ASSERT(out == &e1.node);

  struct entry e2 = {.key = 42}; /* same key */
  ASSERT(ccmap1_insert(&m, &e2.node, &out) == -1);
  ASSERT(out == &e1.node); /* out → existing node */
  ASSERT(ccmap1_size(&m) == 1);
}

TEST(find) {
  ccmap1_t m; ccmap1_init(&m, cmp_fn);
  struct entry e[3] = {{.key=10},{.key=20},{.key=30}};
  for (int i = 0; i < 3; i++) ccmap1_insert(&m, &e[i].node, NULL);

  struct entry probe;
  probe.key = 10; ASSERT(ccmap1_find(&m, &probe.node) == &e[0].node);
  probe.key = 20; ASSERT(ccmap1_find(&m, &probe.node) == &e[1].node);
  probe.key = 30; ASSERT(ccmap1_find(&m, &probe.node) == &e[2].node);
  probe.key = 99; ASSERT(ccmap1_find(&m, &probe.node) == NULL);
}

TEST(erase) {
  ccmap1_t m; ccmap1_init(&m, cmp_fn);
  struct entry e[5];
  for (int i = 0; i < 5; i++) { e[i].key = i; ccmap1_insert(&m, &e[i].node, NULL); }
  ASSERT(ccmap1_size(&m) == 5);

  ccmap1_erase(&m, &e[2].node); /* remove middle */
  ASSERT(ccmap1_size(&m) == 4);
  ASSERT(count_nodes(&m) == 4);

  ccmap1_erase(&m, &e[0].node); /* remove first */
  ASSERT(ccmap1_size(&m) == 3);

  ccmap1_erase(&m, &e[4].node); /* remove last */
  ASSERT(ccmap1_size(&m) == 2);
}

TEST(erase_update_first) {
  ccmap1_t m; ccmap1_init(&m, cmp_fn);
  struct entry e[3] = {{.key=30},{.key=10},{.key=20}};
  for (int i = 0; i < 3; i++) ccmap1_insert(&m, &e[i].node, NULL);
  ASSERT(E(ccmap1_first(&m))->key == 10);

  ccmap1_erase(&m, &e[1].node); /* remove key=10 (current first) */
  ASSERT(ccmap1_size(&m) == 2);
  ASSERT(E(ccmap1_first(&m))->key == 20); /* new first */
}

TEST(erase_all_ascending) {
  ccmap1_t m; ccmap1_init(&m, cmp_fn);
  enum { N = 100 };
  struct entry *e = calloc(N, sizeof(*e));
  ASSERT(e != NULL);
  for (int i = 0; i < N; i++) { e[i].key = i; ccmap1_insert(&m, &e[i].node, NULL); }
  for (int i = 0; i < N; i++) {
    ccmap1_erase(&m, &e[i].node);
    ASSERT((int)ccmap1_size(&m) == N - 1 - i);
  }
  ASSERT(ccmap1_size(&m) == 0);
  free(e);
}

TEST(erase_all_descending) {
  ccmap1_t m; ccmap1_init(&m, cmp_fn);
  enum { N = 100 };
  struct entry *e = calloc(N, sizeof(*e));
  ASSERT(e != NULL);
  for (int i = 0; i < N; i++) { e[i].key = i; ccmap1_insert(&m, &e[i].node, NULL); }
  for (int i = N - 1; i >= 0; i--) {
    ccmap1_erase(&m, &e[i].node);
    ASSERT((int)ccmap1_size(&m) == i);
  }
  ASSERT(ccmap1_size(&m) == 0);
  free(e);
}

TEST(clear) {
  ccmap1_t m; ccmap1_init(&m, cmp_fn);
  struct entry e[3] = {{.key=1},{.key=2},{.key=3}};
  for (int i = 0; i < 3; i++) ccmap1_insert(&m, &e[i].node, NULL);
  ccmap1_clear(&m);
  ASSERT(ccmap1_size(&m) == 0);
  ASSERT(ccmap1_begin(&m) == NULL);
  ASSERT(ccmap1_first(&m) == NULL);
}

TEST(iterate_forward) {
  ccmap1_t m; ccmap1_init(&m, cmp_fn);
  struct entry e[5];
  for (int i = 0; i < 5; i++) { e[i].key = 4 - i; ccmap1_insert(&m, &e[i].node, NULL); } /* 4,3,2,1,0 */
  int prev = -1;
  for (ccmap1_node_t *n = ccmap1_begin(&m); n != ccmap1_end(&m); n = ccmap1_next(n)) {
    int k = E(n)->key;
    ASSERT(k > prev);
    prev = k;
  }
  ASSERT(count_nodes(&m) == 5);
}

TEST(iterate_reverse) {
  ccmap1_t m; ccmap1_init(&m, cmp_fn);
  struct entry e[5];
  for (int i = 0; i < 5; i++) { e[i].key = i; ccmap1_insert(&m, &e[i].node, NULL); }
  int prev = 999;
  for (ccmap1_node_t *n = ccmap1_rbegin(&m); n; n = ccmap1_prev(n)) {
    int k = E(n)->key;
    ASSERT(k < prev);
    prev = k;
  }
  ASSERT(count_nodes_rev(&m) == 5);
}

TEST(next_prev_roundtrip) {
  ccmap1_t m; ccmap1_init(&m, cmp_fn);
  struct entry e1 = {.key = 10}, e2 = {.key = 20}, e3 = {.key = 30};
  ccmap1_insert(&m, &e1.node, NULL);
  ccmap1_insert(&m, &e2.node, NULL);
  ccmap1_insert(&m, &e3.node, NULL);

  ASSERT(ccmap1_next(&e3.node) == NULL); /* largest → no next */
  ASSERT(ccmap1_prev(&e1.node) == NULL); /* smallest → no prev */

  /* roundtrip for middle element */
  ASSERT(ccmap1_prev(ccmap1_next(&e2.node)) == &e2.node);
  ASSERT(ccmap1_next(ccmap1_prev(&e2.node)) == &e2.node);
}

TEST(null_safety) {
  ccmap1_t m; ccmap1_init(&m, cmp_fn);
  ccmap1_init(NULL, NULL);              /* should not crash */
  ASSERT(ccmap1_insert(NULL, NULL, NULL) == -1);
  ASSERT(ccmap1_find(NULL, NULL) == NULL);
  ccmap1_erase(NULL, NULL);
  ccmap1_clear(NULL);
  ASSERT(ccmap1_size(NULL) == 0);
  ASSERT(ccmap1_begin(NULL) == NULL);
  ASSERT(ccmap1_end(NULL) == NULL);
  ASSERT(ccmap1_first(NULL) == NULL);
  ASSERT(ccmap1_rbegin(NULL) == NULL);
  ASSERT(ccmap1_next(NULL) == NULL);
  ASSERT(ccmap1_prev(NULL) == NULL);
}

TEST(large_insert) {
  ccmap1_t m; ccmap1_init(&m, cmp_fn);
  enum { N = 1000 };
  struct entry *e = calloc(N, sizeof(*e));
  ASSERT(e != NULL);
  for (int i = 0; i < N; i++) { e[i].key = (i * 7919) % N; ccmap1_insert(&m, &e[i].node, NULL); }
  ASSERT(ccmap1_size(&m) == N);
  ASSERT(count_nodes(&m) == N);
  /* verify ordering */
  int prev = -1, cnt = 0;
  for (ccmap1_node_t *n = ccmap1_begin(&m); n != ccmap1_end(&m); n = ccmap1_next(n)) {
    int k = E(n)->key;
    ASSERT(k > prev);
    prev = k; cnt++;
  }
  ASSERT(cnt == N);
  /* find each one */
  for (int i = 0; i < N; i++) {
    ccmap1_node_t *f = ccmap1_find(&m, &e[i].node);
    ASSERT(f == &e[i].node);
  }
  free(e);
}

TEST(insert_1000_reverse_order) {
  ccmap1_t m; ccmap1_init(&m, cmp_fn);
  enum { N = 1000 };
  struct entry *e = calloc(N, sizeof(*e));
  for (int i = 0; i < N; i++) { e[i].key = N - i; ccmap1_insert(&m, &e[i].node, NULL); }
  ASSERT(count_nodes(&m) == N);
  ASSERT(E(ccmap1_first(&m))->key == 1);
  free(e);
}

TEST(erase_and_reinsert) {
  ccmap1_t m; ccmap1_init(&m, cmp_fn);
  enum { N = 100 };
  struct entry *e = calloc(N, sizeof(*e));
  ASSERT(e != NULL);
  for (int i = 0; i < N; i++) { e[i].key = i; ccmap1_insert(&m, &e[i].node, NULL); }

  /* erase all in random-ish order */
  int order[100];
  for (int i = 0; i < N; i++) order[i] = (i * 17 + 31) % N;
  for (int i = 0; i < N; i++) ccmap1_erase(&m, &e[order[i]].node);
  ASSERT(ccmap1_size(&m) == 0);
  ASSERT(ccmap1_begin(&m) == NULL);

  /* reinsert */
  for (int i = 0; i < N; i++) ccmap1_insert(&m, &e[i].node, NULL);
  ASSERT(ccmap1_size(&m) == N);
  int prev = -1, cnt = 0;
  for (ccmap1_node_t *n = ccmap1_begin(&m); n; n = ccmap1_next(n)) {
    int k = E(n)->key;
    ASSERT(k > prev);
    prev = k; cnt++;
  }
  ASSERT(cnt == N);
  free(e);
}

TEST(sequential_insert_stress) {
  ccmap1_t m; ccmap1_init(&m, cmp_fn);
  enum { N = 500 };
  struct entry *e = calloc(N, sizeof(*e));
  ASSERT(e != NULL);
  for (int i = 0; i < N; i++) { e[i].key = i; ccmap1_insert(&m, &e[i].node, NULL); }
  for (int i = 0; i < N; i++) {
    struct entry p; p.key = i;
    ASSERT(ccmap1_find(&m, &p.node) == &e[i].node);
  }
  free(e);
}

TEST(erase_first_repeatedly) {
  ccmap1_t m; ccmap1_init(&m, cmp_fn);
  enum { N = 100 };
  struct entry *e = calloc(N, sizeof(*e));
  ASSERT(e != NULL);
  for (int i = 0; i < N; i++) { e[i].key = i; ccmap1_insert(&m, &e[i].node, NULL); }
  for (int i = 0; i < N; i++) {
    ASSERT((int)ccmap1_size(&m) == N - i);
    ASSERT(E(ccmap1_first(&m))->key == i);
    ccmap1_erase(&m, &e[i].node);
  }
  ASSERT(ccmap1_size(&m) == 0);
  free(e);
}

TEST(erase_last_repeatedly) {
  ccmap1_t m; ccmap1_init(&m, cmp_fn);
  enum { N = 100 };
  struct entry *e = calloc(N, sizeof(*e));
  ASSERT(e != NULL);
  for (int i = 0; i < N; i++) { e[i].key = i; ccmap1_insert(&m, &e[i].node, NULL); }
  for (int i = N - 1; i >= 0; i--) {
    ASSERT((int)ccmap1_size(&m) == i + 1);
    ASSERT(E(ccmap1_last(&m))->key == i);
    ccmap1_erase(&m, &e[i].node);
  }
  ASSERT(ccmap1_size(&m) == 0);
  free(e);
}

TEST(erase_middle_repeatedly) {
  ccmap1_t m; ccmap1_init(&m, cmp_fn);
  enum { N = 101 };
  struct entry *e = calloc(N, sizeof(*e));
  ASSERT(e != NULL);
  for (int i = 0; i < N; i++) { e[i].key = i; ccmap1_insert(&m, &e[i].node, NULL); }
  /* repeatedly erase the median */
  int remaining = N;
  while (remaining > 0) {
    ccmap1_erase(&m, &e[remaining / 2].node);
    remaining--;
    ASSERT((int)ccmap1_size(&m) == remaining);
    /* verify ordering holds */
    int prev = -1;
    for (ccmap1_node_t *n = ccmap1_begin(&m); n; n = ccmap1_next(n)) {
      ASSERT(E(n)->key > prev);
      prev = E(n)->key;
    }
  }
  free(e);
}

/* ── main ─────────────────────────────────────────────────────────────── */
int main(void) {
  printf("ccmap1 (Weak AVL) tests:\n");
  RUN(init_empty);
  RUN(insert_and_size);
  RUN(insert_duplicate);
  RUN(find);
  RUN(erase);
  RUN(erase_update_first);
  RUN(erase_all_ascending);
  RUN(erase_all_descending);
  RUN(clear);
  RUN(iterate_forward);
  RUN(iterate_reverse);
  RUN(next_prev_roundtrip);
  RUN(null_safety);
  RUN(large_insert);
  RUN(insert_1000_reverse_order);
  RUN(erase_and_reinsert);
  RUN(sequential_insert_stress);
  RUN(erase_first_repeatedly);
  RUN(erase_last_repeatedly);
  RUN(erase_middle_repeatedly);
  printf("\n  %d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}