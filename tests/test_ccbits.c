/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Test suite for ccbits.h — bit-manipulation primitives
**  Build:  gcc -std=c99 -Wall -Wextra -o test_ccbits test_ccbits.c && ./test_ccbits
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "../include/ccbits.h"

/* ── test harness ─────────────────────────────────────────────────────── */
static int passed, failed;
#define TEST(name) static void test_##name(void)
#define ASSERT(cond) do { \
  if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failed++; } \
  else passed++; \
} while(0)
#define RUN(name) do { printf("  %s\n", #name); test_##name(); } while(0)

/* ═══════════════════════════════════════════════════════════════════════
**  popcount tests
** ═══════════════════════════════════════════════════════════════════════ */

TEST(popcount8) {
  ASSERT(ccbits_popcount8(0)     == 0);
  ASSERT(ccbits_popcount8(1)     == 1);
  ASSERT(ccbits_popcount8(0x80)  == 1);
  ASSERT(ccbits_popcount8(0x0F)  == 4);
  ASSERT(ccbits_popcount8(0xF0)  == 4);
  ASSERT(ccbits_popcount8(0xFF)  == 8);
  ASSERT(ccbits_popcount8(0x55)  == 4);
  ASSERT(ccbits_popcount8(0xAA)  == 4);
  ASSERT(ccbits_popcount8(0x81)  == 2);
  ASSERT(ccbits_popcount8(0x7F)  == 7);
}

TEST(popcount16) {
  ASSERT(ccbits_popcount16(0)       == 0);
  ASSERT(ccbits_popcount16(1)       == 1);
  ASSERT(ccbits_popcount16(0x8000)  == 1);
  ASSERT(ccbits_popcount16(0xFFFF)  == 16);
  ASSERT(ccbits_popcount16(0x00FF)  == 8);
  ASSERT(ccbits_popcount16(0xFF00)  == 8);
  ASSERT(ccbits_popcount16(0xAAAA)  == 8);
  ASSERT(ccbits_popcount16(0x5555)  == 8);
  ASSERT(ccbits_popcount16(0x1234)  == 5);  /* 0001 0010 0011 0100 */
}

TEST(popcount32) {
  ASSERT(ccbits_popcount32(0)            == 0);
  ASSERT(ccbits_popcount32(1U)           == 1);
  ASSERT(ccbits_popcount32(0x80000000U)  == 1);
  ASSERT(ccbits_popcount32(0xFFFFFFFFU)  == 32);
  ASSERT(ccbits_popcount32(0x55555555U)  == 16);
  ASSERT(ccbits_popcount32(0xAAAAAAAAU)  == 16);
  ASSERT(ccbits_popcount32(0x12345678U)  == 13);
  ASSERT(ccbits_popcount32(0x0F0F0F0FU)  == 16);
  ASSERT(ccbits_popcount32(0x00FF00FFU)  == 16);
  ASSERT(ccbits_popcount32(0x00000000U)  == 0);
}

TEST(popcount64) {
  ASSERT(ccbits_popcount64(0ULL)                    == 0);
  ASSERT(ccbits_popcount64(1ULL)                    == 1);
  ASSERT(ccbits_popcount64(0x8000000000000000ULL)   == 1);
  ASSERT(ccbits_popcount64(0xFFFFFFFFFFFFFFFFULL)   == 64);
  ASSERT(ccbits_popcount64(0x5555555555555555ULL)   == 32);
  ASSERT(ccbits_popcount64(0xAAAAAAAAAAAAAAAAULL)   == 32);
  ASSERT(ccbits_popcount64(0x123456789ABCDEF0ULL)   == 32);
  ASSERT(ccbits_popcount64(0x00000000FFFFFFFFULL)   == 32);
  ASSERT(ccbits_popcount64(0xFFFFFFFF00000000ULL)   == 32);
  ASSERT(ccbits_popcount64(0x0101010101010101ULL)   == 8);
}

/* ═══════════════════════════════════════════════════════════════════════
**  CLZ tests
** ═══════════════════════════════════════════════════════════════════════ */

