/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Benchmark: ccbi big-integer arithmetic throughput
**
**  Covers: mul (schoolbook / Karatsuba / Toom-3), div, pow_mod (Montgomery),
**          string conversion (to_str), and basic ops (add/sub).
**
**  Build:  g++ -std=c++11 -O2 -o bench_ccbi bench_ccbi.cpp && ./bench_ccbi
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <chrono>

extern "C" {
#include "../include/ccbi.h"
}

/* ── timer ─────────────────────────────────────────────────────────────── */
using clk = std::chrono::high_resolution_clock;
static double ms(clk::time_point s, clk::time_point e) {
  return std::chrono::duration<double, std::milli>(e - s).count();
}

static volatile uint64_t sink_u64;

/* ── helpers ──────────────────────────────────────────────────────────── */

/* Build a bigint from a string, abort on failure */
static void set(ccbi_t *z, const char *hex) {
  if (ccbi_from_str(z, hex, 16) != 0) {
    fprintf(stderr, "FAIL: set(%s)\n", hex);
    exit(1);
  }
}

/* Build a 2^n - 1 value (all-ones, n-bit) — convenient benchmark operand */
static void mk_ones(ccbi_t *z, int bits) {
  char *buf = new char[bits / 4 + 3];
  int len = (bits + 3) / 4;
  for (int i = 0; i < len; i++) buf[i] = 'f';
  buf[len] = '\0';
  set(z, buf);
  delete[] buf;
}

/* Count iterations per microbench */
enum { REPS = 500 };

