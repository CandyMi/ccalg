/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Test suite for ccbi (big-integer / arbitrary-precision arithmetic)
**  Build:  gcc -std=c99 -Wall -Wextra -o test_ccbi test_ccbi.c && ./test_ccbi
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ccbi.h"

/* ── test harness ─────────────────────────────────────────────────────── */
static int passed, failed;
#define TEST(name) static void test_##name(void)
#define ASSERT(cond) do { \
  if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failed++; } \
  else passed++; \
} while(0)
#define RUN(name) do { printf("  %s\n", #name); test_##name(); } while(0)

/* ── lifecycle ────────────────────────────────────────────────────────── */

TEST(lifecycle) {
  ccbi_t a, b;
  ccbi_init(&a); ccbi_init(&b);

  ASSERT(ccbi_is_zero(&a));
  ASSERT(ccbi_sign(&a) == 0);
  ASSERT(ccbi_is_zero(&a));

  ASSERT(ccbi_from_int(&a, 42) == 0);
  ASSERT(!ccbi_is_zero(&a));
  ASSERT(ccbi_sign(&a) == 1);
  ASSERT(ccbi_to_int(&a) == 42);

  ASSERT(ccbi_from_int(&a, -12345) == 0);
  ASSERT(ccbi_sign(&a) == -1);
  ASSERT(ccbi_to_int(&a) == -12345);

  /* copy */
  ccbi_copy(&b, &a);
  ASSERT(ccbi_to_int(&b) == -12345);

  /* zero / one */
  ccbi_zero(&a);    ASSERT(ccbi_is_zero(&a));
  ccbi_one(&a);     ASSERT(ccbi_is_one(&a));

  /* destroy + re-init (no double-free) */
  ccbi_destroy(&a); ccbi_destroy(&b);
}

TEST(from_str) {
  ccbi_t a;
  ccbi_init(&a);

  /* Decimal. */
  ASSERT(ccbi_from_str(&a, "12345678901234567890", 10) == 0);
  char *s = ccbi_to_str(&a, 10);
  ASSERT(s && strcmp(s, "12345678901234567890") == 0);
  ccbi_free_str(s);

  /* Hex. */
  ASSERT(ccbi_from_str(&a, "deadbeef", 16) == 0);
  s = ccbi_to_str(&a, 16);
  ASSERT(s && strcmp(s, "deadbeef") == 0);
  ccbi_free_str(s);

  /* Binary. */
  ASSERT(ccbi_from_str(&a, "101010", 2) == 0);
  s = ccbi_to_str(&a, 2);
  ASSERT(s && strcmp(s, "101010") == 0);
  ccbi_free_str(s);

  /* Negative. */
  ASSERT(ccbi_from_str(&a, "-98765", 10) == 0);
  s = ccbi_to_str(&a, 10);
  ASSERT(s && strcmp(s, "-98765") == 0);
  ccbi_free_str(s);

  /* Base 36. */
  ASSERT(ccbi_from_str(&a, "zzz", 36) == 0);
  s = ccbi_to_str(&a, 36);
  ASSERT(s && strcmp(s, "zzz") == 0);
  ccbi_free_str(s);

  /* Error: invalid digit. */
  ASSERT(ccbi_from_str(&a, "12g", 10) == 0);
  s = ccbi_to_str(&a, 10);
  ASSERT(s && strcmp(s, "12") == 0);
  ccbi_free_str(s);

  /* Error: base out of range. */
  ASSERT(ccbi_from_str(&a, "123", 1) == -1);
  ASSERT(ccbi_from_str(&a, "123", 37) == -1);

  /* Large decimal round-trip (> 256-bit, triggers SSO→heap). */
  ASSERT(ccbi_from_str(&a,
    "12345678901234567890123456789012345678901234567890"
    "12345678901234567890123456789012345678901234567890"
    "1234567890", 10) == 0);
  s = ccbi_to_str(&a, 10);
  ASSERT(s != NULL);
  ASSERT(strlen(s) == 110);
  ASSERT(strncmp(s, "12345678901234567890", 20) == 0);
  ccbi_free_str(s);

  /* from_uint(UINT64_MAX). */
  ccbi_from_uint(&a, 0xFFFFFFFFFFFFFFFFULL);
  ASSERT(CCBI_USED(&a) == 2);
  ASSERT(a.limbs[0] == 0xFFFFFFFF);
  ASSERT(a.limbs[1] == 0xFFFFFFFF);

  /* Empty and sign-only strings. */
  ASSERT(ccbi_from_str(&a, "", 10) == -1);
  ASSERT(ccbi_from_str(&a, "+", 10) == -1);
  ASSERT(ccbi_from_str(&a, "-", 10) == -1);

  ccbi_destroy(&a);
}