TEST(clz16) {
  ASSERT(ccbits_clz16(0)       == 16);
  ASSERT(ccbits_clz16(1)       == 15);
  ASSERT(ccbits_clz16(0x8000)  == 0);
  ASSERT(ccbits_clz16(0x7FFF)  == 1);
  ASSERT(ccbits_clz16(0x00FF)  == 8);
  ASSERT(ccbits_clz16(0x0010)  == 11);
  ASSERT(ccbits_clz16(0x0002)  == 14);
  ASSERT(ccbits_clz16(0x4000)  == 1);
  ASSERT(ccbits_clz16(0x0008)  == 12);
}

TEST(clz32) {
  ASSERT(ccbits_clz32(0)            == 32);
  ASSERT(ccbits_clz32(1U)           == 31);
  ASSERT(ccbits_clz32(0x80000000U)  == 0);
  ASSERT(ccbits_clz32(0xFFFFFFFFU)  == 0);
  ASSERT(ccbits_clz32(0x7FFFFFFFU)  == 1);
  ASSERT(ccbits_clz32(0x00FFFFFFU)  == 8);
  ASSERT(ccbits_clz32(0x000FFFFFU)  == 12);
  ASSERT(ccbits_clz32(0x00000100U)  == 23);
  ASSERT(ccbits_clz32(0x00000002U)  == 30);
  ASSERT(ccbits_clz32(0x00010000U)  == 15);
}

TEST(clz64) {
  ASSERT(ccbits_clz64(0ULL)                        == 64);
  ASSERT(ccbits_clz64(1ULL)                        == 63);
  ASSERT(ccbits_clz64(0x8000000000000000ULL)       == 0);
  ASSERT(ccbits_clz64(0xFFFFFFFFFFFFFFFFULL)       == 0);
  ASSERT(ccbits_clz64(0x7FFFFFFFFFFFFFFFULL)       == 1);
  ASSERT(ccbits_clz64(0x00FFFFFFFFFFFFFFULL)       == 8);
  ASSERT(ccbits_clz64(0x0000FFFFFFFFFFFFULL)       == 16);
  ASSERT(ccbits_clz64(0x00000000FFFFFFFFULL)       == 32);
  ASSERT(ccbits_clz64(0x000000000000FFFFULL)       == 48);
  ASSERT(ccbits_clz64(0x0000000000000080ULL)       == 56);
}

/* ═══════════════════════════════════════════════════════════════════════
**  CTZ tests
** ═══════════════════════════════════════════════════════════════════════ */

TEST(ctz16) {
  ASSERT(ccbits_ctz16(0)       == 16);
  ASSERT(ccbits_ctz16(1)       == 0);
  ASSERT(ccbits_ctz16(0x8000)  == 15);
  ASSERT(ccbits_ctz16(0x0002)  == 1);
  ASSERT(ccbits_ctz16(0x0004)  == 2);
  ASSERT(ccbits_ctz16(0x0008)  == 3);
  ASSERT(ccbits_ctz16(0x00F0)  == 4);
  ASSERT(ccbits_ctz16(0x0100)  == 8);
}

TEST(ctz32) {
  ASSERT(ccbits_ctz32(0)            == 32);
  ASSERT(ccbits_ctz32(1U)           == 0);
  ASSERT(ccbits_ctz32(0x80000000U)  == 31);
  ASSERT(ccbits_ctz32(0x00000002U)  == 1);
  ASSERT(ccbits_ctz32(0x00000004U)  == 2);
  ASSERT(ccbits_ctz32(0x00000008U)  == 3);
  ASSERT(ccbits_ctz32(0x00000010U)  == 4);
  ASSERT(ccbits_ctz32(0x10000000U)  == 28);
  ASSERT(ccbits_ctz32(0x80000000U)  == 31);
  ASSERT(ccbits_ctz32(0x7FFFFFFFU)  == 0);  /* LSB is 1 */
}

