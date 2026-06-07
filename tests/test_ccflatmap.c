/*
**  Test suite for ccflatmap (sorted-array map)
**  Build:  gcc -std=c99 -Wall -Wextra -o test_ccflatmap test_ccflatmap.c && ./test_ccflatmap
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "../include/ccflatmap.h"

/* ── test harness ─────────────────────────────────────────────────────── */
static int passed, failed;
#define TEST(name) static void test_##name(void)
#define ASSERT(cond) do { \
  if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failed++; } \
  else passed++; \
} while(0)
#define RUN(name) do { printf("  %s\n", #name); test_##name(); } while(0)

/* ── comparison (function pointer mode) ──────────────────────────────── */
static int64_t cmp_fn(const ccflatmap_node_t *a, const ccflatmap_node_t *b) {
  return (int64_t)a->key - (int64_t)b->key;
}

/* ── tests ────────────────────────────────────────────────────────────── */

TEST(init_empty) {
  ccflatmap_t m;
  ASSERT(ccflatmap_init(&m, cmp_fn) == 0);
  ASSERT(ccflatmap_size(&m) == 0);
  ASSERT(ccflatmap_capacity(&m) == 8);
  ASSERT(ccflatmap_empty(&m));
  ccflatmap_destroy(&m);
}

TEST(insert_and_size) {
  ccflatmap_t m; ccflatmap_init(&m, cmp_fn);
  for (int i = 0; i < 100; i++) {
    ccflatmap_node_t n = {i, (uint64_t)(i * 2)};
    ASSERT(ccflatmap_insert(&m, n, NULL) == 0);
  }
  ASSERT(ccflatmap_size(&m) == 100);
  ASSERT(!ccflatmap_empty(&m));
  ccflatmap_destroy(&m);
}

TEST(insert_duplicate) {
  ccflatmap_t m; ccflatmap_init(&m, cmp_fn);
  ccflatmap_node_t n = {42, 99};
  ASSERT(ccflatmap_insert(&m, n, NULL) == 0);
  /* same key → duplicate */
  ccflatmap_node_t n2 = {42, 77};
  ccflatmap_node_t *out = NULL;
  ASSERT(ccflatmap_insert(&m, n2, &out) == -1);
  ASSERT(out != NULL);
  ASSERT(out->key == 42);
  ASSERT(out->value == 99);  /* original value preserved */
  ccflatmap_destroy(&m);
}

TEST(find) {
  ccflatmap_t m; ccflatmap_init(&m, cmp_fn);
  for (int i = 0; i < 50; i++) {
    ccflatmap_node_t n = {i * 2, (uint64_t)i};  /* even keys */
    ccflatmap_insert(&m, n, NULL);
  }
  /* find existing */
  ccflatmap_node_t probe = {24, 0};
  ccflatmap_node_t *f = ccflatmap_find(&m, &probe);
  ASSERT(f != NULL);
  ASSERT(f->key == 24);
  ASSERT(f->value == 12);
  /* find missing */
  probe.key = 25;
  ASSERT(ccflatmap_find(&m, &probe) == NULL);
  ccflatmap_destroy(&m);
}

TEST(erase) {
  ccflatmap_t m; ccflatmap_init(&m, cmp_fn);
  for (int i = 0; i < 10; i++) {
    ccflatmap_node_t n = {i, (uint64_t)i};
    ccflatmap_insert(&m, n, NULL);
  }
  ASSERT(ccflatmap_size(&m) == 10);

  ccflatmap_node_t probe = {5, 0};
  ccflatmap_erase(&m, &probe);
  ASSERT(ccflatmap_size(&m) == 9);
  ASSERT(ccflatmap_find(&m, &probe) == NULL);

  /* erase non-existing */
  probe.key = 999;
  ccflatmap_erase(&m, &probe);
  ASSERT(ccflatmap_size(&m) == 9);

  /* erase first */
  probe.key = 0;
  ccflatmap_erase(&m, &probe);
  ASSERT(ccflatmap_size(&m) == 8);

  /* erase last */
  probe.key = 9;
  ccflatmap_erase(&m, &probe);
  ASSERT(ccflatmap_size(&m) == 7);

  /* remaining in order */
  int expected[] = {1, 2, 3, 4, 6, 7, 8};
  for (size_t i = 0; i < ccflatmap_size(&m); i++) {
    ASSERT(ccflatmap_at(&m, i)->key == expected[i]);
  }
  ccflatmap_destroy(&m);
}

