/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  @file    ccbi.h
**  @brief   Arbitrary-precision integer arithmetic library (big integer)
**  @version 1.0.0
**
**  Header-only, C99/C++ compatible, cross-platform (32/64-bit).
**
**  ── Quick Start ──
**
**    ccbi_t a, b, c;
**    ccbi_init(&a);  ccbi_init(&b);  ccbi_init(&c);
**
**    ccbi_from_int(&a, 12345);
**    ccbi_from_str(&b, "987654321098765432109876543210", 10);
**
**    ccbi_add(&c, &a, &b);
**    ccbi_mul(&c, &a, &b);
**
**    char *s = ccbi_to_str(&c, 10);
**    printf("result = %s\n", s);
**    ccbi_free_str(s);
**
**    ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&c);
**
**  ── Bitwise / single-bit ──
**
**    ccbi_and(&z, &a, &b);            //  z = a & b
**    ccbi_or (&z, &a, &b);            //  z = a | b
**    ccbi_xor(&z, &a, &b);            //  z = a ^ b
**    ccbi_not(&z, &a);                //  z = ~a (bit_length 范围)
**
**    if (ccbi_test_bit(&a, 7)) …      //  测试第 7 位
**    ccbi_set_bit(&m, 63);            //  第 63 位置 1，自动扩容
**    ccbi_clear_bit(&m, 31);          //  第 31 位清零
**    ccbi_flip_bit(&m, 0);            //  第 0 位翻转
**
**  ── Allocator hooks ──
**
**    #define CCBI_REALLOC my_realloc
**    #include "ccbi.h"
**
**  @define CCBI_MALLOC(sz)     Allocate (default: CCBI_REALLOC(NULL, sz))
**  @define CCBI_REALLOC(p,sz)  Reallocate (default: realloc)
**  @define CCBI_FREE(p)        Free (default: free)
*/
#ifndef CCBI_H
#define CCBI_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#if defined(_MSC_VER)
  #include <intrin.h>
#endif

/* ── portable inline ──────────────────────────────────────────────────── */

#if   defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
  #define CCBI_INLINE static inline
#elif defined(_MSC_VER)
  #define CCBI_INLINE static __inline
#elif defined(__GNUC__)
  #define CCBI_INLINE static __inline__
#else
  #define CCBI_INLINE static
#endif

#if defined(__cplusplus) && __cplusplus >= 201703L
  #define CCBI_NOEXCEPT noexcept
#else
  #define CCBI_NOEXCEPT
#endif

/* ── CLZ (count leading zeros, 32-bit) ────────────────────────────────── */

#ifndef ccbi_clz32
  #if defined(__GNUC__) || defined(__clang__)
    #define ccbi_clz32(x) ((uint32_t)__builtin_clz(x))
  #elif defined(_MSC_VER)
    CCBI_INLINE uint32_t ccbi_clz32_msvc(uint32_t x) {
      unsigned long i;
      return _BitScanReverse(&i, x) ? 31 - (uint32_t)i : 32;
    }
    #define ccbi_clz32(x) ccbi_clz32_msvc(x)
  #else
    static uint32_t ccbi_clz32_fallback(uint32_t x) {
      uint32_t n = 32; uint32_t y;
      y = x >> 16; if (y) { n -= 16; x = y; }
      y = x >>  8; if (y) { n -=  8; x = y; }
      y = x >>  4; if (y) { n -=  4; x = y; }
      y = x >>  2; if (y) { n -=  2; x = y; }
      y = x >>  1; if (y) { n -=  1; }
      return n - x;
    }
    #define ccbi_clz32(x) ccbi_clz32_fallback(x)
  #endif
#endif

/* ── allocator hooks ──────────────────────────────────────────────────── */

#ifndef CCBI_REALLOC
  #define CCBI_REALLOC realloc
#endif
#ifndef CCBI_MALLOC
  #define CCBI_MALLOC(sz) CCBI_REALLOC(NULL, (sz))
#endif
#ifndef CCBI_FREE
  #define CCBI_FREE(ptr) free(ptr)
#endif

/* ── SSO buffer size (default: 256-bit = 8 limbs) ────────────────────── */

#ifndef CCBI_SSO_LIMBS
  #define CCBI_SSO_LIMBS  8
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 *  ── Types ──
 * ========================================================================== */

/**
 *  @brief Big integer: sign-magnitude, little-endian 32-bit limbs.
 *
 *  Small-String-Optimization (SSO): when `limbs` points to `internal[]`,
 *  zero heap allocation.  Once used exceeds `CCBI_SSO_LIMBS`, a heap
 *  buffer is allocated.  Meta-data (sign/used/cap) is packed into one
 *  uint32_t via shift macros — no bitfields, portable C99.
 *
 *  meta bit layout (little-endian):
 *    bit  1–0:  sign  (2-bit signed: -1/0/1)
 *    bit 16–2:  used  (15-bit: 0..32767)
 *    bit 31–17: cap   (15-bit: 0..32767)
 */
typedef struct ccbi {
  uint32_t  *limbs;                    /* 8B: → internal[] or heap */
  uint32_t   internal[CCBI_SSO_LIMBS]; /* 4*SSO_LIMBS B: inline buffer */
  uint32_t   meta;                     /* 4B: sign|used|cap packed */
} ccbi_t;  /* total: 12 + 4*SSO_LIMBS B (44B at SSO_LIMBS=8) */

/* ── meta-field access macros ── */

/** Extract sign (-1/0/1) from packed meta.
 *  Bit 0 = sign magnitude (1 for positive), Bit 1 = sign extension (-1). */
#define CCBI_SIGN(z)    ((int32_t)((((z)->meta) & 2u) ? -1 : (int32_t)(((z)->meta) & 1u)))

/** Store sign (-1/0/1) into packed meta. */
#define CCBI_SET_SIGN(z,v)  do {                                           \
    (z)->meta = ((z)->meta & ~3u) | ((uint32_t)(int32_t)(v) & 3u);         \
  } while(0)

/** Extract used-limb count from packed meta. */
#define CCBI_USED(z)    (((z)->meta >> 2) & 0x7FFFu)

/** Store used-limb count into packed meta. */
#define CCBI_SET_USED(z,v)  do {                                           \
    (z)->meta = ((z)->meta & ~(0x7FFFu << 2))                              \
              | (((uint32_t)(v) & 0x7FFFu) << 2);                          \
  } while(0)

/** Extract allocated limb capacity from packed meta. */
#define CCBI_CAP(z)     (((z)->meta >> 17) & 0x7FFFu)

/** Store allocated limb capacity into packed meta. */
#define CCBI_SET_CAP(z,v)  do {                                            \
    (z)->meta = ((z)->meta & ~(0x7FFFu << 17))                             \
              | (((uint32_t)(v) & 0x7FFFu) << 17);                         \
  } while(0)

/* ==========================================================================
 *  ── Internal helpers ──
 * ========================================================================== */

/** @brief Ensure capacity, preserving existing data. */
CCBI_INLINE int
ccbi_grow(ccbi_t *z, uint32_t need) CCBI_NOEXCEPT {
  /* Current capacity: SSO_LIMBS while limbs points to internal, else from meta. */
  uint32_t cur_cap = (z->limbs == z->internal) ? (uint32_t)CCBI_SSO_LIMBS : CCBI_CAP(z);
  if (need <= cur_cap) return 0;
  uint32_t new_cap = cur_cap ? cur_cap * 2 : 8;
  while (new_cap < need) new_cap *= 2;
  uint32_t *p = (uint32_t *)CCBI_MALLOC(new_cap * sizeof(uint32_t));
  if (!p) return -1;
  uint32_t used = CCBI_USED(z);
  if (used) memcpy(p, z->limbs, used * sizeof(uint32_t));
  if (z->limbs != z->internal)
    CCBI_FREE(z->limbs);
  z->limbs = p;
  CCBI_SET_CAP(z, new_cap);
  return 0;
}

/** @brief Remove leading-zero limbs, set sign=0 when zero. */
CCBI_INLINE void
ccbi_normalize(ccbi_t *z) CCBI_NOEXCEPT {
  uint32_t u = CCBI_USED(z);
  while (u > 0 && z->limbs[u - 1] == 0) u--;
  CCBI_SET_USED(z, u);
  if (u == 0) CCBI_SET_SIGN(z, 0);
}

/** @brief a + b + carry_in → *r = low, return carry. */
CCBI_INLINE uint32_t
ccbi_limb_add(uint32_t *r, uint32_t a, uint32_t b, uint32_t carry) CCBI_NOEXCEPT {
  uint64_t s = (uint64_t)a + (uint64_t)b + (uint64_t)carry;
  *r = (uint32_t)s;
  return (uint32_t)(s >> 32);
}

/** @brief a - b - borrow_in → *r = low, return borrow. */
CCBI_INLINE uint32_t
ccbi_limb_sub(uint32_t *r, uint32_t a, uint32_t b, uint32_t borrow) CCBI_NOEXCEPT {
  uint64_t d = (uint64_t)a - (uint64_t)b - (uint64_t)borrow;
  *r = (uint32_t)d;
  return (uint32_t)((d >> 32) & 1);
}

/** @brief a * b + carry_in → *r = low, return high. */
CCBI_INLINE uint32_t
ccbi_limb_mul(uint32_t *r, uint32_t a, uint32_t b, uint32_t carry) CCBI_NOEXCEPT {
  uint64_t p = (uint64_t)a * (uint64_t)b + (uint64_t)carry;
  *r = (uint32_t)p;
  return (uint32_t)(p >> 32);
}

/* Forward decls for functions defined later but called early. */
CCBI_INLINE void ccbi_zero(ccbi_t *z) CCBI_NOEXCEPT;
CCBI_INLINE int  ccbi_mul_uint(ccbi_t *z, uint32_t v) CCBI_NOEXCEPT;
CCBI_INLINE int  ccbi_add_uint(ccbi_t *z, uint32_t v) CCBI_NOEXCEPT;
CCBI_INLINE int  ccbi_shl(ccbi_t *z, const ccbi_t *a, size_t n) CCBI_NOEXCEPT;
CCBI_INLINE int  ccbi_shr(ccbi_t *z, const ccbi_t *a, size_t n) CCBI_NOEXCEPT;
CCBI_INLINE void ccbi_abs(ccbi_t *z, const ccbi_t *a) CCBI_NOEXCEPT;


/* ── unsigned magnitude arithmetic (|a|, |b| assumed) ─────────────────── */

