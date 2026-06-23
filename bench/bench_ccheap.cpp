/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Benchmark: ccheap vs std::priority_queue
**
**  Build (function-pointer, container_of — default):
**    g++ -std=c++11 -O2 -o bench_ccheap bench_ccheap.cpp && ./bench_ccheap
**
**  Build (CCHEAP_COMPARE macro, inline — zero call overhead):
**    g++ -std=c++11 -O2 -DBENCH_MACRO -o bench_ccheap bench_ccheap.cpp && ./bench_ccheap
**
**  Build (4-ary heap):
**    g++ -std=c++11 -O2 -DCCHEAP_ARITY=4 -o bench_ccheap bench_ccheap.cpp && ./bench_ccheap
**
**  Build (macro + 4-ary):
**    g++ -std=c++11 -O2 -DBENCH_MACRO -DCCHEAP_ARITY=4 -o bench_ccheap bench_ccheap.cpp && ./bench_ccheap
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <queue>
#include <vector>
#include <chrono>
#include <algorithm>
#include <random>

/* ── CCHEAP_COMPARE: must be defined BEFORE #include ─────────────────── */

#ifdef BENCH_MACRO
  /* macro mode: compare node fields directly (no container_of needed —
     the union puts priority/timeout right on ccheap_node_t) */
  #define CCHEAP_COMPARE(a, b) \
      ((int64_t)(b)->priority - (int64_t)(a)->priority)
#endif

#include "../include/ccheap.h"

/* ── user struct: embedded ccheap_node_t + payload ──────────────────── */

struct task {
  ccheap_node_t node;
  uint32_t      payload;
};

/* ── comparison (function-pointer mode, uses container_of) ──────────── */

#ifndef BENCH_MACRO
static int64_t min_cmp(const ccheap_node_t *a, const ccheap_node_t *b) noexcept {
  const struct task *ta = CCHEAP_CONTAINER(a, struct task, node);
  const struct task *tb = CCHEAP_CONTAINER(b, struct task, node);
  return (int64_t)tb->node.priority - (int64_t)ta->node.priority;
}
#endif

/* ── helpers ────────────────────────────────────────────────────────── */

using clk = std::chrono::high_resolution_clock;
static double ms(clk::time_point s, clk::time_point e) {
  return std::chrono::duration<double, std::milli>(e - s).count();
}

struct stl_item { uint32_t key; uint32_t payload; };
struct stl_cmp  { bool operator()(const stl_item &a, const stl_item &b) const {
    return a.key > b.key;
}};

/* ── run once ───────────────────────────────────────────────────────── */

static void run(int N) {
  /* prepare shuffled data */
  std::vector<uint32_t> keys(N);
  for (int i = 0; i < N; i++) keys[i] = (uint32_t)i;
  std::mt19937 rng(42);
  std::shuffle(keys.begin(), keys.end(), rng);

  auto tasks = new struct task[N];
  auto stl   = new stl_item[N];
  for (int i = 0; i < N; i++) {
    tasks[i].node.priority = keys[i];
    tasks[i].payload       = keys[i];
    stl[i].key             = keys[i];
    stl[i].payload         = keys[i];
  }

  /* ── push ─────────────────────────────────────────────────────────── */
  ccheap_t h;
#ifdef BENCH_MACRO
  ccheap_init(&h, NULL);            /* 2nd arg ignored */
#else
  ccheap_init(&h, min_cmp);
#endif
  auto t0 = clk::now();
  for (int i = 0; i < N; i++) ccheap_push(&h, &tasks[i].node);
  auto t1 = clk::now();
  printf("  push:     ccheap            %8.2f ms\n", ms(t0, t1));

  std::priority_queue<stl_item, std::vector<stl_item>, stl_cmp> pq;
  auto t2 = clk::now();
  for (int i = 0; i < N; i++) pq.push(stl[i]);
  auto t3 = clk::now();
  printf("            std::priority_q   %8.2f ms\n", ms(t2, t3));
  printf("            ratio             %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  /* ── pop ──────────────────────────────────────────────────────────── */
  t0 = clk::now();
  for (int i = 0; i < N; i++) {
    ccheap_node_t *np = ccheap_pop(&h);
    volatile auto *t = CCHEAP_CONTAINER(np, struct task, node);
    (void)t;
  }
  t1 = clk::now();
  printf("  pop:      ccheap            %8.2f ms\n", ms(t0, t1));

  t2 = clk::now();
  for (int i = 0; i < N; i++) pq.pop();
  t3 = clk::now();
  printf("            std::priority_q   %8.2f ms\n", ms(t2, t3));
  printf("            ratio             %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

  ccheap_destroy(&h);
  delete[] tasks;
  delete[] stl;
}

/* ── main ───────────────────────────────────────────────────────────── */

int main() {
  enum { N = 200000 };

#ifdef BENCH_MACRO
  printf("=== ccheap (CCHEAP_COMPARE macro, zero-overhead inline) ===\n");
#else
  printf("=== ccheap (function pointer + container_of) ===\n");
#endif
#ifdef CCHEAP_ARITY
  printf("    arity = %d\n", CCHEAP_ARITY);
#else
  printf("    arity = 2 (default)\n");
#endif
  printf("    %d elements\n\n", N);

  run(N);

  printf("  Build variations:\n");
  printf("    -DBENCH_MACRO        CCHEAP_COMPARE inline (zero call overhead)\n");
  printf("    -DCCHEAP_ARITY=4     4-ary heap\n");
  printf("    -DCCHEAP_ARITY=8     8-ary heap\n");
  printf("    -DBENCH_MACRO -DCCHEAP_ARITY=4    combine\n");
  return 0;
}