TEST(erase_at) {
  ccflatmap_t m; ccflatmap_init(&m, cmp_fn);
  for (int i = 0; i < 10; i++) {
    ccflatmap_node_t n = {i, (uint64_t)i};
    ccflatmap_insert(&m, n, NULL);
  }
  /* erase middle by index */
  ccflatmap_erase_at(&m, 5);
  ASSERT(ccflatmap_size(&m) == 9);
  ASSERT(ccflatmap_at(&m, 5)->key == 6); /* shifted */

  /* erase first */
  ccflatmap_erase_at(&m, 0);
  ASSERT(ccflatmap_at(&m, 0)->key == 1);

  /* erase last */
  ccflatmap_erase_at(&m, ccflatmap_size(&m) - 1);
  ASSERT(ccflatmap_size(&m) == 7);

  /* out of bounds */
  ccflatmap_erase_at(&m, 999);
  ASSERT(ccflatmap_size(&m) == 7);
  ccflatmap_destroy(&m);
}

TEST(erase_unordered) {
  ccflatmap_t m; ccflatmap_init(&m, cmp_fn);
  for (int i = 0; i < 20; i++) {
    ccflatmap_node_t n = {i, (uint64_t)i};
    ccflatmap_insert(&m, n, NULL);
  }
  ASSERT(ccflatmap_size(&m) == 20);

  /* unordered erase a few — breaks sorted order */
  ccflatmap_node_t probe = {5, 0};
  ccflatmap_erase_unordered(&m, &probe);
  ASSERT(ccflatmap_size(&m) == 19);

  probe.key = 12;
  ccflatmap_erase_unordered(&m, &probe);
  ASSERT(ccflatmap_size(&m) == 18);

  /* after unordered erasures, re-sort to restore find */
  ccflatmap_sort(&m);

  /* all remaining keys findable */
  for (int i = 0; i < 20; i++) {
    if (i == 5 || i == 12) continue;
    probe.key = i;
    ASSERT(ccflatmap_find(&m, &probe) != NULL);
  }
  ASSERT(ccflatmap_find(&m, &(ccflatmap_node_t){5, 0}) == NULL);
  ASSERT(ccflatmap_find(&m, &(ccflatmap_node_t){12, 0}) == NULL);
  ccflatmap_destroy(&m);
}

TEST(clear) {
  ccflatmap_t m; ccflatmap_init(&m, cmp_fn);
  for (int i = 0; i < 10; i++) {
    ccflatmap_node_t n = {i, (uint64_t)i};
    ccflatmap_insert(&m, n, NULL);
  }
  ASSERT(ccflatmap_size(&m) == 10);
  ccflatmap_clear(&m);
  ASSERT(ccflatmap_size(&m) == 0);
  ASSERT(ccflatmap_empty(&m));
  /* re-insert after clear */
  ccflatmap_node_t n = {99, 1};
  ASSERT(ccflatmap_insert(&m, n, NULL) == 0);
  ASSERT(ccflatmap_find(&m, &n) != NULL);
  ccflatmap_destroy(&m);
}

TEST(iterate_forward) {
  ccflatmap_t m; ccflatmap_init(&m, cmp_fn);
  for (int i = 99; i >= 0; i--) {    /* insert reverse */
    ccflatmap_node_t n = {i, (uint64_t)i};
    ccflatmap_insert(&m, n, NULL);
  }
  int expect = 0;
  for (ccflatmap_node_t *p = ccflatmap_begin(&m); p; p = ccflatmap_next(&m, p)) {
    ASSERT(p->key == expect);
    expect++;
  }
  ASSERT(expect == 100);
  ccflatmap_destroy(&m);
}

TEST(iterate_reverse) {
  ccflatmap_t m; ccflatmap_init(&m, cmp_fn);
  for (int i = 0; i < 50; i++) {
    ccflatmap_node_t n = {i, (uint64_t)i};
    ccflatmap_insert(&m, n, NULL);
  }
  int expect = 49;
  for (ccflatmap_node_t *p = ccflatmap_rbegin(&m); p; p = ccflatmap_prev(&m, p)) {
    ASSERT(p->key == expect);
    expect--;
  }
  ASSERT(expect == -1);
  ccflatmap_destroy(&m);
}

TEST(end_sentinel) {
  ccflatmap_t m; ccflatmap_init(&m, cmp_fn);
  ASSERT(ccflatmap_end(&m) == NULL);
  ccflatmap_node_t n = {1, 1};
  ccflatmap_insert(&m, n, NULL);
  ASSERT(ccflatmap_end(&m) == NULL);
  ccflatmap_destroy(&m);
}

