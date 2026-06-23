/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Benchmark: ccmap prefetch ON vs OFF
**  Build:  g++ -std=c++11 -O2 -o bench_prefetch_nop bench_ccmap_prefetch.cpp
**          g++ -std=c++11 -O2 -DCCMAP_PREFETCH -o bench_prefetch_yes bench_ccmap_prefetch.cpp
**
**  The CCMAP_PREFETCH macro is controlled at compile time and listed in
**  the output header so both runs can be compared.
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <chrono>
#include <algorithm>
#include <random>

#include "../include/ccmap.h"

/* ── entry + comparator ───────────────────────────────────────────────── */
struct entry {
  int          key;
  ccmap_node_t node;
};
static entry *e(const ccmap_node_t *n) {
  return CCMAP_CONTAINER(n, entry, node);
}
static int64_t cmp_fn(const ccmap_node_t *a, const ccmap_node_t *b) noexcept {
  return (int64_t)e(a)->key - (int64_t)e(b)->key;
}

/* ── timer ────────────────────────────────────────────────────────────── */
using clk = std::chrono::high_resolution_clock;
static double ms(clk::time_point s, clk::time_point e) {
  return std::chrono::duration<double, std::milli>(e - s).count();
}
static volatile long long sink;

int main() {
  enum { N = 200000, TRIALS = 5 };
  const char *mode =
#ifdef CCMAP_PREFETCH
    "ON ";
#else
    "OFF";
#endif

  printf("=== ccmap prefetch=%s  (%d elements, %d trials) ===\n\n", mode, N, TRIALS);

  auto entries = new entry[N];

  /* ── random shuffle for realistic branch prediction ─────────────────── */
  for (int i = 0; i < N; i++) entries[i].key = i;
  std::mt19937 rng(42);
  std::shuffle(entries, entries + N, rng);

  /* ── insert ─────────────────────────────────────────────────────────── */
  {
    double best = 1e9;
    for (int t = 0; t < TRIALS; t++) {
      ccmap_t m; ccmap_init(&m, cmp_fn);
      auto t0 = clk::now();
      for (int i = 0; i < N; i++) ccmap_insert(&m, &entries[i].node, NULL);
      auto t1 = clk::now();
      double d = ms(t0, t1);
      if (d < best) best = d;
      if (t < TRIALS - 1) { ccmap_clear(&m); /* nodes still owned by entries[] */ }
    }
    printf("  insert:  %8.2f ms  (best of %d)\n", best, TRIALS);
  }

  /* ── find (random order) ────────────────────────────────────────────── */
  {
    std::shuffle(entries, entries + N, rng);  /* re-shuffle lookup order */
    ccmap_t m; ccmap_init(&m, cmp_fn);
    for (int i = 0; i < N; i++) ccmap_insert(&m, &entries[i].node, NULL);

    double best = 1e9;
    for (int t = 0; t < TRIALS; t++) {
      sink = 0;
      auto t0 = clk::now();
      for (int i = 0; i < N; i++) {
        ccmap_node_t *f = ccmap_find(&m, &entries[i].node);
        sink += (long long)(uintptr_t)f;
      }
      auto t1 = clk::now();
      double d = ms(t0, t1);
      if (d < best) best = d;
    }
    printf("  find:    %8.2f ms  (best of %d, sum=%lld)\n", best, TRIALS, sink);
  }

  /* ── iterate (ccmap_next full traversal) ────────────────────────────── */
  {
    ccmap_t m; ccmap_init(&m, cmp_fn);
    for (int i = 0; i < N; i++) ccmap_insert(&m, &entries[i].node, NULL);

    double best = 1e9;
    for (int t = 0; t < TRIALS; t++) {
      sink = 0;
      auto t0 = clk::now();
      for (ccmap_node_t *n = ccmap_first(&m); n; n = ccmap_next(n))
        sink += e(n)->key;
      auto t1 = clk::now();
      double d = ms(t0, t1);
      if (d < best) best = d;
    }
    printf("  iterate: %8.2f ms  (best of %d, sum=%lld)\n", best, TRIALS, sink);
  }

  /* ── erase (random order) ───────────────────────────────────────────── */
  {
    std::shuffle(entries, entries + N, rng);
    ccmap_t m; ccmap_init(&m, cmp_fn);
    for (int i = 0; i < N; i++) ccmap_insert(&m, &entries[i].node, NULL);

    double best = 1e9;
    for (int t = 0; t < TRIALS; t++) {
      /* re-insert after each trial (erase consumes nodes) */
      if (t > 0) for (int i = 0; i < N; i++) ccmap_insert(&m, &entries[i].node, NULL);

      auto t0 = clk::now();
      for (int i = 0; i < N; i++) ccmap_erase(&m, &entries[i].node);
      auto t1 = clk::now();
      double d = ms(t0, t1);
      if (d < best) best = d;
    }
    printf("  erase:   %8.2f ms  (best of %d)\n", best, TRIALS);
  }

  delete[] entries;
  return 0;
}
