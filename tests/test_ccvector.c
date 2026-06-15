/*
**  Test suite for ccvector (dynamic array)
**  Build:  gcc -std=c99 -Wall -Wextra -o test_ccvector test_ccvector.c && ./test_ccvector
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "../include/ccvector.h"

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
  ccvector_t v;
  ASSERT(ccvector_init(&v) == 0);
  ASSERT(ccvector_size(&v) == 0);
  ASSERT(ccvector_capacity(&v) == 8);
  ASSERT(ccvector_empty(&v));
  ccvector_destroy(&v);
  ASSERT(ccvector_size(&v) == 0);
}

TEST(push_back_and_access) {
  ccvector_t v; ccvector_init(&v);
  ccvector_node_t e1 = {.value = 10};
  ccvector_node_t e2 = {.value = 20};
  ccvector_node_t e3 = {.value = 30};

  ASSERT(ccvector_push_back(&v, e1) == 0);
  ASSERT(ccvector_push_back(&v, e2) == 0);
  ASSERT(ccvector_push_back(&v, e3) == 0);
  ASSERT(ccvector_size(&v) == 3);

  ASSERT(ccvector_at(&v, 0)->value == 10);
  ASSERT(ccvector_at(&v, 1)->value == 20);
  ASSERT(ccvector_at(&v, 2)->value == 30);
  ASSERT(ccvector_at(&v, 3) == NULL);

  ASSERT(ccvector_front(&v)->value == 10);
  ASSERT(ccvector_back(&v)->value == 30);

  ccvector_destroy(&v);
}

TEST(pop_back) {
  ccvector_t v; ccvector_init(&v);
  for (int i = 0; i < 5; i++) {
    ccvector_node_t e = {.value = (uint32_t)i};
    ccvector_push_back(&v, e);
  }
  ASSERT(ccvector_size(&v) == 5);
  ASSERT(ccvector_pop_back(&v) == 0);
  ASSERT(ccvector_size(&v) == 4);
  ASSERT(ccvector_back(&v)->value == 3);
  ASSERT(ccvector_pop_back(&v) == 0);
  ASSERT(ccvector_pop_back(&v) == 0);
  ASSERT(ccvector_pop_back(&v) == 0);
  ASSERT(ccvector_pop_back(&v) == 0);
  ASSERT(ccvector_size(&v) == 0);
  ASSERT(ccvector_empty(&v));
  ASSERT(ccvector_pop_back(&v) == -1);  /* empty */
  ccvector_destroy(&v);
}

TEST(clear) {
  ccvector_t v; ccvector_init(&v);
  for (int i = 0; i < 10; i++) {
    ccvector_node_t e = {.value = (uint32_t)i};
    ccvector_push_back(&v, e);
  }
  ASSERT(ccvector_size(&v) == 10);
  ccvector_clear(&v);
  ASSERT(ccvector_size(&v) == 0);
  ASSERT(ccvector_empty(&v));
  /* can push after clear */
  ccvector_node_t e = {.value = 99};
  ASSERT(ccvector_push_back(&v, e) == 0);
  ASSERT(ccvector_front(&v)->value == 99);
  ccvector_destroy(&v);
}

TEST(auto_grow) {
  ccvector_t v; ccvector_init(&v);
  /* default cap is 8, push 200 to trigger multiple resizes */
  for (int i = 0; i < 200; i++) {
    ccvector_node_t e = {.value = (uint32_t)i};
    ASSERT(ccvector_push_back(&v, e) == 0);
  }
  ASSERT(ccvector_size(&v) == 200);
  ASSERT(ccvector_capacity(&v) >= 200);
  for (int i = 0; i < 200; i++)
    ASSERT(ccvector_at(&v, (size_t)i)->value == (uint32_t)i);
  ccvector_destroy(&v);
}

