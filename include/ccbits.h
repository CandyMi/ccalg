/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Cross-platform bit-manipulation primitives (popcount, clz, ctz, rotate,
**  byte swap, bit reverse, and power-of-two utilities).
**
**  Header-only, C99/C++ compatible, compiles on GCC / Clang / MSVC /
**  ICC / TinyCC / IAR / ARMCC and any other C99-capable toolchain.
**
**  Every function is guarded by an overridable `#ifndef`/`#define` macro:
**
**      // default: uses compiler builtin or portable fallback
**      #include "ccbits.h"
**
**      // override: force a custom implementation for all TU's
**      #define ccbits_popcount32(x) my_fast_popcount(x)
**      #include "ccbits.h"
**
**  ── popcount ──
**    int  ccbits_popcount8 (uint8_t  x);    // # of 1-bits in x
**    int  ccbits_popcount16(uint16_t x);
**    int  ccbits_popcount32(uint32_t x);
**    int  ccbits_popcount64(uint64_t x);
**
**  ── count leading zeros (clz) ──
**    int  ccbits_clz16(uint16_t x);         // x==0 → 16
**    int  ccbits_clz32(uint32_t x);         // x==0 → 32
**    int  ccbits_clz64(uint64_t x);         // x==0 → 64
**
**  ── count trailing zeros (ctz) ──
**    int  ccbits_ctz16(uint16_t x);         // x==0 → 16
**    int  ccbits_ctz32(uint32_t x);         // x==0 → 32
**    int  ccbits_ctz64(uint64_t x);         // x==0 → 64
**
**  ── rotate ──
**    uint32_t ccbits_rotl32(uint32_t x, unsigned int k);
**    uint32_t ccbits_rotr32(uint32_t x, unsigned int k);
**    uint64_t ccbits_rotl64(uint64_t x, unsigned int k);
**    uint64_t ccbits_rotr64(uint64_t x, unsigned int k);
**
**  ── byte swap ──
**    uint16_t ccbits_bswap16(uint16_t x);
**    uint32_t ccbits_bswap32(uint32_t x);
**    uint64_t ccbits_bswap64(uint64_t x);
**
**  ── bit reverse ──
**    uint8_t  ccbits_bitrev8 (uint8_t  x);
**    uint32_t ccbits_bitrev32(uint32_t x);
**    uint64_t ccbits_bitrev64(uint64_t x);
**
**  ── power-of-two utilities ──
**    uint32_t ccbits_ceilpow2_32(uint32_t x);  // next power-of-two ≥ x; 0→0
**    uint64_t ccbits_ceilpow2_64(uint64_t x);  // next power-of-two ≥ x; 0→0
**    #define ccbits_ispow2_32(x)                // 1 if x is a power of two
**    #define ccbits_ispow2_64(x)                // 1 if x is a power of two
*/
#ifndef CCBITS_H
#define CCBITS_H

#include <stdint.h>

#if defined(_MSC_VER)
  #include <intrin.h>
  #include <stdlib.h>   /* _rotl, _rotr, _byteswap_* */
#endif

/* ── portable inline ──────────────────────────────────────────────────── */

#if   defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
  #define CCBITS_INLINE static inline
#elif defined(_MSC_VER)
  #define CCBITS_INLINE static __inline
#elif defined(__GNUC__)
  #define CCBITS_INLINE static __inline__
#else
  #define CCBITS_INLINE static
#endif

#if defined(__cplusplus) && __cplusplus >= 201703L
  #define CCBITS_NOEXCEPT noexcept
#else
  #define CCBITS_NOEXCEPT
#endif

/* ── MSVC 64-bit target detection ────────────────────────────────────────
** __popcnt64 / _BitScanForward64 / _BitScanReverse64 require a 64-bit
** target (x64, ARM64, ARM64EC).  The 32-bit x86 variants always exist.    */

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64) || \
    defined(_M_ARM64) || defined(_M_ARM64EC))
  #define CCBITS_MSVC_64 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
**  popcount  (population count / Hamming weight)
** ═══════════════════════════════════════════════════════════════════════════ */

#ifndef ccbits_popcount8
  #if defined(__GNUC__) || defined(__clang__)
    /* __builtin_popcount on promoted unsigned int; upper bits are zero
    ** so the result is the same as for 8-bit. */
    #define ccbits_popcount8(x) ((int)__builtin_popcount((unsigned int)(x)))
  #elif defined(_MSC_VER)
    CCBITS_INLINE int ccbits_popcount8_msvc(uint8_t x) CCBITS_NOEXCEPT {
      return (int)__popcnt16((unsigned short)x);
    }
    #define ccbits_popcount8(x) ccbits_popcount8_msvc(x)
  #else
    /* 8-bit SWAR: fold upper nibble into lower nibble */
    CCBITS_INLINE int ccbits_popcount8_fb(uint8_t x) CCBITS_NOEXCEPT {
      x = (uint8_t)(x - ((x >> 1) & (uint8_t)0x55));
      x = (uint8_t)((x & (uint8_t)0x33) + ((x >> 2) & (uint8_t)0x33));
      return (int)(((uint8_t)(x + (x >> 4))) & (uint8_t)0x0F);
    }
    #define ccbits_popcount8(x) ccbits_popcount8_fb(x)
  #endif