CCBI_INLINE int
ccbi_uadd(ccbi_t *z, const ccbi_t *a, const ccbi_t *b) CCBI_NOEXCEPT {
  uint32_t alen = CCBI_USED(a), blen = CCBI_USED(b);
  uint32_t maxlen = (alen > blen) ? alen : blen;
  int a_isz = (a == z), b_isz = (b == z);  /* alias detection before grow */
  if (ccbi_grow(z, maxlen + 1)) return -1;
  /* Save limb pointers AFTER grow — z->limbs may have been reallocated.
   * If a/z or b/z alias, use z->limbs (data was memcpy'd by grow). */
  uint32_t *z_limbs = z->limbs;
  const uint32_t *a_limbs = a_isz ? z_limbs : a->limbs;
  const uint32_t *b_limbs = b_isz ? z_limbs : b->limbs;
  uint32_t carry = 0, i;
  for (i = 0; i < maxlen; i++) {
    uint32_t av = (i < alen) ? a_limbs[i] : 0;
    uint32_t bv = (i < blen) ? b_limbs[i] : 0;
    carry = ccbi_limb_add(&z_limbs[i], av, bv, carry);
  }
  z_limbs[maxlen] = carry;
  CCBI_SET_USED(z, maxlen + (carry ? 1 : 0));
  return 0;
}

CCBI_INLINE int
ccbi_usub(ccbi_t *z, const ccbi_t *a, const ccbi_t *b) CCBI_NOEXCEPT {
  uint32_t alen = CCBI_USED(a), blen = CCBI_USED(b);
  int a_isz = (a == z);  /* alias detection before grow */
  if (ccbi_grow(z, alen)) return -1;
  uint32_t *z_limbs = z->limbs;
  const uint32_t *a_limbs = a_isz ? z_limbs : a->limbs;
  const uint32_t *b_limbs = (b == z) ? z_limbs : b->limbs;
  uint32_t borrow = 0, i;
  uint32_t minlen = (alen < blen) ? alen : blen;
  for (i = 0; i < minlen; i++)
    borrow = ccbi_limb_sub(&z_limbs[i], a_limbs[i], b_limbs[i], borrow);
  for (; i < alen; i++)
    borrow = ccbi_limb_sub(&z_limbs[i], a_limbs[i], 0, borrow);
  CCBI_SET_USED(z, alen);
  ccbi_normalize(z);
  return 0;
}

/* Single-limb unsigned division: q = a / v, *r = a % v. */
CCBI_INLINE int
ccbi_udiv_limb(ccbi_t *q, uint32_t *r, const ccbi_t *a, uint32_t v) CCBI_NOEXCEPT {
  if (v == 0) return -1;
  uint32_t au = CCBI_USED(a);
  if (au == 0) { *r = 0; ccbi_zero(q); return 0; }
  if (ccbi_grow(q, au)) return -1;
  uint64_t rem = 0;
  uint32_t i = au;
  while (i > 0) { i--;
    rem = (rem << 32) | a->limbs[i];
    q->limbs[i] = (uint32_t)(rem / v);
    rem = rem % v;
  }
  CCBI_SET_USED(q, au); ccbi_normalize(q);
  *r = (uint32_t)rem;
  return 0;
}

/* ==========================================================================
 *  ── Lifecycle ──
 * ========================================================================== */

CCBI_INLINE void ccbi_init(ccbi_t *z) CCBI_NOEXCEPT {
  z->limbs = z->internal;
  z->meta  = 0;  /* sign=0, used=0, cap implicitly = SSO_LIMBS via grow */
}

CCBI_INLINE void ccbi_destroy(ccbi_t *z) CCBI_NOEXCEPT {
  if (!z) return;
  if (z->limbs != z->internal)
    CCBI_FREE(z->limbs);
  z->limbs = z->internal;
  z->meta  = 0;
}

CCBI_INLINE void ccbi_zero(ccbi_t *z) CCBI_NOEXCEPT {
  if (z->limbs != z->internal) {
    CCBI_FREE(z->limbs);
    z->limbs = z->internal;
  }
  z->meta = 0;
}

CCBI_INLINE void ccbi_one(ccbi_t *z) CCBI_NOEXCEPT {
  if (z->limbs != z->internal) {
    CCBI_FREE(z->limbs);
    z->limbs = z->internal;
  }
  z->internal[0] = 1;
  CCBI_SET_USED(z, 1);
  CCBI_SET_SIGN(z, 1);
}

CCBI_INLINE int ccbi_copy(ccbi_t *z, const ccbi_t *src) CCBI_NOEXCEPT {
  if (z == src) return 0;
  uint32_t su = CCBI_USED(src);
  if (ccbi_grow(z, su)) return -1;
  memcpy(z->limbs, src->limbs, su * sizeof(uint32_t));
  CCBI_SET_USED(z, su);
  CCBI_SET_SIGN(z, CCBI_SIGN(src));
  return 0;
}

CCBI_INLINE void ccbi_swap(ccbi_t *a, ccbi_t *b) CCBI_NOEXCEPT {
  if (a == b) return;

  /* Record whether each side currently uses its SSO buffer (before swap). */
  int a_sso = (a->limbs == a->internal);
  int b_sso = (b->limbs == b->internal);

  /* Full struct exchange (including limbs ptr + internal array). */
  ccbi_t t = *a; *a = *b; *b = t;

  /* After exchange, if one side was SSO, the other's limbs now points
   * to the wrong internal array — redirect to the correct one. */
  if (a_sso) b->limbs = b->internal;
  if (b_sso) a->limbs = a->internal;
}

/* ==========================================================================
 *  ── Assignment ──
 * ========================================================================== */

CCBI_INLINE int ccbi_from_int(ccbi_t *z, int64_t v) CCBI_NOEXCEPT {
  if (v == 0) { ccbi_zero(z); return 0; }
  if (ccbi_grow(z, 2)) return -1;
  /* Cast to uint64_t first to avoid UB on INT64_MIN (-v overflows int64_t). */
  uint64_t uv = v < 0 ? -(uint64_t)v : (uint64_t)v;
  CCBI_SET_SIGN(z, (int32_t)(v < 0 ? -1 : 1));
  z->limbs[0] = (uint32_t)uv;
  z->limbs[1] = (uint32_t)(uv >> 32);
  CCBI_SET_USED(z, (uv > 0xFFFFFFFFULL) ? 2 : 1);
  return 0;
}
CCBI_INLINE int ccbi_from_uint(ccbi_t *z, uint64_t v) CCBI_NOEXCEPT {
  if (v == 0) { ccbi_zero(z); return 0; }
  if (ccbi_grow(z, 2)) return -1;
  CCBI_SET_SIGN(z, 1); CCBI_SET_USED(z, 1);
  z->limbs[0] = (uint32_t)v;
  if (v > 0xFFFFFFFFULL) {
    z->limbs[1] = (uint32_t)(v >> 32); CCBI_SET_USED(z, 2);
  }
  return 0;
}

/* ── decimal chunk constants ─────────────────────────────────────────── */
#define CCBI_STR_CHUNK     9
#define CCBI_STR_CHUNK_BASE 1000000000u  /* 10^9 */

CCBI_INLINE int
ccbi_from_str(ccbi_t *z, const char *str, int base) CCBI_NOEXCEPT {
  if (!z || !str) return -1;
  if (base < 2 || base > 36) return -1;
  while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') str++;
  int sign = 1;
  if (*str == '-') { sign = -1; str++; }
  else if (*str == '+') { str++; }
  if (!*str) return -1;
  ccbi_zero(z);
  if (base == 10) {
    /* Decimal fast path: 9-digit chunks, reduces mul/add from N to N/9. */
    const char *p = str;
    while (*p >= '0' && *p <= '9') p++;
    size_t total = (size_t)(p - str);
    if (total == 0) return -1;
    size_t first = total % CCBI_STR_CHUNK;
    if (first == 0) first = CCBI_STR_CHUNK;
    uint32_t chunk = 0;
    for (size_t i = 0; i < first; i++)
      chunk = chunk * 10 + (uint32_t)(str[i] - '0');
    if (ccbi_from_uint(z, chunk)) return -1;
    str += first;
    while (*str) {
      chunk = 0;
      for (int i = 0; i < CCBI_STR_CHUNK; i++) {
        if (str[i] < '0' || str[i] > '9') goto done_decimal;
        chunk = chunk * 10 + (uint32_t)(str[i] - '0');
      }
      if (ccbi_mul_uint(z, CCBI_STR_CHUNK_BASE)) return -1;
      if (ccbi_add_uint(z, chunk)) return -1;
      str += CCBI_STR_CHUNK;
    }
    done_decimal:;
  } else {
    while (*str) {
      char c = *str;
      int digit;
      if      (c >= '0' && c <= '9') digit = c - '0';
      else if (c >= 'a' && c <= 'z') digit = c - 'a' + 10;
      else if (c >= 'A' && c <= 'Z') digit = c - 'A' + 10;
      else break;
      if (digit >= base) return -1;
      if (ccbi_mul_uint(z, (uint32_t)base)) return -1;
      if (ccbi_add_uint(z, (uint32_t)digit)) return -1;
      str++;
    }
  }
  if (CCBI_USED(z) > 0) CCBI_SET_SIGN(z, (int32_t)sign);
  return 0;
}

/* ==========================================================================
 *  ── Comparison ──
 * ========================================================================== */

CCBI_INLINE int ccbi_sign(const ccbi_t *z) CCBI_NOEXCEPT { return (int)CCBI_SIGN(z); }
CCBI_INLINE int ccbi_is_zero(const ccbi_t *z) CCBI_NOEXCEPT { return CCBI_USED(z) == 0; }
CCBI_INLINE int ccbi_is_one(const ccbi_t *z) CCBI_NOEXCEPT {
  return CCBI_SIGN(z) == 1 && CCBI_USED(z) == 1 && z->limbs[0] == 1;
}
CCBI_INLINE int ccbi_is_neg(const ccbi_t *z) CCBI_NOEXCEPT { return CCBI_SIGN(z) < 0; }

CCBI_INLINE int
ccbi_cmp_abs(const ccbi_t *a, const ccbi_t *b) CCBI_NOEXCEPT {
  uint32_t au = CCBI_USED(a), bu = CCBI_USED(b);
  if (au != bu) return (au > bu) ? 1 : -1;
  uint32_t i = au;
  while (i > 0) { i--;
    if (a->limbs[i] != b->limbs[i])
      return (a->limbs[i] > b->limbs[i]) ? 1 : -1;
  }
  return 0;
}

CCBI_INLINE int
ccbi_cmp(const ccbi_t *a, const ccbi_t *b) CCBI_NOEXCEPT {
  int as = CCBI_SIGN(a), bs = CCBI_SIGN(b);
  if (as != bs) return (as > bs) ? 1 : -1;
  if (CCBI_USED(a) == 0 && CCBI_USED(b) == 0) return 0;
  int r = ccbi_cmp_abs(a, b);
  return (as < 0) ? -r : r;
}

CCBI_INLINE int
ccbi_cmp_int(const ccbi_t *a, int64_t b) CCBI_NOEXCEPT {
  ccbi_t tmp; ccbi_init(&tmp); ccbi_from_int(&tmp, b);
  int r = ccbi_cmp(a, &tmp);
  ccbi_destroy(&tmp);
  return r;
}

/* ==========================================================================
 *  ── Arithmetic ──
 * ========================================================================== */

