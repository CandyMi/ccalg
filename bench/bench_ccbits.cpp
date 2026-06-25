/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Benchmark: ccbits bit-manipulation primitives vs C++ builtins
**  Build:  g++ -std=c++11 -O2 -o bench_ccbits bench_ccbits.cpp && ./bench_ccbits
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <chrono>
#include <random>

#include "../include/ccbits.h"

/* ── timer ─────────────────────────────────────────────────────────────── */
using clk = std::chrono::high_resolution_clock;
static double ms(clk::time_point s, clk::time_point e) {
  return std::chrono::duration<double, std::milli>(e - s).count();
}

static volatile uint64_t sink_u64;
static volatile int      sink_int;

/* ── generate N random uint64 values ──────────────────────────────────── */
static uint64_t *gen_vals(size_t n, uint64_t seed) {
  uint64_t *v = new uint64_t[n];
  std::mt19937_64 rng(seed);
  for (size_t i = 0; i < n; i++) v[i] = rng();
  return v;
}

int main() {
  enum { N = 10000000 };
  uint64_t *vals = gen_vals(N, 42ULL);
  printf("=== ccbits bit primitives  (%d random values) ===\n\n", N);

  /* ── popcount64 ─────────────────────────────────────────────────────── */
  double cc_ms, blt_ms;
  {
    auto t0 = clk::now();
    for (int i = 0; i < N; i++) sink_int  = (int)ccbits_popcount64(vals[i]);
    auto t1 = clk::now();
    cc_ms = ms(t0, t1);
    printf("  ccbits_popcount64    : %8.2f ms  (%4.0f M/s)\n", cc_ms, N / cc_ms * 1000 / 1e6);
  }
  {
    auto t0 = clk::now();
    for (int i = 0; i < N; i++) sink_int += __builtin_popcountll(vals[i]);
    auto t1 = clk::now();
    blt_ms = ms(t0, t1);
    printf("  __builtin_popcountll : %8.2f ms  (%4.0f M/s)\n", blt_ms, N / blt_ms * 1000 / 1e6);
    printf("  ratio                : %8.2fx\n\n", cc_ms / blt_ms);
  }

  /* ── clz64 ──────────────────────────────────────────────────────────── */
  {
    auto t0 = clk::now();
    for (int i = 0; i < N; i++) sink_int  = (int)ccbits_clz64(vals[i]);
    auto t1 = clk::now();
    cc_ms = ms(t0, t1);
    printf("  ccbits_clz64         : %8.2f ms  (%4.0f M/s)\n", cc_ms, N / cc_ms * 1000 / 1e6);
  }
  {
    auto t0 = clk::now();
    for (int i = 0; i < N; i++) sink_int += __builtin_clzll(vals[i]);
    auto t1 = clk::now();
    blt_ms = ms(t0, t1);
    printf("  __builtin_clzll      : %8.2f ms  (%4.0f M/s)\n", blt_ms, N / blt_ms * 1000 / 1e6);
    printf("  ratio                : %8.2fx\n\n", cc_ms / blt_ms);
  }

  /* ── ctz64 ──────────────────────────────────────────────────────────── */
  {
    auto t0 = clk::now();
    for (int i = 0; i < N; i++) sink_int  = (int)ccbits_ctz64(vals[i]);
    auto t1 = clk::now();
    cc_ms = ms(t0, t1);
    printf("  ccbits_ctz64         : %8.2f ms  (%4.0f M/s)\n", cc_ms, N / cc_ms * 1000 / 1e6);
  }
  {
    auto t0 = clk::now();
    for (int i = 0; i < N; i++) sink_int += __builtin_ctzll(vals[i]);
    auto t1 = clk::now();
    blt_ms = ms(t0, t1);
    printf("  __builtin_ctzll      : %8.2f ms  (%4.0f M/s)\n", blt_ms, N / blt_ms * 1000 / 1e6);
    printf("  ratio                : %8.2fx\n\n", cc_ms / blt_ms);
  }

  /* ── bswap64 ────────────────────────────────────────────────────────── */
  {
    auto t0 = clk::now();
    for (int i = 0; i < N; i++) sink_u64  = ccbits_bswap64(vals[i]);
    auto t1 = clk::now();
    cc_ms = ms(t0, t1);
    printf("  ccbits_bswap64       : %8.2f ms  (%4.0f M/s)\n", cc_ms, N / cc_ms * 1000 / 1e6);
  }
  {
    auto t0 = clk::now();
    for (int i = 0; i < N; i++) sink_u64 += __builtin_bswap64(vals[i]);
    auto t1 = clk::now();
    blt_ms = ms(t0, t1);
    printf("  __builtin_bswap64    : %8.2f ms  (%4.0f M/s)\n", blt_ms, N / blt_ms * 1000 / 1e6);
    printf("  ratio                : %8.2fx\n\n", cc_ms / blt_ms);
  }

  /* ── rotl64 ─────────────────────────────────────────────────────────── */
  {
    auto t0 = clk::now();
    for (int i = 0; i < N; i++) sink_u64  = ccbits_rotl64(vals[i], (uint32_t)i);
    auto t1 = clk::now();
    cc_ms = ms(t0, t1);
    printf("  ccbits_rotl64        : %8.2f ms  (%4.0f M/s)\n", cc_ms, N / cc_ms * 1000 / 1e6);
  }
  {
    auto t0 = clk::now();
    for (int i = 0; i < N; i++) {
        uint64_t x = vals[i];
        int k = i & 63;
        sink_u64 += (x << k) | (x >> (64 - k));
    }
    auto t1 = clk::now();
    blt_ms = ms(t0, t1);
    printf("  manual rotl          : %8.2f ms  (%4.0f M/s)\n", blt_ms, N / blt_ms * 1000 / 1e6);
    printf("  ratio                : %8.2fx\n\n", cc_ms / blt_ms);
  }

  delete[] vals;
  return 0;
}
