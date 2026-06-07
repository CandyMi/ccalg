/*
**  Benchmark: ccflatmap vs ccmap vs std::map
**  Build:  g++ -std=c++11 -O2 -o bench_ccflatmap bench_ccflatmap.cpp && ./bench_ccflatmap
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <map>
#include <chrono>
#include <vector>
#include <algorithm>
#include <random>

/* ── ccmap (function-pointer mode) ────────────────────────────────────── */
#include "../include/ccmap.h"

struct ccmap_entry {
  int          key;
  uint64_t     value;
  ccmap_node_t node;
};
static ccmap_entry *ce(const ccmap_node_t *n) {
  return CCMAP_CONTAINER(n, ccmap_entry, node);
}
static int64_t ccmap_cmp(const ccmap_node_t *a, const ccmap_node_t *b) {
  return (int64_t)ce(a)->key - (int64_t)ce(b)->key;
}

/* ── ccflatmap (macro mode) ───────────────────────────────────────────── */
#define CCFLATMAP_NODE_T struct ccflatmap_entry
struct ccflatmap_entry {
  int      key;
  uint64_t value;
};
#define CCFLATMAP_COMPARE(a, b) \
  ((int64_t)(a)->key - (int64_t)(b)->key)
#include "../include/ccflatmap.h"

/* ── timer ─────────────────────────────────────────────────────────────── */
using clk = std::chrono::high_resolution_clock;
static double ms(clk::time_point s, clk::time_point e) {
  return std::chrono::duration<double, std::milli>(e - s).count();
}
static volatile long long sink;

static void print_4(const char *label, double a, double b, double c, double d) {
  printf("  %-9s %9.2f  %9.2f  %9.2f  %9.2f\n", label, a, b, c, d);
}