#endif

#ifndef ccbits_popcount16
  #if defined(__GNUC__) || defined(__clang__)
    #define ccbits_popcount16(x) ((int)__builtin_popcount((unsigned int)(x)))
  #elif defined(_MSC_VER)
    CCBITS_INLINE int ccbits_popcount16_msvc(uint16_t x) CCBITS_NOEXCEPT {
      return (int)__popcnt16(x);
    }
    #define ccbits_popcount16(x) ccbits_popcount16_msvc(x)
  #else
    /* 16-bit SWAR */
    CCBITS_INLINE int ccbits_popcount16_fb(uint16_t x) CCBITS_NOEXCEPT {
      x = (uint16_t)(x - ((x >> 1) & UINT16_C(0x5555)));
      x = (uint16_t)((x & UINT16_C(0x3333)) + ((x >> 2) & UINT16_C(0x3333)));
      x = (uint16_t)((x + (x >> 4)) & UINT16_C(0x0F0F));
      return (int)((uint16_t)(x + (x >> 8))) & 0x1F;
    }
    #define ccbits_popcount16(x) ccbits_popcount16_fb(x)
  #endif
#endif

#ifndef ccbits_popcount32
  #if defined(__GNUC__) || defined(__clang__)
    #define ccbits_popcount32(x) ((int)__builtin_popcount(x))
  #elif defined(_MSC_VER)
    CCBITS_INLINE int ccbits_popcount32_msvc(uint32_t x) CCBITS_NOEXCEPT {
      return (int)__popcnt(x);
    }
    #define ccbits_popcount32(x) ccbits_popcount32_msvc(x)
  #else
    /* 32-bit SWAR (5-stage parallel adder) */
    CCBITS_INLINE int ccbits_popcount32_fb(uint32_t x) CCBITS_NOEXCEPT {
      x = x - ((x >> 1) & UINT32_C(0x55555555));
      x = (x & UINT32_C(0x33333333)) + ((x >> 2) & UINT32_C(0x33333333));
      x = (x + (x >> 4)) & UINT32_C(0x0F0F0F0F);
      x = x + (x >> 8);
      x = x + (x >> 16);
      return (int)(x & UINT32_C(0x0000003F));
    }
    #define ccbits_popcount32(x) ccbits_popcount32_fb(x)
  #endif
#endif

#ifndef ccbits_popcount64
  #if defined(__GNUC__) || defined(__clang__)
    #define ccbits_popcount64(x) ((int)__builtin_popcountll(x))
  #elif defined(_MSC_VER)
    #if defined(CCBITS_MSVC_64)
      CCBITS_INLINE int ccbits_popcount64_msvc(uint64_t x) CCBITS_NOEXCEPT {
        return (int)__popcnt64(x);
      }
      #define ccbits_popcount64(x) ccbits_popcount64_msvc(x)
    #else
      /* 32-bit MSVC: split into two 32-bit halves */
      CCBITS_INLINE int ccbits_popcount64_msvc(uint64_t x) CCBITS_NOEXCEPT {
        return (int)(__popcnt((uint32_t)x) + __popcnt((uint32_t)(x >> 32)));
      }
      #define ccbits_popcount64(x) ccbits_popcount64_msvc(x)
    #endif
  #else
    /* 64-bit SWAR */
    CCBITS_INLINE int ccbits_popcount64_fb(uint64_t x) CCBITS_NOEXCEPT {
      x = x - ((x >> 1) & UINT64_C(0x5555555555555555));
      x = (x & UINT64_C(0x3333333333333333)) + ((x >> 2) & UINT64_C(0x3333333333333333));
      x = (x + (x >> 4)) & UINT64_C(0x0F0F0F0F0F0F0F0F);
      x = x + (x >> 8);
      x = x + (x >> 16);
      x = x + (x >> 32);
      return (int)(x & UINT64_C(0x000000000000007F));
    }
    #define ccbits_popcount64(x) ccbits_popcount64_fb(x)
  #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════
**  count leading zeros (clz)
**  Returns the number of leading zero bits.  On zero input returns the
**  bit-width (16/32/64) — safe by construction, no UB on any platform.
** ═══════════════════════════════════════════════════════════════════════════ */

#ifndef ccbits_clz16
  #if defined(__GNUC__) || defined(__clang__)
    /* __builtin_clz on promoted unsigned int; zero guard via ternary */
    #define ccbits_clz16(x) ((int)((x) ? (unsigned)__builtin_clz((unsigned int)(x)) - 16U : 16U))
  #elif defined(_MSC_VER)
    CCBITS_INLINE int ccbits_clz16_msvc(uint16_t x) CCBITS_NOEXCEPT {
      unsigned long idx;
      if (_BitScanReverse(&idx, (unsigned long)x)) return 15 - (int)idx;
      return 16;
    }
    #define ccbits_clz16(x) ccbits_clz16_msvc(x)
  #else
    /* binary decomposition */
    CCBITS_INLINE int ccbits_clz16_fb(uint16_t x) CCBITS_NOEXCEPT {
      int n = 16;
      uint16_t y;
      y = x >> 8;   if (y) { n -= 8;  x = y; }
      y = x >> 4;   if (y) { n -= 4;  x = y; }
      y = x >> 2;   if (y) { n -= 2;  x = y; }
      y = x >> 1;   if (y) { n -= 1; }
      return n - (int)x;
    }
    #define ccbits_clz16(x) ccbits_clz16_fb(x)
  #endif
