/*
**  Test suite for cclist (intrusive doubly-linked list)
**  Build:  gcc -std=c99 -Wall -Wextra -o test_cclist test_cclist.c && ./test_cclist
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../include/cclist.h"

struct entry {
  int           val;
  cclist_node_t node;
};
static struct entry *le(const cclist_node_t *n) {
  return CCLIST_CONTAINER(n, struct entry, node);
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

TEST(init_empty) {
  cclist_t l; cclist_init(&l);
  ASSERT(cclist_size(&l) == 0);
  ASSERT(cclist_empty(&l));
  ASSERT(cclist_begin(&l) == NULL);
  ASSERT(cclist_end(&l) == NULL);
  ASSERT(cclist_front(&l) == NULL);
  ASSERT(cclist_back(&l) == NULL);
}

TEST(push_pop_front) {
  cclist_t l; cclist_init(&l);
  struct entry e1 = {1}, e2 = {2};

  cclist_push_front(&l, &e1.node);
  ASSERT(cclist_size(&l) == 1);
  ASSERT(cclist_front(&l) == &e1.node);
  ASSERT(cclist_back(&l) == &e1.node);

  cclist_push_front(&l, &e2.node);
  ASSERT(cclist_front(&l) == &e2.node);
  ASSERT(cclist_back(&l) == &e1.node);

  cclist_node_t *n = cclist_pop_front(&l);
  ASSERT(n == &e2.node);
  ASSERT(cclist_size(&l) == 1);
  ASSERT(cclist_front(&l) == &e1.node);

  n = cclist_pop_front(&l);
  ASSERT(n == &e1.node);
  ASSERT(cclist_empty(&l));
  ASSERT(cclist_pop_front(&l) == NULL); /* empty */
}

TEST(push_pop_back) {
  cclist_t l; cclist_init(&l);
  struct entry e1 = {1}, e2 = {2};

  cclist_push_back(&l, &e1.node);
  cclist_push_back(&l, &e2.node);
  ASSERT(cclist_back(&l) == &e2.node);

  cclist_node_t *n = cclist_pop_back(&l);
  ASSERT(n == &e2.node);
  ASSERT(cclist_back(&l) == &e1.node);
  ASSERT(cclist_size(&l) == 1);
  ASSERT(cclist_pop_back(&l) == &e1.node);
  ASSERT(cclist_pop_back(&l) == NULL);
}

TEST(insert_before_after) {
  cclist_t l; cclist_init(&l);
  struct entry e1 = {1}, e2 = {2}, e3 = {3};

  cclist_push_back(&l, &e1.node);                           /* [1] */
  cclist_insert_after(&l, &e1.node, &e3.node);              /* [1,3] */
  cclist_insert_before(&l, &e3.node, &e2.node);             /* [1,2,3] */

  ASSERT(cclist_size(&l) == 3);
  ASSERT(le(cclist_front(&l))->val == 1);
  ASSERT(le(cclist_next(cclist_front(&l)))->val == 2);
  ASSERT(le(cclist_back(&l))->val == 3);
}

TEST(remove_node) {
  cclist_t l; cclist_init(&l);
  struct entry e[3] = {{1},{2},{3}};
  for (int i = 0; i < 3; i++) cclist_push_back(&l, &e[i].node);

  cclist_remove(&l, &e[1].node); /* remove middle */
  ASSERT(cclist_size(&l) == 2);
  ASSERT(le(cclist_front(&l))->val == 1);
  ASSERT(le(cclist_back(&l))->val == 3);

  cclist_remove(&l, &e[0].node); /* remove head */
  ASSERT(le(cclist_front(&l))->val == 3);
  ASSERT(cclist_size(&l) == 1);

  cclist_remove(&l, &e[2].node); /* remove tail */
  ASSERT(cclist_empty(&l));
}

TEST(splice_back) {
  cclist_t a, b; cclist_init(&a); cclist_init(&b);
  struct entry e1 = {1}, e2 = {2}, e3 = {3}, e4 = {4};

  cclist_push_back(&a, &e1.node);
  cclist_push_back(&a, &e2.node);
  cclist_push_back(&b, &e3.node);
  cclist_push_back(&b, &e4.node);

  cclist_splice_back(&a, &b);
  ASSERT(cclist_size(&a) == 4);
  ASSERT(cclist_empty(&b));
  ASSERT(le(cclist_back(&a))->val == 4);
}

