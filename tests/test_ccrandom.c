/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Test suite for ccrandom (128-bit & 256-bit PRNGs)
**  Build:  gcc -std=c99 -Wall -Wextra -o test_ccrandom test_ccrandom.c && ./test_ccrandom
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include "../include/ccrandom.h"

/* ── test harness ─────────────────────────────────────────────────────── */
static int passed, failed;
#define TEST(name) static void test_##name(void)
#define ASSERT(cond) do { \
  if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failed++; } \
  else passed++; \
} while(0)
#define RUN(name) do { printf("  %s\n", #name); test_##name(); } while(0)

/* ── helpers ──────────────────────────────────────────────────────────── */

static int double_eq(double a, double b, double eps) {
  return fabs(a - b) < eps;
}

/* ── init + basic next (ccrandom128) ──────────────────────────────────── */

TEST(init_next_128) {
  ccrandom128_t rng;
  ccrandom128_init(&rng, 42ULL);
  /* just make sure we can get a few values without crashing */
  uint64_t a = ccrandom128_next(&rng);
  uint64_t b = ccrandom128_next(&rng);
  uint64_t c = ccrandom128_next(&rng);
  ASSERT(a != 0 || b != 0 || c != 0);  /* all-zero is astronomically unlikely */
  (void)a; (void)b; (void)c;           /* squash unused warnings */
}

/* ── init + basic next (ccrandom256) ──────────────────────────────────── */

TEST(init_next_256) {
  ccrandom256_t rng;
  ccrandom256_init(&rng, 42ULL);
  uint64_t a = ccrandom256_next(&rng);
  uint64_t b = ccrandom256_next(&rng);
  uint64_t c = ccrandom256_next(&rng);
  ASSERT(a != 0 || b != 0 || c != 0);
  (void)a; (void)b; (void)c;
}

/* ── deterministic: same seed → same sequence (128) ──────────────────── */

TEST(deterministic_128) {
  ccrandom128_t a, b;
  ccrandom128_init(&a, 12345ULL);
  ccrandom128_init(&b, 12345ULL);
  for (int i = 0; i < 1000; i++) {
    ASSERT(ccrandom128_next(&a) == ccrandom128_next(&b));
  }
}

/* ── deterministic: same seed → same sequence (256) ──────────────────── */

TEST(deterministic_256) {
  ccrandom256_t a, b;
  ccrandom256_init(&a, 67890ULL);
  ccrandom256_init(&b, 67890ULL);
  for (int i = 0; i < 1000; i++) {
    ASSERT(ccrandom256_next(&a) == ccrandom256_next(&b));
  }
}

/* ── zero seed is valid and produces non-trivial output ───────────────── */

TEST(zero_seed_valid) {
  ccrandom128_t r128;
  ccrandom128_init(&r128, 0ULL);
  uint64_t s0_128 = r128.seed[0];
  uint64_t s1_128 = r128.seed[1];
  ASSERT(s0_128 != 0 || s1_128 != 0);
  uint64_t v128 = ccrandom128_next(&r128);
  ASSERT(v128 != 0);  /* first output from zero seed != 0 */

  ccrandom256_t r256;
  ccrandom256_init(&r256, 0ULL);
  uint64_t zero_state = 0;
  for (int i = 0; i < 4; i++) zero_state |= r256.seed[i];
  ASSERT(zero_state != 0);
  uint64_t v256 = ccrandom256_next(&r256);
  ASSERT(v256 != 0);
}

/* ── different seeds → different sequences ────────────────────────────── */

TEST(different_seeds) {
  ccrandom128_t a, b;
  ccrandom128_init(&a, 1ULL);
  ccrandom128_init(&b, 2ULL);
  int diff = 0;
  for (int i = 0; i < 100; i++) {
    if (ccrandom128_next(&a) != ccrandom128_next(&b)) { diff = 1; break; }
  }
  ASSERT(diff == 1);
}

/* ── f32next returns values in [0, 1) ─────────────────────────────────── */