#endif

#ifndef ccbits_clz32
  #if defined(__GNUC__) || defined(__clang__)
    /* Ternary guards against __builtin_clz(0) which is UB in GCC/Clang */
    #define ccbits_clz32(x) ((int)((x) ? (unsigned)__builtin_clz(x) : 32U))
  #elif defined(_MSC_VER)
    CCBITS_INLINE int ccbits_clz32_msvc(uint32_t x) CCBITS_NOEXCEPT {
      unsigned long idx;
      if (_BitScanReverse(&idx, x)) return 31 - (int)idx;
      return 32;
    }
    #define ccbits_clz32(x) ccbits_clz32_msvc(x)
  #else
    CCBITS_INLINE int ccbits_clz32_fb(uint32_t x) CCBITS_NOEXCEPT {
      int n = 32;
      uint32_t y;
      y = x >> 16;  if (y) { n -= 16; x = y; }
      y = x >>  8;  if (y) { n -=  8; x = y; }
      y = x >>  4;  if (y) { n -=  4; x = y; }
      y = x >>  2;  if (y) { n -=  2; x = y; }
      y = x >>  1;  if (y) { n -=  1; }
      return n - (int)x;
    }
    #define ccbits_clz32(x) ccbits_clz32_fb(x)
  #endif
#endif

#ifndef ccbits_clz64
  #if defined(__GNUC__) || defined(__clang__)
    #define ccbits_clz64(x) ((int)((x) ? (unsigned)__builtin_clzll(x) : 64U))
  #elif defined(_MSC_VER)
    #if defined(CCBITS_MSVC_64)
      CCBITS_INLINE int ccbits_clz64_msvc(uint64_t x) CCBITS_NOEXCEPT {
        unsigned long idx;
        if (_BitScanReverse64(&idx, x)) return 63 - (int)idx;
        return 64;
      }
      #define ccbits_clz64(x) ccbits_clz64_msvc(x)
    #else
      /* 32-bit MSVC: split into hi/lo halves */
      CCBITS_INLINE int ccbits_clz64_msvc(uint64_t x) CCBITS_NOEXCEPT {
        uint32_t hi = (uint32_t)(x >> 32);
        if (hi) {
          unsigned long idx;
          _BitScanReverse(&idx, hi);
          return 31 - (int)idx;
        }
        return ccbits_clz32((uint32_t)x) + 32;
      }
      #define ccbits_clz64(x) ccbits_clz64_msvc(x)
    #endif
  #else
    CCBITS_INLINE int ccbits_clz64_fb(uint64_t x) CCBITS_NOEXCEPT {
      int n = 64;
      uint64_t y;
      y = x >> 32;  if (y) { n -= 32; x = y; }
      y = x >> 16;  if (y) { n -= 16; x = y; }
      y = x >>  8;  if (y) { n -=  8; x = y; }
      y = x >>  4;  if (y) { n -=  4; x = y; }
      y = x >>  2;  if (y) { n -=  2; x = y; }
      y = x >>  1;  if (y) { n -=  1; }
      return n - (int)x;
    }
    #define ccbits_clz64(x) ccbits_clz64_fb(x)
  #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════
**  count trailing zeros (ctz)
**  Returns the number of trailing zero bits.  On zero input returns the
**  bit-width (16/32/64) — safe by construction, no UB on any platform.
** ═══════════════════════════════════════════════════════════════════════════ */

#ifndef ccbits_ctz16
  #if defined(__GNUC__) || defined(__clang__)
    #define ccbits_ctz16(x) ((int)((x) ? (unsigned)__builtin_ctz((unsigned int)(x)) : 16U))
  #elif defined(_MSC_VER)
    CCBITS_INLINE int ccbits_ctz16_msvc(uint16_t x) CCBITS_NOEXCEPT {
      unsigned long idx;
      if (_BitScanForward(&idx, (unsigned long)x)) return (int)idx;
      return 16;
    }
    #define ccbits_ctz16(x) ccbits_ctz16_msvc(x)
  #else
    /* binary decomposition: test each nibble/byte — all shift+test, no division */
    CCBITS_INLINE int ccbits_ctz16_fb(uint16_t x) CCBITS_NOEXCEPT {
      if (!x) return 16;
      int n = 0;
      if (!(x & 0x00FF)) { n += 8; x >>= 8; }
      if (!(x & 0x000F)) { n += 4; x >>= 4; }
      if (!(x & 0x0003)) { n += 2; x >>= 2; }
      if (!(x & 0x0001)) { n += 1; }
      return n;
    }
    #define ccbits_ctz16(x) ccbits_ctz16_fb(x)
  #endif
