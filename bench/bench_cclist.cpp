/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Benchmark: cclist vs std::list
**  Build:  g++ -std=c++11 -O2 -o bench_cclist bench_cclist.cpp && ./bench_cclist
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <list>
#include <chrono>

#include "../include/cclist.h"

struct lst_entry {
  int           val;
  cclist_node_t node;
};
static lst_entry *le(const cclist_node_t *n) {
  return CCLIST_CONTAINER(n, lst_entry, node);
}

using clk = std::chrono::high_resolution_clock;
static double ms(clk::time_point s, clk::time_point e) {
  return std::chrono::duration<double, std::milli>(e - s).count();
}

int main() {
  enum { N = 200000 };
  printf("=== cclist vs std::list  (%d elements) ===\n\n", N);

  auto cc_entries = new lst_entry[N];
  for (int i = 0; i < N; i++) cc_entries[i].val = i;

  /* ── push_back ──────────────────────────────────────────────────────── */
  cclist_t cl; cclist_init(&cl);
  auto t0 = clk::now();
  for (int i = 0; i < N; i++) cclist_push_back(&cl, &cc_entries[i].node);
  auto t1 = clk::now();
  printf("  push_back: cclist %8.2f ms\n", ms(t0, t1));

  std::list<lst_entry*> sl;
  auto t2 = clk::now();
  for (int i = 0; i < N; i++) sl.push_back(&cc_entries[i]);
  auto t3 = clk::now();
  printf("             stl    %8.2f ms\n", ms(t2, t3));
  printf("             ratio  %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  /* ── iterate with sum ───────────────────────────────────────────────── */
  volatile long long sum = 0;
  t0 = clk::now();
  for (cclist_node_t *n = cclist_begin(&cl); n != cclist_end(&cl); n = cclist_next(n))
    sum += le(n)->val;
  t1 = clk::now();
  printf("  iterate:   cclist %8.2f ms  (sum=%lld)\n", ms(t0, t1), sum);

  sum = 0;
  t2 = clk::now();
  for (auto it = sl.begin(); it != sl.end(); ++it) sum += (*it)->val;
  t3 = clk::now();
  printf("             stl    %8.2f ms  (sum=%lld)\n", ms(t2, t3), sum);
  printf("             ratio  %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  /* ── pop_front ──────────────────────────────────────────────────────── */
  t0 = clk::now();
  while (!cclist_empty(&cl)) cclist_pop_front(&cl);
  t1 = clk::now();
  printf("  pop_front:  cclist %8.2f ms\n", ms(t0, t1));

  t2 = clk::now();
  while (!sl.empty()) sl.pop_front();
  t3 = clk::now();
  printf("              stl    %8.2f ms\n", ms(t2, t3));
  printf("              ratio  %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  /* ── splice ─────────────────────────────────────────────────────────── */
  for (int i = 0; i < N; i++) { cclist_push_back(&cl, &cc_entries[i].node); sl.push_back(&cc_entries[i]); }

  cclist_t src; cclist_init(&src);
  std::list<lst_entry*> ssrc;
  for (int i = 0; i < N / 2; i++) { cclist_push_back(&src, &cc_entries[i].node); ssrc.push_back(&cc_entries[i]); }

  t0 = clk::now();
  cclist_splice_back(&cl, &src);
  t1 = clk::now();
  printf("  splice:     cclist %8.2f ms\n", ms(t0, t1));

  t2 = clk::now();
  sl.splice(sl.end(), ssrc);
  t3 = clk::now();
  printf("              stl    %8.2f ms\n", ms(t2, t3));
  printf("              ratio  %8.2fx\n", ms(t0, t1) / ms(t2, t3));

  delete[] cc_entries;
  return 0;
}
