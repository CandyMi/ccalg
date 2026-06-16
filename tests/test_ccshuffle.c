/*
**  Test suite for ccshuffle — Fisher-Yates (Knuth) shuffle
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../include/ccshuffle.h"

/* ── test harness ─────────────────────────────────────────────────────── */
static int passed, failed;
#define TEST(name) static void test_##name(void)
#define ASSERT(cond) do { \
  if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failed++; } \
  else passed++; \
} while(0)
#define RUN(name) do { printf("  %s\n", #name); test_##name(); } while(0)

/* ── deterministic PRNG for testing ──────────────────────────────────────
 *  xorshift64 — NOT for general use, only for reproducible test vectors.
 */
static uint64_t xorshift64(uint64_t *s) {
  uint64_t x = *s;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  return (*s = x);
}

static uint64_t test_prng(void *state) {
  return xorshift64((uint64_t *)state);
}

/* ── helpers ──────────────────────────────────────────────────────────── */

/* return 1 if arr[0..len) is a permutation of ref[0..len) */
static int is_permutation(const int *arr, const int *ref, size_t len) {
  /* simple O(n²) check — good enough for test-sized arrays */
  for (size_t i = 0; i < len; i++) {
    int found = 0;
    for (size_t j = 0; j < len; j++) {
      if (arr[i] == ref[j]) { found = 1; break; }
    }
    if (!found) return 0;
  }
  return 1;
}

/* ── NULL safety ──────────────────────────────────────────────────────── */

TEST(null_safety) {
  int arr[10] = {0,1,2,3,4,5,6,7,8,9};
  uint64_t seed = 42;

  /* all three NULL params should be safe */
  ccshuffle_knuth(NULL, 5, sizeof(int), &seed, test_prng);
  ccshuffle_knuth(arr, 5, sizeof(int), NULL,     test_prng);
  ccshuffle_knuth(arr, 5, sizeof(int), &seed,    NULL);

  /* original array unchanged after NULL calls */
  for (int i = 0; i < 5; i++) ASSERT(arr[i] == i);
}

/* ── len=0 / len=1 (trivial, nothing to shuffle) ──────────────────────── */

TEST(trivial_lengths) {
  int arr[5] = {0,1,2,3,4};
  uint64_t seed = 42;

  ccshuffle_knuth(arr, 0, sizeof(int), &seed, test_prng);
  ASSERT(arr[0] == 0 && arr[1] == 1);  /* untouched */

  ccshuffle_knuth(arr, 1, sizeof(int), &seed, test_prng);
  ASSERT(arr[0] == 0);                  /* single element unchanged */
}

/* ── determinism: same seed → same result ─────────────────────────────── */

TEST(deterministic) {
  int a[20], b[20];
  uint64_t seed_a = 12345, seed_b = 12345;

  for (int i = 0; i < 20; i++) a[i] = b[i] = i;

  ccshuffle_knuth(a, 20, sizeof(int), &seed_a, test_prng);
  ccshuffle_knuth(b, 20, sizeof(int), &seed_b, test_prng);

  for (int i = 0; i < 20; i++) ASSERT(a[i] == b[i]);
}

/* ── different seeds → different results (very likely) ────────────────── */

TEST(different_seeds) {
  int a[20], b[20];
  uint64_t seed_a = 111, seed_b = 999;

  for (int i = 0; i < 20; i++) a[i] = b[i] = i;

  ccshuffle_knuth(a, 20, sizeof(int), &seed_a, test_prng);
  ccshuffle_knuth(b, 20, sizeof(int), &seed_b, test_prng);

  int same = 1;
  for (int i = 0; i < 20; i++) if (a[i] != b[i]) { same = 0; break; }
  ASSERT(same == 0);
}

/* ── elements preserved after shuffle (no data loss) ──────────────────── */

TEST(elements_preserved) {
  int arr[100], ref[100];
  uint64_t seed = 42;

  for (int i = 0; i < 100; i++) arr[i] = ref[i] = i;

  ccshuffle_knuth(arr, 100, sizeof(int), &seed, test_prng);

  ASSERT(is_permutation(arr, ref, 100) == 1);
}

/* ── large elements ( > CCSHUFFLE_BSIZE = 64 ) — heap alloc path ──────── */

TEST(large_elements) {
# define BIG_SZ 128
  char arr[8][BIG_SZ];
  char ref[8][BIG_SZ];

  for (int i = 0; i < 8; i++)
    memset(arr[i], 'A' + i, BIG_SZ);
  memcpy(ref, arr, sizeof(arr));

  uint64_t seed = 77;
  ccshuffle_knuth(arr, 8, BIG_SZ, &seed, test_prng);

  /* all original bytes still present somewhere */
  int found_all = 1;
  for (int ch = 'A'; ch < 'A' + 8; ch++) {
    int found = 0;
    for (int i = 0; i < 8 && !found; i++)
      if (arr[i][0] == ch) found = 1;
    if (!found) { found_all = 0; break; }
  }
  ASSERT(found_all == 1);
# undef BIG_SZ
}

/* ── array size = power of two (edge case for modulo) ─────────────────── */

TEST(power_of_two_length) {
# define N 16
  int arr[N], ref[N];
  uint64_t seed = 2024;

  for (int i = 0; i < N; i++) arr[i] = ref[i] = i;

  ccshuffle_knuth(arr, N, sizeof(int), &seed, test_prng);
  ASSERT(is_permutation(arr, ref, N) == 1);
# undef N
}

/* ── repeated shuffles should not produce the same order twice ──────────── *
 *  With 6 elements there are 720 permutations; 50 trials should yield at
 *  least 2 distinct orders unless something is very wrong.                */

TEST(not_always_same) {
# define N 6
# define TRIALS 50
  int baseline[N];
  uint64_t seed = 1;
  int all_same = 1;

  for (int t = 0; t < TRIALS; t++) {
    int arr[N];
    for (int i = 0; i < N; i++) arr[i] = i;

    ccshuffle_knuth(arr, N, sizeof(int), &seed, test_prng);

    if (t == 0) memcpy(baseline, arr, sizeof(arr));
    else if (memcmp(baseline, arr, sizeof(arr)) != 0) all_same = 0;
  }

  ASSERT(all_same == 0);
# undef TRIALS
# undef N
}

/* ── large array — stress test ────────────────────────────────────────── */

TEST(large_array) {
# define N 10000
  int *arr = (int *)malloc(N * sizeof(int));
  int *ref = (int *)malloc(N * sizeof(int));
  ASSERT(arr != NULL && ref != NULL);

  for (int i = 0; i < N; i++) arr[i] = ref[i] = i;

  uint64_t seed = 9999;
  ccshuffle_knuth(arr, N, sizeof(int), &seed, test_prng);

  ASSERT(is_permutation(arr, ref, N) == 1);

  free(arr);
  free(ref);
# undef N
}

/* ── main ─────────────────────────────────────────────────────────────── */
int main(void) {
  printf("ccshuffle tests:\n");
  RUN(null_safety);
  RUN(trivial_lengths);
  RUN(deterministic);
  RUN(different_seeds);
  RUN(elements_preserved);
  RUN(large_elements);
  RUN(power_of_two_length);
  RUN(not_always_same);
  RUN(large_array);
  printf("\n  %d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