TEST(ctz64) {
  ASSERT(ccbits_ctz64(0ULL)                        == 64);
  ASSERT(ccbits_ctz64(1ULL)                        == 0);
  ASSERT(ccbits_ctz64(0x8000000000000000ULL)       == 63);
  ASSERT(ccbits_ctz64(0x0000000000000002ULL)       == 1);
  ASSERT(ccbits_ctz64(0x0000000000000004ULL)       == 2);
  ASSERT(ccbits_ctz64(0x0000000100000000ULL)       == 32);
  ASSERT(ccbits_ctz64(0x8000000000000000ULL)       == 63);
  ASSERT(ccbits_ctz64(0x7FFFFFFFFFFFFFFFULL)       == 0);  /* LSB is 1 */
  ASSERT(ccbits_ctz64(0xAAAAAAAAAAAAAAAAULL)       == 1);  /* LSB is 0 */
}

/* ═══════════════════════════════════════════════════════════════════════
**  Rotate tests
** ═══════════════════════════════════════════════════════════════════════ */

TEST(rotl32) {
  uint32_t v = 0x80000001U;
  ASSERT(ccbits_rotl32(v, 0)  == v);
  ASSERT(ccbits_rotl32(v, 1)  == 0x00000003U);
  ASSERT(ccbits_rotl32(v, 31) == 0xC0000000U);
  /* roundtrip */
  ASSERT(ccbits_rotl32(v, 17) == ccbits_rotr32(v, 15));
  ASSERT(ccbits_rotl32(ccbits_rotr32(v, 7), 7) == v);
  /* k > 32 is masked */
  ASSERT(ccbits_rotl32(v, 33) == ccbits_rotl32(v, 1));
  /* all-ones is invariant */
  ASSERT(ccbits_rotl32(0xFFFFFFFFU, 13) == 0xFFFFFFFFU);
}

TEST(rotr32) {
  uint32_t v = 0x80000001U;
  ASSERT(ccbits_rotr32(v, 0)  == v);
  ASSERT(ccbits_rotr32(v, 1)  == 0xC0000000U);
  ASSERT(ccbits_rotr32(v, 31) == 0x00000003U);
  /* roundtrip */
  ASSERT(ccbits_rotr32(ccbits_rotl32(v, 11), 11) == v);
  /* k > 32 is masked */
  ASSERT(ccbits_rotr32(v, 33) == ccbits_rotr32(v, 1));
}

TEST(rotl64) {
  uint64_t v = UINT64_C(0x8000000000000001);
  ASSERT(ccbits_rotl64(v, 0)  == v);
  ASSERT(ccbits_rotl64(v, 1)  == UINT64_C(0x0000000000000003));
  ASSERT(ccbits_rotl64(v, 63) == UINT64_C(0xC000000000000000));
  /* roundtrip */
  ASSERT(ccbits_rotl64(v, 17) == ccbits_rotr64(v, 47));
  ASSERT(ccbits_rotl64(ccbits_rotr64(v, 7), 7) == v);
  /* k > 64 is masked */
  ASSERT(ccbits_rotl64(v, 65) == ccbits_rotl64(v, 1));
  ASSERT(ccbits_rotl64(UINT64_C(0xFFFFFFFFFFFFFFFF), 29) == UINT64_C(0xFFFFFFFFFFFFFFFF));
}

TEST(rotr64) {
  uint64_t v = UINT64_C(0x8000000000000001);
  ASSERT(ccbits_rotr64(v, 0)  == v);
  ASSERT(ccbits_rotr64(v, 1)  == UINT64_C(0xC000000000000000));
  ASSERT(ccbits_rotr64(v, 63) == UINT64_C(0x0000000000000003));
  /* roundtrip */
  ASSERT(ccbits_rotr64(ccbits_rotl64(v, 11), 11) == v);
  ASSERT(ccbits_rotr64(v, 65) == ccbits_rotr64(v, 1));
}

/* ═══════════════════════════════════════════════════════════════════════
**  Byte swap (bswap) tests
** ═══════════════════════════════════════════════════════════════════════ */

TEST(bswap16) {
  ASSERT(ccbits_bswap16(0x1234) == 0x3412);
  ASSERT(ccbits_bswap16(0x0000) == 0x0000);
  ASSERT(ccbits_bswap16(0xFFFF) == 0xFFFF);
  ASSERT(ccbits_bswap16(0x0102) == 0x0201);
  ASSERT(ccbits_bswap16(0x8080) == 0x8080);
  /* double swap is identity */
  ASSERT(ccbits_bswap16(ccbits_bswap16(0xABCD)) == 0xABCD);
}