TEST(reserve) {
  ccvector_t v; ccvector_init(&v);
  ASSERT(ccvector_capacity(&v) == 8);
  ASSERT(ccvector_reserve(&v, 100) == 0);
  ASSERT(ccvector_capacity(&v) == 100);
  /* 100 elements fit without resize */
  for (int i = 0; i < 100; i++) {
    ccvector_node_t e = {.value = (uint32_t)i};
    ASSERT(ccvector_push_back(&v, e) == 0);
  }
  ASSERT(ccvector_size(&v) == 100);
  ASSERT(ccvector_capacity(&v) == 100);
  ccvector_destroy(&v);
}

TEST(init_cap) {
  ccvector_t v;

  /* explicit capacity */
  ASSERT(ccvector_init_cap(&v, 64) == 0);
  ASSERT(ccvector_size(&v) == 0);
  ASSERT(ccvector_capacity(&v) == 64);
  ccvector_destroy(&v);

  /* cap=0 falls back to default */
  ASSERT(ccvector_init_cap(&v, 0) == 0);
  ASSERT(ccvector_capacity(&v) == CCVECTOR_DEFAULT_CAP);
  ccvector_destroy(&v);

  /* large capacity */
  ASSERT(ccvector_init_cap(&v, 10000) == 0);
  ASSERT(ccvector_capacity(&v) == 10000);
  /* fill without resize */
  for (int i = 0; i < 10000; i++) {
    ccvector_node_t e = {.value = (uint32_t)i};
    ASSERT(ccvector_push_back(&v, e) == 0);
  }
  ASSERT(ccvector_size(&v) == 10000);
  ASSERT(ccvector_capacity(&v) == 10000);
  ccvector_destroy(&v);

  /* NULL safety */
  ASSERT(ccvector_init_cap(NULL, 10) == -1);
}

TEST(null_safety) {
  ASSERT(ccvector_init(NULL) == -1);
  ASSERT(ccvector_init_cap(NULL, 10) == -1);
  ASSERT(ccvector_push_back(NULL, (ccvector_node_t){0}) == -1);
  ASSERT(ccvector_pop_back(NULL) == -1);
  ASSERT(ccvector_at(NULL, 0) == NULL);
  ASSERT(ccvector_front(NULL) == NULL);
  ASSERT(ccvector_back(NULL) == NULL);
  ASSERT(ccvector_size(NULL) == 0);
  ASSERT(ccvector_capacity(NULL) == 0);
  ASSERT(ccvector_empty(NULL));
  ASSERT(ccvector_reserve(NULL, 10) == -1);
  ccvector_destroy(NULL); /* should not crash */
}

/* ── sort ───────────────────────────────────────────────────────────────── */

static int
sort_asc(const void *a, const void *b) {
  uint32_t va = ((const ccvector_node_t *)a)->value;
  uint32_t vb = ((const ccvector_node_t *)b)->value;
  return (va > vb) - (va < vb);
}

static int
sort_desc(const void *a, const void *b) {
  uint32_t va = ((const ccvector_node_t *)a)->value;
  uint32_t vb = ((const ccvector_node_t *)b)->value;
  return (vb > va) - (vb < va);
}

static int
sort_by_mod3(const void *a, const void *b) {
  uint32_t va = ((const ccvector_node_t *)a)->value % 3;
  uint32_t vb = ((const ccvector_node_t *)b)->value % 3;
  return (va > vb) - (va < vb);
}

TEST(sort_basic) {
  ccvector_t v; ccvector_init(&v);
  uint32_t vals[] = {5, 3, 9, 1, 7, 2, 8, 4, 6, 0};
  for (size_t i = 0; i < 10; i++) {
    ccvector_node_t e = {.value = vals[i]};
    ccvector_push_back(&v, e);
  }

  ccvector_sort(&v, sort_asc);
  ASSERT(ccvector_size(&v) == 10);
  for (size_t i = 0; i < 10; i++)
    ASSERT(ccvector_at(&v, i)->value == (uint32_t)i);

  ccvector_destroy(&v);
}