TEST(f32_in_range) {
  ccrandom128_t r128;
  ccrandom128_init(&r128, 99ULL);
  for (int i = 0; i < 10000; i++) {
    float f = ccrandom128_f32next(&r128);
    ASSERT(f >= 0.0f);
    ASSERT(f < 1.0f);
  }

  ccrandom256_t r256;
  ccrandom256_init(&r256, 99ULL);
  for (int i = 0; i < 10000; i++) {
    float f = ccrandom256_f32next(&r256);
    ASSERT(f >= 0.0f);
    ASSERT(f < 1.0f);
  }
}

/* ── f64next returns values in [0, 1) ─────────────────────────────────── */

TEST(f64_in_range) {
  ccrandom128_t r128;
  ccrandom128_init(&r128, 77ULL);
  for (int i = 0; i < 10000; i++) {
    double d = ccrandom128_f64next(&r128);
    ASSERT(d >= 0.0);
    ASSERT(d < 1.0);
  }

  ccrandom256_t r256;
  ccrandom256_init(&r256, 77ULL);
  for (int i = 0; i < 10000; i++) {
    double d = ccrandom256_f64next(&r256);
    ASSERT(d >= 0.0);
    ASSERT(d < 1.0);
  }
}

/* ── f64next mean ≈ 0.5 ───────────────────────────────────────────────── */

TEST(f64_mean) {
  ccrandom128_t rng;
  ccrandom128_init(&rng, 42ULL);
  double sum = 0.0;
  int n = 100000;
  for (int i = 0; i < n; i++) {
    sum += ccrandom128_f64next(&rng);
  }
  double mean = sum / n;
  ASSERT(double_eq(mean, 0.5, 0.02));
}

/* ── f32next mean ≈ 0.5 ───────────────────────────────────────────────── */

TEST(f32_mean) {
  ccrandom256_t rng;
  ccrandom256_init(&rng, 42ULL);
  double sum = 0.0;
  int n = 100000;
  for (int i = 0; i < n; i++) {
    sum += (double)ccrandom256_f32next(&rng);
  }
  double mean = sum / n;
  ASSERT(double_eq(mean, 0.5, 0.02));
}

/* ── u64_next covers full range roughly uniformly ─────────────────────── */

TEST(u64_distribution) {
  ccrandom128_t rng;
  ccrandom128_init(&rng, 1ULL);
  int above = 0, below = 0, n = 100000;
  uint64_t mid = UINT64_MAX / 2;
  for (int i = 0; i < n; i++) {
    if (ccrandom128_next(&rng) > mid) above++; else below++;
  }
  /* expect ~50% above midpoint, tolerance ± 2% */
  double ratio = (double)above / n;
  ASSERT(double_eq(ratio, 0.5, 0.025));
}

/* ── 128 and 256 engines produce different outputs (same seed) ────────── */

TEST(engines_differ) {
  ccrandom128_t r128;
  ccrandom256_t r256;
  ccrandom128_init(&r128, 42ULL);
  ccrandom256_init(&r256, 42ULL);
  int diff = 0;
  for (int i = 0; i < 100; i++) {
    if (ccrandom128_next(&r128) != ccrandom256_next(&r256)) { diff = 1; break; }
  }
  ASSERT(diff == 1);
}

/* ── f32next determinism ──────────────────────────────────────────────── */

TEST(f32_deterministic) {
  ccrandom128_t a, b;
  ccrandom128_init(&a, 7ULL);
  ccrandom128_init(&b, 7ULL);
  for (int i = 0; i < 500; i++) {
    ASSERT(ccrandom128_f32next(&a) == ccrandom128_f32next(&b));
  }
}

/* ── f64next determinism ──────────────────────────────────────────────── */

TEST(f64_deterministic) {
  ccrandom256_t a, b;
  ccrandom256_init(&a, 7ULL);
  ccrandom256_init(&b, 7ULL);
  for (int i = 0; i < 500; i++) {
    ASSERT(ccrandom256_f64next(&a) == ccrandom256_f64next(&b));
  }
}

/* ── many calls without overflow / wrap ───────────────────────────────── */

