/*
**  Test suite for ccheap CCHEAP_NODE_INDEX + ccheap_update
**  Build: gcc -std=c99 -Wall -Wextra -o test_ccheap_update test_ccheap_update.c && ./test_ccheap_update
*/
#define CCHEAP_NODE_INDEX _idx
#define CCHEAP_COMPARE(a, b) \
    ((int64_t)((b)->cost - (a)->cost))
#include "../include/ccheap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── test harness ─────────────────────────────────────────────────────── */
static int passed, failed;
#define TEST(name) static void test_##name(void)
#define ASSERT(cond) do { \
  if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failed++; } \
  else passed++; \
} while(0)
#define RUN(name) do { printf("  %s\n", #name); test_##name(); } while(0)

/* ── helpers ──────────────────────────────────────────────────────────── */

/* verify that every node in the heap has correct _idx */
static void check_idxs(ccheap_t *h) {
  for (size_t i = 0; i < h->len; i++) {
    ccheap_node_t *n = h->data[i];
    if (n->_idx != i) {
      printf("  FAIL idx mismatch: data[%zu]->_idx=%zu\n", i, n->_idx);
      failed++;
      return;
    }
  }
  passed++;
}

/* verify heap property for entire array */
static void check_heap(ccheap_t *h) {
  for (size_t i = 1; i < h->len; i++) {
    size_t p = (i - 1) / CCHEAP_ARITY_N;
    if (CCHEAP_CMP(h, h->data[p], h->data[i]) < 0) {
      printf("  FAIL heap property at %zu (parent %zu)\n", i, p);
      failed++;
      return;
    }
  }
  passed++;
}

/* ── tests ────────────────────────────────────────────────────────────── */

TEST(idx_after_push) {
  ccheap_t h; ccheap_init(&h, NULL);
  ccheap_node_t a = {.cost = 5.0};
  ccheap_push(&h, &a);
  ASSERT(h.data[0] == &a);
  ASSERT(a._idx == 0);
  ccheap_destroy(&h);
}

TEST(idx_after_multi_push) {
  ccheap_t h; ccheap_init(&h, NULL);
  ccheap_node_t n[10];
  for (int i = 0; i < 10; i++) {
    n[i].cost = (double)(rand() % 100);
    ccheap_push(&h, &n[i]);
  }
  check_idxs(&h);
  check_heap(&h);
  ccheap_destroy(&h);
}

TEST(idx_after_pop) {
  ccheap_t h; ccheap_init(&h, NULL);
  ccheap_node_t n[10];
  for (int i = 0; i < 10; i++) {
    n[i].cost = (double)(9 - i);  /* 9, 8, 7, ..., 0 */
    ccheap_push(&h, &n[i]);
  }
  /* pop half */
  for (int i = 0; i < 5; i++) ccheap_pop(&h);
  check_idxs(&h);
  check_heap(&h);
  ccheap_destroy(&h);
}

TEST(update_decrease_key) {
  ccheap_t h; ccheap_init(&h, NULL);
  ccheap_node_t a = {.cost = 100.0};
  ccheap_node_t b = {.cost = 50.0};
  ccheap_node_t c = {.cost = 10.0};
  ccheap_push(&h, &a);
  ccheap_push(&h, &b);
  ccheap_push(&h, &c);
  ASSERT(ccheap_peek(&h) == &c);  /* 10 is min */

  /* decrease a: 100 -> 5, should become new root */
  a.cost = 5.0;
  ASSERT(ccheap_update(&h, &a) == 0);
  ASSERT(ccheap_peek(&h) == &a);
  check_idxs(&h);
  check_heap(&h);
  ccheap_destroy(&h);
}

TEST(update_increase_key) {
  ccheap_t h; ccheap_init(&h, NULL);
  ccheap_node_t a = {.cost = 1.0};
  ccheap_node_t b = {.cost = 20.0};
  ccheap_node_t c = {.cost = 30.0};
  ccheap_push(&h, &a);
  ccheap_push(&h, &b);
  ccheap_push(&h, &c);
  ASSERT(ccheap_peek(&h) == &a);

  /* increase a: 1 -> 100, should sift to bottom */
  a.cost = 100.0;
  ASSERT(ccheap_update(&h, &a) == 0);
  ASSERT(ccheap_peek(&h) != &a);
  check_idxs(&h);
  check_heap(&h);

  /* pop all — a should be last */
  for (int i = 0; i < 2; i++) ccheap_pop(&h);
  ASSERT(ccheap_pop(&h) == &a);
  ccheap_destroy(&h);
}

TEST(update_stale_idx) {
  ccheap_t h; ccheap_init(&h, NULL);
  ccheap_node_t a = {.cost = 5.0};
  ccheap_push(&h, &a);
  ccheap_pop(&h);      /* a is popped, _idx is stale */
  ASSERT(ccheap_update(&h, &a) == -1);  /* should reject */
  ccheap_destroy(&h);
}

TEST(update_null_safety) {
  ccheap_t h; ccheap_init(&h, NULL);
  ASSERT(ccheap_update(NULL, NULL) == -1);
  ASSERT(ccheap_update(&h, NULL) == -1);
  ccheap_destroy(&h);
}

TEST(full_Astar_simulation) {
  /* simulate A* open-set: push 20 nodes, decrease some costs, pop in order */
  ccheap_t open; ccheap_init(&open, NULL);
  enum { N = 20 };
  ccheap_node_t nodes[N];
  double costs[N];

  for (int i = 0; i < N; i++) {
    costs[i] = (double)(rand() % 1000);
    nodes[i].cost = costs[i];
    ccheap_push(&open, &nodes[i]);
  }

  /* randomly decrease a few */
  for (int iter = 0; iter < 5; iter++) {
    int i = rand() % N;
    double old = nodes[i].cost;
    nodes[i].cost = old * 0.5;   /* better path found */
    if (ccheap_update(&open, &nodes[i]) == 0)
      costs[i] = nodes[i].cost;
  }

  check_idxs(&open);
  check_heap(&open);

  /* pop all and verify ascending cost order */
  double prev = -1.0;
  for (int i = 0; i < N; i++) {
    ccheap_node_t *n = ccheap_pop(&open);
    ASSERT(n->cost >= prev - 1e-9);
    prev = n->cost;
  }
  ccheap_destroy(&open);
}

/* ── main ─────────────────────────────────────────────────────────────── */
int main(void) {
  printf("ccheap update tests (CCHEAP_NODE_INDEX):\n");
  RUN(idx_after_push);
  RUN(idx_after_multi_push);
  RUN(idx_after_pop);
  RUN(update_decrease_key);
  RUN(update_increase_key);
  RUN(update_stale_idx);
  RUN(update_null_safety);
  RUN(full_Astar_simulation);
  printf("\n  %d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