int main() {
  enum { N = 50000 };
  printf("=== ccflatmap vs ccmap vs std::map  (%d elements) ===\n\n", N);

  auto fm_entries = new ccflatmap_entry[N];
  auto cm_entries = new ccmap_entry[N];
  for (int i = 0; i < N; i++) {
    fm_entries[i].key   = i;
    fm_entries[i].value = (uint64_t)i;
    cm_entries[i].key   = i;
    cm_entries[i].value = (uint64_t)i;
  }

  std::vector<int> keys(N);
  for (int i = 0; i < N; i++) keys[i] = i;
  std::mt19937 rng(42);
  std::shuffle(keys.begin(), keys.end(), rng);

  printf("  %-9s %9s  %9s  %9s  %9s\n",
         "", "flatmap", "flat+sort", "ccmap", "std::map");
  printf("  %-9s %9s  %9s  %9s  %9s\n",
         "", "-------", "---------", "-----", "--------");

  /* ═══════════════════════════════════════════════════════════════════
  **  INSERT
  ** ═══════════════════════════════════════════════════════════════════ */
  {
    /* --- flatmap: sorted insert (O(n²) total) --- */
    ccflatmap_t fm; ccflatmap_init(&fm, NULL);
    auto t0 = clk::now();
    for (int i = 0; i < N; i++)
      ccflatmap_insert(&fm, fm_entries[keys[i]], NULL);
    auto t1 = clk::now();
    double fm_i = ms(t0, t1);
    ccflatmap_destroy(&fm);

    /* --- flatmap: push_back + sort (O(n log n) total) --- */
    ccflatmap_init(&fm, NULL);
    ccflatmap_reserve(&fm, (size_t)N);
    t0 = clk::now();
    for (int i = 0; i < N; i++)
      ccflatmap_push_back(&fm, fm_entries[keys[i]]);
    ccflatmap_sort(&fm);
    t1 = clk::now();
    double fm_b = ms(t0, t1);
    ccflatmap_destroy(&fm);

    /* --- ccmap --- */
    ccmap_t cm; ccmap_init(&cm, ccmap_cmp);
    t0 = clk::now();
    for (int i = 0; i < N; i++)
      ccmap_insert(&cm, &cm_entries[keys[i]].node, NULL);
    t1 = clk::now();
    double cm_t = ms(t0, t1);
    ccmap_clear(&cm);

    /* --- std::map --- */
    std::map<int, uint64_t> sm;
    t0 = clk::now();
    for (int i = 0; i < N; i++) sm[keys[i]] = i;
    t1 = clk::now();
    double sm_t = ms(t0, t1);

    print_4("insert:", fm_i, fm_b, cm_t, sm_t);
  }

  /* ═══════════════════════════════════════════════════════════════════
  **  FIND
  ** ═══════════════════════════════════════════════════════════════════ */
  std::shuffle(keys.begin(), keys.end(), rng);

  {
    ccflatmap_t fm; ccflatmap_init(&fm, NULL);
    for (int i = 0; i < N; i++) ccflatmap_insert(&fm, fm_entries[i], NULL);
    sink = 0;
    auto t0 = clk::now();
    for (int i = 0; i < N; i++) {
      auto *f = ccflatmap_find(&fm, &fm_entries[keys[i]]);
      sink += (long long)(uintptr_t)f;
    }
    auto t1 = clk::now();
    double fm_t = ms(t0, t1);
    ccflatmap_destroy(&fm);

    ccmap_t cm; ccmap_init(&cm, ccmap_cmp);
    for (int i = 0; i < N; i++) ccmap_insert(&cm, &cm_entries[i].node, NULL);
    sink = 0;
    t0 = clk::now();
    for (int i = 0; i < N; i++) {
      auto *f = ccmap_find(&cm, &cm_entries[keys[i]].node);
      sink += (long long)(uintptr_t)f;
    }
    t1 = clk::now();
    double cm_t = ms(t0, t1);
    ccmap_clear(&cm);

    std::map<int, uint64_t> sm;
    for (int i = 0; i < N; i++) sm[i] = i;
    sink = 0;
    t0 = clk::now();
    for (int i = 0; i < N; i++) {
      auto it = sm.find(keys[i]);
      sink += (it != sm.end()) ? (long long)it->second : 0;
    }
    t1 = clk::now();
    double sm_t = ms(t0, t1);

    print_4("find:", fm_t, 0, cm_t, sm_t);
  }

  /* ═══════════════════════════════════════════════════════════════════
  **  ITERATE
  ** ═══════════════════════════════════════════════════════════════════ */
  {
    ccflatmap_t fm; ccflatmap_init(&fm, NULL);
    for (int i = 0; i < N; i++) ccflatmap_insert(&fm, fm_entries[i], NULL);
    sink = 0;
    auto t0 = clk::now();
    for (auto *p = ccflatmap_begin(&fm); p; p = ccflatmap_next(&fm, p))
      sink += (long long)p->value;
    auto t1 = clk::now();
    double fm_t = ms(t0, t1);
    ccflatmap_destroy(&fm);

    ccmap_t cm; ccmap_init(&cm, ccmap_cmp);
    for (int i = 0; i < N; i++) ccmap_insert(&cm, &cm_entries[i].node, NULL);
    sink = 0;
    t0 = clk::now();
    for (auto *p = ccmap_begin(&cm); p; p = ccmap_next(p))
      sink += (long long)ce(p)->value;
    t1 = clk::now();
    double cm_t = ms(t0, t1);
    ccmap_clear(&cm);

    std::map<int, uint64_t> sm;
    for (int i = 0; i < N; i++) sm[i] = i;
    sink = 0;
    t0 = clk::now();
    for (auto &kv : sm) sink += (long long)kv.second;
    t1 = clk::now();
    double sm_t = ms(t0, t1);

    print_4("iterate:", fm_t, 0, cm_t, sm_t);
  }

  /* ═══════════════════════════════════════════════════════════════════
  **  ERASE (sorted, random order — O(n) memmove each)
  ** ═══════════════════════════════════════════════════════════════════ */
  std::shuffle(keys.begin(), keys.end(), rng);

  {
    ccflatmap_t fm; ccflatmap_init(&fm, NULL);
    for (int i = 0; i < N; i++) ccflatmap_insert(&fm, fm_entries[i], NULL);
    auto t0 = clk::now();
    for (int i = 0; i < N; i++) ccflatmap_erase(&fm, &fm_entries[keys[i]]);
    auto t1 = clk::now();
    double fm_t = ms(t0, t1);
    ccflatmap_destroy(&fm);

    ccmap_t cm; ccmap_init(&cm, ccmap_cmp);
    for (int i = 0; i < N; i++) ccmap_insert(&cm, &cm_entries[i].node, NULL);
    t0 = clk::now();
    for (int i = 0; i < N; i++) ccmap_erase(&cm, &cm_entries[keys[i]].node);
    t1 = clk::now();
    double cm_t = ms(t0, t1);
    ccmap_clear(&cm);

    std::map<int, uint64_t> sm;
    for (int i = 0; i < N; i++) sm[i] = i;
    t0 = clk::now();
    for (int i = 0; i < N; i++) sm.erase(keys[i]);
    t1 = clk::now();
    double sm_t = ms(t0, t1);

    print_4("erase:", fm_t, 0, cm_t, sm_t);
  }

  /* ═══════════════════════════════════════════════════════════════════
  **  ERASE (unordered — swap+pop O(1) each, then sort O(n log n))
  ** ═══════════════════════════════════════════════════════════════════ */
  std::shuffle(keys.begin(), keys.end(), rng);

  {
    ccflatmap_t fm; ccflatmap_init(&fm, NULL);
    for (int i = 0; i < N; i++) ccflatmap_insert(&fm, fm_entries[i], NULL);
    auto t0 = clk::now();
    for (int i = 0; i < N; i++)
      ccflatmap_erase_unordered(&fm, &fm_entries[keys[i]]);
    ccflatmap_sort(&fm);   /* restore order — once for the batch */
    auto t1 = clk::now();
    double fm_t = ms(t0, t1);
    ccflatmap_destroy(&fm);

    print_4("ers+sort:", fm_t, 0, 0, 0);
  }

  delete[] fm_entries;
  delete[] cm_entries;
  return 0;
}