CCBI_INLINE int
ccbi_add(ccbi_t *z, const ccbi_t *a, const ccbi_t *b) CCBI_NOEXCEPT {
  if (CCBI_USED(a) == 0) return ccbi_copy(z, b);
  if (CCBI_USED(b) == 0) return ccbi_copy(z, a);
  ccbi_t t; ccbi_init(&t);
  int r;
  if (CCBI_SIGN(a) == CCBI_SIGN(b)) {
    r = ccbi_uadd(&t, a, b);
    CCBI_SET_SIGN(&t, CCBI_SIGN(a));
  } else {
    int c = ccbi_cmp_abs(a, b);
    if (c == 0) { ccbi_zero(z); ccbi_destroy(&t); return 0; }
    if (c > 0) { r = ccbi_usub(&t, a, b); CCBI_SET_SIGN(&t, CCBI_SIGN(a)); }
    else       { r = ccbi_usub(&t, b, a); CCBI_SET_SIGN(&t, CCBI_SIGN(b)); }
  }
  if (r == 0) { ccbi_swap(z, &t); ccbi_destroy(&t); }
  else ccbi_destroy(&t);
  return r;
}

CCBI_INLINE int
ccbi_sub(ccbi_t *z, const ccbi_t *a, const ccbi_t *b) CCBI_NOEXCEPT {
  ccbi_t nb; ccbi_init(&nb);
  ccbi_copy(&nb, b); CCBI_SET_SIGN(&nb, (int32_t)(-CCBI_SIGN(&nb)));
  int r = ccbi_add(z, a, &nb);
  ccbi_destroy(&nb);
  return r;
}

CCBI_INLINE int
ccbi_add_uint(ccbi_t *z, uint32_t v) CCBI_NOEXCEPT {
  if (v == 0) return 0;
  uint32_t zu = CCBI_USED(z);
  if (zu == 0) {
    if (ccbi_grow(z, 1)) return -1;
    CCBI_SET_SIGN(z, 1); CCBI_SET_USED(z, 1); z->limbs[0] = v;
    return 0;
  }
  if (CCBI_SIGN(z) < 0) {
    if (z->limbs[0] >= v) { z->limbs[0] -= v; ccbi_normalize(z); return 0; }
    /* |z| < v, flip positive. */
    ccbi_t tmp; ccbi_init(&tmp); ccbi_from_uint(&tmp, v);
    int r = ccbi_usub(z, &tmp, z); CCBI_SET_SIGN(z, 1);
    ccbi_destroy(&tmp);
    return r;
  }
  uint64_t s = (uint64_t)z->limbs[0] + v;
  z->limbs[0] = (uint32_t)s;
  uint32_t carry = (uint32_t)(s >> 32), i = 1;
  while (carry && i < zu)
    carry = ccbi_limb_add(&z->limbs[i], z->limbs[i], 0, carry), i++;
  if (carry) {
    if (ccbi_grow(z, zu + 1)) return -1;
    z->limbs[zu] = carry;
    CCBI_SET_USED(z, zu + 1);
  }
  return 0;
}

CCBI_INLINE int
ccbi_mul_uint(ccbi_t *z, uint32_t v) CCBI_NOEXCEPT {
  uint32_t zu = CCBI_USED(z);
  if (v == 0 || zu == 0) { ccbi_zero(z); return 0; }
  if (v == 1) return 0;
  if (ccbi_grow(z, zu + 1)) return -1;
  uint32_t carry = 0;
  for (uint32_t i = 0; i < zu; i++)
    carry = ccbi_limb_mul(&z->limbs[i], z->limbs[i], v, carry);
  z->limbs[zu] = carry;
  CCBI_SET_USED(z, zu + (carry ? 1 : 0));
  return 0;
}

/* ── Karatsuba threshold (default: 16 limbs = 512-bit) ────────────────── */
#ifndef CCBI_KARATSUBA_THRESH
  #define CCBI_KARATSUBA_THRESH  16
#endif

/* ── Schoolbook O(n²) inner loop (sign-agnostic) ─────────────────────── */

CCBI_INLINE void
_ccbi_mul_schoolbook(ccbi_t *z, const ccbi_t *a, const ccbi_t *b) CCBI_NOEXCEPT {
  uint32_t alen = CCBI_USED(a), blen = CCBI_USED(b);
  if (alen == 0 || blen == 0) { ccbi_zero(z); return; }

  ccbi_grow(z, alen + blen);
  memset(z->limbs, 0, (alen + blen) * sizeof(uint32_t));
  for (uint32_t i = 0; i < alen; i++) {
    uint32_t carry = 0;
    for (uint32_t j = 0; j < blen; j++) {
      uint64_t p = (uint64_t)a->limbs[i] * (uint64_t)b->limbs[j]
                 + (uint64_t)z->limbs[i + j] + (uint64_t)carry;
      z->limbs[i + j] = (uint32_t)p;
      carry = (uint32_t)(p >> 32);
    }
    z->limbs[i + blen] = carry;
  }
  CCBI_SET_USED(z, alen + blen);
  ccbi_normalize(z);
  CCBI_SET_SIGN(z, (int32_t)(CCBI_SIGN(a) * CCBI_SIGN(b)));
}

/* ── Karatsuba O(n^1.585) recursive multiplication (absolute values) ──── */

CCBI_INLINE int
_ccbi_mul_karatsuba(ccbi_t *z, const ccbi_t *a, const ccbi_t *b) CCBI_NOEXCEPT {
  uint32_t alen = CCBI_USED(a), blen = CCBI_USED(b);
  uint32_t n = (alen > blen) ? alen : blen;
  int ret = 0;

  if (n < CCBI_KARATSUBA_THRESH) {
    ccbi_t t; ccbi_init(&t);
    _ccbi_mul_schoolbook(&t, a, b);
    ccbi_swap(z, &t); ccbi_destroy(&t);
    return 0;
  }

  uint32_t m = n / 2;                    /* split point */
  uint32_t a0n = (m < alen) ? m : alen;  /* low part limbs */
  uint32_t a1n = (alen > m) ? alen - m : 0;
  uint32_t b0n = (m < blen) ? m : blen;
  uint32_t b1n = (blen > m) ? blen - m : 0;

  ccbi_t a0, a1, b0, b1, z0, z1, z2, s0, s1;
  ccbi_init(&a0); ccbi_init(&a1); ccbi_init(&b0); ccbi_init(&b1);
  ccbi_init(&z0); ccbi_init(&z1); ccbi_init(&z2);
  ccbi_init(&s0); ccbi_init(&s1);

  /* Split operands by copying. */
  if (a0n) { ccbi_grow(&a0, a0n); memcpy(a0.limbs, a->limbs, a0n * 4); CCBI_SET_USED(&a0, a0n); CCBI_SET_SIGN(&a0, 1); }
  if (a1n) { ccbi_grow(&a1, a1n); memcpy(a1.limbs, a->limbs + m, a1n * 4); CCBI_SET_USED(&a1, a1n); CCBI_SET_SIGN(&a1, 1); }
  if (b0n) { ccbi_grow(&b0, b0n); memcpy(b0.limbs, b->limbs, b0n * 4); CCBI_SET_USED(&b0, b0n); CCBI_SET_SIGN(&b0, 1); }
  if (b1n) { ccbi_grow(&b1, b1n); memcpy(b1.limbs, b->limbs + m, b1n * 4); CCBI_SET_USED(&b1, b1n); CCBI_SET_SIGN(&b1, 1); }

  /* z0 = a0 * b0   (low cross) */
  if (a0n && b0n && (ret = _ccbi_mul_karatsuba(&z0, &a0, &b0))) goto cleanup;
  /* z2 = a1 * b1   (high cross) */
  if (a1n && b1n && (ret = _ccbi_mul_karatsuba(&z2, &a1, &b1))) goto cleanup;

  /* s0 = a0 + a1, s1 = b0 + b1  (with full carry propagation).
   *
   * We use proper carry-propagating unsigned addition (ccbi_uadd) so that
   * s0 and s1 represent the true integer sums.  Each has at most n/2+1
   * limbs, guaranteeing the recursive call in step z1 reduces the
   * problem size and cannot recurse infinitely. */
  ccbi_uadd(&s0, &a0, &a1);
  ccbi_uadd(&s1, &b0, &b1);
  CCBI_SET_SIGN(&s0, 1);
  CCBI_SET_SIGN(&s1, 1);

  /* z1 = s0 * s1 - z0 - z2
   * With carry-free addition, s0 and s1 have ~ n/2 limbs, so the
   * recursive call reduces the problem size correctly. */
  if ((ret = _ccbi_mul_karatsuba(&z1, &s0, &s1))) goto cleanup;
  ccbi_sub(&z1, &z1, &z0);
  if (CCBI_USED(&z2)) ccbi_sub(&z1, &z1, &z2);

  /* Assemble: result = z0 + z1·B^m + z2·B^(2m) */
  if ((ret = ccbi_grow(z, alen + blen))) goto cleanup;
  memset(z->limbs, 0, (alen + blen) * sizeof(uint32_t));

  /* Direct limb-array assembly (avoids ccbi_shl temps). */
  {
    uint32_t maxu = 0;
    uint32_t u0 = CCBI_USED(&z0), u1 = CCBI_USED(&z1), u2 = CCBI_USED(&z2);

    if (u0) { memcpy(z->limbs, z0.limbs, u0 * 4); maxu = u0; }

    if (u1) {
      uint32_t carry = 0, k;
      for (k = 0; k < u1; k++) {
        uint64_t s = (uint64_t)z->limbs[m + k] + z1.limbs[k] + carry;
        z->limbs[m + k] = (uint32_t)s;
        carry = (uint32_t)(s >> 32);
      }
      while (carry) {
        uint64_t s = (uint64_t)z->limbs[m + k] + carry;
        z->limbs[m + k] = (uint32_t)s;
        carry = (uint32_t)(s >> 32);
        k++;
      }
      if (m + k > maxu) maxu = m + k;
    }

    if (u2) {
      uint32_t carry = 0, k;
      for (k = 0; k < u2; k++) {
        uint64_t s = (uint64_t)z->limbs[2*m + k] + z2.limbs[k] + carry;
        z->limbs[2*m + k] = (uint32_t)s;
        carry = (uint32_t)(s >> 32);
      }
      while (carry) {
        uint64_t s = (uint64_t)z->limbs[2*m + k] + carry;
        z->limbs[2*m + k] = (uint32_t)s;
        carry = (uint32_t)(s >> 32);
        k++;
      }
      if (2*m + k > maxu) maxu = 2*m + k;
    }

    CCBI_SET_USED(z, maxu);
    ccbi_normalize(z);
  }
  CCBI_SET_SIGN(z, (int32_t)(CCBI_SIGN(a) * CCBI_SIGN(b)));

cleanup:
  ccbi_destroy(&a0); ccbi_destroy(&a1); ccbi_destroy(&b0); ccbi_destroy(&b1);
  ccbi_destroy(&z0); ccbi_destroy(&z1); ccbi_destroy(&z2);
  ccbi_destroy(&s0); ccbi_destroy(&s1);
  return ret;
}

/* ── Toom-3 threshold (default: 64 limbs = 2048-bit) ──────────────────── */
#ifndef CCBI_TOOM3_THRESH
  #define CCBI_TOOM3_THRESH  64
#endif