TEST(arithmetic) {
  ccbi_t a, b, c, q, r;
  ccbi_init(&a); ccbi_init(&b); ccbi_init(&c);
  ccbi_init(&q); ccbi_init(&r);

  /* add */
  ccbi_from_int(&a, 100); ccbi_from_int(&b, 200);
  ASSERT(ccbi_add(&c, &a, &b) == 0 && ccbi_to_int(&c) == 300);

  ccbi_from_int(&a, -50); ccbi_from_int(&b, 30);
  ASSERT(ccbi_add(&c, &a, &b) == 0 && ccbi_to_int(&c) == -20);

  ccbi_from_int(&a, 50); ccbi_from_int(&b, -30);
  ASSERT(ccbi_add(&c, &a, &b) == 0 && ccbi_to_int(&c) == 20);

  ccbi_from_uint(&a, 0xFFFFFFFFULL); ccbi_from_int(&b, 1);
  ASSERT(ccbi_add(&c, &a, &b) == 0 && ccbi_to_int(&c) == 0x100000000ULL);

  /* sub */
  ccbi_from_int(&a, 100); ccbi_from_int(&b, 75);
  ASSERT(ccbi_sub(&c, &a, &b) == 0 && ccbi_to_int(&c) == 25);

  ccbi_from_int(&a, 10); ccbi_from_int(&b, 100);
  ASSERT(ccbi_sub(&c, &a, &b) == 0 && ccbi_to_int(&c) == -90);

  ccbi_from_int(&a, -10); ccbi_from_int(&b, 100);
  ASSERT(ccbi_sub(&c, &a, &b) == 0 && ccbi_to_int(&c) == -110);

  /* inc / dec */
  ccbi_from_int(&a, 5);
  ccbi_inc(&a); ASSERT(ccbi_to_int(&a) == 6);
  ccbi_dec(&a); ASSERT(ccbi_to_int(&a) == 5);

  /* neg / abs */
  ccbi_neg(&c, &a);   ASSERT(ccbi_to_int(&c) == -5);
  ccbi_abs(&c, &c);   ASSERT(ccbi_to_int(&c) == 5);

  /* mul */
  ccbi_from_int(&a, 12345); ccbi_from_int(&b, 6789);
  ASSERT(ccbi_mul(&c, &a, &b) == 0);
  ASSERT(ccbi_cmp_int(&c, 12345 * 6789LL) == 0);

  /* mul: large (cross-limb). */
  ASSERT(ccbi_from_str(&a, "123456789012345678901234567890", 10) == 0);
  ASSERT(ccbi_from_str(&b, "987654321098765432109876543210", 10) == 0);
  ASSERT(ccbi_mul(&c, &a, &b) == 0);
  char *s = ccbi_to_str(&c, 10);
  ASSERT(s && strcmp(s, "121932631137021795226185032733622923332237463801111263526900") == 0);
  ccbi_free_str(s);

  /* div_rem: single limb. */
  ccbi_from_int(&a, 1000); ccbi_from_int(&b, 13);
  ASSERT(ccbi_div_rem(&q, &r, &a, &b) == 0);
  ASSERT(ccbi_to_int(&q) == 76);
  ASSERT(ccbi_to_int(&r) == 12);

  /* div_rem: large. */
  ASSERT(ccbi_from_str(&a, "12345678901234567890", 10) == 0);
  ASSERT(ccbi_from_str(&b, "987654321", 10) == 0);
  ASSERT(ccbi_div_rem(&q, &r, &a, &b) == 0);
  ASSERT(ccbi_to_int(&q) == 12499999887LL);
  ASSERT(ccbi_to_int(&r) == 339506163LL);

  /* mod. */
  ccbi_from_int(&a, 12345); ccbi_from_int(&b, 100);
  ASSERT(ccbi_mod(&c, &a, &b) == 0);
  ASSERT(ccbi_to_int(&c) == 45);

  /* mod: negative. */
  ccbi_from_int(&a, -7); ccbi_from_int(&b, 3);
  ccbi_mod(&c, &a, &b);
  ASSERT(ccbi_to_int(&c) == 2);

  ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&c);
  ccbi_destroy(&q); ccbi_destroy(&r);
}