TEST(move) {
  cclist_t a, b; cclist_init(&a); cclist_init(&b);
  struct entry e = {99};
  cclist_push_back(&b, &e.node);

  /* self-move */
  ASSERT(cclist_move(&a, &a) == -3);

  /* normal move */
  ASSERT(cclist_move(&a, &b) == 0);
  ASSERT(cclist_size(&a) == 1);
  ASSERT(cclist_empty(&b));
  ASSERT(le(cclist_front(&a))->val == 99);

  /* empty move */
  ASSERT(cclist_move(&a, &b) == -2);
}

TEST(verify_valid) {
  cclist_t l; cclist_init(&l);
  ASSERT(cclist_verify(&l) == NOERROR); /* empty list is valid */

  struct entry e1 = {1}, e2 = {2};
  cclist_push_back(&l, &e1.node);
  cclist_push_back(&l, &e2.node);
  ASSERT(cclist_verify(&l) == NOERROR);
}

TEST(has_cycle) {
  cclist_t l; cclist_init(&l);
  ASSERT(!cclist_has_cycle(&l)); /* empty */

  struct entry e1 = {1}, e2 = {2};
  cclist_push_back(&l, &e1.node);
  cclist_push_back(&l, &e2.node);
  ASSERT(!cclist_has_cycle(&l));

  /* manually create a cycle */
  e2.node.next = &e1.node;
  e1.node.prev = &e2.node;
  ASSERT(cclist_has_cycle(&l));
}

TEST(iterate_forward_reverse) {
  cclist_t l; cclist_init(&l);
  struct entry e[5];
  for (int i = 0; i < 5; i++) { e[i].val = i; cclist_push_back(&l, &e[i].node); }

  int i = 0;
  for (cclist_node_t *n = cclist_begin(&l); n != cclist_end(&l); n = cclist_next(n)) {
    ASSERT(le(n)->val == i); i++;
  }
  ASSERT(i == 5);

  i = 4;
  for (cclist_node_t *n = cclist_rbegin(&l); n; n = cclist_prev(n)) {
    ASSERT(le(n)->val == i); i--;
  }
  ASSERT(i == -1);
}

TEST(next_prev_symmetry) {
  cclist_t l; cclist_init(&l);
  struct entry e1 = {10}, e2 = {20}, e3 = {30};
  cclist_push_back(&l, &e1.node);
  cclist_push_back(&l, &e2.node);
  cclist_push_back(&l, &e3.node);
  /* end sentinels */
  ASSERT(cclist_next(&e3.node) == NULL);
  ASSERT(cclist_prev(&e1.node) == NULL);
  /* roundtrip for middle element */
  ASSERT(cclist_prev(cclist_next(&e2.node)) == &e2.node);
  ASSERT(cclist_next(cclist_prev(&e2.node)) == &e2.node);
}

TEST(clear) {
  cclist_t l; cclist_init(&l);
  struct entry e[3] = {{1},{2},{3}};
  for (int i = 0; i < 3; i++) cclist_push_back(&l, &e[i].node);
  cclist_clear(&l);
  ASSERT(cclist_size(&l) == 0);
  ASSERT(cclist_empty(&l));
  ASSERT(cclist_begin(&l) == NULL);
}

TEST(null_safety) {
  cclist_t l; cclist_init(&l);
  cclist_init(NULL);
  ASSERT(cclist_size(NULL) == 0);
  ASSERT(cclist_empty(NULL));
  ASSERT(cclist_begin(NULL) == NULL);
  ASSERT(cclist_end(NULL) == NULL);
  ASSERT(cclist_rbegin(NULL) == NULL);
  ASSERT(cclist_front(NULL) == NULL);
  ASSERT(cclist_back(NULL) == NULL);
  ASSERT(cclist_next(NULL) == NULL);
  ASSERT(cclist_prev(NULL) == NULL);
  cclist_push_front(NULL, NULL);
  cclist_push_back(NULL, NULL);
  ASSERT(cclist_pop_front(NULL) == NULL);
  ASSERT(cclist_pop_back(NULL) == NULL);
  cclist_insert_before(NULL, NULL, NULL);
  cclist_insert_after(NULL, NULL, NULL);
  cclist_remove(NULL, NULL);
  cclist_splice_back(NULL, NULL);
  cclist_clear(NULL);
  ASSERT(!cclist_has_cycle(NULL));
}

/* ── main ─────────────────────────────────────────────────────────────── */
int main(void) {
  printf("cclist tests:\n");
  RUN(init_empty);
  RUN(push_pop_front);
  RUN(push_pop_back);
  RUN(insert_before_after);
  RUN(remove_node);
  RUN(splice_back);
  RUN(move);
  RUN(verify_valid);
  RUN(has_cycle);
  RUN(iterate_forward_reverse);
  RUN(next_prev_symmetry);
  RUN(clear);
  RUN(null_safety);
  printf("\n  %d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