/* ── Toom-3 O(n^1.465) multiplication (sign-agnostic) ─────────────────── */

/** @brief Helper: add src to dest starting at limb offset. */
CCBI_INLINE void
_ccbi_asm_add(uint32_t *dest, const uint32_t *src, uint32_t srcn,
              uint32_t offset, uint32_t *maxu) CCBI_NOEXCEPT {
  uint32_t carry = 0, k;
  for (k = 0; k < srcn; k++) {
    uint64_t s = (uint64_t)dest[offset + k] + src[k] + carry;
    dest[offset + k] = (uint32_t)s;
    carry = (uint32_t)(s >> 32);
  }
  while (carry) {
    uint64_t s = (uint64_t)dest[offset + k] + carry;
    dest[offset + k] = (uint32_t)s;
    carry = (uint32_t)(s >> 32);
    k++;
  }
  if (offset + k > *maxu) *maxu = offset + k;
}

CCBI_INLINE int
_ccbi_mul_toom3(ccbi_t *z, const ccbi_t *a, const ccbi_t *b) CCBI_NOEXCEPT {
  uint32_t alen = CCBI_USED(a), blen = CCBI_USED(b);
  uint32_t n = (alen > blen) ? alen : blen;
  int ret = 0;

  if (n < CCBI_TOOM3_THRESH) return _ccbi_mul_karatsuba(z, a, b);

  uint32_t k = (n + 2) / 3;          /* split size: ceil(n/3) */
  uint32_t a0n = (k < alen) ? k : alen;
  uint32_t a1n = (alen > k) ? (alen - k > k ? k : alen - k) : 0;
  uint32_t a2n = (alen > 2 * k) ? alen - 2 * k : 0;
  uint32_t b0n = (k < blen) ? k : blen;
  uint32_t b1n = (blen > k) ? (blen - k > k ? k : blen - k) : 0;
  uint32_t b2n = (blen > 2 * k) ? blen - 2 * k : 0;

  ccbi_t a0, a1, a2, b0, b1, b2;
  ccbi_t va[5], vb[5], w[5];
  ccbi_init(&a0); ccbi_init(&a1); ccbi_init(&a2);
  ccbi_init(&b0); ccbi_init(&b1); ccbi_init(&b2);
  for (int i = 0; i < 5; i++) { ccbi_init(&va[i]); ccbi_init(&vb[i]); ccbi_init(&w[i]); }

  /* ── Split operands ── */
  #define CCBI_SPLIT(dst, src, off, lim) do { \
    if (lim) { ccbi_grow(&dst, lim); memcpy(dst.limbs, src->limbs + off, lim * 4); \
               CCBI_SET_USED(&dst, lim); CCBI_SET_SIGN(&dst, 1); } } while(0)
  CCBI_SPLIT(a0, a, 0, a0n);
  CCBI_SPLIT(a1, a, k, a1n);
  CCBI_SPLIT(a2, a, 2*k, a2n);
  CCBI_SPLIT(b0, b, 0, b0n);
  CCBI_SPLIT(b1, b, k, b1n);
  CCBI_SPLIT(b2, b, 2*k, b2n);

  /* ── Evaluate at 5 points ── */
  /* va[0] = a(0) = a0 */
  if (a0n) ccbi_copy(&va[0], &a0);
  /* va[4] = a(∞) = a2 */
  if (a2n) ccbi_copy(&va[4], &a2);
  /* va[1] = a(1) = a0 + a1 + a2 */
  ccbi_add(&va[1], &a0, &a1);
  if (a2n) ccbi_add(&va[1], &va[1], &a2);
  /* va[2] = a(-1) = a0 - a1 + a2 */
  ccbi_sub(&va[2], &a0, &a1);
  if (a2n) ccbi_add(&va[2], &va[2], &a2);
  /* va[3] = a(2) = a0 + 2*a1 + 4*a2 */
  if (a1n) {
    ccbi_copy(&va[3], &a1); ccbi_shl(&va[3], &va[3], 1);
    ccbi_add(&va[3], &va[3], &a0);
  } else { ccbi_copy(&va[3], &a0); }
  if (a2n) {
    ccbi_t t4; ccbi_init(&t4);
    ccbi_copy(&t4, &a2); ccbi_shl(&t4, &t4, 2);
    ccbi_add(&va[3], &va[3], &t4);
    ccbi_destroy(&t4);
  }

  /* Same for b */
  if (b0n) ccbi_copy(&vb[0], &b0);
  if (b2n) ccbi_copy(&vb[4], &b2);
  ccbi_add(&vb[1], &b0, &b1);
  if (b2n) ccbi_add(&vb[1], &vb[1], &b2);
  ccbi_sub(&vb[2], &b0, &b1);
  if (b2n) ccbi_add(&vb[2], &vb[2], &b2);
  if (b1n) {
    ccbi_copy(&vb[3], &b1); ccbi_shl(&vb[3], &vb[3], 1);
    ccbi_add(&vb[3], &vb[3], &b0);
  } else { ccbi_copy(&vb[3], &b0); }
  if (b2n) {
    ccbi_t t4; ccbi_init(&t4);
    ccbi_copy(&t4, &b2); ccbi_shl(&t4, &t4, 2);
    ccbi_add(&vb[3], &vb[3], &t4);
    ccbi_destroy(&t4);
  }

  /* ── 5 recursive multiplications ── */
  for (int i = 0; i < 5; i++) {
    if (CCBI_USED(&va[i]) && CCBI_USED(&vb[i]))
      ret = _ccbi_mul_toom3(&w[i], &va[i], &vb[i]);
    if (ret) goto cleanup;
  }

  /* ── Interpolation: solve 5×5 Vandermonde for coefficients c0..c4 ── */
  /* System (points 0,1,-1,2,∞):
   *   a = w1 - w0 - w4   (= c1 + c2 + c3)
   *   b = w2 - w0 - w4   (= -c1 + c2 - c3)
   *   d = w3 - w0 - 16*w4 (= 2c1 + 4c2 + 8c3)
   * Then:
   *   c2 = (a + b) / 2
   *   c3 = (d - 3*a - b) / 6
   *   c1 = (a - b) / 2 - c3
   */
  ccbi_sub(&w[1], &w[1], &w[0]);           /* w1 = w1 - w0 */
  ccbi_sub(&w[2], &w[2], &w[0]);           /* w2 = w2 - w0 */
  {
    ccbi_t t16; ccbi_init(&t16);
    ccbi_copy(&t16, &w[4]); ccbi_shl(&t16, &t16, 4);  /* 16*w4 */
    ccbi_sub(&w[3], &w[3], &t16);                      /* w3 -= 16*w4 */
    ccbi_sub(&w[3], &w[3], &w[0]);                     /* w3 -= w0 (now = d) */
    ccbi_destroy(&t16);
  }

  /* w1 = a, w2 = b, w3 = d.  Compute c2 = (a+b)/2 → store in w2.
   * Compute c3 = (d - 3*a - b) / 6 → store in w3.
   * Compute c1 = (a-b)/2 - c3 → store in w1. */
  ccbi_t sa, sb, sd;
  ccbi_init(&sa); ccbi_init(&sb); ccbi_init(&sd);
  ccbi_copy(&sa, &w[1]);                 /* save a = c1+c2+c3 */
  ccbi_copy(&sb, &w[2]);                 /* save b = -c1+c2-c3 */
  ccbi_copy(&sd, &w[3]);                 /* save d = 2c1+4c2+8c3 */

  /* c2 = (a + b) / 2 */
  ccbi_uadd(&w[2], &sa, &sb);
  if (CCBI_USED(&w[2])) ccbi_shr(&w[2], &w[2], 1);

  /* c3 = (d - 3a - b) / 6.  3a = a+a+a (avoids recursive mul). */
  {
    ccbi_t three_a; ccbi_init(&three_a);
    ccbi_copy(&three_a, &sa);
    ccbi_add(&three_a, &three_a, &sa);   /* 2a */
    ccbi_add(&three_a, &three_a, &sa);   /* 3a */
    ccbi_add(&three_a, &three_a, &sb);   /* 3a + b */
    ccbi_sub(&w[3], &sd, &three_a);      /* d - (3a+b) */
    ccbi_destroy(&three_a);
  }
  if (CCBI_USED(&w[3])) {
    int s3 = CCBI_SIGN(&w[3]);
    ccbi_abs(&w[3], &w[3]);
    { uint32_t rem; ccbi_udiv_limb(&w[3], &rem, &w[3], 6); }
    CCBI_SET_SIGN(&w[3], s3);
  }

  /* c1 = (a - b) / 2 - c3 */
  ccbi_sub(&w[1], &sa, &sb);
  if (CCBI_USED(&w[1])) {
    int s1 = CCBI_SIGN(&w[1]);
    ccbi_abs(&w[1], &w[1]); ccbi_shr(&w[1], &w[1], 1);
    CCBI_SET_SIGN(&w[1], s1);
  }
  ccbi_sub(&w[1], &w[1], &w[3]);

  ccbi_destroy(&sa); ccbi_destroy(&sb); ccbi_destroy(&sd);

  /* ── Assemble: z = w0 + w1·B^k + w2·B^(2k) + w3·B^(3k) + w4·B^(4k) ── */
  /* w4 sits at offset 4k and can have up to (a2n + b2n) limbs with carry. */
  {
    uint32_t asm_need = 4 * k + a2n + b2n + 4;
    if (asm_need < alen + blen) asm_need = alen + blen;
    if ((ret = ccbi_grow(z, asm_need))) goto cleanup;
    memset(z->limbs, 0, asm_need * sizeof(uint32_t));
  }

  {
    uint32_t maxu = 0;
    uint32_t u[5];
    for (int i = 0; i < 5; i++) u[i] = CCBI_USED(&w[i]);

    if (u[0]) { memcpy(z->limbs, w[0].limbs, u[0] * 4); maxu = u[0]; }
    if (u[1]) _ccbi_asm_add(z->limbs, w[1].limbs, u[1], k, &maxu);
    if (u[2]) _ccbi_asm_add(z->limbs, w[2].limbs, u[2], 2*k, &maxu);
    if (u[3]) _ccbi_asm_add(z->limbs, w[3].limbs, u[3], 3*k, &maxu);
    if (u[4]) _ccbi_asm_add(z->limbs, w[4].limbs, u[4], 4*k, &maxu);

    CCBI_SET_USED(z, maxu);
    ccbi_normalize(z);
  }
  CCBI_SET_SIGN(z, (int32_t)(CCBI_SIGN(a) * CCBI_SIGN(b)));

cleanup:
  ccbi_destroy(&a0); ccbi_destroy(&a1); ccbi_destroy(&a2);
  ccbi_destroy(&b0); ccbi_destroy(&b1); ccbi_destroy(&b2);
  for (int i = 0; i < 5; i++) {
    ccbi_destroy(&va[i]); ccbi_destroy(&vb[i]); ccbi_destroy(&w[i]);
  }
  return ret;
}

/* ── Public multiplication (dispatch) ──
 *   n < KARATSUBA_THRESH  → schoolbook
 *   KARATSUBA_THRESH ≤ n < TOOM3_THRESH → Karatsuba
 *   n ≥ TOOM3_THRESH      → Toom-3                              */