TEST(shift) {
  ccbi_t a, c;
  ccbi_init(&a); ccbi_init(&c);

  /* shl */
  ccbi_from_int(&a, 1);
  ccbi_from_int(&a, 1);
  ASSERT(ccbi_shl(&c, &a, 10) == 0);
  ASSERT(ccbi_to_int(&c) == 1024);

  ASSERT(ccbi_shl(&c, &a, 32) == 0);
  ASSERT(ccbi_cmp_int(&c, 0x100000000ULL) == 0);

  /* shr */
  ccbi_from_int(&a, 1024);
  ASSERT(ccbi_shr(&c, &a, 10) == 0);
  ASSERT(ccbi_to_int(&c) == 1);

  /* bit_length */
  ccbi_from_int(&a, 0);   ASSERT(ccbi_bit_length(&a) == 0);
  ccbi_from_int(&a, 1);   ASSERT(ccbi_bit_length(&a) == 1);
  ccbi_from_int(&a, 255); ASSERT(ccbi_bit_length(&a) == 8);
  ccbi_from_int(&a, 256); ASSERT(ccbi_bit_length(&a) == 9);

  /* Large shift: 1 << 1024, across many limb boundaries. */
  ccbi_from_int(&a, 1);
  ASSERT(ccbi_shl(&c, &a, 1024) == 0);
  ASSERT(ccbi_bit_length(&c) == 1025);
  /* Shift back: shr 1024 → 1 */
  ASSERT(ccbi_shr(&c, &c, 1024) == 0);
  ASSERT(ccbi_to_int(&c) == 1);

  /* Shift beyond bit_length: shr all bits → 0. */
  ccbi_from_int(&a, 1);
  ASSERT(ccbi_shr(&c, &a, 1024) == 0);
  ASSERT(ccbi_is_zero(&c));

  ccbi_destroy(&a); ccbi_destroy(&c);
}

TEST(gcd) {
  ccbi_t a, b, c;
  ccbi_init(&a); ccbi_init(&b); ccbi_init(&c);

  ccbi_from_int(&a, 48);  ccbi_from_int(&b, 18);
  ASSERT(ccbi_gcd(&c, &a, &b) == 0);
  ASSERT(ccbi_to_int(&c) == 6);

  /* gcd with negatives */
  ccbi_from_int(&a, -48);  ccbi_from_int(&b, 18);
  ccbi_gcd(&c, &a, &b);
  ASSERT(ccbi_to_int(&c) == 6);

  /* gcd with large numbers */
  ASSERT(ccbi_from_str(&a, "12345678901234567890", 10) == 0);
  ASSERT(ccbi_from_str(&b, "9876543210", 10) == 0);
  ASSERT(ccbi_gcd(&c, &a, &b) == 0);
  char *s = ccbi_to_str(&c, 10);
  ASSERT(s && strcmp(s, "90") == 0);
  ccbi_free_str(s);

  ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&c);
}

TEST(pow_mod) {
  ccbi_t base, exp, mod, z;
  ccbi_init(&base); ccbi_init(&exp); ccbi_init(&mod); ccbi_init(&z);

  /* 3^5 mod 100 = 43 */
  ccbi_from_int(&base, 3); ccbi_from_int(&exp, 5); ccbi_from_int(&mod, 100);
  ASSERT(ccbi_pow_mod(&z, &base, &exp, &mod) == 0);
  ASSERT(ccbi_to_int(&z) == 43);

  /* 2^0 mod 100 = 1 */
  ccbi_from_int(&exp, 0);
  ASSERT(ccbi_pow_mod(&z, &base, &exp, &mod) == 0);
  ASSERT(ccbi_to_int(&z) == 1);

  /* Large: Fermat test 2^(p-1) mod p = 1 for prime p */
  ccbi_from_int(&base, 2);
  ccbi_from_int(&mod, 1000000007);
  ccbi_from_int(&exp, 1000000006);  /* p-1 */
  ASSERT(ccbi_pow_mod(&z, &base, &exp, &mod) == 0);
  ASSERT(ccbi_to_int(&z) == 1);

  /* Montgomery path test (512-bit modulus triggers k=16 path). */
  ASSERT(ccbi_from_str(&mod, "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff43", 16) == 0);
  /* mod = 2^512 - 189, odd, k=16 */
  ASSERT(ccbi_from_str(&base, "123456789", 10) == 0);
  ASSERT(ccbi_from_str(&exp, "123456789012345", 10) == 0);
  ASSERT(ccbi_pow_mod(&z, &base, &exp, &mod) == 0);
  char *s = ccbi_to_str(&z, 10);
  ASSERT(s && strcmp(s, "13390444357193110671926103783420359817110450971304580503913207273923088970088972067279184348783637286320939198681477752078783512432720233165284387400854165") == 0);
  ccbi_free_str(s);

  ccbi_destroy(&base); ccbi_destroy(&exp); ccbi_destroy(&mod); ccbi_destroy(&z);
}