TEST(bswap32) {
  ASSERT(ccbits_bswap32(0x01020304U)           == 0x04030201U);
  ASSERT(ccbits_bswap32(0x00000000U)           == 0x00000000U);
  ASSERT(ccbits_bswap32(0xFFFFFFFFU)           == 0xFFFFFFFFU);
  ASSERT(ccbits_bswap32(0x12345678U)           == 0x78563412U);
  ASSERT(ccbits_bswap32(0xAABBCCDDU)           == 0xDDCCBBAAU);
  ASSERT(ccbits_bswap32(0x80000000U)           == 0x00000080U);
  /* double swap is identity */
  ASSERT(ccbits_bswap32(ccbits_bswap32(0xDEADBEEFU)) == 0xDEADBEEFU);
}

TEST(bswap64) {
  ASSERT(ccbits_bswap64(UINT64_C(0x0102030405060708)) == UINT64_C(0x0807060504030201));
  ASSERT(ccbits_bswap64(UINT64_C(0x0000000000000000)) == UINT64_C(0x0000000000000000));
  ASSERT(ccbits_bswap64(UINT64_C(0xFFFFFFFFFFFFFFFF)) == UINT64_C(0xFFFFFFFFFFFFFFFF));
  ASSERT(ccbits_bswap64(UINT64_C(0x123456789ABCDEF0)) == UINT64_C(0xF0DEBC9A78563412));
  /* double swap is identity */
  ASSERT(ccbits_bswap64(ccbits_bswap64(UINT64_C(0xDEADBEEFCAFEBABE))) == UINT64_C(0xDEADBEEFCAFEBABE));
}

/* ═══════════════════════════════════════════════════════════════════════
**  Bit reverse tests
** ═══════════════════════════════════════════════════════════════════════ */

TEST(bitrev8) {
  ASSERT(ccbits_bitrev8(0x00) == 0x00);
  ASSERT(ccbits_bitrev8(0xFF) == 0xFF);
  ASSERT(ccbits_bitrev8(0x01) == 0x80);
  ASSERT(ccbits_bitrev8(0x80) == 0x01);
  ASSERT(ccbits_bitrev8(0x0F) == 0xF0);
  ASSERT(ccbits_bitrev8(0xF0) == 0x0F);
  ASSERT(ccbits_bitrev8(0x55) == 0xAA);
  ASSERT(ccbits_bitrev8(0xAA) == 0x55);
  ASSERT(ccbits_bitrev8(0x81) == 0x81);
  ASSERT(ccbits_bitrev8(0x42) == 0x42);  /* 0100 0010 → 0100 0010 */
  /* double reverse is identity */
  ASSERT(ccbits_bitrev8(ccbits_bitrev8(0x9A)) == 0x9A);
}

TEST(bitrev32) {
  ASSERT(ccbits_bitrev32(0x00000000U)          == 0x00000000U);
  ASSERT(ccbits_bitrev32(0xFFFFFFFFU)          == 0xFFFFFFFFU);
  ASSERT(ccbits_bitrev32(0x00000001U)          == 0x80000000U);
  ASSERT(ccbits_bitrev32(0x80000000U)          == 0x00000001U);
  ASSERT(ccbits_bitrev32(0x0000FFFFU)          == 0xFFFF0000U);
  ASSERT(ccbits_bitrev32(0x12345678U)          == 0x1E6A2C48U);
  ASSERT(ccbits_bitrev32(0x55555555U)          == 0xAAAAAAAAU);
  ASSERT(ccbits_bitrev32(0xAAAAAAAAU)          == 0x55555555U);
  /* double reverse is identity */
  ASSERT(ccbits_bitrev32(ccbits_bitrev32(0xDEADBEEFU)) == 0xDEADBEEFU);
}