CCBI_INLINE int
ccbi_mul(ccbi_t *z, const ccbi_t *a, const ccbi_t *b) CCBI_NOEXCEPT {
  uint32_t alen = CCBI_USED(a), blen = CCBI_USED(b);
  if (alen == 0 || blen == 0) { ccbi_zero(z); return 0; }
  uint32_t n = (alen > blen) ? alen : blen;
  if (n >= CCBI_TOOM3_THRESH) return _ccbi_mul_toom3(z, a, b);
  if (n >= CCBI_KARATSUBA_THRESH) return _ccbi_mul_karatsuba(z, a, b);
  ccbi_t t; ccbi_init(&t);
  _ccbi_mul_schoolbook(&t, a, b);
  CCBI_SET_SIGN(&t, (int32_t)(CCBI_SIGN(a) * CCBI_SIGN(b)));
  ccbi_swap(z, &t); ccbi_destroy(&t);
  return 0;
}

CCBI_INLINE int
ccbi_muladd(ccbi_t *z, const ccbi_t *x, uint32_t y) CCBI_NOEXCEPT {
  /* z = z + x * y  (used by div_rem). */
  uint32_t xu = CCBI_USED(x);
  if (y == 0 || xu == 0) return 0;
  uint32_t zu = CCBI_USED(z);
  uint32_t need = (zu > xu + 1) ? zu : xu + 1;
  if (ccbi_grow(z, need + 1)) return -1;
  uint32_t cur_cap = CCBI_CAP(z);
  uint32_t carry = 0, i;
  for (i = 0; i < xu; i++)
    carry = ccbi_limb_mul(&z->limbs[i], x->limbs[i], y, carry);
  while (carry) {
    if (i >= zu) {
      if (i >= cur_cap && ccbi_grow(z, i + 2)) return -1;
      cur_cap = CCBI_CAP(z);
      z->limbs[i] = 0;
    }
    carry = ccbi_limb_add(&z->limbs[i], z->limbs[i], 0, carry);
    i++;
  }
  if (i > zu) CCBI_SET_USED(z, i);
  return 0;
}

CCBI_INLINE int
ccbi_div_rem(ccbi_t *q, ccbi_t *r, const ccbi_t *a, const ccbi_t *b) CCBI_NOEXCEPT {
  if (!b || CCBI_USED(b) == 0) return -1;
  int sign_a = CCBI_SIGN(a), sign_b = CCBI_SIGN(b);
  /* Work on magnitudes. */
  ccbi_t u, v; ccbi_init(&u); ccbi_init(&v);
  ccbi_copy(&u, a); CCBI_SET_SIGN(&u, 1);
  ccbi_copy(&v, b); CCBI_SET_SIGN(&v, 1);

  if (CCBI_USED(&u) < CCBI_USED(&v)) {
    if (q) ccbi_zero(q);
    if (r) ccbi_copy(r, &u);
    ccbi_destroy(&u); ccbi_destroy(&v);
    return 0;
  }

  /* Single-limb divisor fast path. */
  if (CCBI_USED(&v) == 1) {
    uint32_t rem;
    if (q) { if (ccbi_udiv_limb(q, &rem, &u, v.limbs[0])) { ccbi_destroy(&u); ccbi_destroy(&v); return -1; } }
    else   { ccbi_t tq; ccbi_init(&tq); int ok = ccbi_udiv_limb(&tq, &rem, &u, v.limbs[0]); ccbi_destroy(&tq); if (ok) { ccbi_destroy(&u); ccbi_destroy(&v); return -1; } }
    if (r) { ccbi_from_uint(r, rem); CCBI_SET_SIGN(r, sign_a); }
    if (q) CCBI_SET_SIGN(q, (int32_t)(sign_a * sign_b));
    ccbi_destroy(&u); ccbi_destroy(&v);
    return 0;
  }

  /* Multi-limb: quotient digit schoolbook division.
   * Uses the left-anchored method: take top (v.used+1) limbs of u,
   * divide by v, store quotient digit, multiply-subtract, repeat. */
  uint32_t n = CCBI_USED(&v);
  uint32_t uu = CCBI_USED(&u);
  uint32_t qlen = uu - n + 2;   /* max quotient limbs (+1 safety) */
  uint32_t max_qpos = 0;        /* highest ui_start where a digit was written */
  ccbi_t quo; ccbi_init(&quo);
  if (q && ccbi_grow(q, qlen)) { ccbi_destroy(&u); ccbi_destroy(&v); ccbi_destroy(&quo); return -1; }
  if (ccbi_grow(&quo, qlen)) { ccbi_destroy(&u); ccbi_destroy(&v); ccbi_destroy(&quo); return -1; }
  memset(quo.limbs, 0, qlen * sizeof(uint32_t));

  uu = CCBI_USED(&u);

  /* Knuth normalisation: shift u and v so v's top limb ≥ 2³¹,
   * making the trial quotient digit accurate to within ±2. */
  uint32_t shift = ccbi_clz32(v.limbs[n - 1]);
  if (shift) {
    if (ccbi_shl(&u, &u, shift) || ccbi_shl(&v, &v, shift)) {
      ccbi_destroy(&u); ccbi_destroy(&v); ccbi_destroy(&quo); return -1;
    }
    n = CCBI_USED(&v);
    uu = CCBI_USED(&u);
    /* Normalisation may have added a limb; grow quo if the max
     * quotient length increased. */
    if (uu - n + 2 > qlen) {
      qlen = uu - n + 2;
      if (ccbi_grow(&quo, qlen)) { ccbi_destroy(&u); ccbi_destroy(&v); ccbi_destroy(&quo); return -1; }
      /* Zero the entire buffer — ccbi_grow via malloc doesn't zero. */
      memset(quo.limbs, 0, qlen * sizeof(uint32_t));
    }
  }

  /* Precompute reciprocal: v_recip = ceil(2^64 / v_top).
   * Replaces each trial quotient's 64/32 divq with a mulq+shift. */
  uint32_t v_top_norm = v.limbs[n - 1];
#if defined(__SIZEOF_INT128__)
  uint64_t v_recip = (~0ULL / v_top_norm) + 1;  /* ceil(2^64 / v_top) */
#endif

  while (uu >= n && !(uu == n && ccbi_cmp_abs(&u, &v) < 0)) {
    /* Guess quotient digit via multiply-by-reciprocal (avoids divq). */
    uint64_t u_top = (uint64_t)u.limbs[uu - 1];
    if (uu > n) u_top = (u_top << 32) | u.limbs[uu - 2];
    uint32_t qd;
    if (u_top >= ((uint64_t)v_top_norm << 32))
      qd = 0xFFFFFFFF;
    else
#if defined(__SIZEOF_INT128__)
      qd = (uint32_t)(((__uint128_t)u_top * v_recip) >> 64);
#else
      qd = (uint32_t)(u_top / v_top_norm);
#endif

    if (qd == 0) { qd = 1; }
    /* Clamp: qd * v must not exceed u's top (n+1) limbs. */
    if (qd > 1) {
      /* Quick check: qd * v_hi */
      uint64_t prod = (uint64_t)qd * v_top_norm;
      if (prod > u_top) {
        while (qd > 1 && (uint64_t)qd * v_top_norm > u_top) qd--;
      }
    }

    /* Multiply-subtract: u -= qd * v (shifted).
     * carry = multiplication carry from qd*v[i]; borrow = subtraction borrow (0/1).
     * Combined: after each iteration, carry += diff_borrow so carry carries both. */
    const uint32_t *v_limbs = v.limbs;
    uint32_t carry = 0;
    uint32_t borrow = 0;
    uint32_t ui_start = uu - n - (uu > n ? 1 : 0);

    for (uint32_t i = 0; i < n; i++) {
      uint64_t p = (uint64_t)qd * v_limbs[i] + carry;
      carry = (uint32_t)(p >> 32);
      borrow = ccbi_limb_sub(&u.limbs[ui_start + i], u.limbs[ui_start + i], (uint32_t)p, borrow);
      carry += borrow;
      borrow = 0;
    }
    /* Propagate combined carry+borrow to the next limb.
     * When carry > 0 and ui_start + n >= uu (i.e. uu == n), there is no
     * extra limb to propagate to — the borrow already propagated through
     * all n limbs, meaning qd was too large.  Correct by adding back v. */
    if (carry) {
      if (ui_start + n < uu) {
        uint32_t b2 = ccbi_limb_sub(&u.limbs[ui_start + n], u.limbs[ui_start + n], 0, carry);
        if (b2) {
          /* Borrow propagated past top — qd too large, add back. */
          qd--;
          uint32_t carry2 = 0;
          for (uint32_t i = 0; i < n; i++)
            carry2 = ccbi_limb_add(&u.limbs[ui_start + i], u.limbs[ui_start + i], v_limbs[i], carry2);
          u.limbs[ui_start + n] += carry2;
        }
      } else {
        /* uu == n: no extra limb — carry > 0 means qd was too large.
         * Add back v to restore correct state. */
        uint32_t add_carry = 0;
        for (uint32_t i = 0; i < n; i++)
          add_carry = ccbi_limb_add(&u.limbs[i], u.limbs[i], v_limbs[i], add_carry);
        (void)add_carry;  /* add-back ensures u < v after this iteration */
        qd--;
      }
    }

    /* Strip leading zero limbs from u (cheaper than full ccbi_normalize).
     * Must sync uu back to the struct metadata so ccbi_cmp_abs in the
     * loop condition sees the correct used count. */
    while (uu > 0 && u.limbs[uu - 1] == 0) uu--;
    CCBI_SET_USED(&u, uu);

    /* Store quotient digit at its actual limb position (ui_start).
     * The initial memset ensures all unwritten slots stay zero. */
    quo.limbs[ui_start] = qd;
    if (ui_start + 1 > max_qpos) max_qpos = ui_start + 1;
  }

  /* Transfer quotient to output.
   * Digits are already at correct limb positions; no memmove needed. */
  if (q) {
    CCBI_SET_USED(&quo, max_qpos);
    ccbi_normalize(&quo);
    CCBI_SET_SIGN(&quo, (int32_t)(sign_a * sign_b));
    ccbi_swap(q, &quo);
  }
  if (r) {
    if (shift) ccbi_shr(&u, &u, shift);
    ccbi_normalize(&u);
    CCBI_SET_SIGN(&u, (int32_t)(sign_a < 0 ? -1 : 1));
    ccbi_swap(r, &u);
  }
  ccbi_destroy(&quo); ccbi_destroy(&u); ccbi_destroy(&v);
  return 0;
}

CCBI_INLINE int
ccbi_mod(ccbi_t *r, const ccbi_t *a, const ccbi_t *m) CCBI_NOEXCEPT {
  int ret = ccbi_div_rem(NULL, r, a, m);
  if (ret) return ret;
  if (CCBI_SIGN(r) < 0) { ccbi_t tmp; ccbi_init(&tmp); ccbi_abs(&tmp, m); ccbi_add(r, r, &tmp); ccbi_destroy(&tmp); }
  return 0;
}