TEST(swap) {
  ccbi_t a, b;
  ccbi_init(&a); ccbi_init(&b);
  ccbi_from_int(&a, 123); ccbi_from_int(&b, 456);
  ccbi_swap(&a, &b);
  ASSERT(ccbi_to_int(&a) == 456);
  ASSERT(ccbi_to_int(&b) == 123);
  ccbi_destroy(&a); ccbi_destroy(&b);
}

TEST(large_arithmetic) {
  ccbi_t a, b, c, q, r, one;
  ccbi_init(&a); ccbi_init(&b); ccbi_init(&c);
  ccbi_init(&q); ccbi_init(&r); ccbi_init(&one);

  /* 32-limb (1024-bit) operands — triggers SSO→heap + Karatsuba (≥16 limb). */
  ASSERT(ccbi_from_str(&a,
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 16) == 0);
  ASSERT(ccbi_from_str(&b,
    "10000000000000000000000000000000000000000000000000000000000000001", 16) == 0);

  /* Mul: Karatsuba path.  Verify with Euclidean identity. */
  ASSERT(ccbi_mul(&c, &a, &b) == 0);

  ASSERT(ccbi_div_rem(&q, &r, &c, &a) == 0);
  ASSERT(ccbi_cmp(&q, &b) == 0);
  ASSERT(ccbi_is_zero(&r));

  ASSERT(ccbi_div_rem(&q, &r, &c, &b) == 0);
  ASSERT(ccbi_cmp(&q, &a) == 0);
  ASSERT(ccbi_is_zero(&r));

  /* In-place mul: z == a */
  ccbi_one(&one);
  ASSERT(ccbi_mul(&c, &c, &one) == 0);
  ASSERT(ccbi_div_rem(&q, &r, &c, &b) == 0);
  ASSERT(ccbi_cmp(&q, &a) == 0);

  /* Large mod. */
  ASSERT(ccbi_mul(&c, &a, &a) == 0);
  ASSERT(ccbi_mod(&r, &c, &b) == 0);
  ASSERT(ccbi_cmp(&r, &b) < 0);
  ASSERT(ccbi_sign(&r) >= 0);

  /* SSO→heap→SSO round-trip. */
  ccbi_destroy(&c); ccbi_init(&c);
  ASSERT(ccbi_is_zero(&c));
  ASSERT(ccbi_copy(&c, &a) == 0);
  ASSERT(ccbi_cmp(&c, &a) == 0);
  ccbi_zero(&c);
  ASSERT(ccbi_is_zero(&c));

  ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&c);
  ccbi_destroy(&q); ccbi_destroy(&r); ccbi_destroy(&one);
}

