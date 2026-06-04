/*
**  Benchmark: ccmap vs std::map
**  Build:  g++ -std=c++11 -O2 -o bench_ccmap bench_ccmap.cpp && ./bench_ccmap
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <map>
#include <chrono>
#include <vector>

#include "../include/ccmap.h"

/* ── ccmap entry + comparator ──────────────────────────────────────────── */
struct ccmap_entry {
  int          key;
  ccmap_node_t node;
};
static ccmap_entry *me(const ccmap_node_t *n) {
  return CCMAP_CONTAINER(n, ccmap_entry, node);
}
static int64_t cmp_fn(const ccmap_node_t *a, const ccmap_node_t *b) {
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

int main() {
  enum { N = 100000 };
  printf("=== ccmap vs std::map  (%d elements) ===\n\n", N);

  auto cc_entries = new ccmap_entry[N];
  auto st_entries = new stl_entry[N];
  for (int i = 0; i < N; i++) { cc_entries[i].key = i; st_entries[i].key = i; }

  /* ── ccmap: insert ──────────────────────────────────────────────────── */
  ccmap_t m; ccmap_init(&m, cmp_fn);
  auto t0 = clk::now();
  for (int i = 0; i < N; i++) ccmap_insert(&m, &cc_entries[i].node, NULL);
  auto t1 = clk::now();
  printf("  insert:  ccmap %8.2f ms\n", ms(t0, t1));

  std::map<stl_entry*, int, stl_cmp> sm;
  auto t2 = clk::now();
  for (int i = 0; i < N; i++) sm[&st_entries[i]] = i;
  auto t3 = clk::now();
  printf("           stl   %8.2f ms\n", ms(t2, t3));
  printf("           ratio %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  /* ── find ───────────────────────────────────────────────────────────── */
  t0 = clk::now();
  for (int i = 0; i < N; i++) { volatile auto *f = ccmap_find(&m, &cc_entries[i].node); (void)f; }
  t1 = clk::now();
  printf("  find:    ccmap %8.2f ms\n", ms(t0, t1));

  t2 = clk::now();
  for (int i = 0; i < N; i++) { volatile auto it = sm.find(&st_entries[i]); (void)it; }
  t3 = clk::now();
  printf("           stl   %8.2f ms\n", ms(t2, t3));
  printf("           ratio %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  /* ── erase ──────────────────────────────────────────────────────────── */
  t0 = clk::now();
  for (int i = 0; i < N; i++) ccmap_erase(&m, &cc_entries[i].node);
  t1 = clk::now();
  printf("  erase:   ccmap %8.2f ms\n", ms(t0, t1));

  t2 = clk::now();
  for (int i = 0; i < N; i++) sm.erase(&st_entries[i]);
  t3 = clk::now();
  printf("           stl   %8.2f ms\n", ms(t2, t3));
  printf("           ratio %8.2fx\n", ms(t0, t1) / ms(t2, t3));

  delete[] cc_entries;
  delete[] st_entries;
  return 0;
}