CCBI_INLINE void ccbi_neg(ccbi_t *z, const ccbi_t *a) CCBI_NOEXCEPT {
  ccbi_copy(z, a); CCBI_SET_SIGN(z, (int32_t)(-CCBI_SIGN(z)));
}
CCBI_INLINE void ccbi_abs(ccbi_t *z, const ccbi_t *a) CCBI_NOEXCEPT {
  ccbi_copy(z, a); if (CCBI_SIGN(z) < 0) CCBI_SET_SIGN(z, 1);
}
CCBI_INLINE int ccbi_inc(ccbi_t *z) CCBI_NOEXCEPT { return ccbi_add_uint(z, 1); }
CCBI_INLINE int ccbi_dec(ccbi_t *z) CCBI_NOEXCEPT {
  ccbi_t one; ccbi_init(&one); ccbi_one(&one);
  int r = ccbi_sub(z, z, &one);
  ccbi_destroy(&one);
  return r;
}

/* ==========================================================================
 *  ── Shifts ──
 * ========================================================================== */

CCBI_INLINE size_t
ccbi_bit_length(const ccbi_t *z) CCBI_NOEXCEPT {
  uint32_t zu = CCBI_USED(z);
  if (zu == 0) return 0;
  return (size_t)(zu - 1) * 32 + (32 - ccbi_clz32(z->limbs[zu - 1]));
}

CCBI_INLINE int
ccbi_shl(ccbi_t *z, const ccbi_t *a, size_t n) CCBI_NOEXCEPT {
  uint32_t au = CCBI_USED(a);
  if (n == 0 || au == 0) return ccbi_copy(z, a);
  size_t limb_shift = n / 32, bit_shift = n % 32;
  uint32_t new_used = au + (uint32_t)limb_shift + (bit_shift ? 1 : 0);
  ccbi_t t; ccbi_init(&t);
  if (ccbi_grow(&t, new_used)) return -1;
  memset(t.limbs, 0, new_used * sizeof(uint32_t));
  if (bit_shift == 0) {
    memcpy(t.limbs + limb_shift, a->limbs, au * sizeof(uint32_t));
  } else {
    uint32_t carry = 0;
    for (uint32_t i = 0; i < au; i++) {
      t.limbs[i + limb_shift] = (a->limbs[i] << bit_shift) | carry;
      carry = a->limbs[i] >> (32 - bit_shift);
    }
    if (carry) t.limbs[au + limb_shift] = carry;
  }
  CCBI_SET_USED(&t, new_used); ccbi_normalize(&t); CCBI_SET_SIGN(&t, CCBI_SIGN(a));
  ccbi_swap(z, &t); ccbi_destroy(&t);
  return 0;
}

CCBI_INLINE int
ccbi_shr(ccbi_t *z, const ccbi_t *a, size_t n) CCBI_NOEXCEPT {
  uint32_t au = CCBI_USED(a);
  if (n == 0 || au == 0) return ccbi_copy(z, a);
  size_t limb_shift = n / 32, bit_shift = n % 32;
  if (limb_shift >= au) { ccbi_zero(z); CCBI_SET_SIGN(z, CCBI_SIGN(a)); return 0; }
  uint32_t new_used = au - (uint32_t)limb_shift;
  ccbi_t t; ccbi_init(&t);
  if (ccbi_grow(&t, new_used)) return -1;
  if (bit_shift == 0) {
    memcpy(t.limbs, a->limbs + limb_shift, new_used * sizeof(uint32_t));
  } else {
    for (uint32_t i = 0; i < new_used; i++) {
      uint32_t cur  = a->limbs[i + limb_shift];
      uint32_t next = (i + limb_shift + 1 < au) ? a->limbs[i + limb_shift + 1] : 0;
      t.limbs[i] = (cur >> bit_shift) | (next << (32 - bit_shift));
    }
  }
  CCBI_SET_USED(&t, new_used); ccbi_normalize(&t); CCBI_SET_SIGN(&t, CCBI_SIGN(a));
  ccbi_swap(z, &t); ccbi_destroy(&t);
  return 0;
}

/* ==========================================================================
 *  ── Bitwise operations ──
 * ========================================================================== */

/*  z = a & b  — bitwise AND of magnitudes (result ≥ 0).
 *
 *  Returns 0 on success, -1 on allocation failure.
 */
CCBI_INLINE int
ccbi_and(ccbi_t *z, const ccbi_t *a, const ccbi_t *b) CCBI_NOEXCEPT {
  uint32_t au = CCBI_USED(a), bu = CCBI_USED(b);
  uint32_t min_len = (au < bu) ? au : bu;
  if (ccbi_grow(z, min_len)) return -1;
  int a_isz = (a == z), b_isz = (b == z);
  uint32_t *zl = z->limbs;
  const uint32_t *al = a_isz ? zl : a->limbs;
  const uint32_t *bl = b_isz ? zl : b->limbs;
  for (uint32_t i = 0; i < min_len; i++) zl[i] = al[i] & bl[i];
  CCBI_SET_USED(z, min_len);
  ccbi_normalize(z);
  CCBI_SET_SIGN(z, 1);
  return 0;
}

/*  z = a | b  — bitwise OR of magnitudes (result ≥ 0).
 *
 *  Returns 0 on success, -1 on allocation failure.
 */
CCBI_INLINE int
ccbi_or(ccbi_t *z, const ccbi_t *a, const ccbi_t *b) CCBI_NOEXCEPT {
  uint32_t au = CCBI_USED(a), bu = CCBI_USED(b);
  uint32_t max_len = (au > bu) ? au : bu;
  if (ccbi_grow(z, max_len)) return -1;
  int a_isz = (a == z), b_isz = (b == z);
  uint32_t *zl = z->limbs;
  const uint32_t *al = a_isz ? zl : a->limbs;
  const uint32_t *bl = b_isz ? zl : b->limbs;
  uint32_t min_len = (au < bu) ? au : bu;
  uint32_t i;
  for (i = 0; i < min_len; i++) zl[i] = al[i] | bl[i];
  /* copy high limbs from the longer operand (x | 0 = x) */
  const uint32_t *src = (au >= bu) ? al : bl;
  uint32_t src_len = (au >= bu) ? au : bu;
  for (; i < src_len; i++) zl[i] = src[i];
  CCBI_SET_USED(z, max_len);
  CCBI_SET_SIGN(z, 1);
  return 0;
}

/*  z = a ^ b  — bitwise XOR of magnitudes (result ≥ 0).
 *
 *  Returns 0 on success, -1 on allocation failure.
 */
CCBI_INLINE int
ccbi_xor(ccbi_t *z, const ccbi_t *a, const ccbi_t *b) CCBI_NOEXCEPT {
  uint32_t au = CCBI_USED(a), bu = CCBI_USED(b);
  uint32_t max_len = (au > bu) ? au : bu;
  if (ccbi_grow(z, max_len)) return -1;
  int a_isz = (a == z), b_isz = (b == z);
  uint32_t *zl = z->limbs;
  const uint32_t *al = a_isz ? zl : a->limbs;
  const uint32_t *bl = b_isz ? zl : b->limbs;
  uint32_t min_len = (au < bu) ? au : bu;
  uint32_t i;
  for (i = 0; i < min_len; i++) zl[i] = al[i] ^ bl[i];
  /* high limbs: x ^ 0 = x */
  const uint32_t *src = (au >= bu) ? al : bl;
  uint32_t src_len = (au >= bu) ? au : bu;
  for (; i < src_len; i++) zl[i] = src[i];
  CCBI_SET_USED(z, max_len);
  ccbi_normalize(z);   /* XOR can zero out the MSB limb */
  CCBI_SET_SIGN(z, 1);
  return 0;
}

/*  z = ~a  — one's complement, within the significant bit-length of a.
 *
 *  Only bits 0..bit_length(a)-1 are flipped; all higher bits are cleared.
 *  So ~0 = 0, ~1 = 0, ~2 = 1, ~(2^k-1) = 0.  Result ≥ 0.
 *
 *  Returns 0 on success, -1 on allocation failure.
 */
CCBI_INLINE int
ccbi_not(ccbi_t *z, const ccbi_t *a) CCBI_NOEXCEPT {
  uint32_t au = CCBI_USED(a);
  if (au == 0) { ccbi_zero(z); return 0; }
  size_t bl = ccbi_bit_length(a);
  uint32_t full = (uint32_t)(bl / 32);
  uint32_t bits = (uint32_t)(bl % 32);
  uint32_t need = full + (bits ? 1 : 0);
  if (ccbi_grow(z, need)) return -1;
  int a_isz = (a == z);
  uint32_t *zl = z->limbs;
  const uint32_t *al = a_isz ? zl : a->limbs;
  uint32_t i;
  for (i = 0; i < full; i++) zl[i] = ~al[i];
  if (bits) {
    uint32_t mask = (UINT32_C(1) << bits) - 1;
    zl[full] = (~al[full]) & mask;
  }
  CCBI_SET_USED(z, need);
  ccbi_normalize(z);
  CCBI_SET_SIGN(z, 1);
  return 0;
}

/* ── Single-bit operations ── */

/*  Return 0/1 — test bit i (0-indexed, LSB first). */
CCBI_INLINE int
ccbi_test_bit(const ccbi_t *z, size_t i) CCBI_NOEXCEPT {
  size_t limb = i / 32;
  if (limb >= CCBI_USED(z)) return 0;
  return (int)((z->limbs[limb] >> (i % 32)) & UINT32_C(1));
}

/*  Set bit i to 1.  Grows the limb array if i is beyond current length. */
CCBI_INLINE int
ccbi_set_bit(ccbi_t *z, size_t i) CCBI_NOEXCEPT {
  size_t limb = i / 32;
  uint32_t bit = (uint32_t)(i % 32);
  if (limb >= CCBI_USED(z)) {
    if (ccbi_grow(z, (uint32_t)(limb + 1))) return -1;
    /* zero out intermediate limbs up to the target */
    memset(z->limbs + CCBI_USED(z), 0,
           (limb + 1 - CCBI_USED(z)) * sizeof(uint32_t));
    CCBI_SET_USED(z, (uint32_t)(limb + 1));
    CCBI_SET_SIGN(z, 1);
  }
  z->limbs[limb] |= (UINT32_C(1) << bit);
  return 0;
}

/*  Clear bit i to 0.  No-op if i is beyond current limb span. */
CCBI_INLINE int
ccbi_clear_bit(ccbi_t *z, size_t i) CCBI_NOEXCEPT {
  size_t limb = i / 32;
  if (limb >= CCBI_USED(z)) return 0;
  z->limbs[limb] &= ~(UINT32_C(1) << (i % 32));
  ccbi_normalize(z);
  return 0;
}

/*  Flip (toggle) bit i.  Grows the limb array if i is beyond current
 *  length (since 0 ^ 1 = 1). */
