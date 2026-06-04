/*
**  Test suite for ccheap (D-ary heap, pointer mode + function pointer)
**  Build:  gcc -std=c99 -Wall -Wextra -o test_ccheap test_ccheap.c && ./test_ccheap
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define CCNODE_T struct my_node
struct my_node {
  uint32_t conv;
  uint32_t priority;
};

static int64_t min_cmp(const struct my_node *a, const struct my_node *b) {
  /* higher priority = smaller value → min-heap: b smaller → a higher? No:
     f(a,b) > 0 means a closer to root. For min-heap, smaller value wins.
     So if a->priority < b->priority, return >0 to push a up. */
  return (int64_t)b->priority - (int64_t)a->priority;
}

#include "../include/ccheap.h"

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
  ASSERT(heap_init(&h, min_cmp) == 0);
  ASSERT(heap_size(&h) == 0);
  ASSERT(heap_peek(&h) == NULL);
  heap_destroy(&h);
  ASSERT(heap_size(&h) == 0);
}

TEST(insert_and_peek) {
  ccheap_t h; heap_init(&h, min_cmp);
  struct my_node n = {0, 5};
  ASSERT(heap_insert(&h, &n) == 0);
  ASSERT(heap_size(&h) == 1);
  ASSERT(heap_peek(&h) == &n);
  heap_destroy(&h);
}

TEST(pop_empty) {
  ccheap_t h; heap_init(&h, min_cmp);
  ASSERT(heap_pop(&h) == NULL);
  heap_destroy(&h);
}

TEST(min_heap_ordering) {
  /* insert 9,8,7,...,1 — pop should return 1,2,...,9 */
  ccheap_t h; heap_init(&h, min_cmp);
  struct my_node n[9];
  for (int i = 0; i < 9; i++) { n[i].priority = 9 - i; heap_insert(&h, &n[i]); }
  ASSERT(heap_size(&h) == 9);
  for (int i = 0; i < 9; i++) {
    struct my_node *p = heap_pop(&h);
    ASSERT(p != NULL);
    ASSERT(p->priority == (uint32_t)(i + 1));
  }
  ASSERT(heap_size(&h) == 0);
  ASSERT(heap_pop(&h) == NULL);
  heap_destroy(&h);
}

static int64_t max_cmp(const struct my_node *a, const struct my_node *b) {
  return (int64_t)a->priority - (int64_t)b->priority;
}

TEST(max_heap_ordering) {
  ccheap_t h; heap_init(&h, max_cmp);
  struct my_node n[7];
  for (int i = 0; i < 7; i++) { n[i].priority = (uint32_t)i; heap_insert(&h, &n[i]); }
  for (int i = 0; i < 7; i++) {
    struct my_node *p = heap_pop(&h);
    ASSERT(p->priority == (uint32_t)(6 - i)); /* 6,5,4,3,2,1,0 */
  }
  heap_destroy(&h);
}

TEST(interleaved_insert_pop) {
  ccheap_t h; heap_init(&h, min_cmp);
  struct my_node n[5];
  for (int i = 0; i < 3; i++) { n[i].priority = (uint32_t)(i * 10 + 10); heap_insert(&h, &n[i]); }
  ASSERT(heap_pop(&h)->priority == 10);
  ASSERT(heap_size(&h) == 2);
  n[3].priority = 5; heap_insert(&h, &n[3]);
  n[4].priority = 15; heap_insert(&h, &n[4]);
  ASSERT(heap_pop(&h)->priority == 5);
  ASSERT(heap_pop(&h)->priority == 15);
  ASSERT(heap_pop(&h)->priority == 20);
  ASSERT(heap_pop(&h)->priority == 30);
  ASSERT(heap_size(&h) == 0);
  heap_destroy(&h);
}

TEST(large_insert_pop) {
  ccheap_t h; heap_init(&h, min_cmp);
  enum { N = 1000 };
  struct my_node *n = calloc(N, sizeof(*n));
  ASSERT(n != NULL);
  for (int i = 0; i < N; i++) { n[i].priority = (uint32_t)(N - i); heap_insert(&h, &n[i]); }
  ASSERT(heap_size(&h) == N);
  for (int i = 0; i < N; i++) {
    struct my_node *p = heap_pop(&h);
    ASSERT(p->priority == (uint32_t)(i + 1));
  }
  ASSERT(heap_size(&h) == 0);
  free(n);
  heap_destroy(&h);
}

TEST(resize_trigger) {
  ccheap_t h; heap_init(&h, min_cmp);
  /* default cap is 32, insert 200 to force multiple resizes */
  struct my_node n[200];
  for (int i = 0; i < 200; i++) { n[i].priority = (uint32_t)(200 - i); heap_insert(&h, &n[i]); }
  ASSERT(heap_size(&h) == 200);
  for (int i = 0; i < 200; i++) {
    ASSERT(heap_pop(&h)->priority == (uint32_t)(i + 1));
  }
  heap_destroy(&h);
}

TEST(push_alias) {
  ccheap_t h; heap_init(&h, min_cmp);
  struct my_node n = {0, 42};
  ASSERT(heap_push(&h, &n) == 0); /* push = insert alias */
  ASSERT(heap_size(&h) == 1);
  ASSERT(heap_peek(&h)->priority == 42);
  heap_destroy(&h);
}

TEST(null_safety) {
  ccheap_t h; heap_init(&h, min_cmp);
  ASSERT(heap_init(NULL, NULL) == -1);
  ASSERT(heap_insert(NULL, NULL) == -1);
  ASSERT(heap_pop(NULL) == NULL);
  ASSERT(heap_peek(NULL) == NULL);
  ASSERT(heap_size(NULL) == 0);
  heap_destroy(NULL); /* should not crash */
  heap_destroy(&h);
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
  RUN(null_safety);
  printf("\n  %d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
