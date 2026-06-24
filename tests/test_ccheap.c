/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Test suite for ccheap (D-ary heap, pointer mode + function pointer)
**  Build:  gcc -std=c99 -Wall -Wextra -o test_ccheap test_ccheap.c && ./test_ccheap
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "../include/ccheap.h"

/* ── user struct embedding ccheap_node_t ──────────────────────────────── */

struct my_node {
  ccheap_node_t node;     /* embedded heap node */
  uint32_t priority;
};

/* ── comparison functions ─────────────────────────────────────────────── */

static int64_t min_cmp(const ccheap_node_t *a, const ccheap_node_t *b) {
  struct my_node *ma = CCHEAP_CONTAINER(a, struct my_node, node);
  struct my_node *mb = CCHEAP_CONTAINER(b, struct my_node, node);
  return (int64_t)mb->priority - (int64_t)ma->priority;
}

static int64_t max_cmp(const ccheap_node_t *a, const ccheap_node_t *b) {
  struct my_node *ma = CCHEAP_CONTAINER(a, struct my_node, node);
  struct my_node *mb = CCHEAP_CONTAINER(b, struct my_node, node);
  return (int64_t)ma->priority - (int64_t)mb->priority;
}

/* ── test harness ─────────────────────────────────────────────────────── */
static int passed, failed;
#define TEST(name) static void test_##name(void)
#define ASSERT(cond) do { \
  if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failed++; } \
  else passed++; \
} while(0)
#define RUN(name) do { printf("  %s\n", #name); test_##name(); } while(0)

/* ── tests ────────────────────────────────────────────────────────────── */

TEST(init_destroy) {
  ccheap_t h;
  ASSERT(ccheap_init(&h, min_cmp) == 0);
  ASSERT(ccheap_size(&h) == 0);
  ASSERT(ccheap_peek(&h) == NULL);
  ccheap_destroy(&h);
  ASSERT(ccheap_size(&h) == 0);
}

TEST(insert_and_peek) {
  ccheap_t h; ccheap_init(&h, min_cmp);
  struct my_node n = {{0}, 5};
  ASSERT(ccheap_insert(&h, &n.node) == 0);
  ASSERT(ccheap_size(&h) == 1);
  ASSERT(ccheap_peek(&h) == &n.node);
  ccheap_destroy(&h);
}

TEST(pop_empty) {
  ccheap_t h; ccheap_init(&h, min_cmp);
  ASSERT(ccheap_pop(&h) == NULL);
  ccheap_destroy(&h);
}

TEST(min_heap_ordering) {
  /* insert 9,8,7,...,1 — pop should return 1,2,...,9 */
  ccheap_t h; ccheap_init(&h, min_cmp);
  struct my_node n[9];
  for (int i = 0; i < 9; i++) { n[i].priority = 9 - i; ccheap_insert(&h, &n[i].node); }
  ASSERT(ccheap_size(&h) == 9);
  for (int i = 0; i < 9; i++) {
    ccheap_node_t *np = ccheap_pop(&h);
    ASSERT(np != NULL);
    struct my_node *p = CCHEAP_CONTAINER(np, struct my_node, node);
    ASSERT(p->priority == (uint32_t)(i + 1));
  }
  ASSERT(ccheap_size(&h) == 0);
  ASSERT(ccheap_pop(&h) == NULL);
  ccheap_destroy(&h);
}

TEST(max_heap_ordering) {
  ccheap_t h; ccheap_init(&h, max_cmp);
  struct my_node n[7];
  for (int i = 0; i < 7; i++) { n[i].priority = (uint32_t)i; ccheap_insert(&h, &n[i].node); }
  for (int i = 0; i < 7; i++) {
    ccheap_node_t *np = ccheap_pop(&h);
    struct my_node *p = CCHEAP_CONTAINER(np, struct my_node, node);
    ASSERT(p->priority == (uint32_t)(6 - i)); /* 6,5,4,3,2,1,0 */
  }
  ccheap_destroy(&h);
}