#endif

#ifndef ccbits_ctz32
  #if defined(__GNUC__) || defined(__clang__)
    #define ccbits_ctz32(x) ((int)((x) ? (unsigned)__builtin_ctz(x) : 32U))
  #elif defined(_MSC_VER)
    CCBITS_INLINE int ccbits_ctz32_msvc(uint32_t x) CCBITS_NOEXCEPT {
      unsigned long idx;
      if (_BitScanForward(&idx, x)) return (int)idx;
      return 32;
    }
    #define ccbits_ctz32(x) ccbits_ctz32_msvc(x)
  #else
    /* de Bruijn multiplication — 32-entry table, branch-free for x≠0 */
    static const uint8_t ccbits_ctz32_tab[32] = {
      0,  1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17,  4, 8,
      31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18,  6, 11,  5, 10, 9
    };
    CCBITS_INLINE int ccbits_ctz32_fb(uint32_t x) CCBITS_NOEXCEPT {
      if (!x) return 32;
      return (int)ccbits_ctz32_tab[(uint32_t)((x & (uint32_t)-(int32_t)x) * UINT32_C(0x077CB531)) >> 27];
    }
    #define ccbits_ctz32(x) ccbits_ctz32_fb(x)
  #endif
#endif

#ifndef ccbits_ctz64
  #if defined(__GNUC__) || defined(__clang__)
    #define ccbits_ctz64(x) ((int)((x) ? (unsigned)__builtin_ctzll(x) : 64U))
  #elif defined(_MSC_VER)
    #if defined(CCBITS_MSVC_64)
      CCBITS_INLINE int ccbits_ctz64_msvc(uint64_t x) CCBITS_NOEXCEPT {
        unsigned long idx;
        if (_BitScanForward64(&idx, x)) return (int)idx;
        return 64;
      }
      #define ccbits_ctz64(x) ccbits_ctz64_msvc(x)
    #else
      /* 32-bit MSVC: split into lo/hi halves */
      CCBITS_INLINE int ccbits_ctz64_msvc(uint64_t x) CCBITS_NOEXCEPT {
        uint32_t lo = (uint32_t)x;
        if (lo) {
          unsigned long idx;
          _BitScanForward(&idx, lo);
          return (int)idx;
        }
        return ccbits_ctz32((uint32_t)(x >> 32)) + 32;
      }
      #define ccbits_ctz64(x) ccbits_ctz64_msvc(x)
    #endif
  #else
    /* de Bruijn multiplication — 64-entry table */
    static const uint8_t ccbits_ctz64_tab[64] = {
      0,  1,  2, 53,  3,  7, 54, 27, 4,  38,  8, 34, 55, 48, 28, 14,
      5,  9, 39, 35, 56, 49, 15, 40, 57, 50, 41, 58, 42, 59, 60, 61,
      62, 10, 43, 44, 63, 11, 45, 12, 46, 13, 47, 32, 16, 17, 18, 19,
      20, 21, 22, 23, 24, 25, 26, 52, 29, 30, 31, 33, 36, 37, 51,  6
    };
    CCBITS_INLINE int ccbits_ctz64_fb(uint64_t x) CCBITS_NOEXCEPT {
      if (!x) return 64;
      return (int)ccbits_ctz64_tab[(uint64_t)((x & (uint64_t)-(int64_t)x) * UINT64_C(0x3F08A4C2ACB9DB4D)) >> 58];
    }
    #define ccbits_ctz64(x) ccbits_ctz64_fb(x)
  #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════
**  rotate left / rotate right
**  k is masked to the relevant width (< 32 / < 64) before the shift,
**  eliminating any chance of undefined behavior even if the caller passes
**  a large k.  All compilers recognize the idiom and emit a single rol/ror.
** ═══════════════════════════════════════════════════════════════════════════ */

#ifndef ccbits_rotl32
  /* Clang provides __builtin_rotateleft32 as a recognized builtin that
   * compiles to a single rotate instruction at any optimisation level.
   * GCC recognises the (x << k) | (x >> (32-k)) pattern instead, and
   * emits the same instruction — but __builtin_rotateleft32 emits an
   * unresolved libcall at -O0 on older GCC, so use the inline form. */
  #if defined(__clang__) && defined(__has_builtin)
    #if __has_builtin(__builtin_rotateleft32)
      #define ccbits_rotl32(x, k) __builtin_rotateleft32((x), (k))
    #endif
  #endif
  #ifndef ccbits_rotl32
    #if defined(_MSC_VER)
      #define ccbits_rotl32(x, k) _rotl((x), (k))
    #else
      CCBITS_INLINE uint32_t ccbits_rotl32_impl(uint32_t x, unsigned int k) CCBITS_NOEXCEPT {
        k &= 31U;
        return (x << k) | (x >> (32U - k));
      }
      #define ccbits_rotl32(x, k) ccbits_rotl32_impl((x), (k))
    #endif
  #endif