TEST(bitrev64) {
  ASSERT(ccbits_bitrev64(UINT64_C(0x0000000000000000)) == UINT64_C(0x0000000000000000));
  ASSERT(ccbits_bitrev64(UINT64_C(0xFFFFFFFFFFFFFFFF)) == UINT64_C(0xFFFFFFFFFFFFFFFF));
  ASSERT(ccbits_bitrev64(UINT64_C(0x0000000000000001)) == UINT64_C(0x8000000000000000));
  ASSERT(ccbits_bitrev64(UINT64_C(0x8000000000000000)) == UINT64_C(0x0000000000000001));
  ASSERT(ccbits_bitrev64(UINT64_C(0x00000000FFFFFFFF)) == UINT64_C(0xFFFFFFFF00000000));
  ASSERT(ccbits_bitrev64(UINT64_C(0xAAAAAAAAAAAAAAAA)) == UINT64_C(0x5555555555555555));
  /* double reverse is identity */
  ASSERT(ccbits_bitrev64(ccbits_bitrev64(UINT64_C(0xDEADBEEFCAFEBABE))) == UINT64_C(0xDEADBEEFCAFEBABE));
}

/* ═══════════════════════════════════════════════════════════════════════
**  ceilpow2 tests
** ═══════════════════════════════════════════════════════════════════════ */

TEST(ceilpow2_32) {
  ASSERT(ccbits_ceilpow2_32(0)           == 0);
  ASSERT(ccbits_ceilpow2_32(1)           == 1);
  ASSERT(ccbits_ceilpow2_32(2)           == 2);
  ASSERT(ccbits_ceilpow2_32(3)           == 4);
  ASSERT(ccbits_ceilpow2_32(4)           == 4);
  ASSERT(ccbits_ceilpow2_32(5)           == 8);
  ASSERT(ccbits_ceilpow2_32(7)           == 8);
  ASSERT(ccbits_ceilpow2_32(8)           == 8);
  ASSERT(ccbits_ceilpow2_32(9)           == 16);
  ASSERT(ccbits_ceilpow2_32(255)         == 256);
  ASSERT(ccbits_ceilpow2_32(256)         == 256);
  ASSERT(ccbits_ceilpow2_32(0x40000000U) == 0x40000000U);
  ASSERT(ccbits_ceilpow2_32(0x40000001U) == 0x80000000U);
  ASSERT(ccbits_ceilpow2_32(0x80000000U) == 0x80000000U);
  /* overflow: > 2^31 wraps to 0 */
  ASSERT(ccbits_ceilpow2_32(0x80000001U) == 0);
}

TEST(ceilpow2_64) {
  ASSERT(ccbits_ceilpow2_64(0ULL)            == 0);
  ASSERT(ccbits_ceilpow2_64(1ULL)            == 1);
  ASSERT(ccbits_ceilpow2_64(3ULL)            == 4);
  ASSERT(ccbits_ceilpow2_64(0xFFFFFFFFULL)   == UINT64_C(0x100000000));
  ASSERT(ccbits_ceilpow2_64(UINT64_C(0x4000000000000000)) == UINT64_C(0x4000000000000000));
  ASSERT(ccbits_ceilpow2_64(UINT64_C(0x4000000000000001)) == UINT64_C(0x8000000000000000));
  ASSERT(ccbits_ceilpow2_64(UINT64_C(0x8000000000000000)) == UINT64_C(0x8000000000000000));
  /* overflow: > 2^63 wraps to 0 */
  ASSERT(ccbits_ceilpow2_64(UINT64_C(0x8000000000000001)) == 0);
}

/* ═══════════════════════════════════════════════════════════════════════
**  ispow2 tests
** ═══════════════════════════════════════════════════════════════════════ */

TEST(ispow2_32) {
  ASSERT(ccbits_ispow2_32(0)          == 0);
  ASSERT(ccbits_ispow2_32(1)          == 1);
  ASSERT(ccbits_ispow2_32(2)          == 1);
  ASSERT(ccbits_ispow2_32(3)          == 0);
  ASSERT(ccbits_ispow2_32(4)          == 1);
  ASSERT(ccbits_ispow2_32(8)          == 1);
  ASSERT(ccbits_ispow2_32(0x80000000U) == 1);
  ASSERT(ccbits_ispow2_32(0xFFFFFFFFU) == 0);
  ASSERT(ccbits_ispow2_32(0x7FFFFFFFU) == 0);
  ASSERT(ccbits_ispow2_32(0x40000000U) == 1);
}

