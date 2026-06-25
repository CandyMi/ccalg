/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Benchmark: ccshuffle_knuth vs std::shuffle
**  Build:  g++ -std=c++11 -O2 -o bench_ccshuffle bench_ccshuffle.cpp && ./bench_ccshuffle
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <algorithm>
#include <chrono>
#include <random>

#include "../include/ccshuffle.h"

/* ── timer ─────────────────────────────────────────────────────────────── */
using clk = std::chrono::high_resolution_clock;
static double ms(clk::time_point s, clk::time_point e) {
  return std::chrono::duration<double, std::milli>(e - s).count();
}

/* anti-optimisation sinks */
static volatile int      sink_int;

/* ── deterministic PRNG for ccshuffle (wrapper around ccrandom128) ───── */
#include "../include/ccrandom.h"

static uint64_t my_prng(void *state) {
  return ccrandom128_next((ccrandom128_t *)state);
}

/* ── main ──────────────────────────────────────────────────────────────── */
int main() {
  printf("=== ccshuffle_knuth vs std::shuffle ===\n\n");

  /* ── 100K elements ──────────────────────────────────────────────── */
  {
    enum { N = 100000 };
    int *arr_cc = new int[N];
    int *arr_st = new int[N];
    for (int i = 0; i < N; i++) arr_cc[i] = arr_st[i] = i;

    /* ccshuffle */
    ccrandom128_t rng;
    ccrandom128_init(&rng, 42ULL);
    auto t0 = clk::now();
    ccshuffle_knuth(arr_cc, N, sizeof(int), &rng, my_prng);
    auto t1 = clk::now();
    sink_int = arr_cc[0]; /* prevent elision */
    printf("  ccshuffle_knuth (%d)      : %8.2f ms\n", N, ms(t0, t1));

    /* std::shuffle */
    std::mt19937_64 stl_rng(42ULL);
    auto t2 = clk::now();
    std::shuffle(arr_st, arr_st + N, stl_rng);
    auto t3 = clk::now();
    sink_int = arr_st[0];
    printf("  std::shuffle     (%d)      : %8.2f ms\n", N, ms(t2, t3));
    printf("  ratio                      : %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

    delete[] arr_cc;
    delete[] arr_st;
  }

  /* ── 1M elements ────────────────────────────────────────────────── */
  {
    enum { N = 1000000 };
    int *arr_cc = new int[N];
    int *arr_st = new int[N];
    for (int i = 0; i < N; i++) arr_cc[i] = arr_st[i] = i;

    ccrandom128_t rng;
    ccrandom128_init(&rng, 42ULL);
    auto t0 = clk::now();
    ccshuffle_knuth(arr_cc, N, sizeof(int), &rng, my_prng);
    auto t1 = clk::now();
    sink_int = arr_cc[0];

    std::mt19937_64 stl_rng(42ULL);
    auto t2 = clk::now();
    std::shuffle(arr_st, arr_st + N, stl_rng);
    auto t3 = clk::now();
    sink_int = arr_st[0];

    printf("  ccshuffle_knuth (%d)      : %8.2f ms\n", N, ms(t0, t1));
    printf("  std::shuffle     (%d)      : %8.2f ms\n", N, ms(t2, t3));
    printf("  ratio                      : %8.2fx\n\n", ms(t0, t1) / ms(t2, t3));

    delete[] arr_cc;
    delete[] arr_st;
  }

  /* ── large element (128 bytes, heap-swap path) ──────────────────── */
  {
    enum { N = 50000, ELEM_SZ = 128 };
    char (*arr_cc)[ELEM_SZ] = new char[N][ELEM_SZ];
    char (*arr_st)[ELEM_SZ] = new char[N][ELEM_SZ];

    for (int i = 0; i < N; i++) {
      memset(arr_cc[i], (char)i, ELEM_SZ);
      memset(arr_st[i], (char)i, ELEM_SZ);
    }

    ccrandom128_t rng;
    ccrandom128_init(&rng, 42ULL);
    auto t0 = clk::now();
    ccshuffle_knuth(arr_cc, N, ELEM_SZ, &rng, my_prng);
    auto t1 = clk::now();
    sink_int = arr_cc[0][0];

    std::mt19937_64 stl_rng(42ULL);
    auto t2 = clk::now();
    std::shuffle(arr_st, arr_st + N, stl_rng);
    auto t3 = clk::now();
    sink_int = arr_st[0][0];

    printf("  ccshuffle_knuth (%d x %dB) : %8.2f ms  (heap-swap)\n", N, ELEM_SZ, ms(t0, t1));
    printf("  std::shuffle     (%d x %dB) : %8.2f ms\n", N, ELEM_SZ, ms(t2, t3));
    printf("  ratio                        : %8.2fx\n", ms(t0, t1) / ms(t2, t3));

    delete[] arr_cc;
    delete[] arr_st;
  }

  return 0;
}
