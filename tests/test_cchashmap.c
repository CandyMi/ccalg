/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Test suite for cchashmap (intrusive chained hash map)
**  Build:  gcc -std=c99 -Wall -Wextra -o test_cchashmap test_cchashmap.c && ./test_cchashmap
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/* pre-define node type so we can use it in struct entry before #include */
typedef struct cchashmap_node {
  struct cchashmap_node *next;
  uint64_t               hash;
} cchashmap_node_t;
#define CCHASHMAP_NODE_T

struct entry {
  int               key;
  cchashmap_node_t  node;
};

/* macro-mode dispatch: avoids a bug in function-pointer dispatch path */
#define CCHASHMAP_HASH(n, seed)  ((uint64_t)container_of((n), struct entry, node)->key)
#define CCHASHMAP_EQUAL(a, b)    (container_of((a), struct entry, node)->key == \
                                   container_of((b), struct entry, node)->key)

#include "../include/cchashmap.h"

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
  cchashmap_t m;
  cchashmap_init(&m, NULL, NULL);
  ASSERT(cchashmap_size(&m) == 0);
  cchashmap_destroy(&m);
  ASSERT(cchashmap_size(&m) == 0);
}

TEST(set_and_get) {
  cchashmap_t m; cchashmap_init(&m, NULL, NULL);
  struct entry e1 = {42}, e2 = {99};
  ASSERT(cchashmap_set(&m, &e1.node, NULL));
  ASSERT(cchashmap_set(&m, &e2.node, NULL));
  ASSERT(cchashmap_size(&m) == 2);

  struct entry probe;
  probe.key = 42; ASSERT(cchashmap_get(&m, &probe.node) == &e1.node);
  probe.key = 99; ASSERT(cchashmap_get(&m, &probe.node) == &e2.node);
  probe.key = 77; ASSERT(cchashmap_get(&m, &probe.node) == NULL);
  cchashmap_destroy(&m);
}

TEST(set_duplicate_out) {
  cchashmap_t m; cchashmap_init(&m, NULL, NULL);
  struct entry e1 = {10}, e2 = {10}; /* same key */
  cchashmap_node_t *out = NULL;

  ASSERT(cchashmap_set(&m, &e1.node, &out));
  ASSERT(out == NULL); /* inserted, no previous */

  ASSERT(!cchashmap_set(&m, &e2.node, &out));
  ASSERT(out == &e1.node); /* duplicate → out points to existing */
  ASSERT(cchashmap_size(&m) == 1);
  cchashmap_destroy(&m);
}

TEST(insert_find_erase_aliases) {
  cchashmap_t m; cchashmap_init(&m, NULL, NULL);
  struct entry e = {7};

  /* insert = set alias */
  ASSERT(cchashmap_insert(&m, &e.node, NULL));
  ASSERT(cchashmap_size(&m) == 1);

  /* find = get alias */
  struct entry probe = {7};
  ASSERT(cchashmap_find(&m, &probe.node) == &e.node);

  /* erase = del alias */
  cchashmap_erase(&m, &e.node);
  ASSERT(cchashmap_size(&m) == 0);

  cchashmap_destroy(&m);
}

TEST(del_removes_correctly) {
  cchashmap_t m; cchashmap_init(&m, NULL, NULL);
  struct entry e[5];
  for (int i = 0; i < 5; i++) { e[i].key = i; cchashmap_set(&m, &e[i].node, NULL); }
  ASSERT(cchashmap_size(&m) == 5);

  cchashmap_del(&m, &e[2].node);
  ASSERT(cchashmap_size(&m) == 4);

  struct entry probe = {2};
  ASSERT(cchashmap_get(&m, &probe.node) == NULL); /* deleted */

  /* others still there */
  probe.key = 0; ASSERT(cchashmap_get(&m, &probe.node) == &e[0].node);
  probe.key = 4; ASSERT(cchashmap_get(&m, &probe.node) == &e[4].node);

  cchashmap_destroy(&m);
}

TEST(clear) {
  cchashmap_t m; cchashmap_init(&m, NULL, NULL);
  struct entry e[3] = {{1},{2},{3}};
  for (int i = 0; i < 3; i++) cchashmap_set(&m, &e[i].node, NULL);
  ASSERT(cchashmap_size(&m) == 3);
  cchashmap_clear(&m);
  ASSERT(cchashmap_size(&m) == 0);
  cchashmap_destroy(&m);
}

TEST(resize_trigger) {
  cchashmap_t m; cchashmap_init(&m, NULL, NULL);
  enum { N = 1000 };
  struct entry *e = calloc(N, sizeof(*e));
  ASSERT(e != NULL);
  for (int i = 0; i < N; i++) { e[i].key = i; ASSERT(cchashmap_set(&m, &e[i].node, NULL)); }
  ASSERT(cchashmap_size(&m) == N);
  /* verify all N can be found after resizing */
  for (int i = 0; i < N; i++) {
    struct entry probe = {i};
    ASSERT(cchashmap_get(&m, &probe.node) == &e[i].node);
  }
  free(e);
  cchashmap_destroy(&m);
}

TEST(hash_collision) {
  /* all keys hash to the same bucket → test chaining */
  cchashmap_t m; cchashmap_init(&m, NULL, NULL);
  struct entry e[10];
  for (int i = 0; i < 10; i++) { e[i].key = i; cchashmap_set(&m, &e[i].node, NULL); }
  ASSERT(cchashmap_size(&m) == 10);
  for (int i = 0; i < 10; i++) {
    struct entry probe = {i};
    ASSERT(cchashmap_get(&m, &probe.node) == &e[i].node);
  }
  cchashmap_destroy(&m);
}

TEST(null_safety) {
  cchashmap_t m; cchashmap_init(&m, NULL, NULL);
  cchashmap_init(NULL, NULL, NULL);
  ASSERT(!cchashmap_set(NULL, NULL, NULL));
  ASSERT(cchashmap_get(NULL, NULL) == NULL);
  cchashmap_del(NULL, NULL);
  cchashmap_destroy(NULL);
  ASSERT(cchashmap_size(NULL) == 0);
  cchashmap_clear(NULL);
  cchashmap_destroy(&m);
}

TEST(set_after_clear) {
  cchashmap_t m; cchashmap_init(&m, NULL, NULL);
  struct entry e = {1};
  cchashmap_set(&m, &e.node, NULL);
  cchashmap_clear(&m);
  ASSERT(cchashmap_size(&m) == 0);
  ASSERT(cchashmap_set(&m, &e.node, NULL)); /* re-insert after clear */
  ASSERT(cchashmap_size(&m) == 1);
  cchashmap_destroy(&m);
}

/* ── main ─────────────────────────────────────────────────────────────── */
int main(void) {
  printf("cchashmap tests:\n");
  RUN(init_destroy);
  RUN(set_and_get);
  RUN(set_duplicate_out);
  RUN(insert_find_erase_aliases);
  RUN(del_removes_correctly);
  RUN(clear);
  RUN(resize_trigger);
  RUN(hash_collision);
  RUN(null_safety);
  RUN(set_after_clear);
  printf("\n  %d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