TEST(sort_descending) {
  ccvector_t v; ccvector_init(&v);
  for (uint32_t i = 0; i < 10; i++) {
    ccvector_node_t e = {.value = i};
    ccvector_push_back(&v, e);
  }

  ccvector_sort(&v, sort_desc);
  ASSERT(ccvector_size(&v) == 10);
  for (size_t i = 0; i < 10; i++)
    ASSERT(ccvector_at(&v, i)->value == (uint32_t)(9 - i));

  ccvector_destroy(&v);
}

TEST(sort_already_sorted) {
  ccvector_t v; ccvector_init(&v);
  for (uint32_t i = 0; i < 100; i++) {
    ccvector_node_t e = {.value = i};
    ccvector_push_back(&v, e);
  }

  /* sort already-ascending data — must remain correct */
  ccvector_sort(&v, sort_asc);
  ASSERT(ccvector_size(&v) == 100);
  for (size_t i = 0; i < 100; i++)
    ASSERT(ccvector_at(&v, i)->value == (uint32_t)i);

  ccvector_destroy(&v);
}

TEST(sort_single_element) {
  ccvector_t v; ccvector_init(&v);
  ccvector_node_t e = {.value = 42};
  ccvector_push_back(&v, e);

  /* single element — no-op */
  ccvector_sort(&v, sort_asc);
  ASSERT(ccvector_size(&v) == 1);
  ASSERT(ccvector_at(&v, 0)->value == 42);

  ccvector_destroy(&v);
}

TEST(sort_empty) {
  ccvector_t v; ccvector_init(&v);

  /* empty — no-op, must not crash */
  ccvector_sort(&v, sort_asc);
  ASSERT(ccvector_empty(&v));

  ccvector_destroy(&v);
}

TEST(sort_custom_comparator) {
  ccvector_t v; ccvector_init(&v);
  uint32_t vals[] = {2, 5, 1, 4, 3, 0};
  for (size_t i = 0; i < 6; i++) {
    ccvector_node_t e = {.value = vals[i]};
    ccvector_push_back(&v, e);
  }

  /* sort by value % 3 — stable groups: 0,3 / 1,4 / 2,5 */
  ccvector_sort(&v, sort_by_mod3);
  ASSERT(ccvector_size(&v) == 6);
  /* after mod-3 sort: 3,0,1,4,2,5  (depends on qsort stability, just check ordering by mod3) */
  for (size_t i = 1; i < 6; i++) {
    uint32_t a = ccvector_at(&v, i - 1)->value % 3;
    uint32_t b = ccvector_at(&v, i)->value % 3;
    ASSERT(a <= b);
  }

  ccvector_destroy(&v);
}

TEST(sort_large) {
  ccvector_t v; ccvector_init_cap(&v, 5000);
  for (int32_t i = 4999; i >= 0; i--) {
    ccvector_node_t e = {.value = (uint32_t)i};
    ccvector_push_back(&v, e);
  }

  ccvector_sort(&v, sort_asc);
  ASSERT(ccvector_size(&v) == 5000);
  for (size_t i = 0; i < 5000; i++)
    ASSERT(ccvector_at(&v, i)->value == (uint32_t)i);

  ccvector_destroy(&v);
}

TEST(sort_null_safety) {
  /* must not crash */
  ccvector_sort(NULL, sort_asc);
}

/* ── main ─────────────────────────────────────────────────────────────── */
int main(void) {
  printf("ccvector tests:\n");
  RUN(init_destroy);
  RUN(push_back_and_access);
  RUN(pop_back);
  RUN(clear);
  RUN(auto_grow);
  RUN(reserve);
  RUN(init_cap);
  RUN(null_safety);
  RUN(sort_basic);
  RUN(sort_descending);
  RUN(sort_already_sorted);
  RUN(sort_single_element);
  RUN(sort_empty);
  RUN(sort_custom_comparator);
  RUN(sort_large);
  RUN(sort_null_safety);
  printf("\n  %d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
