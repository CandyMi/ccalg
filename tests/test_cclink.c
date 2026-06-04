/*
**  Test suite for cclink (intrusive singly-linked list)
**  Build:  gcc -std=c99 -Wall -Wextra -o test_cclink test_cclink.c && ./test_cclink
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../include/cclink.h"

struct entry {
  int            val;
  cclink_node_t  node;
};
static struct entry *le(const cclink_node_t *n) {
  return CCLINK_CONTAINER(n, struct entry, node);
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
  cclink_t l; cclink_init(&l);
  ASSERT(cclink_size(&l) == 0);
  ASSERT(cclink_empty(&l));
  ASSERT(cclink_begin(&l) == NULL);
  ASSERT(cclink_end(&l) == NULL);
  ASSERT(cclink_front(&l) == NULL);
}

TEST(push_and_iterate) {
  cclink_t l; cclink_init(&l);
  struct entry e1 = {10}, e2 = {20}, e3 = {30};
  cclink_push(&l, &e1.node);
  cclink_push(&l, &e2.node);
  cclink_push(&l, &e3.node);
  ASSERT(cclink_size(&l) == 3);
  /* LIFO order: 30, 20, 10 */
  cclink_node_t *n = cclink_begin(&l);
  ASSERT(le(n)->val == 30);
  n = cclink_next(n); ASSERT(le(n)->val == 20);
  n = cclink_next(n); ASSERT(le(n)->val == 10);
  n = cclink_next(n); ASSERT(n == NULL);
}

TEST(push_back) {
  cclink_t l; cclink_init(&l);
  struct entry e1 = {1}, e2 = {2}, e3 = {3};
  cclink_push_back(&l, &e1.node);
  cclink_push_back(&l, &e2.node);
  cclink_push_back(&l, &e3.node);
  ASSERT(cclink_size(&l) == 3);
  cclink_node_t *n = cclink_begin(&l);
  ASSERT(le(n)->val == 1);
  n = cclink_next(n); ASSERT(le(n)->val == 2);
  n = cclink_next(n); ASSERT(le(n)->val == 3);
}

TEST(remove_head) {
  cclink_t l; cclink_init(&l);
  struct entry e[3] = {{1},{2},{3}};
  for (int i = 0; i < 3; i++) cclink_push_back(&l, &e[i].node);
  cclink_remove(&l, &e[0].node);
  ASSERT(cclink_size(&l) == 2);
  ASSERT(le(cclink_front(&l))->val == 2);
}

TEST(remove_middle) {
  cclink_t l; cclink_init(&l);
  struct entry e[3] = {{1},{2},{3}};
  for (int i = 0; i < 3; i++) cclink_push_back(&l, &e[i].node);
  cclink_remove(&l, &e[1].node);
  ASSERT(cclink_size(&l) == 2);
  ASSERT(le(cclink_front(&l))->val == 1);
  ASSERT(le(cclink_next(cclink_front(&l)))->val == 3);
}

TEST(remove_tail) {
  cclink_t l; cclink_init(&l);
  struct entry e[3] = {{1},{2},{3}};
  for (int i = 0; i < 3; i++) cclink_push_back(&l, &e[i].node);
  cclink_remove(&l, &e[2].node);
  ASSERT(cclink_size(&l) == 2);
  cclink_node_t *n = cclink_begin(&l);
  ASSERT(le(n)->val == 1);
  n = cclink_next(n); ASSERT(le(n)->val == 2);
  n = cclink_next(n); ASSERT(n == NULL);
}

TEST(pop_front) {
  cclink_t l; cclink_init(&l);
  struct entry e1 = {1}, e2 = {2}, e3 = {3};
  cclink_push_back(&l, &e1.node);
  cclink_push_back(&l, &e2.node);
  cclink_push_back(&l, &e3.node);

  cclink_node_t *n = cclink_pop_front(&l);
  ASSERT(n == &e1.node);
  ASSERT(cclink_size(&l) == 2);
  ASSERT(cclink_front(&l) == &e2.node);

  n = cclink_pop_front(&l);
  ASSERT(n == &e2.node);
  ASSERT(cclink_size(&l) == 1);

  n = cclink_pop_front(&l);
  ASSERT(n == &e3.node);
  ASSERT(cclink_empty(&l));

  ASSERT(cclink_pop_front(&l) == NULL);  /* empty */
}