TEST(interleaved_insert_pop) {
  ccheap_t h; ccheap_init(&h, min_cmp);
  struct my_node n[5];
  for (int i = 0; i < 3; i++) { n[i].priority = (uint32_t)(i * 10 + 10); ccheap_insert(&h, &n[i].node); }
  ASSERT(CCHEAP_CONTAINER(ccheap_pop(&h), struct my_node, node)->priority == 10);
  ASSERT(ccheap_size(&h) == 2);
  n[3].priority = 5; ccheap_insert(&h, &n[3].node);
  n[4].priority = 15; ccheap_insert(&h, &n[4].node);
  ASSERT(CCHEAP_CONTAINER(ccheap_pop(&h), struct my_node, node)->priority == 5);
  ASSERT(CCHEAP_CONTAINER(ccheap_pop(&h), struct my_node, node)->priority == 15);
  ASSERT(CCHEAP_CONTAINER(ccheap_pop(&h), struct my_node, node)->priority == 20);
  ASSERT(CCHEAP_CONTAINER(ccheap_pop(&h), struct my_node, node)->priority == 30);
  ASSERT(ccheap_size(&h) == 0);
  ccheap_destroy(&h);
}

TEST(large_insert_pop) {
  ccheap_t h; ccheap_init(&h, min_cmp);
  enum { N = 1000 };
  struct my_node *n = calloc(N, sizeof(*n));
  ASSERT(n != NULL);
  for (int i = 0; i < N; i++) { n[i].priority = (uint32_t)(N - i); ccheap_insert(&h, &n[i].node); }
  ASSERT(ccheap_size(&h) == N);
  for (int i = 0; i < N; i++) {
    ccheap_node_t *np = ccheap_pop(&h);
    struct my_node *p = CCHEAP_CONTAINER(np, struct my_node, node);
    ASSERT(p->priority == (uint32_t)(i + 1));
  }
  ASSERT(ccheap_size(&h) == 0);
  free(n);
  ccheap_destroy(&h);
}

TEST(resize_trigger) {
  ccheap_t h; ccheap_init(&h, min_cmp);
  /* default cap is 32, insert 200 to force multiple resizes */
  struct my_node n[200];
  for (int i = 0; i < 200; i++) { n[i].priority = (uint32_t)(200 - i); ccheap_insert(&h, &n[i].node); }
  ASSERT(ccheap_size(&h) == 200);
  for (int i = 0; i < 200; i++) {
    struct my_node *p = CCHEAP_CONTAINER(ccheap_pop(&h), struct my_node, node);
    ASSERT(p->priority == (uint32_t)(i + 1));
  }
  ccheap_destroy(&h);
}

TEST(push_alias) {
  ccheap_t h; ccheap_init(&h, min_cmp);
  struct my_node n = {{0}, 42};
  ASSERT(ccheap_push(&h, &n.node) == 0); /* push = insert alias */
  ASSERT(ccheap_size(&h) == 1);
  ASSERT(CCHEAP_CONTAINER(ccheap_peek(&h), struct my_node, node)->priority == 42);
  ccheap_destroy(&h);
}

/* ── Floyd build / heapify tests ──────────────────────────────────── */

TEST(build_empty) {
  ccheap_t h; ccheap_init(&h, min_cmp);
  ASSERT(ccheap_build(&h, NULL, 0) == 0);
  ASSERT(ccheap_size(&h) == 0);
  ASSERT(ccheap_peek(&h) == NULL);
  ccheap_destroy(&h);
}

TEST(build_single) {
  ccheap_t h; ccheap_init(&h, min_cmp);
  struct my_node n = {{0}, 42};
  ccheap_node_t *arr[] = { &n.node };
  ASSERT(ccheap_build(&h, arr, 1) == 0);
  ASSERT(ccheap_size(&h) == 1);
  ASSERT(CCHEAP_CONTAINER(ccheap_peek(&h), struct my_node, node)->priority == 42);
  ccheap_destroy(&h);
}

TEST(build_ordering) {
  ccheap_t h; ccheap_init(&h, min_cmp);
  struct my_node n[7];
  ccheap_node_t *arr[7];
  /* insert in reverse order so build < insert for same result */
  for (int i = 0; i < 7; i++) { n[i].priority = (uint32_t)(7 - i); arr[i] = &n[i].node; }
  ASSERT(ccheap_build(&h, arr, 7) == 0);
  ASSERT(ccheap_size(&h) == 7);

  /* pop order should be 1,2,...,7 */
  for (int i = 0; i < 7; i++) {
    struct my_node *p = CCHEAP_CONTAINER(ccheap_pop(&h), struct my_node, node);
    ASSERT(p->priority == (uint32_t)(i + 1));
  }
  ccheap_destroy(&h);
}

