/*
**  Benchmark: ccvector vs std::vector
**  Build:  g++ -std=c++11 -O2 -o bench_ccvector bench_ccvector.cpp && ./bench_ccvector
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <vector>
#include <chrono>
#include <algorithm>
#include <random>

#include "../include/ccvector.h"

using clk = std::chrono::high_resolution_clock;
static double ms(clk::time_point s, clk::time_point e) {
  return std::chrono::duration<double, std::milli>(e - s).count();
}
static volatile long long sink;

int main() {
  enum { N = 500000 };
  printf("=== ccvector vs std::vector  (%d elements) ===\n\n", N);

  /* ── push_back ─────────────────────────────────────────────────────── */
  ccvector_t v; ccvector_init(&v);
  auto t0 = clk::now();
  for (int i = 0; i < N; i++) {
    ccvector_node_t e = {.value = (uint32_t)i};
    ccvector_push_back(&v, e);
  }
  auto t1 = clk::now();
  printf("  push_back:  ccvector   %8.2f ms  (size=%zu, cap=%zu)\n",
         ms(t0, t1), ccvector_size(&v), ccvector_capacity(&v));

  std::vector<uint32_t> sv;
  auto t2 = clk::now();
  for (int i = 0; i < N; i++) sv.push_back((uint32_t)i);
  auto t3 = clk::now();
  printf("              stl        %8.2f ms  (size=%zu, cap=%zu)\n",
         ms(t2, t3), sv.size(), sv.capacity());
  printf("              ratio      %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  /* ── random access (read, unchecked) ───────────────────────────────── */
  std::vector<size_t> indices(N);
  for (size_t i = 0; i < N; i++) indices[i] = i;
  std::mt19937 rng(42);
  std::shuffle(indices.begin(), indices.end(), rng);

  sink = 0;
  t0 = clk::now();
  for (int i = 0; i < N; i++)
    sink += ccvector_at(&v, indices[i])->value;
  t1 = clk::now();
  printf("  access:     ccvector   %8.2f ms  (sum=%lld)\n", ms(t0, t1), sink);

  sink = 0;
  t2 = clk::now();
  for (int i = 0; i < N; i++) sink += sv[indices[i]];
  t3 = clk::now();
  printf("              stl        %8.2f ms  (sum=%lld)\n", ms(t2, t3), sink);
  printf("              ratio      %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  /* ── pop_back ──────────────────────────────────────────────────────── */
  t0 = clk::now();
  while (!ccvector_empty(&v)) ccvector_pop_back(&v);
  t1 = clk::now();
  printf("  pop_back:   ccvector   %8.2f ms\n", ms(t0, t1));

  t2 = clk::now();
  while (!sv.empty()) sv.pop_back();
  t3 = clk::now();
  printf("              stl        %8.2f ms\n", ms(t2, t3));
  printf("              ratio      %8.2fx\n", ms(t0, t1) / ms(t2, t3));

  ccvector_destroy(&v);

  /* ── sort (qsort vs std::sort) ──────────────────────────────────────── */
  {
    enum { SN = 200000 };
    std::vector<uint32_t> sv_sorted;
    for (int i = 0; i < SN; i++) sv_sorted.push_back((uint32_t)i);

    std::mt19937 rng2(42);
    std::vector<uint32_t> sv_shuffled = sv_sorted;
    std::shuffle(sv_shuffled.begin(), sv_shuffled.end(), rng2);

    /* fill ccvector with shuffled data */
    ccvector_t v2; ccvector_init_cap(&v2, (size_t)SN);
    for (int i = 0; i < SN; i++) {
      ccvector_node_t e = {.value = sv_shuffled[i]};
      ccvector_push_back(&v2, e);
    }

    auto qcmp = [](const void *a, const void *b) noexcept -> int {
      uint32_t va = ((const ccvector_node_t *)a)->value;
      uint32_t vb = ((const ccvector_node_t *)b)->value;
      return (va > vb) - (va < vb);
    };

    t0 = clk::now();
    ccvector_sort(&v2, qcmp);
    t1 = clk::now();
    printf("  sort:       ccvector   %8.2f ms\n", ms(t0, t1));

    std::vector<uint32_t> sv2 = sv_shuffled;
    t2 = clk::now();
    std::sort(sv2.begin(), sv2.end());
    t3 = clk::now();
    printf("              stl        %8.2f ms\n", ms(t2, t3));
    printf("              ratio      %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

    /* verify correctness */
    int ok = 1;
    for (int i = 0; i < SN; i++)
      if (ccvector_at(&v2, (size_t)i)->value != sv2[i]) { ok = 0; break; }
    printf("  sort correct: %s\n\n", ok ? "yes" : "NO");

    ccvector_destroy(&v2);
  }

  return 0;
}