/* ── main ──────────────────────────────────────────────────────────────── */
int main() {
  printf("=== ccbi big-integer benchmark ===\n\n");

  /* ── mul: schoolbook (256-bit × 256-bit) ──────────────────────────── */
  {
    ccbi_t a, b, c;
    ccbi_init(&a); ccbi_init(&b); ccbi_init(&c);
    mk_ones(&a, 256);  mk_ones(&b, 256);

    auto t0 = clk::now();
    for (int i = 0; i < REPS; i++) ccbi_mul(&c, &a, &b);
    auto t1 = clk::now();
    printf("  mul (256-bit)           : %8.2f ms  (%5.0f ops/s)\n",
           ms(t0, t1), REPS / ms(t0, t1) * 1000);
    sink_u64 = *c.limbs;
    ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&c);
  }

  /* ── mul: Karatsuba (1024-bit × 1024-bit) ─────────────────────────── */
  {
    ccbi_t a, b, c;
    ccbi_init(&a); ccbi_init(&b); ccbi_init(&c);
    mk_ones(&a, 1024); mk_ones(&b, 1024);

    auto t0 = clk::now();
    for (int i = 0; i < REPS; i++) ccbi_mul(&c, &a, &b);
    auto t1 = clk::now();
    printf("  mul (1024-bit)          : %8.2f ms  (%5.0f ops/s)\n",
           ms(t0, t1), REPS / ms(t0, t1) * 1000);
    sink_u64 = *c.limbs;
    ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&c);
  }

  /* ── mul: Toom-3 (2048-bit × 2048-bit) ─────────────────────────────── */
  {
    ccbi_t a, b, c;
    ccbi_init(&a); ccbi_init(&b); ccbi_init(&c);
    mk_ones(&a, 2048); mk_ones(&b, 2048);

    auto t0 = clk::now();
    for (int i = 0; i < REPS; i++) ccbi_mul(&c, &a, &b);
    auto t1 = clk::now();
    printf("  mul (2048-bit)          : %8.2f ms  (%5.0f ops/s)\n",
           ms(t0, t1), REPS / ms(t0, t1) * 1000);
    sink_u64 = *c.limbs;
    ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&c);
  }

  /* ── div: 1024-bit ÷ 512-bit (Knuth algorithm D) ────────────────── */
  {
    ccbi_t a, b, q, r;
    ccbi_init(&a); ccbi_init(&b); ccbi_init(&q); ccbi_init(&r);
    mk_ones(&a, 1024);
    set(&b,    "8000000000000000000000000000000000000000000000000000000000000001");

    auto t0 = clk::now();
    for (int i = 0; i < REPS; i++) ccbi_div_rem(&q, &r, &a, &b);
    auto t1 = clk::now();
    printf("  div (1024-bit / 512-bit): %8.2f ms  (%5.0f ops/s)\n",
           ms(t0, t1), REPS / ms(t0, t1) * 1000);
    sink_u64 = *q.limbs;
    ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&q); ccbi_destroy(&r);
  }

  /* ── div: 2048-bit ÷ 256-bit (multi-limb quotient) ───────────────── */
  {
    ccbi_t a, b, q, r;
    ccbi_init(&a); ccbi_init(&b); ccbi_init(&q); ccbi_init(&r);
    mk_ones(&a, 2048);
    set(&b, "ffffffffffffffffffffffffffffffff");

    auto t0 = clk::now();
    for (int i = 0; i < REPS; i++) ccbi_div_rem(&q, &r, &a, &b);
    auto t1 = clk::now();
    printf("  div (2048-bit / 256-bit): %8.2f ms  (%5.0f ops/s)\n",
           ms(t0, t1), REPS / ms(t0, t1) * 1000);
    sink_u64 = *q.limbs;
    ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&q); ccbi_destroy(&r);
  }

  /* ── pow_mod (512-bit, Montgomery) ─────────────────────────────────── */
  {
    ccbi_t base, exp, mod, z;
    ccbi_init(&base); ccbi_init(&exp); ccbi_init(&mod); ccbi_init(&z);
    set(&base, "123456789");
    set(&mod,  "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff43");
    set(&exp,  "123456789012345");

    auto t0 = clk::now();
    for (int i = 0; i < 100; i++) ccbi_pow_mod(&z, &base, &exp, &mod);
    auto t1 = clk::now();
    printf("  pow_mod (512-bit)       : %8.2f ms  (%5.0f ops/s)\n",
           ms(t0, t1), 100.0 / ms(t0, t1) * 1000);
    sink_u64 = *z.limbs;
    ccbi_destroy(&base); ccbi_destroy(&exp); ccbi_destroy(&mod); ccbi_destroy(&z);
  }

  /* ── to_str (1024-bit → decimal string) ──────────────────────────── */
  {
    ccbi_t a;
    ccbi_init(&a);
    mk_ones(&a, 1024);

    auto t0 = clk::now();
    for (int i = 0; i < 100; i++) {
      char *s = ccbi_to_str(&a, 10);
      sink_u64 = (uint64_t)(uintptr_t)s;
      ccbi_free_str(s);
    }
    auto t1 = clk::now();
    printf("  to_str (1024-bit, dec)  : %8.2f ms  (%5.0f ops/s)\n",
           ms(t0, t1), 100.0 / ms(t0, t1) * 1000);
    ccbi_destroy(&a);
  }

  /* ── add (1024-bit) ─────────────────────────────────────────────────── */
  {
    ccbi_t a, b, c;
    ccbi_init(&a); ccbi_init(&b); ccbi_init(&c);
    mk_ones(&a, 1024); mk_ones(&b, 1024);

    auto t0 = clk::now();
    for (int i = 0; i < REPS * 10; i++) ccbi_add(&c, &a, &b);
    auto t1 = clk::now();
    printf("  add (1024-bit)          : %8.2f ms  (%5.0f ops/s)\n",
           ms(t0, t1), REPS * 10.0 / ms(t0, t1) * 1000);
    sink_u64 = *c.limbs;
    ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&c);
  }

  /* ── sub (1024-bit) ─────────────────────────────────────────────────── */
  {
    ccbi_t a, b, c;
    ccbi_init(&a); ccbi_init(&b); ccbi_init(&c);
    mk_ones(&a, 1024);
    set(&b, "10000000000000000000000000000000000000000000000000000000000000001");

    auto t0 = clk::now();
    for (int i = 0; i < REPS * 10; i++) ccbi_sub(&c, &a, &b);
    auto t1 = clk::now();
    printf("  sub (1024-bit)          : %8.2f ms  (%5.0f ops/s)\n",
           ms(t0, t1), REPS * 10.0 / ms(t0, t1) * 1000);
    sink_u64 = *c.limbs;
    ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&c);
  }

  /* ── shift: shl (1024-bit, large shift) ─────────────────────────────── */
  {
    ccbi_t a, c;
    ccbi_init(&a); ccbi_init(&c);
    mk_ones(&a, 1024);

    auto t0 = clk::now();
    for (int i = 0; i < REPS; i++) ccbi_shl(&c, &a, 256);
    auto t1 = clk::now();
    printf("  shl (1024-bit, 256)     : %8.2f ms  (%5.0f ops/s)\n",
           ms(t0, t1), REPS / ms(t0, t1) * 1000);
    sink_u64 = *c.limbs;
    ccbi_destroy(&a); ccbi_destroy(&c);
  }

  return 0;
}