CCBI_INLINE int
ccbi_flip_bit(ccbi_t *z, size_t i) CCBI_NOEXCEPT {
  size_t limb = i / 32;
  uint32_t bit = (uint32_t)(i % 32);
  if (limb >= CCBI_USED(z)) {
    if (ccbi_grow(z, (uint32_t)(limb + 1))) return -1;
    memset(z->limbs + CCBI_USED(z), 0,
           (limb + 1 - CCBI_USED(z)) * sizeof(uint32_t));
    CCBI_SET_USED(z, (uint32_t)(limb + 1));
    CCBI_SET_SIGN(z, 1);
  }
  z->limbs[limb] ^= (UINT32_C(1) << bit);
  ccbi_normalize(z);
  return 0;
}

/* ==========================================================================
 *  ── Number theory ──
 * ========================================================================== */

CCBI_INLINE int
ccbi_gcd(ccbi_t *z, const ccbi_t *a, const ccbi_t *b) CCBI_NOEXCEPT {
  uint32_t au = CCBI_USED(a), bu = CCBI_USED(b);
  if (au == 0) { int r = ccbi_copy(z, b); if (r) return r; CCBI_SET_SIGN(z, 1); return 0; }
  if (bu == 0) { int r = ccbi_copy(z, a); if (r) return r; CCBI_SET_SIGN(z, 1); return 0; }
  ccbi_t u, v; ccbi_init(&u); ccbi_init(&v);
  ccbi_abs(&u, a); ccbi_abs(&v, b);
  size_t shift = 0;
  for (;;) {
    uint32_t uu = CCBI_USED(&u), vv = CCBI_USED(&v);
    if (uu == 0 || vv == 0) break;
    if ((u.limbs[0] & 1) || (v.limbs[0] & 1)) break;
    ccbi_shr(&u, &u, 1); ccbi_shr(&v, &v, 1); shift++;
  }
  for (;;) {
    uint32_t uu = CCBI_USED(&u);
    if (uu == 0) break;
    while (uu && (u.limbs[0] & 1) == 0) { ccbi_shr(&u, &u, 1); uu = CCBI_USED(&u); }
    uint32_t vv = CCBI_USED(&v);
    while (vv && (v.limbs[0] & 1) == 0) { ccbi_shr(&v, &v, 1); vv = CCBI_USED(&v); }
    if (ccbi_cmp_abs(&u, &v) >= 0) {
      ccbi_usub(&u, &u, &v); ccbi_shr(&u, &u, 1);
    } else {
      ccbi_usub(&v, &v, &u); ccbi_shr(&v, &v, 1);
    }
  }
  ccbi_copy(z, &v); ccbi_shl(z, z, shift); CCBI_SET_SIGN(z, 1);
  ccbi_destroy(&u); ccbi_destroy(&v);
  return 0;
}

/* ── Montgomery modular multiplication (CIOS) ───────────────────────── */

/** @brief Compute -m[0]^(-1) mod 2^32 via Newton iteration. */
CCBI_INLINE uint32_t
ccbi_mont_m0inv(uint32_t m0) CCBI_NOEXCEPT {
  uint32_t x = m0, inv = 1;
  inv = inv * (2 - x * inv);  /* 2^2  */
  inv = inv * (2 - x * inv);  /* 2^4  */
  inv = inv * (2 - x * inv);  /* 2^8  */
  inv = inv * (2 - x * inv);  /* 2^16 */
  inv = inv * (2 - x * inv);  /* 2^32 */
  return ~inv + 1;             /* -m0^(-1) mod 2^32 */
}

/**
 *  @brief  CIOS Montgomery multiplication and reduction.
 *          r = a * b * R^(-1) mod m, where R = 2^(32*k), k = m->used.
 *  @param  r      Output (k limbs, < m).
 *  @param  a, b   Inputs in Montgomery domain (< m).
 *  @param  m      Modulus (odd, k limbs).
 *  @param  m0inv  Precomputed -m[0]^(-1) mod 2^32.
 *  @param  k      Number of limbs in m.
 *  @return 0 on success, -1 on allocation failure.
 */
CCBI_INLINE int
ccbi_mont_mul(ccbi_t *r, const ccbi_t *a, const ccbi_t *b,
              const ccbi_t *m, uint32_t m0inv, uint32_t k) CCBI_NOEXCEPT {
  /* Pad a and b to k limbs (inputs may be smaller). */
  ccbi_t ap, bp; ccbi_init(&ap); ccbi_init(&bp);
  if (ccbi_grow(&ap, k) || ccbi_grow(&bp, k)) { ccbi_destroy(&ap); ccbi_destroy(&bp); return -1; }
  uint32_t au = CCBI_USED(a), bu = CCBI_USED(b);
  memcpy(ap.limbs, a->limbs, au * sizeof(uint32_t));
  memset(ap.limbs + au, 0, (k - au) * sizeof(uint32_t));
  memcpy(bp.limbs, b->limbs, bu * sizeof(uint32_t));
  memset(bp.limbs + bu, 0, (k - bu) * sizeof(uint32_t));
  CCBI_SET_USED(&ap, k); CCBI_SET_SIGN(&ap, 1);
  CCBI_SET_USED(&bp, k); CCBI_SET_SIGN(&bp, 1);

  /* Allocate temp buffer t[0..2k] — 2k+1 words (larger than original k+2)
   * but eliminates the O(k) memmove right-shift per outer iteration by
   * advancing an offset instead. */
  uint32_t *t = (uint32_t *)CCBI_MALLOC((2 * k + 1) * sizeof(uint32_t));
  if (!t) { ccbi_destroy(&ap); ccbi_destroy(&bp); return -1; }
  memset(t, 0, (2 * k + 1) * sizeof(uint32_t));

  uint32_t offset = 0;
  for (uint32_t i = 0; i < k; i++) {
    /* ── Step A: t[offset..] += a * b[i] ── */
    uint32_t carry = 0;
    for (uint32_t j = 0; j < k; j++) {
      uint64_t p = (uint64_t)ap.limbs[j] * bp.limbs[i]
                 + (uint64_t)t[offset + j] + (uint64_t)carry;
      t[offset + j] = (uint32_t)p;
      carry = (uint32_t)(p >> 32);
    }
    uint64_t hi = (uint64_t)t[offset + k] + carry;
    t[offset + k] = (uint32_t)hi;
    t[offset + k + 1] += (uint32_t)(hi >> 32);

    /* ── Step B: t[offset..] += q * m, q = t[offset] * m0inv ── */
    uint32_t q = t[offset] * m0inv;
    carry = 0;
    for (uint32_t j = 0; j < k; j++) {
      uint64_t p = (uint64_t)q * m->limbs[j]
                 + (uint64_t)t[offset + j] + (uint64_t)carry;
      t[offset + j] = (uint32_t)p;
      carry = (uint32_t)(p >> 32);
    }
    hi = (uint64_t)t[offset + k] + carry;
    t[offset + k] = (uint32_t)hi;
    t[offset + k + 1] += (uint32_t)(hi >> 32);

    /* t[offset] is guaranteed zero after Montgomery reduction;
     * advance offset instead of memmove-shifting the entire array. */
    offset++;
  }

  /* Result in t[offset..offset+k-1] with potential carry in t[2k].
   * The last outer iteration may overflow into t[offset+k+1] = t[2k];
   * if non-zero we must include it as an extra limb so the final
   * comparison and subtraction are correct. */
  uint32_t hi_limb = t[2 * k];
  uint32_t need = k + (hi_limb ? 1 : 0);
  if (ccbi_grow(r, need)) { CCBI_FREE(t); ccbi_destroy(&ap); ccbi_destroy(&bp); return -1; }
  memcpy(r->limbs, t + offset, k * sizeof(uint32_t));
  if (hi_limb) r->limbs[k] = hi_limb;
  CCBI_SET_USED(r, need);
  CCBI_SET_SIGN(r, 1);
  CCBI_FREE(t);
  ccbi_destroy(&ap);
  ccbi_destroy(&bp);

  /* Conditional subtraction + normalize.
   * With hi_limb included, r may hold up to k+1 limbs, but the
   * Montgomery bound guarantees result < 2m, so at most one extra
   * subtraction is needed, and hi_limb <= 2. */
  while (CCBI_USED(r) >= CCBI_USED(m) && ccbi_cmp_abs(r, m) >= 0) {
    ccbi_usub(r, r, m);
  }
  ccbi_normalize(r);
  return 0;
}

/**
 *  @brief  Convert a regular integer to Montgomery domain:  z_R = z * R mod m.
 *          Uses mont_mul(z, R^2) = z * R^2 * R^(-1) = z * R.
 */
CCBI_INLINE int
ccbi_to_mont(ccbi_t *zR, const ccbi_t *z, const ccbi_t *m,
             const ccbi_t *R2, uint32_t m0inv, uint32_t k) CCBI_NOEXCEPT {
  return ccbi_mont_mul(zR, z, R2, m, m0inv, k);
}

/** @brief Return from Montgomery domain: z = z_R * R^(-1) mod m = mont_mul(z_R, 1). */
CCBI_INLINE int
ccbi_from_mont(ccbi_t *z, const ccbi_t *zR, const ccbi_t *m,
               uint32_t m0inv, uint32_t k) CCBI_NOEXCEPT {
  ccbi_t one; ccbi_init(&one);
  ccbi_from_uint(&one, 1);
  ccbi_grow(&one, k); CCBI_SET_USED(&one, k);  /* pad to k limbs */
  memset(one.limbs + 1, 0, (k - 1) * sizeof(uint32_t));  /* zero higher limbs */
  one.limbs[0] = 1;
  int ret = ccbi_mont_mul(z, zR, &one, m, m0inv, k);
  ccbi_destroy(&one);
  ccbi_normalize(z);
  return ret;
}

/**
 *  @brief  Precompute R mod m and R^2 mod m for Montgomery multiplication.
 *  @param  R   Output: R mod m (R = 2^(32k))
 *  @param  R2  Output: R^2 mod m
 *  @param  m   Modulus (k limbs)
 *  @param  k   Limb count of m
 *  @return 0 on success, -1 on failure.
 */
CCBI_INLINE int
ccbi_mont_precompute(ccbi_t *R, ccbi_t *R2, const ccbi_t *m,
                     uint32_t k) CCBI_NOEXCEPT {
  /* R = 2^(32*k) mod m */
  ccbi_t r; ccbi_init(&r);
  if (ccbi_grow(&r, k + 1)) return -1;
  memset(r.limbs, 0, (k + 1) * sizeof(uint32_t));
  r.limbs[k] = 1; CCBI_SET_USED(&r, k + 1); CCBI_SET_SIGN(&r, 1);
  int ret = ccbi_mod(R, &r, m);
  ccbi_destroy(&r);
  if (ret) return -1;

  /* R2 = 2^(64*k) mod m */
  ccbi_t r2; ccbi_init(&r2);
  if (ccbi_grow(&r2, 2 * k + 1)) return -1;
  memset(r2.limbs, 0, (2 * k + 1) * sizeof(uint32_t));
  r2.limbs[2 * k] = 1; CCBI_SET_USED(&r2, 2 * k + 1); CCBI_SET_SIGN(&r2, 1);
  ret = ccbi_mod(R2, &r2, m);
  ccbi_destroy(&r2);
  return ret;
}