TEST(ispow2_64) {
  ASSERT(ccbits_ispow2_64(0ULL)                     == 0);
  ASSERT(ccbits_ispow2_64(1ULL)                     == 1);
  ASSERT(ccbits_ispow2_64(2ULL)                     == 1);
  ASSERT(ccbits_ispow2_64(3ULL)                     == 0);
  ASSERT(ccbits_ispow2_64(0x8000000000000000ULL)    == 1);
  ASSERT(ccbits_ispow2_64(0xFFFFFFFFFFFFFFFFULL)    == 0);
  ASSERT(ccbits_ispow2_64(UINT64_C(0x100000000))    == 1);
  ASSERT(ccbits_ispow2_64(UINT64_C(0x100000001))    == 0);
}

/* ═══════════════════════════════════════════════════════════════════════
**  Cross-category consistency tests
** ═══════════════════════════════════════════════════════════════════════ */

TEST(popcount_equals_clz_ctz_consistency) {
  /* For any power of 2: popcount == 1, clz + ctz == bit_width - 1 */
  uint32_t p;
  for (p = 1; p != 0; p <<= 1) {
    ASSERT(ccbits_popcount32(p) == 1);
    ASSERT((unsigned)(ccbits_clz32(p) + ccbits_ctz32(p)) == 31U);
    ASSERT(ccbits_ispow2_32(p) == 1);
  }
}

TEST(popcount_equals_clz_ctz_consistency_64) {
  uint64_t p;
  for (p = 1; p != 0; p <<= 1) {
    ASSERT(ccbits_popcount64(p) == 1);
    ASSERT((unsigned)(ccbits_clz64(p) + ccbits_ctz64(p)) == 63U);
    ASSERT(ccbits_ispow2_64(p) == 1);
  }
}

TEST(double_reverse_is_identity) {
  /* For every function where f(f(x)) == x, test random-ish values */
  unsigned int i;
  for (i = 0; i < 100; i++) {
    uint32_t v32 = (uint32_t)(i * 2654435761U);  /* Knuth multiplicative hash */
    ASSERT(ccbits_bswap32(ccbits_bswap32(v32)) == v32);
    ASSERT(ccbits_bitrev32(ccbits_bitrev32(v32)) == v32);

    uint64_t v64 = (uint64_t)i * UINT64_C(0x9E3779B97F4A7C15);
    ASSERT(ccbits_bswap64(ccbits_bswap64(v64)) == v64);
    ASSERT(ccbits_bitrev64(ccbits_bitrev64(v64)) == v64);
  }
}

