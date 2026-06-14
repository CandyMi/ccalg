/*
**  Benchmark: ccmap1 (Weak AVL) vs std::map
**  Build:  g++ -std=c++11 -O2 -o bench_ccmap1 bench_ccmap1.cpp && ./bench_ccmap1
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <map>
#include <chrono>
#include <vector>

#include "../include/ccmap1.h"

/* ── ccmap1 entry + comparator ─────────────────────────────────────────── */
struct ccmap1_entry {
  int           key;
  ccmap1_node_t node;
};
static ccmap1_entry *me(const ccmap1_node_t *n) {
  return CCMAP1_CONTAINER(n, ccmap1_entry, node);
}
static int64_t cmp_fn(const ccmap1_node_t *a, const ccmap1_node_t *b) {
  return (int64_t)me(a)->key - (int64_t)me(b)->key;
}

/* ── std::map counterpart ──────────────────────────────────────────────── */
struct stl_entry { int key; };
struct stl_cmp { bool operator()(const stl_entry *a, const stl_entry *b) const { return a->key < b->key; } };

/* ── timer ─────────────────────────────────────────────────────────────── */
using clk = std::chrono::high_resolution_clock;
static double ms(clk::time_point s, clk::time_point e) {
  return std::chrono::duration<double, std::milli>(e - s).count();
}

/* anti-optimisation sink */
static volatile long long sink;

int main() {
  enum { N = 100000 };
  printf("=== ccmap1 (Weak AVL) vs std::map  (%d elements) ===\n\n", N);

  auto cc_entries = new ccmap1_entry[N];
  auto st_entries = new stl_entry[N];
  for (int i = 0; i < N; i++) { cc_entries[i].key = i; st_entries[i].key = i; }

  /* ── ccmap1: insert ──────────────────────────────────────────────────── */
  ccmap1_t m; ccmap1_init(&m, cmp_fn);
  auto t0 = clk::now();
  for (int i = 0; i < N; i++) ccmap1_insert(&m, &cc_entries[i].node, NULL);
  auto t1 = clk::now();
  printf("  insert:  ccmap1 %8.2f ms\n", ms(t0, t1));

  std::map<stl_entry*, int, stl_cmp> sm;
  auto t2 = clk::now();
  for (int i = 0; i < N; i++) sm[&st_entries[i]] = i;
  auto t3 = clk::now();
  printf("           stl    %8.2f ms\n", ms(t2, t3));
  printf("           ratio  %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  /* ── find ───────────────────────────────────────────────────────────── */
  sink = 0;
  t0 = clk::now();
  for (int i = 0; i < N; i++) {
    ccmap1_node_t *f = ccmap1_find(&m, &cc_entries[i].node);
    sink += (long long)(uintptr_t)f;
  }
  t1 = clk::now();
  printf("  find:    ccmap1 %8.2f ms  (sum=%lld)\n", ms(t0, t1), sink);

  sink = 0;
  t2 = clk::now();
  for (int i = 0; i < N; i++) {
    auto it = sm.find(&st_entries[i]);
    sink += (long long)(uintptr_t)&*it;
  }
  t3 = clk::now();
  printf("           stl    %8.2f ms  (sum=%lld)\n", ms(t2, t3), sink);
  printf("           ratio  %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  /* ── erase ──────────────────────────────────────────────────────────── */
  t0 = clk::now();
  for (int i = 0; i < N; i++) ccmap1_erase(&m, &cc_entries[i].node);
  t1 = clk::now();
  printf("  erase:   ccmap1 %8.2f ms\n", ms(t0, t1));

  t2 = clk::now();
  for (int i = 0; i < N; i++) sm.erase(&st_entries[i]);
  t3 = clk::now();
  printf("           stl    %8.2f ms\n", ms(t2, t3));
  printf("           ratio  %8.2fx\n", ms(t0, t1) / ms(t2, t3));

  delete[] cc_entries;
  delete[] st_entries;
  return 0;
}