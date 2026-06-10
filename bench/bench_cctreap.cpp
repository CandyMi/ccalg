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
  cctreap_node_t  node;
};
static treap_entry *te(const cctreap_node_t *n) {
  return CCTREAP_CONTAINER(n, treap_entry, node);
}
static int64_t key_cmp(const cctreap_node_t *a, const cctreap_node_t *b) {
  return (int64_t)te(a)->key - (int64_t)te(b)->key;
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
    tr_entries[i].key = i;
    st_entries[i].key = i;
  }

  /* ── cctreap: insert ────────────────────────────────────────────────── */
  cctreap_t t; cctreap_init(&t, key_cmp);
  auto t0 = clk::now();
  for (int i = 0; i < N; i++) cctreap_insert(&t, &tr_entries[i].node, NULL);
  auto t1 = clk::now();
  printf("  insert:\n\tcctreap %8.2f ms\n", ms(t0, t1));

  std::map<stl_entry*, int, stl_cmp> sm;
  auto t2 = clk::now();
  for (int i = 0; i < N; i++) sm[&st_entries[i]] = i;
  auto t3 = clk::now();
  printf("\tstl     %8.2f ms\n", ms(t2, t3));
  printf("\tratio   %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  /* ── find ───────────────────────────────────────────────────────────── */
  sink = 0;
  t0 = clk::now();
  for (int i = 0; i < N; i++) {
    cctreap_node_t *f = cctreap_find(&t, &tr_entries[i].node);
    sink += (long long)(uintptr_t)f;
  }
  t1 = clk::now();
  printf("  find:\n\tcctreap %8.2f ms  (sum=%lld)\n", ms(t0, t1), sink);

  sink = 0;
  t2 = clk::now();
  for (int i = 0; i < N; i++) {
    auto it = sm.find(&st_entries[i]);
    sink += (long long)(uintptr_t)&*it;
  }
  t3 = clk::now();
  printf("\tstl     %8.2f ms  (sum=%lld)\n", ms(t2, t3), sink);
  printf("\tratio   %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  /* ── kth: random-access by index ────────────────────────────────────── */
  enum { KTH_M = 500 };
  {
    /* generate deterministic random indices */
    auto idx = new size_t[KTH_M];
    uint64_t rng = 0xCAFEBABEDEADBEEFULL;
    for (int i = 0; i < KTH_M; i++) {
      rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
      idx[i] = (size_t)(rng % N);
    }

    /* cctreap: O(log n) per kth → total O(M log N) */
    sink = 0;
    t0 = clk::now();
    for (int i = 0; i < KTH_M; i++) {
      cctreap_node_t *f = cctreap_kth(&t, idx[i]);
      sink += (long long)(uintptr_t)f;
    }
    t1 = clk::now();
    printf("  kth(%d):\n\tcctreap %8.2f ms  O(log n)  (random keys)\n", KTH_M, ms(t0, t1));

    /* std::map: O(k) per advance → total O(M × N/2) */
    sink = 0;
    t2 = clk::now();
    for (int i = 0; i < KTH_M; i++) {
      auto it = sm.begin();
      std::advance(it, idx[i]);
      sink += (long long)(uintptr_t)&*it;
    }
    t3 = clk::now();
    printf("\tstl     %8.2f ms  O(k)    (random keys)\n", ms(t2, t3));
    printf("\tspeedup %8.0fx\n\n", ms(t2, t3) / ms(t0, t1));
    delete[] idx;
  }

  /* ── rank (small set — STL distance is O(n²) for full N) ───────────── */
  enum { RN = 500 };
  printf("  rank (N=%d, STL distance too slow for 100k):\n", RN);
  {
    cctreap_t rt; cctreap_init(&rt, key_cmp);
    auto re = new treap_entry[RN];
    auto se = new stl_entry[RN];
    for (int i = 0; i < RN; i++) { re[i].key = i; se[i].key = i; }
    std::map<stl_entry*, int, stl_cmp> rm;

    for (int i = 0; i < RN; i++) { cctreap_insert(&rt, &re[i].node, NULL); rm[&se[i]] = i; }

    sink = 0;
    t0 = clk::now();
    for (int i = 0; i < RN; i++) sink += cctreap_rank(&rt, &re[i].node);
    t1 = clk::now();
    printf("\tcctreap %8.2f ms  O(log n)\n", ms(t0, t1));

    sink = 0;
    t2 = clk::now();
    for (int i = 0; i < RN; i++) {
      auto it = rm.find(&se[i]);
      sink += (long long)std::distance(rm.begin(), it);
    }
    t3 = clk::now();
    printf("\tstl     %8.2f ms  O(n)\n", ms(t2, t3));
    printf("\tratio   %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));
    delete[] re; delete[] se;
  }

  /* ── iterate forward ────────────────────────────────────────────────── */
  sink = 0;
  t0 = clk::now();
  for (cctreap_node_t *n = cctreap_begin(&t); n != cctreap_end(&t); n = cctreap_next(n)) {
    sink += te(n)->key;
  }
  t1 = clk::now();
  printf("  iterate: \n\tcctreap %8.2f ms  (sum=%lld)\n", ms(t0, t1), sink);

  sink = 0;
  t2 = clk::now();
  for (auto it = sm.begin(); it != sm.end(); ++it) {
    sink += (*it).first->key;
  }
  t3 = clk::now();
  printf("\tstl     %8.2f ms  (sum=%lld)\n", ms(t2, t3), sink);
  printf("\tratio   %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  /* ── erase ──────────────────────────────────────────────────────────── */
  t0 = clk::now();
  for (int i = 0; i < N; i++) cctreap_erase(&t, &tr_entries[i].node);
  t1 = clk::now();
  printf("  erase:\n\tcctreap %8.2f ms\n", ms(t0, t1));

  t2 = clk::now();
  for (int i = 0; i < N; i++) sm.erase(&st_entries[i]);
  t3 = clk::now();
  printf("\tstl     %8.2f ms\n", ms(t2, t3));
  printf("\tratio   %8.2fx\n", ms(t0, t1) / ms(t2, t3));

  delete[] tr_entries;
  delete[] st_entries;
  return 0;
}