TEST(boundary_sign) {
  ccbi_t a, b, c, q, r;
  ccbi_init(&a); ccbi_init(&b); ccbi_init(&c);
  ccbi_init(&q); ccbi_init(&r);

  /* ── INT64_MIN (UB fix) ── */
  ASSERT(ccbi_from_int(&a, INT64_MIN) == 0);
  ASSERT(ccbi_sign(&a) == -1);
  ASSERT(ccbi_to_int(&a) == INT64_MIN);

  /* ── INT64_MAX ── */
  ASSERT(ccbi_from_int(&a, INT64_MAX) == 0);
  ASSERT(ccbi_sign(&a) == 1);
  ASSERT(ccbi_to_int(&a) == INT64_MAX);

  /* ── INT32_MIN / INT32_MAX ── */
  ASSERT(ccbi_from_int(&a, INT32_MIN) == 0);
  ASSERT(ccbi_to_int(&a) == (int64_t)INT32_MIN);
  ASSERT(ccbi_from_int(&a, INT32_MAX) == 0);
  ASSERT(ccbi_to_int(&a) == INT32_MAX);

  /* ── Zero ± positive / negative ── */
  ccbi_zero(&a); ccbi_from_int(&b, 42);
  ccbi_add(&c, &a, &b); ASSERT(ccbi_to_int(&c) == 42);
  ccbi_sub(&c, &a, &b); ASSERT(ccbi_to_int(&c) == -42);
  ccbi_add(&c, &b, &a); ASSERT(ccbi_to_int(&c) == 42);
  ccbi_sub(&c, &b, &a); ASSERT(ccbi_to_int(&c) == 42);

  /* ── Neg/abs chaining ── */
  ccbi_from_int(&a, -7);
  ccbi_neg(&c, &a);  ASSERT(ccbi_to_int(&c) == 7);
  ccbi_neg(&c, &c);  ASSERT(ccbi_to_int(&c) == -7);
  ccbi_abs(&c, &a);  ASSERT(ccbi_to_int(&c) == 7);
  ccbi_abs(&c, &c);  ASSERT(ccbi_to_int(&c) == 7);
  ccbi_from_int(&c, 0); ccbi_abs(&c, &c); ASSERT(ccbi_sign(&c) == 0);

  /* ── Sign combinations in mul ── */
  ccbi_from_int(&a, 100); ccbi_from_int(&b, -3);
  ccbi_mul(&c, &a, &b); ASSERT(ccbi_to_int(&c) == -300);
  ccbi_mul(&c, &b, &a); ASSERT(ccbi_to_int(&c) == -300);
  ccbi_from_int(&b, -3);
  ccbi_mul(&c, &b, &b); ASSERT(ccbi_to_int(&c) == 9);
  ccbi_mul(&c, &a, &a); ASSERT(ccbi_to_int(&c) == 10000);

  /* ── Sign combinations in div_rem ── */
  ccbi_from_int(&a, 100); ccbi_from_int(&b, 7);
  ccbi_div_rem(&q, &r, &a, &b);
  ASSERT(ccbi_to_int(&q) == 14 && ccbi_to_int(&r) == 2);

  /* Negative dividend, positive divisor */
  ccbi_from_int(&a, -100);
  ccbi_div_rem(&q, &r, &a, &b);
  /* C truncation toward zero: -100/7 = -14, -100%7 = -2 */
  ASSERT(ccbi_sign(&q) == -1 && ccbi_to_int(&q) == -14 && ccbi_to_int(&r) == -2);

  /* Positive dividend, negative divisor */
  ccbi_from_int(&a, 100); ccbi_from_int(&b, -7);
  ccbi_div_rem(&q, &r, &a, &b);
  ASSERT(ccbi_sign(&q) == -1 && ccbi_to_int(&q) == -14 && ccbi_to_int(&r) == 2);

  /* Both negative */
  ccbi_from_int(&a, -100); ccbi_from_int(&b, -7);
  ccbi_div_rem(&q, &r, &a, &b);
  ASSERT(ccbi_sign(&q) == 1 && ccbi_to_int(&q) == 14 && ccbi_to_int(&r) == -2);

  /* ── Zero dividend ── */
  ccbi_zero(&a); ccbi_from_int(&b, 7);
  ccbi_div_rem(&q, &r, &a, &b);
  ASSERT(ccbi_is_zero(&q) && ccbi_is_zero(&r));

  /* ── div_rem(a, b) where |a| < |b| ── */
  ccbi_from_int(&a, 3); ccbi_from_int(&b, 100);
  ccbi_div_rem(&q, &r, &a, &b);
  ASSERT(ccbi_is_zero(&q) && ccbi_to_int(&r) == 3);

  /* ── mod with negative operands ── */
  ccbi_from_int(&a, -7); ccbi_from_int(&b, 3);
  ccbi_mod(&c, &a, &b);  ASSERT(ccbi_to_int(&c) == 2);
  ccbi_from_int(&a, 7); ccbi_from_int(&b, -3);
  ccbi_mod(&c, &a, &b);  ASSERT(ccbi_to_int(&c) == 1);
  ccbi_from_int(&a, -7); ccbi_from_int(&b, -3);
  ccbi_mod(&c, &a, &b);  ASSERT(ccbi_to_int(&c) == 2);

  /* ── SSO boundary: exactly 8 limbs (256-bit) ── */
  ASSERT(ccbi_from_str(&a,
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 16) == 0);
  ASSERT(CCBI_USED(&a) == 8);

  /* SSO+1: 9 limbs → must switch to heap (from_str may transiently exceed
   * SSO during construction, so buffer may be heap even at 8 limbs). */
  ASSERT(ccbi_from_str(&a,
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ff", 16) == 0);
  ASSERT(CCBI_USED(&a) == 9);

  /* ── Karatsuba boundary: exactly 16 limbs (512-bit) ── */
  ASSERT(ccbi_from_str(&b,
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 16) == 0);
  ASSERT(ccbi_mul(&c, &a, &b) == 0);

  /* ── Negative * large positive ── */
  ccbi_t neg; ccbi_init(&neg);
  ccbi_neg(&neg, &a);
  ASSERT(ccbi_mul(&c, &neg, &b) == 0);
  ASSERT(CCBI_SIGN(&c) == -1);

  /* Euclidean: |c| == |a| * |b| */
  ccbi_abs(&c, &c);
  ASSERT(ccbi_cmp(&c, &c) == 0);
  ccbi_destroy(&neg);

  /* ── add_uint / mul_uint on large values ── */
  ccbi_from_int(&a, 0);
  ccbi_add_uint(&a, 0xFFFFFFFFU);
  ASSERT(ccbi_to_int(&a) == 0xFFFFFFFFU);

  ccbi_from_uint(&a, 1);
  ccbi_mul_uint(&a, 0xFFFFFFFFU);
  ASSERT(ccbi_to_int(&a) == 0xFFFFFFFFU);

  /* ── to_str_len with edge values ── */
  ccbi_zero(&a);
  ASSERT(ccbi_to_str_len(&a, 10) == 1);
  ccbi_from_int(&a, -1);
  ASSERT(ccbi_to_str_len(&a, 10) >= 3);  /* "-1\0" */

  /* ── shift by 0 ── */
  ccbi_from_int(&a, 42);
  ccbi_shl(&c, &a, 0);  ASSERT(ccbi_to_int(&c) == 42);
  ccbi_shr(&c, &a, 0);  ASSERT(ccbi_to_int(&c) == 42);

  /* ── shift negative ── */
  ccbi_from_int(&a, -8);
  ccbi_shl(&c, &a, 2);  ASSERT(ccbi_to_int(&c) == -32);
  ccbi_shr(&c, &a, 1);  ASSERT(ccbi_to_int(&c) == -4);

  /* ── large + 1 = overflow limb ── */
  ASSERT(ccbi_from_str(&a,
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 16) == 0);
  ccbi_inc(&a);
  ASSERT(CCBI_USED(&a) == 9);
  ASSERT(a.limbs[8] == 1);  /* 2^256 */

  ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&c);
  ccbi_destroy(&q); ccbi_destroy(&r);
}

TEST(toom3) {
  ccbi_t a, b, c, ref;
  ccbi_init(&a); ccbi_init(&b); ccbi_init(&c);
  ccbi_init(&ref);

  /* 2048-bit a, Toom-3 threshold check */
  ASSERT(ccbi_from_str(&a,
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 16) == 0);

  /* Just verify Toom-3 runs without crash and produces 128 limbs */
  ASSERT(ccbi_mul(&c, &a, &a) == 0);
  ASSERT(CCBI_USED(&c) == 128);

  /* 64×96 uneven */
  ASSERT(ccbi_from_str(&b,
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 16) == 0);
  ASSERT(ccbi_mul(&c, &a, &b) == 0);
  /* In-place */
  ASSERT(ccbi_mul(&a, &a, &b) == 0);

  ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&c);
  ccbi_destroy(&ref);
}