TEST(build_vs_insert_equivalent) {
  /* build(unsorted) should produce the same pop order as insert(one-by-one) */
  ccheap_t ha, hb; ccheap_init(&ha, min_cmp); ccheap_init(&hb, min_cmp);
  enum { N = 100 };
  struct my_node na[N], nb[N];
  ccheap_node_t *arr[N];

  for (int i = 0; i < N; i++) {
    uint32_t v = (uint32_t)((i * 17 + 31) % N); /* pseudo-random but deterministic */
    na[i].priority = v;
    nb[i].priority = v;
    arr[i] = &na[i].node;
  }

  /* build */
  ASSERT(ccheap_build(&ha, arr, N) == 0);
  /* insert one-by-one */
  for (int i = 0; i < N; i++) ccheap_insert(&hb, &nb[i].node);

  ASSERT(ccheap_size(&ha) == N);
  ASSERT(ccheap_size(&hb) == N);

  /* pop order must match */
  for (int i = 0; i < N; i++) {
    struct my_node *pa = CCHEAP_CONTAINER(ccheap_pop(&ha), struct my_node, node);
    struct my_node *pb = CCHEAP_CONTAINER(ccheap_pop(&hb), struct my_node, node);
    ASSERT(pa->priority == pb->priority);
  }

  ccheap_destroy(&ha);
  ccheap_destroy(&hb);
}

TEST(build_resize) {
  ccheap_t h; ccheap_init(&h, min_cmp); /* default cap = 32 */
  enum { N = 200 };
  struct my_node n[N];
  ccheap_node_t *arr[N];
  for (int i = 0; i < N; i++) { n[i].priority = (uint32_t)(N - i); arr[i] = &n[i].node; }
  ASSERT(ccheap_build(&h, arr, N) == 0);
  ASSERT(ccheap_size(&h) == N);
  for (int i = 0; i < N; i++) {
    struct my_node *p = CCHEAP_CONTAINER(ccheap_pop(&h), struct my_node, node);
    ASSERT(p->priority == (uint32_t)(i + 1));
  }
  ccheap_destroy(&h);
}

TEST(heapify_in_place) {
  ccheap_t h; ccheap_init(&h, min_cmp);
  struct my_node n[5];
  ccheap_node_t *arr[5];
  for (int i = 0; i < 5; i++) { n[i].priority = (uint32_t)(5 - i); arr[i] = &n[i].node; }
  ccheap_build(&h, arr, 5); /* initially 1,2,3,4,5 (already heap) */

  /* bulk-update: double every priority — now 2,4,6,8,10 */
  for (size_t i = 0; i < ccheap_size(&h); i++)
    CCHEAP_CONTAINER(h.data[i], struct my_node, node)->priority *= 2;

  ASSERT(ccheap_heapify(&h) == 0);
  ASSERT(ccheap_size(&h) == 5);

  for (int i = 0; i < 5; i++) {
    struct my_node *p = CCHEAP_CONTAINER(ccheap_pop(&h), struct my_node, node);
    ASSERT(p->priority == (uint32_t)((i + 1) * 2));
  }
  ccheap_destroy(&h);
}

TEST(heapify_null_safety) {
  ASSERT(ccheap_build(NULL, NULL, 0) == -1);
  ASSERT(ccheap_heapify(NULL) == -1);
}

TEST(null_safety) {
  ccheap_t h; ccheap_init(&h, min_cmp);
  ASSERT(ccheap_init(NULL, NULL) == -1);
  ASSERT(ccheap_insert(NULL, NULL) == -1);
  ASSERT(ccheap_pop(NULL) == NULL);
  ASSERT(ccheap_peek(NULL) == NULL);
  ASSERT(ccheap_size(NULL) == 0);
  ccheap_destroy(NULL); /* should not crash */
  ccheap_destroy(&h);
}

/* ── main ─────────────────────────────────────────────────────────────── */
int main(void) {
  printf("ccheap tests (pointer mode + function pointer):\n");
  RUN(init_destroy);
  RUN(insert_and_peek);
  RUN(pop_empty);
  RUN(min_heap_ordering);
  RUN(max_heap_ordering);
  RUN(interleaved_insert_pop);
  RUN(large_insert_pop);
  RUN(resize_trigger);
  RUN(push_alias);
  RUN(build_empty);
  RUN(build_single);
  RUN(build_ordering);
  RUN(build_vs_insert_equivalent);
  RUN(build_resize);
  RUN(heapify_in_place);
  RUN(heapify_null_safety);
  RUN(null_safety);
  printf("\n  %d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