CCBI_INLINE int
ccbi_pow_mod(ccbi_t *z, const ccbi_t *base, const ccbi_t *exp,
             const ccbi_t *mod) CCBI_NOEXCEPT {
  if (CCBI_USED(mod) == 0) return -1;
  if (CCBI_USED(exp) == 0) { ccbi_one(z); return 0; }

  uint32_t k = CCBI_USED(mod);
  size_t ebits = ccbi_bit_length(exp);

  /* Small modulus: use simple binary method (avoids Montgomery setup). */
  if (k < 4) {
    ccbi_t result, b; ccbi_init(&result); ccbi_init(&b);
    ccbi_one(&result); ccbi_mod(&b, base, mod);
    for (size_t i = 0; i < ebits; i++) {
      size_t bit_pos = ebits - 1 - i;
      size_t limb_idx = bit_pos / 32, bit_idx = bit_pos % 32;
      int bit_set = (int)((exp->limbs[limb_idx] >> bit_idx) & 1);
      ccbi_mul(&result, &result, &result);
      ccbi_mod(&result, &result, mod);
      if (bit_set) { ccbi_mul(&result, &result, &b); ccbi_mod(&result, &result, mod); }
    }
    ccbi_copy(z, &result);
    ccbi_destroy(&result); ccbi_destroy(&b);
    return 0;
  }

  /* ── Montgomery + sliding window (w=4) ── */

  /* Precompute Montgomery constants. */
  uint32_t m0inv = ccbi_mont_m0inv(mod->limbs[0]);
  ccbi_t Rm, R2m; ccbi_init(&Rm); ccbi_init(&R2m);
  if (ccbi_mont_precompute(&Rm, &R2m, mod, k)) goto cleanup_mon;

  /* Convert base to Montgomery domain: bR = base * R mod m */
  ccbi_t bR, accR; ccbi_init(&bR); ccbi_init(&accR);
  if (ccbi_to_mont(&bR, base, mod, &R2m, m0inv, k)) goto cleanup_bR;

  /* accR = 1 * R mod m (Montgomery 1) */
  if (ccbi_copy(&accR, &Rm)) goto cleanup_bR;

  /* Sliding window precompute: table[i] = base^(2i+1) * R mod m for i=0..7 (w=4) */
  #define CCBI_MONT_W 4
  #define CCBI_MONT_TBL (1 << (CCBI_MONT_W - 1))  /* 8 entries */
  ccbi_t table[CCBI_MONT_TBL];
  ccbi_t bR_sq; ccbi_init(&bR_sq);
  /* bR_sq = bR^2 * R^(-1) = base^2 * R mod m */
  if (ccbi_mont_mul(&bR_sq, &bR, &bR, mod, m0inv, k)) goto cleanup_table;

  table[0] = bR;  /* base^1 * R */
  for (int i = 1; i < CCBI_MONT_TBL; i++) {
    ccbi_init(&table[i]);
    /* table[i] = table[i-1] * bR_sq = base^(2i+1) * R */
    if (ccbi_mont_mul(&table[i], &table[i-1], &bR_sq, mod, m0inv, k)) goto cleanup_table;
  }

  /* Main exponentiation loop with sliding window. */
  {
  int i = (int)ebits - 1;
  while (i >= 0) {
    /* Check bit at position i. */
    size_t limb_idx = (size_t)i / 32, bit_idx = (size_t)i % 32;
    int bit_val = (int)((exp->limbs[limb_idx] >> bit_idx) & 1);

    if (bit_val == 0) {
      /* Square once. */
      if (ccbi_mont_mul(&accR, &accR, &accR, mod, m0inv, k)) goto cleanup_table;
      i--;
    } else {
      /* Read a window of up to CCBI_MONT_W bits starting from i.
       * Pack bits MSB→LSB into window (bit 0 = rightmost bit of window),
       * so window value = natural binary value of the bit string. */
      int wlen = (i + 1 < CCBI_MONT_W) ? (i + 1) : CCBI_MONT_W;
      uint32_t window = 0;
      for (int b = 0; b < wlen; b++) {
        int pos = i - (wlen - 1 - b);  /* leftmost → rightmost */
        size_t lidx = (size_t)pos / 32, bidx = (size_t)pos % 32;
        if ((exp->limbs[lidx] >> bidx) & 1) window |= (1u << b);
      }
      /* Trim all trailing zeros: window must be odd (low bit 1). */
      while ((window & 1) == 0) { wlen--; window >>= 1; }

      /* Square wlen times. */
      for (int b = 0; b < wlen; b++) {
        if (ccbi_mont_mul(&accR, &accR, &accR, mod, m0inv, k)) goto cleanup_table;
      }

      /* Multiply by table[window >> 1]. */
      if (ccbi_mont_mul(&accR, &accR, &table[window >> 1], mod, m0inv, k)) goto cleanup_table;

      i -= wlen;
    }
  }
  }

  /* Convert back from Montgomery domain: z = accR * R^(-1) mod m. */
  if (ccbi_from_mont(z, &accR, mod, m0inv, k)) goto cleanup_table;

  /* Cleanup (reverse order: most recently allocated first). */
  for (int i = 1; i < CCBI_MONT_TBL; i++) ccbi_destroy(&table[i]);
  ccbi_destroy(&bR_sq);
  ccbi_destroy(&accR);
  ccbi_destroy(&bR);
  ccbi_destroy(&R2m);
  ccbi_destroy(&Rm);
  return 0;

cleanup_table:
  for (int i = 1; i < CCBI_MONT_TBL; i++) ccbi_destroy(&table[i]);
  ccbi_destroy(&bR_sq);
  /* fall through */
cleanup_bR:
  ccbi_destroy(&accR);
  ccbi_destroy(&bR);
  /* fall through */
cleanup_mon:
  ccbi_destroy(&R2m);
  ccbi_destroy(&Rm);
  return -1;
}

/* ==========================================================================
 *  ── Conversion to string ──
 * ========================================================================== */

/**
 *  @brief  Estimate the string length needed for ccbi_to_str.
 *  @param  z     Big integer.
 *  @param  base  Radix (2–36).
 *  @return Number of characters needed (excluding '\0'), or -1 on error.
 */
CCBI_INLINE int
ccbi_to_str_len(const ccbi_t *z, int base) CCBI_NOEXCEPT {
  if (!z || base < 2 || base > 36) return -1;
  uint32_t zu = CCBI_USED(z);
  if (zu == 0) return 1;  /* "0" */
  size_t bits = (size_t)(zu - 1) * 32 + (32 - ccbi_clz32(z->limbs[zu - 1]));
  /* ceil(bits * log(base=2)/log(target_base)) with safety margin. */
  size_t nd;
  switch (base) {
    case  2: nd = bits;                               break;
    case  8: nd = bits / 3 + 1;                       break;
    case 10: nd = (size_t)((double)bits * 0.30103) + 2; break;
    case 16: nd = bits / 4 + 1;                       break;
    default: /* fallback: bits * log2(base) ≈ bits (worst-case base=2) */
             nd = (size_t)((double)bits * 1.0) + 3;   break;
  }
  return (int)(nd + (CCBI_SIGN(z) < 0 ? 1 : 0) + 1);
}

/**
 *  @brief  Write big integer to a user-provided buffer.
 *  @param  z      Big integer.
 *  @param  buf    Destination buffer.
 *  @param  len    Buffer size (including space for '\0').
 *  @param  base   Radix (2–36).
 *  @return Number of characters written (excluding '\0'), or -1 if buffer
 *          too small or invalid input.
 */
CCBI_INLINE int
ccbi_to_str_buf(const ccbi_t *z, char *buf, size_t len, int base) CCBI_NOEXCEPT {
  if (!z || !buf || base < 2 || base > 36) return -1;
  if (CCBI_USED(z) == 0) {
    if (len < 2) return -1;
    buf[0] = '0'; buf[1] = '\0'; return 1;
  }

  int need = ccbi_to_str_len(z, base);
  if (need < 0 || (size_t)need > len) return -1;
  // size_t sign_ch = (CCBI_SIGN(z) < 0) ? 1 : 0;

  size_t pos = need - 1;
  buf[pos] = '\0';

  /* Divide repeatedly by base, collecting remainders. */
  ccbi_t tmp; ccbi_init(&tmp); ccbi_copy(&tmp, z); CCBI_SET_SIGN(&tmp, 1);
  while (CCBI_USED(&tmp) > 0) {
    uint32_t rem;
    if (ccbi_udiv_limb(&tmp, &rem, &tmp, (uint32_t)base)) {
      ccbi_destroy(&tmp); return -1;
    }
    buf[--pos] = (char)(rem < 10 ? '0' + rem : 'a' + rem - 10);
  }
  ccbi_destroy(&tmp);

  size_t written = (need - 1) - pos;
  if (CCBI_SIGN(z) < 0) { buf[0] = '-'; memmove(buf + 1, buf + pos, written + 1); }
  else if (pos > 0)     { memmove(buf, buf + pos, written + 1); }
  return (int)written;
}

/** @brief Convenience wrapper — allocates the output string. */
CCBI_INLINE char *
ccbi_to_str_alloc(const ccbi_t *z, int base) CCBI_NOEXCEPT {
  int n = ccbi_to_str_len(z, base);
  if (n < 0) return NULL;
  char *s = (char *)CCBI_MALLOC((size_t)(n + 1));
  if (!s) return NULL;
  if (ccbi_to_str_buf(z, s, (size_t)(n + 1), base) < 0) {
    CCBI_FREE(s); return NULL;
  }
  return s;
}

/** Old name kept for compatibility. */
/** Convenience wrapper — ccbi_to_str(z, base) calls alloc under the hood.
 *  For buffer-based conversion use ccbi_to_str_buf(z, buf, len, base). */
#define ccbi_to_str(z, base) ccbi_to_str_alloc(z, base)

CCBI_INLINE void ccbi_free_str(char *s) CCBI_NOEXCEPT { CCBI_FREE(s); }

CCBI_INLINE int64_t
ccbi_to_int(const ccbi_t *z) CCBI_NOEXCEPT {
  uint32_t zu = CCBI_USED(z);
  if (zu == 0) return 0;
  uint64_t v = z->limbs[0];
  if (zu >= 2) v |= (uint64_t)z->limbs[1] << 32;
  /* negation in unsigned arithmetic avoids UB on INT64_MIN;
   * the final cast is implementation-defined on two's complement
   * but yields the correct INT64_MIN on all real platforms. */
  return (int64_t)(CCBI_SIGN(z) < 0 ? -v : v);
}

CCBI_INLINE int
ccbi_print(const ccbi_t *z, int base, int newline) CCBI_NOEXCEPT {
  char *s = ccbi_to_str(z, base);
  if (!s) return -1;
  int r = 0;
  if (fputs(s, stdout) == EOF) r = -1;
  if (newline && fputc('\n', stdout) == EOF) r = -1;
  ccbi_free_str(s);
  return r;
}

#ifdef __cplusplus
}
#endif

#endif /* CCBI_H */