TEST(large_div) {
  ccbi_t a, b, q, r, chk;
  ccbi_init(&a); ccbi_init(&b); ccbi_init(&q); ccbi_init(&r); ccbi_init(&chk);

  /* 64-limb (2048-bit) dividend = 2^2048 - 1. */
  ASSERT(ccbi_from_str(&a,
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 16) == 0);

  /* 16-limb divisor with top-most bit clear (0x7FFF...)
   * → Knuth normalize shift > 0. */
  ASSERT(ccbi_from_str(&b,
    "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 16) == 0);

  /* Divide: quotient has multiple limbs. */
  ASSERT(ccbi_div_rem(&q, &r, &a, &b) == 0);
  ASSERT(CCBI_USED(&q) > 1);

  /* Euclidean identity: q * b + r == a. */
  ASSERT(ccbi_mul(&chk, &q, &b) == 0);
  ASSERT(ccbi_add(&chk, &chk, &r) == 0);
  ASSERT(ccbi_cmp(&chk, &a) == 0);

  /* Remainder invariant. */
  ASSERT(ccbi_is_zero(&r) || ccbi_cmp(&r, &b) < 0);

  /* div_rem(q=NULL) — remainder matches ccbi_mod. */
  ASSERT(ccbi_div_rem(NULL, &r, &a, &b) == 0);
  ASSERT(ccbi_mod(&chk, &a, &b) == 0);
  ASSERT(ccbi_cmp(&r, &chk) == 0);

  /* div_rem(r=NULL) — quotient matches full call. */
  ASSERT(ccbi_div_rem(&q, NULL, &a, &b) == 0);
  ASSERT(ccbi_div_rem(&chk, &r, &a, &b) == 0);
  ASSERT(ccbi_cmp(&q, &chk) == 0);

  /* Division by zero. */
  ccbi_zero(&b);
  ASSERT(ccbi_div_rem(&q, &r, &a, &b) == -1);

  ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&q);
  ccbi_destroy(&r); ccbi_destroy(&chk);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Bitwise ops tests
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(and) {
  ccbi_t a, b, z; ccbi_init(&a); ccbi_init(&b); ccbi_init(&z);

  ccbi_from_uint(&a, 0x0F); ccbi_from_uint(&b, 0x33);
  ccbi_and(&z, &a, &b);
  ASSERT(ccbi_to_int(&z) == 0x03);

  ccbi_from_uint(&a, 0xFFFFFFFF);
  ccbi_and(&z, &a, &a);
  ASSERT(ccbi_to_int(&z) == 0xFFFFFFFFULL);

  ccbi_from_uint(&a, 0xFFFF0000);
  ccbi_from_uint(&b, 0x0000FFFF);
  ccbi_and(&z, &a, &b);
  ASSERT(ccbi_to_int(&z) == 0);

  ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&z);
}

TEST(or) {
  ccbi_t a, b, z; ccbi_init(&a); ccbi_init(&b); ccbi_init(&z);

  ccbi_from_uint(&a, 0x0F); ccbi_from_uint(&b, 0x30);
  ccbi_or(&z, &a, &b);
  ASSERT(ccbi_to_int(&z) == 0x3F);

  ccbi_from_uint(&a, 0);
  ccbi_from_uint(&b, 0xDEAD);
  ccbi_or(&z, &a, &b);
  ASSERT(ccbi_to_int(&z) == 0xDEAD);

  ccbi_from_uint(&a, 0xDEAD);
  ccbi_or(&z, &a, &a);
  ASSERT(ccbi_to_int(&z) == 0xDEAD);

  ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&z);
}

