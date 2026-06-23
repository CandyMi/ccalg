/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Benchmark: cclink vs std::forward_list
**  Build:  g++ -std=c++11 -O2 -o bench_cclink bench_cclink.cpp && ./bench_cclink
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <forward_list>
#include <chrono>

#include "../include/cclink.h"

struct lst_entry {
  int            val;
  cclink_node_t  node;
};
static lst_entry *le(const cclink_node_t *n) {
  return CCLINK_CONTAINER(n, lst_entry, node);
}

using clk = std::chrono::high_resolution_clock;
static double ms(clk::time_point s, clk::time_point e) {
  return std::chrono::duration<double, std::milli>(e - s).count();
}
static volatile long long sink;

int main() {
  enum { N = 200000 };
  printf("=== cclink vs std::forward_list  (%d elements) ===\n\n", N);

  auto entries = new lst_entry[N];
  for (int i = 0; i < N; i++) entries[i].val = i;

  /* ── push_front ─────────────────────────────────────────────────────── */
  cclink_t cl; cclink_init(&cl);
  auto t0 = clk::now();
  for (int i = 0; i < N; i++) cclink_push(&cl, &entries[i].node);
  auto t1 = clk::now();
  printf("  push:     cclink %8.2f ms\n", ms(t0, t1));

  std::forward_list<lst_entry*> fl;
  auto t2 = clk::now();
  for (int i = 0; i < N; i++) fl.push_front(&entries[i]);
  auto t3 = clk::now();
  printf("            stl    %8.2f ms\n", ms(t2, t3));
  printf("            ratio  %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  /* ── iterate ────────────────────────────────────────────────────────── */
  sink = 0;
  t0 = clk::now();
  for (cclink_node_t *n = cclink_begin(&cl); n != cclink_end(&cl); n = cclink_next(n))
    sink += le(n)->val;
  t1 = clk::now();
  printf("  iterate:  cclink %8.2f ms  (sum=%lld)\n", ms(t0, t1), sink);

  sink = 0;
  t2 = clk::now();
  for (auto it = fl.begin(); it != fl.end(); ++it) sink += (*it)->val;
  t3 = clk::now();
  printf("            stl    %8.2f ms  (sum=%lld)\n", ms(t2, t3), sink);
  printf("            ratio  %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  /* ── pop_front ─────────────────────────────────────────────────────── */
  t0 = clk::now();
  while (!cclink_empty(&cl)) cclink_pop_front(&cl);
  t1 = clk::now();
  printf("  pop:      cclink %8.2f ms\n", ms(t0, t1));

  t2 = clk::now();
  while (!fl.empty()) fl.pop_front();
  t3 = clk::now();
  printf("            stl    %8.2f ms\n", ms(t2, t3));
  printf("            ratio  %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  delete[] entries;
  return 0;
}