#endif

#ifndef ccbits_rotr32
  #if defined(__clang__) && defined(__has_builtin)
    #if __has_builtin(__builtin_rotateright32)
      #define ccbits_rotr32(x, k) __builtin_rotateright32((x), (k))
    #endif
  #endif
  #ifndef ccbits_rotr32
    #if defined(_MSC_VER)
      #define ccbits_rotr32(x, k) _rotr((x), (k))
    #else
      CCBITS_INLINE uint32_t ccbits_rotr32_impl(uint32_t x, unsigned int k) CCBITS_NOEXCEPT {
        k &= 31U;
        return (x >> k) | (x << (32U - k));
      }
      #define ccbits_rotr32(x, k) ccbits_rotr32_impl((x), (k))
    #endif
  #endif
#endif

#ifndef ccbits_rotl64
  #if defined(__clang__) && defined(__has_builtin)
    #if __has_builtin(__builtin_rotateleft64)
      #define ccbits_rotl64(x, k) __builtin_rotateleft64((x), (k))
    #endif
  #endif
  #ifndef ccbits_rotl64
    #if defined(_MSC_VER)
      #define ccbits_rotl64(x, k) _rotl64((x), (k))
    #else
      CCBITS_INLINE uint64_t ccbits_rotl64_impl(uint64_t x, unsigned int k) CCBITS_NOEXCEPT {
        k &= 63U;
        return (x << k) | (x >> (64U - k));
      }
      #define ccbits_rotl64(x, k) ccbits_rotl64_impl((x), (k))
    #endif
  #endif
#endif

#ifndef ccbits_rotr64
  #if defined(__clang__) && defined(__has_builtin)
    #if __has_builtin(__builtin_rotateright64)
      #define ccbits_rotr64(x, k) __builtin_rotateright64((x), (k))
    #endif
  #endif
  #ifndef ccbits_rotr64
    #if defined(_MSC_VER)
      #define ccbits_rotr64(x, k) _rotr64((x), (k))
    #else
      CCBITS_INLINE uint64_t ccbits_rotr64_impl(uint64_t x, unsigned int k) CCBITS_NOEXCEPT {
        k &= 63U;
        return (x >> k) | (x << (64U - k));
      }
      #define ccbits_rotr64(x, k) ccbits_rotr64_impl((x), (k))
    #endif
  #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════
**  byte swap (endian reversal)
**  Unconditionally reverses byte order — correct on both little-endian
**  and big-endian platforms.
** ═══════════════════════════════════════════════════════════════════════════ */

#ifndef ccbits_bswap16
  #if defined(__GNUC__) || defined(__clang__)
    #define ccbits_bswap16(x) ((uint16_t)__builtin_bswap16(x))
  #elif defined(_MSC_VER)
    #define ccbits_bswap16(x) _byteswap_ushort((unsigned short)(x))
  #else
    CCBITS_INLINE uint16_t ccbits_bswap16_fb(uint16_t x) CCBITS_NOEXCEPT {
      return (uint16_t)((x >> 8) | (x << 8));
    }
    #define ccbits_bswap16(x) ccbits_bswap16_fb(x)
  #endif
#endif

#ifndef ccbits_bswap32
  #if defined(__GNUC__) || defined(__clang__)
    #define ccbits_bswap32(x) __builtin_bswap32(x)
  #elif defined(_MSC_VER)
    #define ccbits_bswap32(x) _byteswap_ulong(x)
  #else
    CCBITS_INLINE uint32_t ccbits_bswap32_fb(uint32_t x) CCBITS_NOEXCEPT {
      x = ((x >>  8) & UINT32_C(0x00FF00FF)) | ((x & UINT32_C(0x00FF00FF)) <<  8);
      x =  (x >> 16)                          | (x                          << 16);
      return x;
    }
    #define ccbits_bswap32(x) ccbits_bswap32_fb(x)
  #endif
#endif

#ifndef ccbits_bswap64
  #if defined(__GNUC__) || defined(__clang__)
    #define ccbits_bswap64(x) __builtin_bswap64(x)
  #elif defined(_MSC_VER)
    #define ccbits_bswap64(x) _byteswap_uint64(x)
  #else
    CCBITS_INLINE uint64_t ccbits_bswap64_fb(uint64_t x) CCBITS_NOEXCEPT {
      x = ((x >>  8) & UINT64_C(0x00FF00FF00FF00FF)) | ((x & UINT64_C(0x00FF00FF00FF00FF)) <<  8);
      x = ((x >> 16) & UINT64_C(0x0000FFFF0000FFFF)) | ((x & UINT64_C(0x0000FFFF0000FFFF)) << 16);
      x =  (x >> 32)                                 | (x                                   << 32);
      return x;
    }
    #define ccbits_bswap64(x) ccbits_bswap64_fb(x)
  #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════
**  bit reverse
**  Reverses the bit order (MSB ↔ LSB).  Clang provides
**  __builtin_bitreverse{8,32,64} as a single instruction on ARM; GCC
**  and unknown compilers use a branch-free binary delta-swap approach.
** ═══════════════════════════════════════════════════════════════════════════ */