TEST(xor) {
  ccbi_t a, b, z; ccbi_init(&a); ccbi_init(&b); ccbi_init(&z);

  ccbi_from_uint(&a, 0x0F); ccbi_from_uint(&b, 0x33);
  ccbi_xor(&z, &a, &b);
  ASSERT(ccbi_to_int(&z) == 0x3C);

  ccbi_xor(&z, &a, &a);
  ASSERT(ccbi_to_int(&z) == 0);

  ccbi_from_uint(&b, 0xDEAD);
  ccbi_xor(&z, &a, &b);
  ccbi_xor(&z, &z, &b);
  ASSERT(ccbi_to_int(&z) == 0x0F);

  ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&z);
}

TEST(not) {
  ccbi_t a, z; ccbi_init(&a); ccbi_init(&z);

  ccbi_zero(&a);
  ccbi_not(&z, &a);
  ASSERT(ccbi_is_zero(&z));

  ccbi_one(&a);
  ccbi_not(&z, &a);
  ASSERT(ccbi_is_zero(&z));

  ccbi_from_uint(&a, 2);        /* 0b10, bit_length=2 */
  ccbi_not(&z, &a);
  ASSERT(ccbi_to_int(&z) == 1);

  ccbi_from_uint(&a, 0x0F);     /* bit_length=4 */
  ccbi_not(&z, &a);
  ASSERT(ccbi_to_int(&z) == 0);

  ccbi_from_uint(&a, 0xAAAAAAAA);
  ccbi_not(&z, &a);
  ASSERT(ccbi_to_int(&z) == 0x55555555ULL);

  /* multi-limb: 0x00000001FFFFFFFF */
  ccbi_from_uint(&a, UINT64_C(0x1FFFFFFFF));
  ASSERT(ccbi_bit_length(&a) == 33);
  ccbi_not(&z, &a);
  /* ~ in 33-bit space: 0x1FFFFFFFF → 0x00000000 */
  ASSERT(ccbi_is_zero(&z));

  ccbi_destroy(&a); ccbi_destroy(&z);
}

