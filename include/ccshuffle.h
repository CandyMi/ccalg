/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  ccshuffle.h — Fisher-Yates (Knuth) shuffle for contiguous arrays.
**
**  Shuffles an array of `len` elements, each `sz` bytes, in O(n) time and
**  O(1) space (stack swap buffer).  A user-provided PRNG callback supplies
**  the random stream — pass any generator whose `next` returns a uint64_t.
**
**  ── Usage ──
**
**    ccrandom128_t rng;
**    ccrandom128_init(&rng, seed);
**
**    int arr[100];
**    ccshuffle_knuth(arr, 100, sizeof(int), &rng, (ccshuffle_prng_t)ccrandom128_next);
*/

#ifndef CCSHUFFLE_H
#define CCSHUFFLE_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#if defined(_MSC_VER)
  #include <intrin.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ── portable inline ──────────────────────────────────────────────────── */

#if   defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
  #define CCSHUFFLE_INLINE static inline
#elif defined(_MSC_VER)
  #define CCSHUFFLE_INLINE static __inline
#elif defined(__GNUC__) || defined(__clang__)
  #define CCSHUFFLE_INLINE static __inline__
#else
  #define CCSHUFFLE_INLINE static
#endif

/* ── noexcept (C++17+, otherwise empty) ────────────────────────────────── */

#if defined(__cplusplus) && __cplusplus >= 201703L
  #define CCSHUFFLE_NOEXCEPT noexcept
#else
  #define CCSHUFFLE_NOEXCEPT
#endif

/* ── swap buffer size (override before #include) ──────────────────────── */

#ifndef CCSHUFFLE_BSIZE
  #define CCSHUFFLE_BSIZE 64
#endif

/* ── PRNG callback ────────────────────────────────────────────────────── */

/** @brief PRNG callback: return the next uniformly-distributed 64-bit value.
 *  @param state  Opaque generator state (must not be NULL).
 *  @return       A pseudo-random uint64_t in [0, 2^64−1]. */
typedef uint64_t (*ccshuffle_prng_t)(void *state) CCSHUFFLE_NOEXCEPT;

/* ── internal: unbiased range [0, range) via Lemire's multiplication method ─
 *
 * Returns a uniformly-distributed random index in [0, range) using the
 * first 64-bit random value `rnd`, with rejection for the bias zone.
 *
 * Three implementations, selected at compile time:
 *
 *   1) __uint128_t path (GCC/Clang x64 & ARM64, ICC):
 *        (__uint128_t)rnd * range  →  mul+shr, no `div`
 *
 *   2) MSVC intrinsic path (MSVC x64 / ARM64):
 *        _umul128 / __umulh  —  same single-mul + shift, no `div`
 *
 *   3) Fallback (MSVC x86, legacy compilers):
 *        Classic rejection sampling:  UINT64_MAX - (UINT64_MAX % range)
 *        then `% range`.  Two `div` ops per element but always correct.
 *
 *   In all three paths the rejection zone is ~range/2^64 wide,
 *   so re-draws are astronomically rare for any practical range.
 */
CCSHUFFLE_INLINE size_t
_ccshuffle_range(uint64_t rnd, size_t range,
                 void *state, ccshuffle_prng_t next) CCSHUFFLE_NOEXCEPT
{
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
  /* ── GCC/Clang/ICC: native 128-integer path ────────────────────── */
  __uint128_t t = (__uint128_t)rnd * range;
  uint64_t    lo = (uint64_t)t;
  size_t limit = (size_t)(-range) % range;   /* = 2^64 % range */
  while ((size_t)lo < limit) {
    rnd = next(state);
    t   = (__uint128_t)rnd * range;
    lo  = (uint64_t)t;
  }
  return (size_t)(t >> 64);

#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
  /* ── MSVC x64 / ARM64: intrinsic path ──────────────────────────── */
  size_t limit = (size_t)(-range) % range;
  uint64_t lo, hi;
#if defined(_M_ARM64)
  lo = rnd * (uint64_t)range;
  hi = __umulh(rnd, (uint64_t)range);
#else
  lo = _umul128(rnd, (uint64_t)range, &hi);
#endif
  while ((size_t)lo < limit) {
    rnd = next(state);
#if defined(_M_ARM64)
    lo = rnd * (uint64_t)range;
    hi = __umulh(rnd, (uint64_t)range);
#else
    lo = _umul128(rnd, (uint64_t)range, &hi);
#endif
  }
  return (size_t)hi;
#else
  /* ── fallback: rejection sampling (portable, two div ops) ──────── */
  (void)state; (void)next;
  uint64_t limit = UINT64_MAX - (UINT64_MAX % range);
  if (rnd > limit) return _ccshuffle_range(next(state), range, state, next);
  return (size_t)(rnd % range);
#endif
}

/* ── internal: byte-swap helper ───────────────────────────────────────── */

#define ccshuffle_swap(tmp, a, b, sz) do {                  \
  memcpy(tmp, a, sz); memcpy(a, b, sz); memcpy(b, tmp, sz); \
} while (0)

/* ── public API ───────────────────────────────────────────────────────── */

/** @brief Fisher-Yates (Durstenfeld) in-place shuffle.
 *
 *  Shuffles an array of @a len elements, each @a sz bytes wide.
 *  O(n) time, O(1) space (stack swap buffer; heap fallback for elements
 *  larger than CCSHUFFLE_BSIZE).
 *
 *  @param base       Pointer to the start of the array (must not be NULL).
 *  @param len        Number of elements to shuffle.  len ≤ 1 is a no-op.
 *  @param sz         Size in bytes of each element.
 *  @param state      Opaque PRNG state passed to @a prng_next (must not be NULL).
 *  @param prng_next  PRNG callback returning uniformly-distributed uint64_t
 *                    (must not be NULL). */
CCSHUFFLE_INLINE void
ccshuffle_knuth(void *base, size_t len, size_t sz, void *state,
                ccshuffle_prng_t prng_next) CCSHUFFLE_NOEXCEPT {
  if (!base || !state || !prng_next) return;

  uint8_t  stack_buf[CCSHUFFLE_BSIZE];
  uint8_t *tmp;
  size_t   i, j;
  uint8_t *a, *b;

  tmp = stack_buf;
  if (sz > CCSHUFFLE_BSIZE) {
    tmp = (uint8_t *)malloc(sz);
    if (!tmp) return;
  }

  /* Durstenfeld / Fisher-Yates: i steps from len down to 2 */
  for (i = len; i > 1; i--) {
    j = _ccshuffle_range(prng_next(state), i, state, prng_next);
    a = (uint8_t *)base + (i - 1) * sz;  /* element i-1 */
    b = (uint8_t *)base + j * sz;        /* element j   */
    if (a != b) ccshuffle_swap(tmp, a, b, sz);
  }

  if (tmp != stack_buf) free(tmp);
}

#ifdef __cplusplus
}
#endif

#endif /* CCSHUFFLE_H */