#ifndef ccbits_bitrev8
  #if defined(__clang__)
    #define ccbits_bitrev8(x)  __builtin_bitreverse8(x)
  #elif defined(__GNUC__)
    /* binary delta-swap (8-bit) */
    CCBITS_INLINE uint8_t ccbits_bitrev8_impl(uint8_t x) CCBITS_NOEXCEPT {
      x = (uint8_t)(((x >> 1) & (uint8_t)0x55) | ((x & (uint8_t)0x55) << 1));
      x = (uint8_t)(((x >> 2) & (uint8_t)0x33) | ((x & (uint8_t)0x33) << 2));
      x = (uint8_t)((x >> 4) | (x << 4));
      return x;
    }
    #define ccbits_bitrev8(x)  ccbits_bitrev8_impl(x)
  #elif defined(_MSC_VER)
    /* MSVC: use a small nibble table */
    static const uint8_t ccbits_bitrev8_tab[16] = {
      UINT8_C(0x0), UINT8_C(0x8), UINT8_C(0x4), UINT8_C(0xC),
      UINT8_C(0x2), UINT8_C(0xA), UINT8_C(0x6), UINT8_C(0xE),
      UINT8_C(0x1), UINT8_C(0x9), UINT8_C(0x5), UINT8_C(0xD),
      UINT8_C(0x3), UINT8_C(0xB), UINT8_C(0x7), UINT8_C(0xF)
    };
    CCBITS_INLINE uint8_t ccbits_bitrev8_msvc(uint8_t x) CCBITS_NOEXCEPT {
      return (uint8_t)((ccbits_bitrev8_tab[x & 0x0F] << 4) | ccbits_bitrev8_tab[x >> 4]);
    }
    #define ccbits_bitrev8(x)  ccbits_bitrev8_msvc(x)
  #else
    CCBITS_INLINE uint8_t ccbits_bitrev8_fb(uint8_t x) CCBITS_NOEXCEPT {
      x = (uint8_t)(((x >> 1) & (uint8_t)0x55) | ((x & (uint8_t)0x55) << 1));
      x = (uint8_t)(((x >> 2) & (uint8_t)0x33) | ((x & (uint8_t)0x33) << 2));
      x = (uint8_t)((x >> 4) | (x << 4));
      return x;
    }
    #define ccbits_bitrev8(x)  ccbits_bitrev8_fb(x)
  #endif
#endif

#ifndef ccbits_bitrev32
  #if defined(__clang__)
    #define ccbits_bitrev32(x) __builtin_bitreverse32(x)
  #elif defined(__GNUC__)
    CCBITS_INLINE uint32_t ccbits_bitrev32_impl(uint32_t x) CCBITS_NOEXCEPT {
      x = ((x >>  1) & UINT32_C(0x55555555)) | ((x & UINT32_C(0x55555555)) <<  1);
      x = ((x >>  2) & UINT32_C(0x33333333)) | ((x & UINT32_C(0x33333333)) <<  2);
      x = ((x >>  4) & UINT32_C(0x0F0F0F0F)) | ((x & UINT32_C(0x0F0F0F0F)) <<  4);
      x = ((x >>  8) & UINT32_C(0x00FF00FF)) | ((x & UINT32_C(0x00FF00FF)) <<  8);
      x =  (x >> 16)                          | (x                          << 16);
      return x;
    }
    #define ccbits_bitrev32(x) ccbits_bitrev32_impl(x)
  #elif defined(_MSC_VER)
    CCBITS_INLINE uint32_t ccbits_bitrev32_msvc(uint32_t x) CCBITS_NOEXCEPT {
      x = ((x >>  1) & UINT32_C(0x55555555)) | ((x & UINT32_C(0x55555555)) <<  1);
      x = ((x >>  2) & UINT32_C(0x33333333)) | ((x & UINT32_C(0x33333333)) <<  2);
      x = ((x >>  4) & UINT32_C(0x0F0F0F0F)) | ((x & UINT32_C(0x0F0F0F0F)) <<  4);
      x = ((x >>  8) & UINT32_C(0x00FF00FF)) | ((x & UINT32_C(0x00FF00FF)) <<  8);
      x =  (x >> 16)                          | (x                          << 16);
      return x;
    }
    #define ccbits_bitrev32(x) ccbits_bitrev32_msvc(x)
  #else
    CCBITS_INLINE uint32_t ccbits_bitrev32_fb(uint32_t x) CCBITS_NOEXCEPT {
      x = ((x >>  1) & UINT32_C(0x55555555)) | ((x & UINT32_C(0x55555555)) <<  1);
      x = ((x >>  2) & UINT32_C(0x33333333)) | ((x & UINT32_C(0x33333333)) <<  2);
      x = ((x >>  4) & UINT32_C(0x0F0F0F0F)) | ((x & UINT32_C(0x0F0F0F0F)) <<  4);
      x = ((x >>  8) & UINT32_C(0x00FF00FF)) | ((x & UINT32_C(0x00FF00FF)) <<  8);
      x =  (x >> 16)                          | (x                          << 16);
      return x;
    }
    #define ccbits_bitrev32(x) ccbits_bitrev32_fb(x)
  #endif
