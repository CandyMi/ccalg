/*
**  Benchmark: cchashmap vs std::unordered_map  (vs uthash, optional)
**
**  Build (default — cchashmap vs STL):
**    g++ -std=c++11 -O2 -o bench_cchashmap bench_cchashmap.cpp && ./bench_cchashmap
**
**  Build (with uthash — download uthash.h first):
**    curl -sLo deps/uthash.h https://raw.githubusercontent.com/troydhanson/uthash/master/src/uthash.h
**    g++ -std=c++11 -O2 -DCCLAG_HAS_UTHASH -Ideps -o bench_cchashmap bench_cchashmap.cpp && ./bench_cchashmap
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifndef container_of
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

/* ── cchashmap (macro mode) ── pre-define node before #include ──────── */

typedef struct cchashmap_node {
  struct cchashmap_node *next;
  uint64_t               hash;
} cchashmap_node_t;
#define CCHASHMAP_NODE_T

struct cc_entry {
  int               key;
  cchashmap_node_t  node;
};

#define CCHASHMAP_HASH(n, seed)  ((void)(seed), (uint64_t)container_of((n), struct cc_entry, node)->key)
#define CCHASHMAP_EQUAL(a, b)    (container_of((a), struct cc_entry, node)->key == \
                                   container_of((b), struct cc_entry, node)->key)

#include "../include/cchashmap.h"

/* ── uthash (intrusive, macro-based — opt-in via -DCCLAG_HAS_UTHASH) ── */

#ifdef CCLAG_HAS_UTHASH
#include "uthash.h"

struct uth_entry {
  int            key;
  int            val;
  UT_hash_handle hh;
};
#endif

/* ── STL ──────────────────────────────────────────────────────────────── */

#include <unordered_map>
#include <chrono>
#include <vector>
#include <algorithm>
#include <random>

using clk = std::chrono::high_resolution_clock;
static double ms(clk::time_point s, clk::time_point e) {
  return std::chrono::duration<double, std::milli>(e - s).count();
}
static volatile long long sink;

/* ── helpers ──────────────────────────────────────────────────────────── */

static void print_ratio(const char *label, double a, double b) {
  if (b > 0) printf("           %-20s %8.2fx\n", label, a / b);
}

/* ── main ────────────────────────────────────────────────────────────── */