TEST(ceilpow2_is_power_of_two) {
  unsigned int i;
  for (i = 0; i <= 1000; i++) {
    uint32_t c = ccbits_ceilpow2_32((uint32_t)i);
    if (c != 0) {
      ASSERT(ccbits_ispow2_32(c) == 1);
      ASSERT(c >= (uint32_t)i);
    }
  }
  for (i = 0; i <= 1000; i++) {
    uint64_t c = ccbits_ceilpow2_64((uint64_t)i);
    if (c != 0) {
      ASSERT(ccbits_ispow2_64(c) == 1);
      ASSERT(c >= (uint64_t)i);
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════
**  bit_width tests
** ═══════════════════════════════════════════════════════════════════════ */

TEST(bit_width8) {
  ASSERT(ccbits_bit_width8(0)   == 0);
  ASSERT(ccbits_bit_width8(1)   == 1);
  ASSERT(ccbits_bit_width8(0x80)== 8);
  ASSERT(ccbits_bit_width8(0xFF)== 8);
  ASSERT(ccbits_bit_width8(0x0F)== 4);
  ASSERT(ccbits_bit_width8(0x10)== 5);
}

TEST(bit_width16) {
  ASSERT(ccbits_bit_width16(0)       == 0);
  ASSERT(ccbits_bit_width16(0x8000)  == 16);
  ASSERT(ccbits_bit_width16(0xFFFF)  == 16);
  ASSERT(ccbits_bit_width16(0x00FF)  == 8);
  ASSERT(ccbits_bit_width16(0x0100)  == 9);
}

TEST(bit_width32) {
  ASSERT(ccbits_bit_width32(0)          == 0);
  ASSERT(ccbits_bit_width32(1)          == 1);
  ASSERT(ccbits_bit_width32(2)          == 2);
  ASSERT(ccbits_bit_width32(3)          == 2);
  ASSERT(ccbits_bit_width32(4)          == 3);
  ASSERT(ccbits_bit_width32(8)          == 4);
  ASSERT(ccbits_bit_width32(0xFF)       == 8);
  ASSERT(ccbits_bit_width32(0x100)      == 9);
  ASSERT(ccbits_bit_width32(0x7FFFFFFFU) == 31);
  ASSERT(ccbits_bit_width32(0x80000000U) == 32);
}

TEST(bit_width64) {
  ASSERT(ccbits_bit_width64(0ULL)                       == 0);
  ASSERT(ccbits_bit_width64(1ULL)                       == 1);
  ASSERT(ccbits_bit_width64(0xFFFFFFFFULL)               == 32);
  ASSERT(ccbits_bit_width64(UINT64_C(0x100000000))       == 33);
  ASSERT(ccbits_bit_width64(UINT64_C(0x7FFFFFFFFFFFFFFF)) == 63);
  ASSERT(ccbits_bit_width64(UINT64_C(0x8000000000000000)) == 64);
}

/* ═══════════════════════════════════════════════════════════════════════
**  mask_low tests
** ═══════════════════════════════════════════════════════════════════════ */

TEST(mask_low32) {
  ASSERT(ccbits_mask_low32(0)  == 0x00000000U);
  ASSERT(ccbits_mask_low32(1)  == 0x00000001U);
  ASSERT(ccbits_mask_low32(5)  == 0x0000001FU);
  ASSERT(ccbits_mask_low32(8)  == 0x000000FFU);
  ASSERT(ccbits_mask_low32(16) == 0x0000FFFFU);
  ASSERT(ccbits_mask_low32(31) == 0x7FFFFFFFU);
  ASSERT(ccbits_mask_low32(32) == 0xFFFFFFFFU);
  ASSERT(ccbits_mask_low32(33) == 0xFFFFFFFFU);  /* clamped */
}

TEST(mask_low64) {
  ASSERT(ccbits_mask_low64(0)  == UINT64_C(0x0000000000000000));
  ASSERT(ccbits_mask_low64(1)  == UINT64_C(0x0000000000000001));
  ASSERT(ccbits_mask_low64(32) == UINT64_C(0x00000000FFFFFFFF));
  ASSERT(ccbits_mask_low64(48) == UINT64_C(0x0000FFFFFFFFFFFF));
  ASSERT(ccbits_mask_low64(63) == UINT64_C(0x7FFFFFFFFFFFFFFF));
  ASSERT(ccbits_mask_low64(64) == UINT64_C(0xFFFFFFFFFFFFFFFF));
  ASSERT(ccbits_mask_low64(65) == UINT64_C(0xFFFFFFFFFFFFFFFF));  /* clamped */
}

/* ═══════════════════════════════════════════════════════════════════════
**  parity tests
** ═══════════════════════════════════════════════════════════════════════ */

TEST(parity8) {
  ASSERT(ccbits_parity8(0x00) == 0);
  ASSERT(ccbits_parity8(0x01) == 1);
  ASSERT(ccbits_parity8(0x03) == 0);
  ASSERT(ccbits_parity8(0x80) == 1);
  ASSERT(ccbits_parity8(0xFF) == 0);
}

TEST(parity16) {
  ASSERT(ccbits_parity16(0x0000) == 0);
  ASSERT(ccbits_parity16(0x8000) == 1);
  ASSERT(ccbits_parity16(0xFFFF) == 0);
  ASSERT(ccbits_parity16(0xAAAA) == 0);
  ASSERT(ccbits_parity16(0x0001) == 1);
}

TEST(parity32) {
  ASSERT(ccbits_parity32(0x00000000U) == 0);  /* even */
  ASSERT(ccbits_parity32(0x00000001U) == 1);  /* odd  */
  ASSERT(ccbits_parity32(0x00000003U) == 0);  /* even (2 bits) */
  ASSERT(ccbits_parity32(0x00000007U) == 1);  /* odd  (3 bits) */
  ASSERT(ccbits_parity32(0xFFFFFFFFU) == 0);  /* even (32 bits) */
  ASSERT(ccbits_parity32(0x55555555U) == 0);  /* even (16 bits) */
  ASSERT(ccbits_parity32(0xAAAAAAAAU) == 0);  /* even (16 bits) */
  ASSERT(ccbits_parity32(0x80000000U) == 1);  /* odd  (1 bit) */
}

TEST(parity64) {
  ASSERT(ccbits_parity64(0ULL)                      == 0);
  ASSERT(ccbits_parity64(1ULL)                      == 1);
  ASSERT(ccbits_parity64(UINT64_C(0x8000000000000000)) == 1);
  ASSERT(ccbits_parity64(UINT64_C(0xFFFFFFFFFFFFFFFF)) == 0);  /* even */
}

/* ═══════════════════════════════════════════════════════════════════════
**  sign_ext tests
** ═══════════════════════════════════════════════════════════════════════ */

TEST(sign_ext32) {
  /* n=0 or n≥32: identity */
  ASSERT(ccbits_sign_ext32(0x12345678U, 0)  == (int32_t)0x12345678);
  ASSERT(ccbits_sign_ext32(0x12345678U, 32) == (int32_t)0x12345678);
  /* n=8: sign-extend byte */
  ASSERT(ccbits_sign_ext32(0x7FU, 8)  == 127);
  ASSERT(ccbits_sign_ext32(0x80U, 8)  == -128);
  ASSERT(ccbits_sign_ext32(0xFFU, 8)  == -1);
  ASSERT(ccbits_sign_ext32(0x00U, 8)  == 0);
  /* n=5 */
  ASSERT(ccbits_sign_ext32(0x1FU, 5)  == -1);    /* 11111 → -1 */
  ASSERT(ccbits_sign_ext32(0x10U, 5)  == -16);   /* 10000 → -16 */
  ASSERT(ccbits_sign_ext32(0x0FU, 5)  == 15);    /* 01111 → 15 */
  ASSERT(ccbits_sign_ext32(0x01U, 5)  == 1);
}

TEST(sign_ext64) {
  ASSERT(ccbits_sign_ext64(0x7FULL, 8)  == 127);
  ASSERT(ccbits_sign_ext64(0x80ULL, 8)  == -128);
  ASSERT(ccbits_sign_ext64(0xFFULL, 8)  == -1);
  ASSERT(ccbits_sign_ext64(UINT64_C(0x7FFFFFFFFFFFFFFF), 63) == (int64_t)(-1));  /* 63-bit all-1s = -1 */
  /* for n=64: identity */
  ASSERT(ccbits_sign_ext64(UINT64_C(0x8000000000000000), 64) == (int64_t)UINT64_C(0x8000000000000000));
}

/* ═══════════════════════════════════════════════════════════════════════
**  main
** ═══════════════════════════════════════════════════════════════════════ */

int main(void) {
  printf("ccbits tests:\n");
  RUN(popcount8);
  RUN(popcount16);
  RUN(popcount32);
  RUN(popcount64);
  RUN(clz16);
  RUN(clz32);
  RUN(clz64);
  RUN(ctz16);
  RUN(ctz32);
  RUN(ctz64);
  RUN(rotl32);
  RUN(rotr32);
  RUN(rotl64);
  RUN(rotr64);
  RUN(bswap16);
  RUN(bswap32);
  RUN(bswap64);
  RUN(bitrev8);
  RUN(bitrev32);
  RUN(bitrev64);
  RUN(ceilpow2_32);
  RUN(ceilpow2_64);
  RUN(ispow2_32);
  RUN(ispow2_64);
  RUN(popcount_equals_clz_ctz_consistency);
  RUN(popcount_equals_clz_ctz_consistency_64);
  RUN(double_reverse_is_identity);
  RUN(ceilpow2_is_power_of_two);
  RUN(bit_width8);
  RUN(bit_width16);
  RUN(bit_width32);
  RUN(bit_width64);
  RUN(mask_low32);
  RUN(mask_low64);
  RUN(parity8);
  RUN(parity16);
  RUN(parity32);
  RUN(parity64);
  RUN(sign_ext32);
  RUN(sign_ext64);
  printf("\n  %d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