#endif

#ifndef ccbits_bitrev64
  #if defined(__clang__)
    #define ccbits_bitrev64(x) __builtin_bitreverse64(x)
  #elif defined(__GNUC__)
    CCBITS_INLINE uint64_t ccbits_bitrev64_impl(uint64_t x) CCBITS_NOEXCEPT {
      x = ((x >>  1) & UINT64_C(0x5555555555555555)) | ((x & UINT64_C(0x5555555555555555)) <<  1);
      x = ((x >>  2) & UINT64_C(0x3333333333333333)) | ((x & UINT64_C(0x3333333333333333)) <<  2);
      x = ((x >>  4) & UINT64_C(0x0F0F0F0F0F0F0F0F)) | ((x & UINT64_C(0x0F0F0F0F0F0F0F0F)) <<  4);
      x = ((x >>  8) & UINT64_C(0x00FF00FF00FF00FF)) | ((x & UINT64_C(0x00FF00FF00FF00FF)) <<  8);
      x = ((x >> 16) & UINT64_C(0x0000FFFF0000FFFF)) | ((x & UINT64_C(0x0000FFFF0000FFFF)) << 16);
      x =  (x >> 32)                                 | (x                                   << 32);
      return x;
    }
    #define ccbits_bitrev64(x) ccbits_bitrev64_impl(x)
  #elif defined(_MSC_VER)
    CCBITS_INLINE uint64_t ccbits_bitrev64_msvc(uint64_t x) CCBITS_NOEXCEPT {
      x = ((x >>  1) & UINT64_C(0x5555555555555555)) | ((x & UINT64_C(0x5555555555555555)) <<  1);
      x = ((x >>  2) & UINT64_C(0x3333333333333333)) | ((x & UINT64_C(0x3333333333333333)) <<  2);
      x = ((x >>  4) & UINT64_C(0x0F0F0F0F0F0F0F0F)) | ((x & UINT64_C(0x0F0F0F0F0F0F0F0F)) <<  4);
      x = ((x >>  8) & UINT64_C(0x00FF00FF00FF00FF)) | ((x & UINT64_C(0x00FF00FF00FF00FF)) <<  8);
      x = ((x >> 16) & UINT64_C(0x0000FFFF0000FFFF)) | ((x & UINT64_C(0x0000FFFF0000FFFF)) << 16);
      x =  (x >> 32)                                 | (x                                   << 32);
      return x;
    }
    #define ccbits_bitrev64(x) ccbits_bitrev64_msvc(x)
  #else
    CCBITS_INLINE uint64_t ccbits_bitrev64_fb(uint64_t x) CCBITS_NOEXCEPT {
      x = ((x >>  1) & UINT64_C(0x5555555555555555)) | ((x & UINT64_C(0x5555555555555555)) <<  1);
      x = ((x >>  2) & UINT64_C(0x3333333333333333)) | ((x & UINT64_C(0x3333333333333333)) <<  2);
      x = ((x >>  4) & UINT64_C(0x0F0F0F0F0F0F0F0F)) | ((x & UINT64_C(0x0F0F0F0F0F0F0F0F)) <<  4);
      x = ((x >>  8) & UINT64_C(0x00FF00FF00FF00FF)) | ((x & UINT64_C(0x00FF00FF00FF00FF)) <<  8);
      x = ((x >> 16) & UINT64_C(0x0000FFFF0000FFFF)) | ((x & UINT64_C(0x0000FFFF0000FFFF)) << 16);
      x =  (x >> 32)                                 | (x                                   << 32);
      return x;
    }
    #define ccbits_bitrev64(x) ccbits_bitrev64_fb(x)
  #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════
**  power-of-two utilities
**  ceilpow2:  branchless bit-smear + increment.
**    0 → 0  (safe, no overflow)
**    >2^(N-1) → 0  (overflow wraps around, same as 0 input)
**  ispow2:   simple macro — x!=0 && (x & x-1)==0
** ═══════════════════════════════════════════════════════════════════════════ */

#ifndef ccbits_ceilpow2_32
  CCBITS_INLINE uint32_t ccbits_ceilpow2_32_impl(uint32_t x) CCBITS_NOEXCEPT {
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
  }
  #define ccbits_ceilpow2_32(x) ccbits_ceilpow2_32_impl(x)
#endif

#ifndef ccbits_ceilpow2_64
  CCBITS_INLINE uint64_t ccbits_ceilpow2_64_impl(uint64_t x) CCBITS_NOEXCEPT {
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    return x + 1;
  }
  #define ccbits_ceilpow2_64(x) ccbits_ceilpow2_64_impl(x)
#endif

/* ispow2 — simple macros; not overridable since they're already minimal */
#define ccbits_ispow2_32(x)  (((x) > 0U) & (((x) & ((x) - 1U)) == 0))
#define ccbits_ispow2_64(x)  (((x) > 0ULL) & (((x) & ((x) - 1ULL)) == 0ULL))

