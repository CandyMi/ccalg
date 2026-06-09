/*
**  Benchmark: cctreap vs std::map
**  Build:  g++ -std=c++11 -O2 -o bench_cctreap bench_cctreap.cpp && ./bench_cctreap
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <map>
#include <chrono>
#include <vector>

#include "../include/cctreap.h"

/* ── cctreap entry + comparators ──────────────────────────────────────── */
struct treap_entry {
  int             key;
  uint64_t        priority;
  cctreap_node_t  node;
};
static treap_entry *te(const cctreap_node_t *n) {
  return CCTREAP_CONTAINER(n, treap_entry, node);
}
static int64_t key_cmp(const cctreap_node_t *a, const cctreap_node_t *b) {
  return (int64_t)te(a)->key - (int64_t)te(b)->key;
}
static int64_t prio_cmp(const cctreap_node_t *a, const cctreap_node_t *b) {
  uint64_t pa = te(a)->priority, pb = te(b)->priority;
  if (pa > pb) return 1;
  if (pa < pb) return -1;
  return 0;
}

/* ── simple LCG ───────────────────────────────────────────────────────── */
static uint64_t rng_state = 0xCAFEBABEDEADBEEFULL;
static uint64_t rng(void) {
  rng_state = rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
  return rng_state;
}

/* ── std::map counterpart ──────────────────────────────────────────────── */
struct stl_entry { int key; };
struct stl_cmp { bool operator()(const stl_entry *a, const stl_entry *b) const { return a->key < b->key; } };

/* ── timer ─────────────────────────────────────────────────────────────── */
using clk = std::chrono::high_resolution_clock;
static double ms(clk::time_point s, clk::time_point e) {
  return std::chrono::duration<double, std::milli>(e - s).count();
}
static volatile long long sink;

int main() {
  enum { N = 100000 };
  printf("=== cctreap vs std::map  (%d elements) ===\n\n", N);

  auto tr_entries = new treap_entry[N];
  auto st_entries = new stl_entry[N];
  for (int i = 0; i < N; i++) {
    tr_entries[i].key      = i;
    tr_entries[i].priority = rng();
    st_entries[i].key      = i;
  }

  /* ── cctreap: insert ────────────────────────────────────────────────── */
  cctreap_t t; cctreap_init(&t, key_cmp, prio_cmp);
  auto t0 = clk::now();
  for (int i = 0; i < N; i++) cctreap_insert(&t, &tr_entries[i].node, NULL);
  auto t1 = clk::now();
  printf("  insert:  cctreap %8.2f ms\n", ms(t0, t1));

  std::map<stl_entry*, int, stl_cmp> sm;
  auto t2 = clk::now();
  for (int i = 0; i < N; i++) sm[&st_entries[i]] = i;
  auto t3 = clk::now();
  printf("           stl     %8.2f ms\n", ms(t2, t3));
  printf("           ratio   %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  /* ── find ───────────────────────────────────────────────────────────── */
  sink = 0;
  t0 = clk::now();
  for (int i = 0; i < N; i++) {
    cctreap_node_t *f = cctreap_find(&t, &tr_entries[i].node);
    sink += (long long)(uintptr_t)f;
  }
  t1 = clk::now();
  printf("  find:    cctreap %8.2f ms  (sum=%lld)\n", ms(t0, t1), sink);

  sink = 0;
  t2 = clk::now();
  for (int i = 0; i < N; i++) {
    auto it = sm.find(&st_entries[i]);
    sink += (long long)(uintptr_t)&*it;
  }
  t3 = clk::now();
  printf("           stl     %8.2f ms  (sum=%lld)\n", ms(t2, t3), sink);
  printf("           ratio   %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  /* ── kth (treap-only) ───────────────────────────────────────────────── */
  sink = 0;
  t0 = clk::now();
  for (size_t k = 0; k < (size_t)N; k++) {
    cctreap_node_t *f = cctreap_kth(&t, k);
    sink += (long long)(uintptr_t)f;
  }
  t1 = clk::now();
  printf("  kth:     cctreap %8.2f ms  (sum=%lld)\n", ms(t0, t1), sink);

  /* std::map kth equivalent: advance iterator k steps (O(k) each) */
  sink = 0;
  t2 = clk::now();
  {
    auto it = sm.begin();
    for (size_t k = 0; k < (size_t)N; k++) {
      sink += (long long)(uintptr_t)&*it;
      ++it;
    }
  }
  t3 = clk::now();
  printf("           stl     %8.2f ms  (iterator walk)\n", ms(t2, t3));
  printf("           ratio   %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  /* ── rank (small set — STL distance is O(n²) for full N) ───────────── */
  printf("  rank (N=5000, STL distance too slow for 100k):\n");
  {
    enum { RN = 5000 };
    cctreap_t rt; cctreap_init(&rt, key_cmp, prio_cmp);
    auto re = new treap_entry[RN];
    auto se = new stl_entry[RN];
    for (int i = 0; i < RN; i++) { re[i].key = i; re[i].priority = rng(); se[i].key = i; }
    std::map<stl_entry*, int, stl_cmp> rm;

    for (int i = 0; i < RN; i++) { cctreap_insert(&rt, &re[i].node, NULL); rm[&se[i]] = i; }

    sink = 0;
    t0 = clk::now();
    for (int i = 0; i < RN; i++) sink += cctreap_rank(&rt, &re[i].node);
    t1 = clk::now();
    printf("           cctreap %8.2f ms  O(log n)\n", ms(t0, t1));

    sink = 0;
    t2 = clk::now();
    for (int i = 0; i < RN; i++) {
      auto it = rm.find(&se[i]);
      sink += (long long)std::distance(rm.begin(), it);
    }
    t3 = clk::now();
    printf("           stl     %8.2f ms  O(n)\n", ms(t2, t3));
    printf("           ratio   %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));
    delete[] re; delete[] se;
  }

  /* ── iterate forward ────────────────────────────────────────────────── */
  sink = 0;
  t0 = clk::now();
  for (cctreap_node_t *n = cctreap_begin(&t); n != cctreap_end(&t); n = cctreap_next(n)) {
    sink += te(n)->key;
  }
  t1 = clk::now();
  printf("  iterate: cctreap %8.2f ms  (sum=%lld)\n", ms(t0, t1), sink);

  sink = 0;
  t2 = clk::now();
  for (auto it = sm.begin(); it != sm.end(); ++it) {
    sink += (*it).first->key;
  }
  t3 = clk::now();
  printf("           stl     %8.2f ms  (sum=%lld)\n", ms(t2, t3), sink);
  printf("           ratio   %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  /* ── erase ──────────────────────────────────────────────────────────── */
  t0 = clk::now();
  for (int i = 0; i < N; i++) cctreap_erase(&t, &tr_entries[i].node);
  t1 = clk::now();
  printf("  erase:   cctreap %8.2f ms\n", ms(t0, t1));

  t2 = clk::now();
  for (int i = 0; i < N; i++) sm.erase(&st_entries[i]);
  t3 = clk::now();
  printf("           stl     %8.2f ms\n", ms(t2, t3));
  printf("           ratio   %8.2fx\n", ms(t0, t1) / ms(t2, t3));

  delete[] tr_entries;
  delete[] st_entries;
  return 0;
}