TEST(null_safety) {
  ASSERT(ccflatmap_init(NULL, cmp_fn) == -1);
  ASSERT(ccflatmap_size(NULL) == 0);
  ASSERT(ccflatmap_capacity(NULL) == 0);
  ASSERT(ccflatmap_empty(NULL));
  ASSERT(ccflatmap_find(NULL, &(ccflatmap_node_t){0}) == NULL);
  ASSERT(ccflatmap_at(NULL, 0) == NULL);
  ASSERT(ccflatmap_begin(NULL) == NULL);
  ASSERT(ccflatmap_end(NULL) == NULL);
  ASSERT(ccflatmap_rbegin(NULL) == NULL);
  ASSERT(ccflatmap_next(NULL, NULL) == NULL);
  ASSERT(ccflatmap_prev(NULL, NULL) == NULL);
  ccflatmap_destroy(NULL);
  ccflatmap_clear(NULL);
  ccflatmap_erase(NULL, &(ccflatmap_node_t){0});
  ccflatmap_erase_at(NULL, 0);
  ccflatmap_erase_unordered(NULL, &(ccflatmap_node_t){0});
  ccflatmap_reserve(NULL, 100);
}

TEST(large_insert) {
  ccflatmap_t m; ccflatmap_init(&m, cmp_fn);
  enum { N = 5000 };
  /* insert in reverse → exercises mid-array shifts */
  for (int i = N - 1; i >= 0; i--) {
    ccflatmap_node_t n = {i, (uint64_t)i};
    ASSERT(ccflatmap_insert(&m, n, NULL) == 0);
  }
  ASSERT(ccflatmap_size(&m) == N);
  /* verify sorted */
  for (int i = 0; i < N; i++) {
    ASSERT(ccflatmap_at(&m, (size_t)i)->key == i);
  }
  /* find all */
  for (int i = 0; i < N; i++) {
    ccflatmap_node_t probe = {i, 0};
    ccflatmap_node_t *f = ccflatmap_find(&m, &probe);
    ASSERT(f != NULL && f->key == i);
  }
  /* erase all from the middle out */
  for (int i = 0; i < N; i++) {
    ccflatmap_node_t probe = {i, 0};
    ccflatmap_erase(&m, &probe);
  }
  ASSERT(ccflatmap_size(&m) == 0);
  ccflatmap_destroy(&m);
}

TEST(reserve) {
  ccflatmap_t m; ccflatmap_init(&m, cmp_fn);
  ASSERT(ccflatmap_capacity(&m) == 8);
  ASSERT(ccflatmap_reserve(&m, 1024) == 0);
  ASSERT(ccflatmap_capacity(&m) == 1024);
  /* fill without resize */
  for (int i = 0; i < 1024; i++) {
    ccflatmap_node_t n = {i, (uint64_t)i};
    ccflatmap_insert(&m, n, NULL);
  }
  ASSERT(ccflatmap_size(&m) == 1024);
  ASSERT(ccflatmap_capacity(&m) == 1024);
  ccflatmap_destroy(&m);
}

TEST(push_back_and_sort) {
  ccflatmap_t m; ccflatmap_init(&m, cmp_fn);
  /* push in reverse order (unsorted) */
  for (int i = 99; i >= 0; i--) {
    ccflatmap_node_t n = {i, (uint64_t)i};
    ASSERT(ccflatmap_push_back(&m, n) == 0);
  }
  ASSERT(ccflatmap_size(&m) == 100);
  /* verify currently unsorted */
  ASSERT(ccflatmap_at(&m, 0)->key == 99);

  ccflatmap_sort(&m);

  /* now sorted ascending */
  for (int i = 0; i < 100; i++)
    ASSERT(ccflatmap_at(&m, (size_t)i)->key == i);
  ccflatmap_destroy(&m);
}

TEST(sort_preserves_find) {
  ccflatmap_t m; ccflatmap_init(&m, cmp_fn);
  for (int i = 0; i < 200; i++) {
    ccflatmap_node_t n = {i * 3, (uint64_t)i};  /* sparse keys */
    ccflatmap_push_back(&m, n);
  }
  ccflatmap_sort(&m);

  /* all keys findable */
  for (int i = 0; i < 200; i++) {
    ccflatmap_node_t probe = {i * 3, 0};
    ccflatmap_node_t *f = ccflatmap_find(&m, &probe);
    ASSERT(f != NULL);
    ASSERT(f->value == (uint64_t)i);
  }
  /* missing keys */
  ccflatmap_node_t probe = {5, 0};
  ASSERT(ccflatmap_find(&m, &probe) == NULL);
  ccflatmap_destroy(&m);
}

/* ── main ─────────────────────────────────────────────────────────────── */
int main(void) {
  printf("ccflatmap tests:\n");
  RUN(init_empty);
  RUN(insert_and_size);
  RUN(insert_duplicate);
  RUN(find);
  RUN(erase);
  RUN(erase_at);
  RUN(erase_unordered);
  RUN(clear);
  RUN(iterate_forward);
  RUN(iterate_reverse);
  RUN(end_sentinel);
  RUN(null_safety);
  RUN(large_insert);
  RUN(reserve);
  RUN(push_back_and_sort);
  RUN(sort_preserves_find);
  printf("\n  %d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