/* ═══════════════════════════════════════════════════════════════════════════
**  bit_width — minimum bit-width needed to represent x
**    bit_width(0)=0, bit_width(1)=1, bit_width(8)=4, bit_width(0x80000000)=32
**    Composes with clz — no separate compiler intrinsic needed.
** ═══════════════════════════════════════════════════════════════════════════ */

#ifndef ccbits_bit_width8
  /* uint8_t → promoted to uint32_t → clz32 counts all 32 bits;
  ** 32 - clz32 already gives the 8-bit bit_width because the upper
  ** 24 bits are zeroed by the cast. */
  #define ccbits_bit_width8(x)  ((int)((x) ? (int)(32U - (unsigned)ccbits_clz32((uint8_t)(x))) : 0))
#endif

#ifndef ccbits_bit_width16
  #define ccbits_bit_width16(x) ((int)((x) ? (int)(32U - (unsigned)ccbits_clz32((uint16_t)(x))) : 0))
#endif

#ifndef ccbits_bit_width32
  CCBITS_INLINE int ccbits_bit_width32_impl(uint32_t x) CCBITS_NOEXCEPT {
    if (!x) return 0;
    return 32 - ccbits_clz32(x);
  }
  #define ccbits_bit_width32(x) ccbits_bit_width32_impl(x)
#endif

#ifndef ccbits_bit_width64
  CCBITS_INLINE int ccbits_bit_width64_impl(uint64_t x) CCBITS_NOEXCEPT {
    if (!x) return 0;
    return 64 - ccbits_clz64(x);
  }
  #define ccbits_bit_width64(x) ccbits_bit_width64_impl(x)
#endif

/* ═══════════════════════════════════════════════════════════════════════════
**  mask_low — generate N low bits set to 1
**    mask_low(0)=0, mask_low(5)=0x1F, mask_low(32)=0xFFFFFFFF
**    n ≥ width returns all-ones (clamped, no UB from shift).
** ═══════════════════════════════════════════════════════════════════════════ */

#ifndef ccbits_mask_low32
  CCBITS_INLINE uint32_t ccbits_mask_low32_impl(unsigned int n) CCBITS_NOEXCEPT {
    return (n >= 32) ? UINT32_MAX : ((1U << n) - 1U);
  }
  #define ccbits_mask_low32(n) ccbits_mask_low32_impl(n)
#endif

#ifndef ccbits_mask_low64
  CCBITS_INLINE uint64_t ccbits_mask_low64_impl(unsigned int n) CCBITS_NOEXCEPT {
    return (n >= 64) ? UINT64_MAX : ((1ULL << n) - 1ULL);
  }
  #define ccbits_mask_low64(n) ccbits_mask_low64_impl(n)
#endif

/* ═══════════════════════════════════════════════════════════════════════════
**  parity — odd parity flag (popcount modulo 2)
**    parity32(0x11)=0 (even: 2 bits), parity32(0x10)=1 (odd: 1 bit)
**    Composes with popcount — same POPCNT instruction on x86/ARM.
** ═══════════════════════════════════════════════════════════════════════════ */

#ifndef ccbits_parity8
  #define ccbits_parity8(x)  (ccbits_popcount8(x) & 1)
#endif

#ifndef ccbits_parity16
  #define ccbits_parity16(x) (ccbits_popcount16(x) & 1)
#endif

#ifndef ccbits_parity32
  #define ccbits_parity32(x) (ccbits_popcount32(x) & 1)
#endif

#ifndef ccbits_parity64
  #define ccbits_parity64(x) (ccbits_popcount64(x) & 1)
#endif

/* ═══════════════════════════════════════════════════════════════════════════
**  sign_ext — sign-extend the lowest N bits to full width
**    sign_ext32(0x1F,5) = -1, sign_ext32(0x0F,5)=15, sign_ext32(0x10,5)=-16
**    Branchless XOR-sub trick.  n=0 or n≥width → return x as-is.
** ═══════════════════════════════════════════════════════════════════════════ */

#ifndef ccbits_sign_ext32
  CCBITS_INLINE int32_t ccbits_sign_ext32_impl(uint32_t x, unsigned int n) CCBITS_NOEXCEPT {
    if (!n || n >= 32) return (int32_t)x;
    uint32_t m = 1U << (n - 1U);
    return (int32_t)((x ^ m) - m);
  }
  #define ccbits_sign_ext32(x, n) ccbits_sign_ext32_impl((x), (n))
#endif

#ifndef ccbits_sign_ext64
  CCBITS_INLINE int64_t ccbits_sign_ext64_impl(uint64_t x, unsigned int n) CCBITS_NOEXCEPT {
    if (!n || n >= 64) return (int64_t)x;
    uint64_t m = 1ULL << (n - 1U);
    return (int64_t)((x ^ m) - m);
  }
  #define ccbits_sign_ext64(x, n) ccbits_sign_ext64_impl((x), (n))
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#undef CCBITS_MSVC_64

#endif /* CCBITS_H */
