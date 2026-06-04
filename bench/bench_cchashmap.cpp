/*
**  Benchmark: cchashmap vs std::unordered_map
**  Build:  g++ -std=c++11 -O2 -o bench_cchashmap bench_cchashmap.cpp && ./bench_cchashmap
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unordered_map>
#include <chrono>
#include <vector>
#include <algorithm>
#include <random>

/* pre-define node + entry for macro-mode dispatch */
typedef struct cchashmap_node {
  struct cchashmap_node *next;
  uint64_t               hash;
} cchashmap_node_t;
#define CCHASHMAP_NODE_T

struct hmap_entry {
  int               key;
  cchashmap_node_t  node;
};

#define CCHASHMAP_HASH(n, seed)  ((void)(seed), (uint64_t)container_of((n), struct hmap_entry, node)->key)
#define CCHASHMAP_EQUAL(a, b)    (container_of((a), struct hmap_entry, node)->key == \
                                   container_of((b), struct hmap_entry, node)->key)

#include "../include/cchashmap.h"

using clk = std::chrono::high_resolution_clock;
static double ms(clk::time_point s, clk::time_point e) {
  return std::chrono::duration<double, std::milli>(e - s).count();
}

int main() {
  enum { N = 100000 };
  printf("=== cchashmap vs std::unordered_map  (%d elements) ===\n\n", N);

  std::vector<int> keys(N);
  for (int i = 0; i < N; i++) keys[i] = i;
  std::mt19937 rng(42);
  std::shuffle(keys.begin(), keys.end(), rng);

  auto cc_entries = new hmap_entry[N];
  for (int i = 0; i < N; i++) cc_entries[i].key = keys[i];

  /* ── insert ─────────────────────────────────────────────────────────── */
  cchashmap_t m; cchashmap_init(&m, NULL, NULL);
  auto t0 = clk::now();
  for (int i = 0; i < N; i++) cchashmap_set(&m, &cc_entries[i].node, NULL);
  auto t1 = clk::now();
  printf("  insert:  cchashmap %8.2f ms\n", ms(t0, t1));

  std::unordered_map<int, int> um;
  auto t2 = clk::now();
  for (int i = 0; i < N; i++) um[keys[i]] = i;
  auto t3 = clk::now();
  printf("           stl       %8.2f ms\n", ms(t2, t3));
  printf("           ratio     %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  std::shuffle(keys.begin(), keys.end(), rng);

  /* ── find ───────────────────────────────────────────────────────────── */
  t0 = clk::now();
  for (int i = 0; i < N; i++) {
    hmap_entry probe; probe.key = keys[i];
    volatile auto *f = cchashmap_get(&m, &probe.node); (void)f;
  }
  t1 = clk::now();
  printf("  find:    cchashmap %8.2f ms\n", ms(t0, t1));

  t2 = clk::now();
  for (int i = 0; i < N; i++) { volatile auto it = um.find(keys[i]); (void)it; }
  t3 = clk::now();
  printf("           stl       %8.2f ms\n", ms(t2, t3));
  printf("           ratio     %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  std::shuffle(keys.begin(), keys.end(), rng);

  /* ── erase ──────────────────────────────────────────────────────────── */
  t0 = clk::now();
  for (int i = 0; i < N; i++) cchashmap_del(&m, &cc_entries[keys[i]].node);
  t1 = clk::now();
  printf("  erase:   cchashmap %8.2f ms\n", ms(t0, t1));

  t2 = clk::now();
  for (int i = 0; i < N; i++) um.erase(keys[i]);
  t3 = clk::now();
  printf("           stl       %8.2f ms\n", ms(t2, t3));
  printf("           ratio     %8.2fx\n", ms(t0, t1) / ms(t2, t3));

  cchashmap_destroy(&m);
  delete[] cc_entries;
  return 0;
}
