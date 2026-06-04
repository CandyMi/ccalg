/*
**  Benchmark: ccheap vs std::priority_queue
**  Build:  g++ -std=c++11 -O2 -o bench_ccheap bench_ccheap.cpp && ./bench_ccheap
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <queue>
#include <vector>
#include <chrono>
#include <algorithm>
#include <random>

#define CCNODE_T struct heap_node
struct heap_node { uint32_t conv; uint32_t priority; };

static int64_t min_cmp(const heap_node *a, const heap_node *b) {
  return (int64_t)b->priority - (int64_t)a->priority;
}
#include "../include/ccheap.h"

using clk = std::chrono::high_resolution_clock;
static double ms(clk::time_point s, clk::time_point e) {
  return std::chrono::duration<double, std::milli>(e - s).count();
}

struct stl_node { uint32_t priority; };
struct stl_cmp { bool operator()(const stl_node &a, const stl_node &b) const { return a.priority > b.priority; } };

int main() {
  enum { N = 200000 };
  printf("=== ccheap vs std::priority_queue  (%d elements) ===\n\n", N);

  std::vector<uint32_t> prios(N);
  for (int i = 0; i < N; i++) prios[i] = (uint32_t)i;
  std::mt19937 rng(42);
  std::shuffle(prios.begin(), prios.end(), rng);

  auto n1 = new heap_node[N];
  auto sn = new stl_node[N];
  for (int i = 0; i < N; i++) { n1[i].priority = prios[i]; sn[i].priority = prios[i]; }

  /* ── push ───────────────────────────────────────────────────────────── */
  ccheap_t h; heap_init(&h, min_cmp);
  auto t0 = clk::now();
  for (int i = 0; i < N; i++) heap_push(&h, &n1[i]);
  auto t1 = clk::now();
  printf("  push:     ccheap (ptr+fn)  %8.2f ms\n", ms(t0, t1));

  std::priority_queue<stl_node, std::vector<stl_node>, stl_cmp> pq;
  auto t2 = clk::now();
  for (int i = 0; i < N; i++) pq.push(sn[i]);
  auto t3 = clk::now();
  printf("            std::priority_q  %8.2f ms\n", ms(t2, t3));
  printf("            ratio            %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  /* ── pop ────────────────────────────────────────────────────────────── */
  t0 = clk::now();
  for (int i = 0; i < N; i++) { volatile auto *p = heap_pop(&h); (void)p; }
  t1 = clk::now();
  printf("  pop:      ccheap (ptr+fn)  %8.2f ms\n", ms(t0, t1));

  t2 = clk::now();
  for (int i = 0; i < N; i++) pq.pop();
  t3 = clk::now();
  printf("            std::priority_q  %8.2f ms\n", ms(t2, t3));
  printf("            ratio            %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  heap_destroy(&h);
  delete[] n1;
  delete[] sn;

  printf("  Note: ccheap also supports CCHEAP_COMPARE (macro, zero-overhead\n"
         "        inline compare) and CCHEAP_VALUE (contiguous storage, better\n"
         "        cache locality).  Use -D flags to test those modes.\n");
  return 0;
}