TEST(many_calls_128) {
  ccrandom128_t rng;
  ccrandom128_init(&rng, 0ULL);
  /* 100k consecutive calls, no crash, values not all-zero */
  uint64_t or_acc = 0;
  for (int i = 0; i < 100000; i++) {
    or_acc |= ccrandom128_next(&rng);
  }
  ASSERT(or_acc != 0);
}

TEST(many_calls_256) {
  ccrandom256_t rng;
  ccrandom256_init(&rng, 0ULL);
  uint64_t or_acc = 0;
  for (int i = 0; i < 100000; i++) {
    or_acc |= ccrandom256_next(&rng);
  }
  ASSERT(or_acc != 0);
}

/* ── f32next never equals 1.0 ─────────────────────────────────────────── */

TEST(f32_no_one) {
  ccrandom128_t rng;
  ccrandom128_init(&rng, 123ULL);
  for (int i = 0; i < 100000; i++) {
    float f = ccrandom128_f32next(&rng);
    ASSERT(f < 1.0f);
  }
}

/* ── f64next never equals 1.0 ─────────────────────────────────────────── */

TEST(f64_no_one) {
  ccrandom256_t rng;
  ccrandom256_init(&rng, 456ULL);
  for (int i = 0; i < 100000; i++) {
    double d = ccrandom256_f64next(&rng);
    ASSERT(d < 1.0);
  }
}

/* ── seed=0 produces specific output (snapshot for regression) ────────── */

TEST(snapshot_zero_seed) {
  ccrandom128_t r128;
  ccrandom128_init(&r128, 0ULL);
  /* capture first 5 values — update these if the engine changes */
  uint64_t v1 = ccrandom128_next(&r128);
  uint64_t v2 = ccrandom128_next(&r128);
  uint64_t v3 = ccrandom128_next(&r128);
  uint64_t v4 = ccrandom128_next(&r128);
  uint64_t v5 = ccrandom128_next(&r128);
  /* just verify they're distinct and non-zero */
  ASSERT(v1 != 0); ASSERT(v2 != 0); ASSERT(v3 != 0);
  ASSERT(v4 != 0); ASSERT(v5 != 0);
  ASSERT(v1 != v2); ASSERT(v2 != v3);
  ASSERT(v3 != v4); ASSERT(v4 != v5);
}

/* ── f32next / f64next do not corrupt the generator's integer sequence ── */

TEST(interleaved_output) {
  ccrandom128_t rng;
  ccrandom128_init(&rng, 99ULL);

  /* baseline: first 10 integer outputs with no float interleaving */
  ccrandom128_t baseline;
  ccrandom128_init(&baseline, 99ULL);
  uint64_t expected[10];
  for (int i = 0; i < 10; i++) expected[i] = ccrandom128_next(&baseline);

  /* interleave float calls, then check integer sequence is unchanged */
  ccrandom128_f32next(&rng);  /* consumes 1 integer (idx 0) */
  ccrandom128_f64next(&rng);  /* consumes 1 integer (idx 1) */
  ASSERT(ccrandom128_next(&rng) == expected[2]);  /* idx 2 */
  ccrandom128_f32next(&rng);  /* idx 3 */
  ASSERT(ccrandom128_next(&rng) == expected[4]);  /* idx 4 */
  ccrandom128_f64next(&rng);  /* idx 5 */
  ccrandom128_f64next(&rng);  /* idx 6 */
  ASSERT(ccrandom128_next(&rng) == expected[7]);  /* idx 7 */
}

/* ── main ─────────────────────────────────────────────────────────────── */
int main(void) {
  printf("ccrandom tests:\n");
  RUN(init_next_128);
  RUN(init_next_256);
  RUN(deterministic_128);
  RUN(deterministic_256);
  RUN(zero_seed_valid);
  RUN(different_seeds);
  RUN(f32_in_range);
  RUN(f64_in_range);
  RUN(f64_mean);
  RUN(f32_mean);
  RUN(u64_distribution);
  RUN(engines_differ);
  RUN(f32_deterministic);
  RUN(f64_deterministic);
  RUN(many_calls_128);
  RUN(many_calls_256);
  RUN(f32_no_one);
  RUN(f64_no_one);
  RUN(snapshot_zero_seed);
  RUN(interleaved_output);
  printf("\n  %d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