int main() {
  enum { N = 200000 };
#ifdef CCLAG_HAS_UTHASH
  printf("=== cchashmap vs std::unordered_map vs uthash  (%d elements) ===\n\n", N);
#else
  printf("=== cchashmap vs std::unordered_map  (%d elements) ===\n", N);
  printf("    (rebuild with -DCCLAG_HAS_UTHASH for uthash comparison)\n\n");
#endif

  std::vector<int> keys(N);
  for (int i = 0; i < N; i++) keys[i] = i;
  std::mt19937 rng(42);
  std::shuffle(keys.begin(), keys.end(), rng);

  auto cc_entries = new struct cc_entry[N];
  for (int i = 0; i < N; i++) cc_entries[i].key = keys[i];

#ifdef CCLAG_HAS_UTHASH
  auto uth_entries = new struct uth_entry[N];
  for (int i = 0; i < N; i++) { uth_entries[i].key = keys[i]; uth_entries[i].val = i; }
#endif

  /* ───────────────────────────────────────────────────────────────────
  **  INSERT
  ** ─────────────────────────────────────────────────────────────────── */

  cchashmap_t m; cchashmap_init(&m, NULL, NULL);
  auto t0 = clk::now();
  for (int i = 0; i < N; i++) cchashmap_set(&m, &cc_entries[i].node, NULL);
  auto t1 = clk::now();
  printf("  insert:  cchashmap          %8.2f ms  (size=%zu)\n", ms(t0, t1), cchashmap_size(&m));

#ifdef CCLAG_HAS_UTHASH
  struct uth_entry *uth_tbl = NULL;
  auto t2 = clk::now();
  for (int i = 0; i < N; i++) HASH_ADD_INT(uth_tbl, key, &uth_entries[i]);
  auto t3 = clk::now();
  printf("           uthash             %8.2f ms\n", ms(t2, t3));
#endif

  std::unordered_map<int, int> um;
  auto t4 = clk::now();
  for (int i = 0; i < N; i++) um[keys[i]] = i;
  auto t5 = clk::now();
  printf("           std::unordered_map %8.2f ms\n", ms(t4, t5));
  print_ratio("cchashmap/stl", ms(t0, t1), ms(t4, t5));
#ifdef CCLAG_HAS_UTHASH
  print_ratio("uthash/stl", ms(t2, t3), ms(t4, t5));
  print_ratio("cchashmap/uthash", ms(t0, t1), ms(t2, t3));
#endif
  printf("\n");

  std::shuffle(keys.begin(), keys.end(), rng);

  /* ───────────────────────────────────────────────────────────────────
  **  FIND
  ** ─────────────────────────────────────────────────────────────────── */

  sink = 0;
  t0 = clk::now();
  for (int i = 0; i < N; i++) {
    struct cc_entry probe; probe.key = keys[i];
    cchashmap_node_t *f = cchashmap_get(&m, &probe.node);
    sink += (long long)(uintptr_t)f;
  }
  t1 = clk::now();
  printf("  find:    cchashmap          %8.2f ms  (sum=%lld)\n", ms(t0, t1), sink);

#ifdef CCLAG_HAS_UTHASH
  sink = 0;
  t2 = clk::now();
  for (int i = 0; i < N; i++) {
    struct uth_entry *f;
    HASH_FIND_INT(uth_tbl, &keys[i], f);
    sink += (long long)(uintptr_t)f;
  }
  t3 = clk::now();
  printf("           uthash             %8.2f ms  (sum=%lld)\n", ms(t2, t3), sink);
#endif

  sink = 0;
  t4 = clk::now();
  for (int i = 0; i < N; i++) {
    auto it = um.find(keys[i]);
    sink += (it != um.end()) ? it->second : 0;
  }
  t5 = clk::now();
  printf("           std::unordered_map %8.2f ms  (sum=%lld)\n", ms(t4, t5), sink);
  print_ratio("cchashmap/stl", ms(t0, t1), ms(t4, t5));
#ifdef CCLAG_HAS_UTHASH
  print_ratio("uthash/stl", ms(t2, t3), ms(t4, t5));
  print_ratio("cchashmap/uthash", ms(t0, t1), ms(t2, t3));
#endif
  printf("\n");

  std::shuffle(keys.begin(), keys.end(), rng);

  /* ───────────────────────────────────────────────────────────────────
  **  ERASE
  ** ─────────────────────────────────────────────────────────────────── */

  t0 = clk::now();
  for (int i = 0; i < N; i++) cchashmap_del(&m, &cc_entries[keys[i]].node);
  t1 = clk::now();
  printf("  erase:   cchashmap          %8.2f ms\n", ms(t0, t1));

#ifdef CCLAG_HAS_UTHASH
  t2 = clk::now();
  for (int i = 0; i < N; i++) HASH_DEL(uth_tbl, &uth_entries[keys[i]]);
  t3 = clk::now();
  printf("           uthash             %8.2f ms\n", ms(t2, t3));
#endif

  t4 = clk::now();
  for (int i = 0; i < N; i++) um.erase(keys[i]);
  t5 = clk::now();
  printf("           std::unordered_map %8.2f ms\n", ms(t4, t5));
  print_ratio("cchashmap/stl", ms(t0, t1), ms(t4, t5));
#ifdef CCLAG_HAS_UTHASH
  print_ratio("uthash/stl", ms(t2, t3), ms(t4, t5));
  print_ratio("cchashmap/uthash", ms(t0, t1), ms(t2, t3));
#endif

  cchashmap_destroy(&m);
  delete[] cc_entries;
#ifdef CCLAG_HAS_UTHASH
  delete[] uth_entries;
#endif
  return 0;
}
