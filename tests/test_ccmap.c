/*
**  Test suite for ccmap (intrusive red-black tree)
**  Build:  gcc -std=c99 -Wall -Wextra -o test_ccmap test_ccmap.c && ./test_ccmap
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "../include/ccmap.h"

/* ── test entry ───────────────────────────────────────────────────────── */
struct entry {
  int          key;
  ccmap_node_t node;
};
static struct entry *E(const ccmap_node_t *n) {
  return CCMAP_CONTAINER(n, struct entry, node);
}

/* ── comparison: sort by int key ascending ───────────────────────────── */
static int64_t cmp_fn(const ccmap_node_t *a, const ccmap_node_t *b) {
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
static int count_nodes(ccmap_t *m) {
  int n = 0;
  for (ccmap_node_t *c = ccmap_begin(m); c != ccmap_end(m); c = ccmap_next(c)) n++;
  return n;
}

static int count_nodes_rev(ccmap_t *m) {
  int n = 0;
  for (ccmap_node_t *c = ccmap_rbegin(m); c; c = ccmap_prev(c)) n++;
  return n;
}

/* ── tests ────────────────────────────────────────────────────────────── */

TEST(init_empty) {
  ccmap_t m;
  ccmap_init(&m, cmp_fn);
  ASSERT(ccmap_size(&m) == 0);
  ASSERT(ccmap_begin(&m) == NULL);
  ASSERT(ccmap_end(&m) == NULL);
  ASSERT(ccmap_first(&m) == NULL);
  ASSERT(ccmap_rbegin(&m) == NULL);
}

TEST(insert_and_size) {
  ccmap_t m; ccmap_init(&m, cmp_fn);
  struct entry e[5];
  for (int i = 0; i < 5; i++) { e[i].key = i * 10; ccmap_insert(&m, &e[i].node, NULL); }
  ASSERT(ccmap_size(&m) == 5);
  ASSERT(count_nodes(&m) == 5);
}

TEST(insert_duplicate) {
  ccmap_t m; ccmap_init(&m, cmp_fn);
  struct entry e1 = {.key = 42}; ccmap_node_t *out = NULL;
  ASSERT(ccmap_insert(&m, &e1.node, &out) == 0);
  ASSERT(out == &e1.node);

  struct entry e2 = {.key = 42}; /* same key */
  ASSERT(ccmap_insert(&m, &e2.node, &out) == -1);
  ASSERT(out == &e1.node); /* out → existing node */
  ASSERT(ccmap_size(&m) == 1);
}

TEST(find) {
  ccmap_t m; ccmap_init(&m, cmp_fn);
  struct entry e[3] = {{.key=10},{.key=20},{.key=30}};
  for (int i = 0; i < 3; i++) ccmap_insert(&m, &e[i].node, NULL);

  struct entry probe;
  probe.key = 10; ASSERT(ccmap_find(&m, &probe.node) == &e[0].node);
  probe.key = 20; ASSERT(ccmap_find(&m, &probe.node) == &e[1].node);
  probe.key = 30; ASSERT(ccmap_find(&m, &probe.node) == &e[2].node);
  probe.key = 99; ASSERT(ccmap_find(&m, &probe.node) == NULL);
}

TEST(erase) {
  ccmap_t m; ccmap_init(&m, cmp_fn);
  struct entry e[5];
  for (int i = 0; i < 5; i++) { e[i].key = i; ccmap_insert(&m, &e[i].node, NULL); }
  ASSERT(ccmap_size(&m) == 5);

  ccmap_erase(&m, &e[2].node); /* remove middle */
  ASSERT(ccmap_size(&m) == 4);
  ASSERT(count_nodes(&m) == 4);

  ccmap_erase(&m, &e[0].node); /* remove first */
  ASSERT(ccmap_size(&m) == 3);

  ccmap_erase(&m, &e[4].node); /* remove last */
  ASSERT(ccmap_size(&m) == 2);
}

TEST(erase_update_first) {
  ccmap_t m; ccmap_init(&m, cmp_fn);
  struct entry e[3] = {{.key=30},{.key=10},{.key=20}};
  for (int i = 0; i < 3; i++) ccmap_insert(&m, &e[i].node, NULL);
  ASSERT(E(ccmap_first(&m))->key == 10);

  ccmap_erase(&m, &e[1].node); /* remove key=10 (current first) */
  ASSERT(ccmap_size(&m) == 2);
  ASSERT(E(ccmap_first(&m))->key == 20); /* new first */
}

TEST(clear) {
  ccmap_t m; ccmap_init(&m, cmp_fn);
  struct entry e[3] = {{.key=1},{.key=2},{.key=3}};
  for (int i = 0; i < 3; i++) ccmap_insert(&m, &e[i].node, NULL);
  ccmap_clear(&m);
  ASSERT(ccmap_size(&m) == 0);
  ASSERT(ccmap_begin(&m) == NULL);
  ASSERT(ccmap_first(&m) == NULL);
}

TEST(iterate_forward) {
  ccmap_t m; ccmap_init(&m, cmp_fn);
  struct entry e[5];
  for (int i = 0; i < 5; i++) { e[i].key = 4 - i; ccmap_insert(&m, &e[i].node, NULL); } /* 4,3,2,1,0 */
  int prev = -1;
  for (ccmap_node_t *n = ccmap_begin(&m); n != ccmap_end(&m); n = ccmap_next(n)) {
    int k = E(n)->key;
    ASSERT(k > prev);
    prev = k;
  }
  ASSERT(count_nodes(&m) == 5);
}

TEST(iterate_reverse) {
  ccmap_t m; ccmap_init(&m, cmp_fn);
  struct entry e[5];
  for (int i = 0; i < 5; i++) { e[i].key = i; ccmap_insert(&m, &e[i].node, NULL); }
  int prev = 999;
  for (ccmap_node_t *n = ccmap_rbegin(&m); n; n = ccmap_prev(n)) {
    int k = E(n)->key;
    ASSERT(k < prev);
    prev = k;
  }
  ASSERT(count_nodes_rev(&m) == 5);
}

TEST(next_prev_roundtrip) {
  ccmap_t m; ccmap_init(&m, cmp_fn);
  struct entry e1 = {.key = 10}, e2 = {.key = 20}, e3 = {.key = 30};
  ccmap_insert(&m, &e1.node, NULL);
  ccmap_insert(&m, &e2.node, NULL);
  ccmap_insert(&m, &e3.node, NULL);

  /* end sentinel: ccmap_end always returns NULL, ccmap_next(last) returns NULL */
  ASSERT(ccmap_next(&e3.node) == NULL); /* e3 (30) is largest → no next */
  ASSERT(ccmap_prev(&e1.node) == NULL); /* e1 (10) is smallest → no prev */

  /* roundtrip for middle element */
  ASSERT(ccmap_prev(ccmap_next(&e2.node)) == &e2.node);
  ASSERT(ccmap_next(ccmap_prev(&e2.node)) == &e2.node);
}

TEST(null_safety) {
  ccmap_t m; ccmap_init(&m, cmp_fn);
  ccmap_init(NULL, NULL);            /* should not crash */
  ASSERT(ccmap_insert(NULL, NULL, NULL) == -1);
  ASSERT(ccmap_find(NULL, NULL) == NULL);
  ccmap_erase(NULL, NULL);
  ccmap_clear(NULL);
  ASSERT(ccmap_size(NULL) == 0);
  ASSERT(ccmap_begin(NULL) == NULL);
  ASSERT(ccmap_end(NULL) == NULL);
  ASSERT(ccmap_first(NULL) == NULL);
  ASSERT(ccmap_rbegin(NULL) == NULL);
  ASSERT(ccmap_next(NULL) == NULL);
  ASSERT(ccmap_prev(NULL) == NULL);
}

TEST(large_insert) {
  ccmap_t m; ccmap_init(&m, cmp_fn);
  enum { N = 1000 };
  struct entry *e = calloc(N, sizeof(*e));
  ASSERT(e != NULL);
  for (int i = 0; i < N; i++) { e[i].key = (i * 7919) % N; ccmap_insert(&m, &e[i].node, NULL); }
  ASSERT(ccmap_size(&m) == N);
  ASSERT(count_nodes(&m) == N);
  /* verify ordering */
  int prev = -1, cnt = 0;
  for (ccmap_node_t *n = ccmap_begin(&m); n != ccmap_end(&m); n = ccmap_next(n)) {
    int k = E(n)->key;
    ASSERT(k > prev);
    prev = k; cnt++;
  }
  ASSERT(cnt == N);
  /* find each one */
  for (int i = 0; i < N; i++) {
    ccmap_node_t *f = ccmap_find(&m, &e[i].node);
    ASSERT(f == &e[i].node);
  }
  free(e);
}

TEST(insert_1000_reverse_order) {
  ccmap_t m; ccmap_init(&m, cmp_fn);
  enum { N = 1000 };
  struct entry *e = calloc(N, sizeof(*e));
  for (int i = 0; i < N; i++) { e[i].key = N - i; ccmap_insert(&m, &e[i].node, NULL); }
  ASSERT(count_nodes(&m) == N);
  ASSERT(E(ccmap_first(&m))->key == 1);
  free(e);
}

/* ── main ─────────────────────────────────────────────────────────────── */
int main(void) {
  printf("ccmap tests:\n");
  RUN(init_empty);
  RUN(insert_and_size);
  RUN(insert_duplicate);
  RUN(find);
  RUN(erase);
  RUN(erase_update_first);
  RUN(clear);
  RUN(iterate_forward);
  RUN(iterate_reverse);
  RUN(next_prev_roundtrip);
  RUN(null_safety);
  RUN(large_insert);
  RUN(insert_1000_reverse_order);
  printf("\n  %d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
