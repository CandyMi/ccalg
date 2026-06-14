/*
**  Benchmark: ccrandom128/ccrandom256 vs std::mt19937
**  Build:  g++ -std=c++11 -O2 -o bench_ccrandom bench_ccrandom.cpp && ./bench_ccrandom
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <random>
#include <chrono>

#include "../include/ccrandom.h"

/* ── timer ─────────────────────────────────────────────────────────────── */
using clk = std::chrono::high_resolution_clock;
static double ms(clk::time_point s, clk::time_point e) {
  return std::chrono::duration<double, std::milli>(e - s).count();
}

/* anti-optimisation sink */
static volatile uint64_t sink_u64;
static volatile double   sink_f64;

int main() {
  enum { N = 20000000 };
  printf("=== ccrandom128 / ccrandom256 vs std::mt19937  (%d draws) ===\n\n", N);

  /* ── ccrandom128 u64 ─────────────────────────────────────── */
  {
    ccrandom128_t rng;
    ccrandom128_init(&rng, 42ULL);
    auto t0 = clk::now();
    for (int i = 0; i < N; i++) sink_u64 = ccrandom128_next(&rng);
    auto t1 = clk::now();
    printf("ccrandom128_next     : %8.2f ms  (%4.0f M/s)\n", ms(t0, t1), N / ms(t0, t1) * 1000 / 1e6);
  }

  /* ── ccrandom256 u64 ─────────────────────────────────────── */
  {
    ccrandom256_t rng;
    ccrandom256_init(&rng, 42ULL);
    auto t0 = clk::now();
    for (int i = 0; i < N; i++) sink_u64 = ccrandom256_next(&rng);
    auto t1 = clk::now();
    printf("ccrandom256_next     : %8.2f ms  (%4.0f M/s)\n", ms(t0, t1), N / ms(t0, t1) * 1000 / 1e6);
  }

  /* ── std::mt19937 u64 ────────────────────────────────────── */
  {
    std::mt19937_64 rng(42ULL);
    auto t0 = clk::now();
    for (int i = 0; i < N; i++) sink_u64 = rng();
    auto t1 = clk::now();
    printf("std::mt19937_64      : %8.2f ms  (%4.0f M/s)\n", ms(t0, t1), N / ms(t0, t1) * 1000 / 1e6);
  }

  /* ── ccrandom128 f64 ──────────────────────────────────────── */
  {
    ccrandom128_t rng;
    ccrandom128_init(&rng, 42ULL);
    auto t0 = clk::now();
    for (int i = 0; i < N; i++) sink_f64 = ccrandom128_f64next(&rng);
    auto t1 = clk::now();
    printf("ccrandom128_f64next  : %8.2f ms  (%4.0f M/s)\n", ms(t0, t1), N / ms(t0, t1) * 1000 / 1e6);
  }

  /* ── ccrandom256 f64 ──────────────────────────────────────── */
  {
    ccrandom256_t rng;
    ccrandom256_init(&rng, 42ULL);
    auto t0 = clk::now();
    for (int i = 0; i < N; i++) sink_f64 = ccrandom256_f64next(&rng);
    auto t1 = clk::now();
    printf("ccrandom256_f64next  : %8.2f ms  (%4.0f M/s)\n", ms(t0, t1), N / ms(t0, t1) * 1000 / 1e6);
  }

  /* ── seed throughput ──────────────────────────────────────── */
  {
    enum { S = 1000000 };
    auto t0 = clk::now();
    for (int i = 0; i < S; i++) {
      ccrandom128_t rng;
      ccrandom128_init(&rng, (uint64_t)i);
      sink_u64 = ccrandom128_next(&rng);
    }
    auto t1 = clk::now();
    printf("ccrandom128 init+next: %8.2f ms  (%4.0f M/s)\n", ms(t0, t1), S / ms(t0, t1) * 1000 / 1e6);
  }

  printf("\n");
  (void)sink_u64;
  (void)sink_f64;
  return 0;
}