TEST(remove_absent) {
  cclink_t l; cclink_init(&l);
  struct entry e1 = {1}, e2 = {2};
  cclink_push_back(&l, &e1.node);
  cclink_remove(&l, &e2.node);  /* not in list */
  ASSERT(cclink_size(&l) == 1);
}

TEST(clear) {
  cclink_t l; cclink_init(&l);
  struct entry e[3] = {{1},{2},{3}};
  for (int i = 0; i < 3; i++) cclink_push_back(&l, &e[i].node);
  cclink_clear(&l);
  ASSERT(cclink_size(&l) == 0);
  ASSERT(cclink_empty(&l));
}

TEST(verify_valid) {
  cclink_t l; cclink_init(&l);
  ASSERT(cclink_verify(&l) == CLINK_NOERROR);
  struct entry e[3] = {{1},{2},{3}};
  for (int i = 0; i < 3; i++) cclink_push_back(&l, &e[i].node);
  ASSERT(cclink_verify(&l) == CLINK_NOERROR);
}

TEST(has_cycle) {
  cclink_t l; cclink_init(&l);
  ASSERT(!cclink_has_cycle(&l));
  struct entry e1 = {1}, e2 = {2};
  cclink_push_back(&l, &e1.node);
  cclink_push_back(&l, &e2.node);
  ASSERT(!cclink_has_cycle(&l));
  e2.node.next = &e1.node;  /* manually create cycle */
  ASSERT(cclink_has_cycle(&l));
}

TEST(front_and_next) {
  cclink_t l; cclink_init(&l);
  struct entry e = {42};
  cclink_push(&l, &e.node);
  ASSERT(cclink_front(&l) == &e.node);
  ASSERT(cclink_next(&e.node) == NULL);
  ASSERT(cclink_next(NULL) == NULL);
}

TEST(null_safety) {
  cclink_t l; cclink_init(&l);
  cclink_init(NULL);
  ASSERT(cclink_size(NULL) == 0);
  ASSERT(cclink_empty(NULL));
  ASSERT(cclink_begin(NULL) == NULL);
  ASSERT(cclink_end(NULL) == NULL);
  ASSERT(cclink_front(NULL) == NULL);
  ASSERT(cclink_next(NULL) == NULL);
  cclink_push(NULL, NULL);
  cclink_push_back(NULL, NULL);
  cclink_remove(NULL, NULL);
  cclink_clear(NULL);
  ASSERT(!cclink_has_cycle(NULL));
  ASSERT(cclink_verify(NULL) == CLINK_ISNULL);
}

TEST(large_push_remove) {
  cclink_t l; cclink_init(&l);
  enum { N = 1000 };
  struct entry *e = calloc(N, sizeof(*e));
  ASSERT(e != NULL);
  for (int i = 0; i < N; i++) { e[i].val = i; cclink_push(&l, &e[i].node); }
  ASSERT(cclink_size(&l) == (size_t)N);
  ASSERT(cclink_verify(&l) == CLINK_NOERROR);
  /* remove all */
  for (int i = 0; i < N; i++) cclink_remove(&l, &e[i].node);
  ASSERT(cclink_empty(&l));
  free(e);
}

/* ── main ─────────────────────────────────────────────────────────────── */
int main(void) {
  printf("cclink tests:\n");
  RUN(init_empty);
  RUN(push_and_iterate);
  RUN(push_back);
  RUN(remove_head);
  RUN(remove_middle);
  RUN(remove_tail);
  RUN(pop_front);
  RUN(remove_absent);
  RUN(clear);
  RUN(verify_valid);
  RUN(has_cycle);
  RUN(front_and_next);
  RUN(null_safety);
  RUN(large_push_remove);
  printf("\n  %d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