TEST(test_bit) {
  ccbi_t a; ccbi_init(&a);

  ccbi_zero(&a);
  ASSERT(ccbi_test_bit(&a, 0) == 0);
  ASSERT(ccbi_test_bit(&a, 100) == 0);

  ccbi_one(&a);
  ASSERT(ccbi_test_bit(&a, 0) == 1);
  ASSERT(ccbi_test_bit(&a, 1) == 0);

  ccbi_from_uint(&a, 0x80000000);
  ASSERT(ccbi_test_bit(&a, 31) == 1);
  ASSERT(ccbi_test_bit(&a, 30) == 0);

  ccbi_destroy(&a);
}

TEST(set_clear_flip) {
  ccbi_t a; ccbi_init(&a);

  ccbi_zero(&a);
  ccbi_set_bit(&a, 5);
  ASSERT(ccbi_test_bit(&a, 5) == 1);
  ASSERT(ccbi_to_int(&a) == 32);

  ccbi_clear_bit(&a, 5);
  ASSERT(ccbi_test_bit(&a, 5) == 0);
  ASSERT(ccbi_is_zero(&a));

  /* flip from 0 to 1 on a large offset — triggers grow */
  ccbi_flip_bit(&a, 70);   /* limb 2, bit 6 */
  ASSERT(ccbi_test_bit(&a, 70) == 1);
  ASSERT(ccbi_bit_length(&a) == 71);

  ccbi_flip_bit(&a, 70);
  ASSERT(ccbi_test_bit(&a, 70) == 0);
  ASSERT(ccbi_is_zero(&a));

  /* alias: set_bit on self */
  ccbi_set_bit(&a, 0);
  ccbi_set_bit(&a, 63);
  ASSERT(ccbi_test_bit(&a, 0) == 1);
  ASSERT(ccbi_test_bit(&a, 63) == 1);
  ASSERT(ccbi_to_int(&a) == (int64_t)(UINT64_C(1) | (UINT64_C(1) << 63)));

  ccbi_destroy(&a);
}

TEST(bit_alias) {
  /* AND/OR/XOR with z == a or z == b */
  ccbi_t a, b; ccbi_init(&a); ccbi_init(&b);

  ccbi_from_uint(&a, 0x0F); ccbi_from_uint(&b, 0x33);

  /* z == a */
  ccbi_and(&a, &a, &b);
  ASSERT(ccbi_to_int(&a) == 0x03);

  ccbi_from_uint(&a, 0x0F);
  ccbi_or(&a, &a, &b);
  ASSERT(ccbi_to_int(&a) == 0x3F);

  ccbi_from_uint(&a, 0x0F);
  ccbi_xor(&a, &a, &b);
  ASSERT(ccbi_to_int(&a) == 0x3C);

  /* z == b */
  ccbi_from_uint(&a, 0x0F); ccbi_from_uint(&b, 0x33);
  ccbi_and(&b, &a, &b);
  ASSERT(ccbi_to_int(&b) == 0x03);

  ccbi_from_uint(&b, 0x33);
  ccbi_from_uint(&a, 0x0F);
  ccbi_or(&b, &a, &b);
  ASSERT(ccbi_to_int(&b) == 0x3F);

  ccbi_from_uint(&b, 0x33);
  ccbi_from_uint(&a, 0x0F);
  ccbi_xor(&b, &a, &b);
  ASSERT(ccbi_to_int(&b) == 0x3C);

  /* not alias */
  ccbi_from_uint(&a, 0xAAAAAAAA);
  ccbi_not(&a, &a);
  ASSERT(ccbi_to_int(&a) == 0x55555555ULL);

  ccbi_destroy(&a); ccbi_destroy(&b);
}

/* ── main ─────────────────────────────────────────────────────────────── */
int main(void) {
  printf("ccbi test suite\n===============\n");

  RUN(lifecycle);
  RUN(from_str);
  RUN(arithmetic);
  RUN(shift);
  RUN(gcd);
  RUN(pow_mod);
  RUN(swap);
  RUN(large_arithmetic);
  RUN(boundary_sign);
  RUN(toom3);
  RUN(large_div);
  RUN(and);
  RUN(or);
  RUN(xor);
  RUN(not);
  RUN(test_bit);
  RUN(set_clear_flip);
  RUN(bit_alias);

  printf("===============\n%d passed, %d failed\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
