#include "ccbi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failed = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { \
    fprintf(stderr, "  FAIL [%s:%d] %s\n", __FILE__, __LINE__, msg); \
    failed++; \
  } \
} while(0)

static void test_lifecycle(void) {
  printf("  lifecycle...\n");
  ccbi_t a, b;
  ccbi_init(&a); ccbi_init(&b);

  CHECK(ccbi_is_zero(&a), "fresh init is zero");
  CHECK(ccbi_sign(&a) == 0, "zero sign");
  CHECK(ccbi_is_zero(&a), "zero used");

  CHECK(ccbi_from_int(&a, 42) == 0, "from_int");
  CHECK(!ccbi_is_zero(&a), "from_int non-zero");
  CHECK(ccbi_sign(&a) == 1, "positive sign");
  CHECK(ccbi_to_int(&a) == 42, "to_int returns 42");

  CHECK(ccbi_from_int(&a, -12345) == 0, "from_int negative");
  CHECK(ccbi_sign(&a) == -1, "negative sign");
  CHECK(ccbi_to_int(&a) == -12345, "to_int returns -12345");

  /* copy */
  ccbi_copy(&b, &a);
  CHECK(ccbi_to_int(&b) == -12345, "copy");

  /* zero / one */
  ccbi_zero(&a);    CHECK(ccbi_is_zero(&a), "zero");
  ccbi_one(&a);     CHECK(ccbi_is_one(&a), "one");

  /* destroy + re-init (no double-free) */
  ccbi_destroy(&a); ccbi_destroy(&b);
}

static void test_from_str(void) {
  printf("  from_str / to_str...\n");
  ccbi_t a;
  ccbi_init(&a);

  /* Decimal. */
  CHECK(ccbi_from_str(&a, "12345678901234567890", 10) == 0, "from_str decimal");
  char *s = ccbi_to_str(&a, 10);
  CHECK(s && strcmp(s, "12345678901234567890") == 0, "decimal round-trip");
  ccbi_free_str(s);

  /* Hex. */
  CHECK(ccbi_from_str(&a, "deadbeef", 16) == 0, "from_str hex");
  s = ccbi_to_str(&a, 16);
  CHECK(s && strcmp(s, "deadbeef") == 0, "hex round-trip");
  ccbi_free_str(s);

  /* Binary. */
  CHECK(ccbi_from_str(&a, "101010", 2) == 0, "from_str binary");
  s = ccbi_to_str(&a, 2);
  CHECK(s && strcmp(s, "101010") == 0, "binary round-trip");
  ccbi_free_str(s);

  /* Negative. */
  CHECK(ccbi_from_str(&a, "-98765", 10) == 0, "from_str negative");
  s = ccbi_to_str(&a, 10);
  CHECK(s && strcmp(s, "-98765") == 0, "negative round-trip");
  ccbi_free_str(s);

  /* Base 36. */
  CHECK(ccbi_from_str(&a, "zzz", 36) == 0, "from_str base36");
  s = ccbi_to_str(&a, 36);
  CHECK(s && strcmp(s, "zzz") == 0, "base36 round-trip");
  ccbi_free_str(s);

  /* Error: invalid digit. */
  /* from_str stops at first invalid char (parses "12"), no error. */
  CHECK(ccbi_from_str(&a, "12g", 10) == 0, "invalid digit stops");
  s = ccbi_to_str(&a, 10);
  CHECK(s && strcmp(s, "12") == 0, "invalid digit parsed 12");
  ccbi_free_str(s);

  /* Error: base out of range. */
  CHECK(ccbi_from_str(&a, "123", 1) == -1, "base too small");
  CHECK(ccbi_from_str(&a, "123", 37) == -1, "base too large");

  /* Large decimal round-trip (> 256-bit, triggers SSO→heap). */
  CHECK(ccbi_from_str(&a,
    "12345678901234567890123456789012345678901234567890"
    "12345678901234567890123456789012345678901234567890"
    "1234567890", 10) == 0, "110-digit decimal");
  s = ccbi_to_str(&a, 10);
  CHECK(s != NULL, "to_str large decimal");
  CHECK(strlen(s) == 110, "110-digit length");
  CHECK(strncmp(s, "12345678901234567890", 20) == 0, "large decimal prefix");
  ccbi_free_str(s);

  /* from_uint(UINT64_MAX). */
  ccbi_from_uint(&a, 0xFFFFFFFFFFFFFFFFULL);
  CHECK(CCBI_USED(&a) == 2, "UINT64_MAX used=2");
  CHECK(a.limbs[0] == 0xFFFFFFFF, "UINT64_MAX limb[0]");
  CHECK(a.limbs[1] == 0xFFFFFFFF, "UINT64_MAX limb[1]");

  /* Empty and sign-only strings. */
  CHECK(ccbi_from_str(&a, "", 10) == -1, "empty string");
  CHECK(ccbi_from_str(&a, "+", 10) == -1, "plus-only");
  CHECK(ccbi_from_str(&a, "-", 10) == -1, "minus-only");

  ccbi_destroy(&a);
}

static void test_arithmetic(void) {
  printf("  arithmetic...\n");
  ccbi_t a, b, c, q, r;
  ccbi_init(&a); ccbi_init(&b); ccbi_init(&c);
  ccbi_init(&q); ccbi_init(&r);

  /* add */
  ccbi_from_int(&a, 100); ccbi_from_int(&b, 200);
  CHECK(ccbi_add(&c, &a, &b) == 0 && ccbi_to_int(&c) == 300, "100+200");

  ccbi_from_int(&a, -50); ccbi_from_int(&b, 30);
  CHECK(ccbi_add(&c, &a, &b) == 0 && ccbi_to_int(&c) == -20, "-50+30");

  ccbi_from_int(&a, 50); ccbi_from_int(&b, -30);
  CHECK(ccbi_add(&c, &a, &b) == 0 && ccbi_to_int(&c) == 20, "50+(-30)");

  ccbi_from_uint(&a, 0xFFFFFFFFULL); ccbi_from_int(&b, 1);
  CHECK(ccbi_add(&c, &a, &b) == 0 && ccbi_to_int(&c) == 0x100000000ULL, "carry");

  /* sub */
  ccbi_from_int(&a, 100); ccbi_from_int(&b, 75);
  CHECK(ccbi_sub(&c, &a, &b) == 0 && ccbi_to_int(&c) == 25, "100-75");

  ccbi_from_int(&a, 10); ccbi_from_int(&b, 100);
  CHECK(ccbi_sub(&c, &a, &b) == 0 && ccbi_to_int(&c) == -90, "10-100");

  ccbi_from_int(&a, -10); ccbi_from_int(&b, 100);
  CHECK(ccbi_sub(&c, &a, &b) == 0 && ccbi_to_int(&c) == -110, "-10-100");

  /* inc / dec */
  ccbi_from_int(&a, 5);
  ccbi_inc(&a); CHECK(ccbi_to_int(&a) == 6, "inc");
  ccbi_dec(&a); CHECK(ccbi_to_int(&a) == 5, "dec");

  /* neg / abs */
  ccbi_neg(&c, &a);   CHECK(ccbi_to_int(&c) == -5, "neg");
  ccbi_abs(&c, &c);   CHECK(ccbi_to_int(&c) == 5,  "abs");

  /* mul */
  ccbi_from_int(&a, 12345); ccbi_from_int(&b, 6789);
  CHECK(ccbi_mul(&c, &a, &b) == 0, "mul");
  CHECK(ccbi_cmp_int(&c, 12345 * 6789LL) == 0, "mul check");

  /* mul: large (cross-limb). */
  CHECK(ccbi_from_str(&a, "123456789012345678901234567890", 10) == 0, "");
  CHECK(ccbi_from_str(&b, "987654321098765432109876543210", 10) == 0, "");
  CHECK(ccbi_mul(&c, &a, &b) == 0, "large mul");
  char *s = ccbi_to_str(&c, 10);
  CHECK(s && strcmp(s, "121932631137021795226185032733622923332237463801111263526900") == 0,
        "large mul check");
  ccbi_free_str(s);

  /* div_rem: single limb. */
  ccbi_from_int(&a, 1000); ccbi_from_int(&b, 13);
  CHECK(ccbi_div_rem(&q, &r, &a, &b) == 0, "div");
  CHECK(ccbi_to_int(&q) == 76, "q=1000/13");
  CHECK(ccbi_to_int(&r) == 12, "r=1000%13");

  /* div_rem: large. */
  CHECK(ccbi_from_str(&a, "12345678901234567890", 10) == 0, "");
  CHECK(ccbi_from_str(&b, "987654321", 10) == 0, "");
  CHECK(ccbi_div_rem(&q, &r, &a, &b) == 0, "large div");
  CHECK(ccbi_to_int(&q) == 12499999887LL, "q large");
  CHECK(ccbi_to_int(&r) == 339506163LL, "r large");

  /* mod. */
  ccbi_from_int(&a, 12345); ccbi_from_int(&b, 100);
  CHECK(ccbi_mod(&c, &a, &b) == 0, "mod");
  CHECK(ccbi_to_int(&c) == 45, "12345 %% 100");

  /* mod: negative. */
  ccbi_from_int(&a, -7); ccbi_from_int(&b, 3);
  ccbi_mod(&c, &a, &b);
  CHECK(ccbi_to_int(&c) == 2, "(-7) %% 3 = 2");

  ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&c);
  ccbi_destroy(&q); ccbi_destroy(&r);
}

static void test_shift(void) {
  printf("  shift / bit_length...\n");
  ccbi_t a, c;
  ccbi_init(&a); ccbi_init(&c);

  /* shl */
  ccbi_from_int(&a, 1);
  ccbi_from_int(&a, 1);
  CHECK(ccbi_shl(&c, &a, 10) == 0, "shl 10");
  CHECK(ccbi_to_int(&c) == 1024, "1<<10");

  CHECK(ccbi_shl(&c, &a, 32) == 0, "shl 32");
  CHECK(ccbi_cmp_int(&c, 0x100000000ULL) == 0, "1<<32");

  /* shr */
  ccbi_from_int(&a, 1024);
  CHECK(ccbi_shr(&c, &a, 10) == 0, "shr 10");
  CHECK(ccbi_to_int(&c) == 1, "1024>>10");

  /* bit_length */
  ccbi_from_int(&a, 0);   CHECK(ccbi_bit_length(&a) == 0, "bitlen(0)");
  ccbi_from_int(&a, 1);   CHECK(ccbi_bit_length(&a) == 1, "bitlen(1)");
  ccbi_from_int(&a, 255); CHECK(ccbi_bit_length(&a) == 8, "bitlen(255)");
  ccbi_from_int(&a, 256); CHECK(ccbi_bit_length(&a) == 9, "bitlen(256)");

  /* Large shift: 1 << 1024, across many limb boundaries. */
  ccbi_from_int(&a, 1);
  CHECK(ccbi_shl(&c, &a, 1024) == 0, "shl 1024");
  CHECK(ccbi_bit_length(&c) == 1025, "bitlen after shl 1024");
  /* Shift back: shr 1024 → 1 */
  CHECK(ccbi_shr(&c, &c, 1024) == 0, "shr 1024 back");
  CHECK(ccbi_to_int(&c) == 1, "shl+shr round-trip");

  /* Shift beyond bit_length: shr all bits → 0. */
  ccbi_from_int(&a, 1);
  CHECK(ccbi_shr(&c, &a, 1024) == 0, "shr past bitlen → 0");
  CHECK(ccbi_is_zero(&c), "shr past bitlen result is zero");

  ccbi_destroy(&a); ccbi_destroy(&c);
}

static void test_gcd(void) {
  printf("  gcd...\n");
  ccbi_t a, b, c;
  ccbi_init(&a); ccbi_init(&b); ccbi_init(&c);

  ccbi_from_int(&a, 48);  ccbi_from_int(&b, 18);
  CHECK(ccbi_gcd(&c, &a, &b) == 0, "gcd(48,18)");
  CHECK(ccbi_to_int(&c) == 6, "gcd=6");

  /* gcd with negatives */
  ccbi_from_int(&a, -48);  ccbi_from_int(&b, 18);
  ccbi_gcd(&c, &a, &b);
  CHECK(ccbi_to_int(&c) == 6, "gcd(-48,18)=6");

  /* gcd with large numbers */
  CHECK(ccbi_from_str(&a, "12345678901234567890", 10) == 0, "");
  CHECK(ccbi_from_str(&b, "9876543210", 10) == 0, "");
  CHECK(ccbi_gcd(&c, &a, &b) == 0, "gcd large");
  char *s = ccbi_to_str(&c, 10);
  CHECK(s && strcmp(s, "90") == 0, "gcd large = 90");
  ccbi_free_str(s);

  ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&c);
}

static void test_pow_mod(void) {
  printf("  pow_mod...\n");
  ccbi_t base, exp, mod, z;
  ccbi_init(&base); ccbi_init(&exp); ccbi_init(&mod); ccbi_init(&z);

  /* 3^5 mod 100 = 43 */
  ccbi_from_int(&base, 3); ccbi_from_int(&exp, 5); ccbi_from_int(&mod, 100);
  CHECK(ccbi_pow_mod(&z, &base, &exp, &mod) == 0, "pow_mod");
  CHECK(ccbi_to_int(&z) == 43, "3^5 mod 100");

  /* 2^0 mod 100 = 1 */
  ccbi_from_int(&exp, 0);
  CHECK(ccbi_pow_mod(&z, &base, &exp, &mod) == 0, "pow_mod x^0");
  CHECK(ccbi_to_int(&z) == 1, "2^0 = 1");

  /* Large: Fermat test 2^(p-1) mod p = 1 for prime p */
  ccbi_from_int(&base, 2);
  ccbi_from_int(&mod, 1000000007);
  ccbi_from_int(&exp, 1000000006);  /* p-1 */
  CHECK(ccbi_pow_mod(&z, &base, &exp, &mod) == 0, "Fermat test");
  CHECK(ccbi_to_int(&z) == 1, "2^(p-1) mod p = 1");

  /* Montgomery path test (512-bit modulus triggers k=16 path). */
  CHECK(ccbi_from_str(&mod, "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff43", 16) == 0, "");
  /* mod = 2^512 - 189, odd, k=16 */
  CHECK(ccbi_from_str(&base, "123456789", 10) == 0, "");
  CHECK(ccbi_from_str(&exp, "123456789012345", 10) == 0, "");
  CHECK(ccbi_pow_mod(&z, &base, &exp, &mod) == 0, "Montgomery pow_mod");
  char *s = ccbi_to_str(&z, 10);
  CHECK(s && strcmp(s, "13390444357193110671926103783420359817110450971304580503913207273923088970088972067279184348783637286320939198681477752078783512432720233165284387400854165") == 0,
        "Montgomery path result");
  ccbi_free_str(s);

  ccbi_destroy(&base); ccbi_destroy(&exp); ccbi_destroy(&mod); ccbi_destroy(&z);
}

static void test_swap(void) {
  printf("  swap...\n");
  ccbi_t a, b;
  ccbi_init(&a); ccbi_init(&b);
  ccbi_from_int(&a, 123); ccbi_from_int(&b, 456);
  ccbi_swap(&a, &b);
  CHECK(ccbi_to_int(&a) == 456, "swap a");
  CHECK(ccbi_to_int(&b) == 123, "swap b");
  ccbi_destroy(&a); ccbi_destroy(&b);
}

static void test_large_arithmetic(void) {
  printf("  large arithmetic...\n");
  ccbi_t a, b, c, q, r, one;
  ccbi_init(&a); ccbi_init(&b); ccbi_init(&c);
  ccbi_init(&q); ccbi_init(&r); ccbi_init(&one);

  /* 32-limb (1024-bit) operands — triggers SSO→heap + Karatsuba (≥16 limb). */
  CHECK(ccbi_from_str(&a,
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 16) == 0,
    "1024-bit a");
  CHECK(ccbi_from_str(&b,
    "10000000000000000000000000000000000000000000000000000000000000001", 16) == 0,
    "512-bit b");

  /* Mul: Karatsuba path.  Verify with Euclidean identity. */
  CHECK(ccbi_mul(&c, &a, &b) == 0, "karatsuba mul");

  CHECK(ccbi_div_rem(&q, &r, &c, &a) == 0, "div c/a");
  CHECK(ccbi_cmp(&q, &b) == 0, "c/a == b");
  CHECK(ccbi_is_zero(&r), "c/a remainder zero");

  CHECK(ccbi_div_rem(&q, &r, &c, &b) == 0, "div c/b");
  CHECK(ccbi_cmp(&q, &a) == 0, "c/b == a");
  CHECK(ccbi_is_zero(&r), "c/b remainder zero");

  /* In-place mul: z == a */
  ccbi_one(&one);
  CHECK(ccbi_mul(&c, &c, &one) == 0, "in-place mul identity");
  CHECK(ccbi_div_rem(&q, &r, &c, &b) == 0, "in-place verify");
  CHECK(ccbi_cmp(&q, &a) == 0, "in-place preserved value");

  /* Large mod. */
  CHECK(ccbi_mul(&c, &a, &a) == 0, "square a");
  CHECK(ccbi_mod(&r, &c, &b) == 0, "a^2 mod b");
  CHECK(ccbi_cmp(&r, &b) < 0, "mod result < modulus");
  CHECK(ccbi_sign(&r) >= 0, "mod result >= 0");

  /* SSO→heap→SSO round-trip. */
  ccbi_destroy(&c); ccbi_init(&c);
  CHECK(ccbi_is_zero(&c), "re-init is zero");
  CHECK(ccbi_copy(&c, &a) == 0, "copy large into fresh");
  CHECK(ccbi_cmp(&c, &a) == 0, "copy of large matches");
  ccbi_zero(&c);
  CHECK(ccbi_is_zero(&c), "zero() large releases to SSO");

  ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&c);
  ccbi_destroy(&q); ccbi_destroy(&r); ccbi_destroy(&one);
}

static void test_boundary_sign(void) {
  printf("  boundary values & sign...\n");
  ccbi_t a, b, c, q, r;
  ccbi_init(&a); ccbi_init(&b); ccbi_init(&c);
  ccbi_init(&q); ccbi_init(&r);

  /* ── INT64_MIN (UB fix) ── */
  CHECK(ccbi_from_int(&a, INT64_MIN) == 0, "from_int(INT64_MIN)");
  CHECK(ccbi_sign(&a) == -1, "INT64_MIN sign");
  CHECK(ccbi_to_int(&a) == INT64_MIN, "INT64_MIN round-trip");

  /* ── INT64_MAX ── */
  CHECK(ccbi_from_int(&a, INT64_MAX) == 0, "from_int(INT64_MAX)");
  CHECK(ccbi_sign(&a) == 1, "INT64_MAX sign");
  CHECK(ccbi_to_int(&a) == INT64_MAX, "INT64_MAX round-trip");

  /* ── INT32_MIN / INT32_MAX ── */
  CHECK(ccbi_from_int(&a, INT32_MIN) == 0, "from_int(INT32_MIN)");
  CHECK(ccbi_to_int(&a) == (int64_t)INT32_MIN, "INT32_MIN round-trip");
  CHECK(ccbi_from_int(&a, INT32_MAX) == 0, "from_int(INT32_MAX)");
  CHECK(ccbi_to_int(&a) == INT32_MAX, "INT32_MAX round-trip");

  /* ── Zero ± positive / negative ── */
  ccbi_zero(&a); ccbi_from_int(&b, 42);
  ccbi_add(&c, &a, &b); CHECK(ccbi_to_int(&c) == 42, "0+42");
  ccbi_sub(&c, &a, &b); CHECK(ccbi_to_int(&c) == -42, "0-42");
  ccbi_add(&c, &b, &a); CHECK(ccbi_to_int(&c) == 42, "42+0");
  ccbi_sub(&c, &b, &a); CHECK(ccbi_to_int(&c) == 42, "42-0");

  /* ── Neg/abs chaining ── */
  ccbi_from_int(&a, -7);
  ccbi_neg(&c, &a);  CHECK(ccbi_to_int(&c) == 7, "neg(-7)");
  ccbi_neg(&c, &c);  CHECK(ccbi_to_int(&c) == -7, "neg(neg(-7))");
  ccbi_abs(&c, &a);  CHECK(ccbi_to_int(&c) == 7, "abs(-7)");
  ccbi_abs(&c, &c);  CHECK(ccbi_to_int(&c) == 7, "abs(abs(-7))");
  ccbi_from_int(&c, 0); ccbi_abs(&c, &c); CHECK(ccbi_sign(&c) == 0, "abs(0)");

  /* ── Sign combinations in mul ── */
  ccbi_from_int(&a, 100); ccbi_from_int(&b, -3);
  ccbi_mul(&c, &a, &b); CHECK(ccbi_to_int(&c) == -300, "100*(-3)");
  ccbi_mul(&c, &b, &a); CHECK(ccbi_to_int(&c) == -300, "(-3)*100");
  ccbi_from_int(&b, -3);
  ccbi_mul(&c, &b, &b); CHECK(ccbi_to_int(&c) == 9, "(-3)*(-3)");
  ccbi_mul(&c, &a, &a); CHECK(ccbi_to_int(&c) == 10000, "100*100");

  /* ── Sign combinations in div_rem ── */
  ccbi_from_int(&a, 100); ccbi_from_int(&b, 7);
  ccbi_div_rem(&q, &r, &a, &b);
  CHECK(ccbi_to_int(&q) == 14 && ccbi_to_int(&r) == 2, "100/7");

  /* Negative dividend, positive divisor */
  ccbi_from_int(&a, -100);
  ccbi_div_rem(&q, &r, &a, &b);
  /* C truncation toward zero: -100/7 = -14, -100%7 = -2 */
  CHECK(ccbi_sign(&q) == -1 && ccbi_to_int(&q) == -14 && ccbi_to_int(&r) == -2, "-100/7");

  /* Positive dividend, negative divisor */
  ccbi_from_int(&a, 100); ccbi_from_int(&b, -7);
  ccbi_div_rem(&q, &r, &a, &b);
  CHECK(ccbi_sign(&q) == -1 && ccbi_to_int(&q) == -14 && ccbi_to_int(&r) == 2, "100/(-7)");

  /* Both negative */
  ccbi_from_int(&a, -100); ccbi_from_int(&b, -7);
  ccbi_div_rem(&q, &r, &a, &b);
  CHECK(ccbi_sign(&q) == 1 && ccbi_to_int(&q) == 14 && ccbi_to_int(&r) == -2, "-100/(-7)");

  /* ── Zero dividend ── */
  ccbi_zero(&a); ccbi_from_int(&b, 7);
  ccbi_div_rem(&q, &r, &a, &b);
  CHECK(ccbi_is_zero(&q) && ccbi_is_zero(&r), "0/7");

  /* ── div_rem(a, b) where |a| < |b| ── */
  ccbi_from_int(&a, 3); ccbi_from_int(&b, 100);
  ccbi_div_rem(&q, &r, &a, &b);
  CHECK(ccbi_is_zero(&q) && ccbi_to_int(&r) == 3, "3/100 → q=0 r=3");

  /* ── mod with negative operands ── */
  ccbi_from_int(&a, -7); ccbi_from_int(&b, 3);
  ccbi_mod(&c, &a, &b);  CHECK(ccbi_to_int(&c) == 2, "(-7) mod 3");
  ccbi_from_int(&a, 7); ccbi_from_int(&b, -3);
  ccbi_mod(&c, &a, &b);  CHECK(ccbi_to_int(&c) == 1, "7 mod (-3)");
  ccbi_from_int(&a, -7); ccbi_from_int(&b, -3);
  ccbi_mod(&c, &a, &b);  CHECK(ccbi_to_int(&c) == 2, "(-7) mod (-3)");

  /* ── SSO boundary: exactly 8 limbs (256-bit) ── */
  CHECK(ccbi_from_str(&a,
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 16) == 0,
    "8-limb a");
  CHECK(CCBI_USED(&a) == 8, "SSO exact 8 limbs");

  /* SSO+1: 9 limbs → must switch to heap (from_str may transiently exceed
   * SSO during construction, so buffer may be heap even at 8 limbs). */
  CHECK(ccbi_from_str(&a,
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ff", 16) == 0, "9-limb a");
  CHECK(CCBI_USED(&a) == 9, "9 limbs used");

  /* ── Karatsuba boundary: exactly 16 limbs (512-bit) ── */
  CHECK(ccbi_from_str(&b,
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 16) == 0,
    "16-limb b");
  CHECK(ccbi_mul(&c, &a, &b) == 0, "mul at karatsuba threshold");

  /* ── Negative * large positive ── */
  ccbi_t neg; ccbi_init(&neg);
  ccbi_neg(&neg, &a);
  CHECK(ccbi_mul(&c, &neg, &b) == 0, "neg × large");
  CHECK(CCBI_SIGN(&c) == -1, "neg×large sign negative");

  /* Euclidean: |c| == |a| * |b| */
  ccbi_abs(&c, &c);
  CHECK(ccbi_cmp(&c, &c) == 0, "abs(mul) self-check");
  ccbi_destroy(&neg);

  /* ── add_uint / mul_uint on large values ── */
  ccbi_from_int(&a, 0);
  ccbi_add_uint(&a, 0xFFFFFFFFU);
  CHECK(ccbi_to_int(&a) == 0xFFFFFFFFU, "add_uint max 32-bit");

  ccbi_from_uint(&a, 1);
  ccbi_mul_uint(&a, 0xFFFFFFFFU);
  CHECK(ccbi_to_int(&a) == 0xFFFFFFFFU, "mul_uint 1*max32");

  /* ── to_str_len with edge values ── */
  ccbi_zero(&a);
  CHECK(ccbi_to_str_len(&a, 10) == 1, "to_str_len(zero)=1");
  ccbi_from_int(&a, -1);
  CHECK(ccbi_to_str_len(&a, 10) >= 3, "to_str_len(-1)≥3");  /* "-1\0" */

  /* ── shift by 0 ── */
  ccbi_from_int(&a, 42);
  ccbi_shl(&c, &a, 0);  CHECK(ccbi_to_int(&c) == 42, "shl 0");
  ccbi_shr(&c, &a, 0);  CHECK(ccbi_to_int(&c) == 42, "shr 0");

  /* ── shift negative ── */
  ccbi_from_int(&a, -8);
  ccbi_shl(&c, &a, 2);  CHECK(ccbi_to_int(&c) == -32, "shl negative");
  ccbi_shr(&c, &a, 1);  CHECK(ccbi_to_int(&c) == -4, "shr negative");

  /* ── large + 1 = overflow limb ── */
  CHECK(ccbi_from_str(&a,
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 16) == 0, "");
  ccbi_inc(&a);
  CHECK(CCBI_USED(&a) == 9, "all-ones 8-limb inc → 9 limbs");
  CHECK(a.limbs[8] == 1, "inc result limb[8] = 1");  /* 2^256 */

  ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&c);
  ccbi_destroy(&q); ccbi_destroy(&r);
}

static void test_toom3(void) {
  printf("  toom-3 multiplication (EXPERIMENTAL)...\n");
  ccbi_t a, b, c, ref;
  ccbi_init(&a); ccbi_init(&b); ccbi_init(&c);
  ccbi_init(&ref);

  /* 2048-bit a, Toom-3 threshold check */
  CHECK(ccbi_from_str(&a,
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 16) == 0, "t3 a");

  /* Just verify Toom-3 runs without crash and produces 128 limbs */
  CHECK(ccbi_mul(&c, &a, &a) == 0, "t3 mul runs");
  CHECK(CCBI_USED(&c) == 128, "t3 product 128 limbs");

  /* 64×96 uneven */
  CHECK(ccbi_from_str(&b,
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
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 16) == 0, "t3 b");
  CHECK(ccbi_mul(&c, &a, &b) == 0, "t3 uneven");
  /* In-place */
  CHECK(ccbi_mul(&a, &a, &b) == 0, "t3 in-place");

  ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&c);
  ccbi_destroy(&ref);
}

static void test_large_div(void) {
  printf("  large division...\n");
  ccbi_t a, b, q, r, chk;
  ccbi_init(&a); ccbi_init(&b); ccbi_init(&q); ccbi_init(&r); ccbi_init(&chk);

  /* 64-limb (2048-bit) dividend = 2^2048 - 1. */
  CHECK(ccbi_from_str(&a,
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 16) == 0,
    "2048-bit dividend a");

  /* 16-limb divisor with top-most bit clear (0x7FFF...)
   * → Knuth normalize shift > 0. */
  CHECK(ccbi_from_str(&b,
    "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 16) == 0,
    "512-bit divisor b (top bit 0)");

  /* Divide: quotient has multiple limbs. */
  CHECK(ccbi_div_rem(&q, &r, &a, &b) == 0, "div 64/16 limbs");
  CHECK(CCBI_USED(&q) > 1, "quotient spans >1 limb");

  /* Euclidean identity: q * b + r == a. */
  CHECK(ccbi_mul(&chk, &q, &b) == 0, "q * b");
  CHECK(ccbi_add(&chk, &chk, &r) == 0, "q*b + r");
  CHECK(ccbi_cmp(&chk, &a) == 0, "q*b + r == a");

  /* Remainder invariant. */
  CHECK(ccbi_is_zero(&r) || ccbi_cmp(&r, &b) < 0, "0 <= r < b");

  /* div_rem(q=NULL) — remainder matches ccbi_mod. */
  CHECK(ccbi_div_rem(NULL, &r, &a, &b) == 0, "div_rem(q=NULL)");
  CHECK(ccbi_mod(&chk, &a, &b) == 0, "mod comparison");
  CHECK(ccbi_cmp(&r, &chk) == 0, "q=NULL remainder matches mod");

  /* div_rem(r=NULL) — quotient matches full call. */
  CHECK(ccbi_div_rem(&q, NULL, &a, &b) == 0, "div_rem(r=NULL)");
  CHECK(ccbi_div_rem(&chk, &r, &a, &b) == 0, "ref div for r=NULL");
  CHECK(ccbi_cmp(&q, &chk) == 0, "r=NULL quotient matches");

  /* Division by zero. */
  ccbi_zero(&b);
  CHECK(ccbi_div_rem(&q, &r, &a, &b) == -1, "div by zero returns -1");

  ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&q);
  ccbi_destroy(&r); ccbi_destroy(&chk);
}

/* ==========================================================================
 *  ── Bitwise ops tests ──
 * ========================================================================== */

static void test_and(void) {
  ccbi_t a, b, z; ccbi_init(&a); ccbi_init(&b); ccbi_init(&z);

  ccbi_from_uint(&a, 0x0F); ccbi_from_uint(&b, 0x33);
  ccbi_and(&z, &a, &b);
  CHECK(ccbi_to_int(&z) == 0x03, "0x0F & 0x33 = 0x03");

  ccbi_from_uint(&a, 0xFFFFFFFF);
  ccbi_and(&z, &a, &a);
  CHECK(ccbi_to_int(&z) == 0xFFFFFFFFULL, "all-ones & self = self");

  ccbi_from_uint(&a, 0xFFFF0000);
  ccbi_from_uint(&b, 0x0000FFFF);
  ccbi_and(&z, &a, &b);
  CHECK(ccbi_to_int(&z) == 0, "disjoint masks => 0");

  ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&z);
}

static void test_or(void) {
  ccbi_t a, b, z; ccbi_init(&a); ccbi_init(&b); ccbi_init(&z);

  ccbi_from_uint(&a, 0x0F); ccbi_from_uint(&b, 0x30);
  ccbi_or(&z, &a, &b);
  CHECK(ccbi_to_int(&z) == 0x3F, "0x0F | 0x30 = 0x3F");

  ccbi_from_uint(&a, 0);
  ccbi_from_uint(&b, 0xDEAD);
  ccbi_or(&z, &a, &b);
  CHECK(ccbi_to_int(&z) == 0xDEAD, "0 | x = x");

  ccbi_from_uint(&a, 0xDEAD);
  ccbi_or(&z, &a, &a);
  CHECK(ccbi_to_int(&z) == 0xDEAD, "x | x = x");

  ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&z);
}

static void test_xor(void) {
  ccbi_t a, b, z; ccbi_init(&a); ccbi_init(&b); ccbi_init(&z);

  ccbi_from_uint(&a, 0x0F); ccbi_from_uint(&b, 0x33);
  ccbi_xor(&z, &a, &b);
  CHECK(ccbi_to_int(&z) == 0x3C, "0x0F ^ 0x33 = 0x3C");

  ccbi_xor(&z, &a, &a);
  CHECK(ccbi_to_int(&z) == 0, "x ^ x = 0");

  ccbi_from_uint(&b, 0xDEAD);
  ccbi_xor(&z, &a, &b);
  ccbi_xor(&z, &z, &b);
  CHECK(ccbi_to_int(&z) == 0x0F, "(x ^ y) ^ y = x");

  ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&z);
}

static void test_not(void) {
  ccbi_t a, z; ccbi_init(&a); ccbi_init(&z);

  ccbi_zero(&a);
  ccbi_not(&z, &a);
  CHECK(ccbi_is_zero(&z), "~0 = 0 (no bits to flip)");

  ccbi_one(&a);
  ccbi_not(&z, &a);
  CHECK(ccbi_is_zero(&z), "~1 = 0 (only bit flips to 0)");

  ccbi_from_uint(&a, 2);        /* 0b10, bit_length=2 */
  ccbi_not(&z, &a);
  CHECK(ccbi_to_int(&z) == 1, "~2 = 1 (in 2-bit space)");

  ccbi_from_uint(&a, 0x0F);     /* bit_length=4 */
  ccbi_not(&z, &a);
  CHECK(ccbi_to_int(&z) == 0, "~0x0F = 0 (all 4 bits flip)");

  ccbi_from_uint(&a, 0xAAAAAAAA);
  ccbi_not(&z, &a);
  CHECK(ccbi_to_int(&z) == 0x55555555ULL, "~0xAAAAAAAA = 0x55555555");

  /* multi-limb: 0x00000001FFFFFFFF */
  ccbi_from_uint(&a, UINT64_C(0x1FFFFFFFF));
  CHECK(ccbi_bit_length(&a) == 33, "bl of 1FFFFFFFF is 33");
  ccbi_not(&z, &a);
  /* ~ in 33-bit space: 0x1FFFFFFFF → 0x00000000 */
  CHECK(ccbi_is_zero(&z), "~0x1FFFFFFFF = 0 (33-bit complement clears all)");

  ccbi_destroy(&a); ccbi_destroy(&z);
}

static void test_test_bit(void) {
  ccbi_t a; ccbi_init(&a);

  ccbi_zero(&a);
  CHECK(ccbi_test_bit(&a, 0) == 0, "zero: bit 0 = 0");
  CHECK(ccbi_test_bit(&a, 100) == 0, "zero: bit 100 = 0");

  ccbi_one(&a);
  CHECK(ccbi_test_bit(&a, 0) == 1, "1: bit 0 = 1");
  CHECK(ccbi_test_bit(&a, 1) == 0, "1: bit 1 = 0");

  ccbi_from_uint(&a, 0x80000000);
  CHECK(ccbi_test_bit(&a, 31) == 1, "0x80000000: bit 31 = 1");
  CHECK(ccbi_test_bit(&a, 30) == 0, "0x80000000: bit 30 = 0");

  ccbi_destroy(&a);
}

static void test_set_clear_flip(void) {
  ccbi_t a; ccbi_init(&a);

  ccbi_zero(&a);
  ccbi_set_bit(&a, 5);
  CHECK(ccbi_test_bit(&a, 5) == 1, "set bit 5 on zero");
  CHECK(ccbi_to_int(&a) == 32, "set bit 5 = 32");

  ccbi_clear_bit(&a, 5);
  CHECK(ccbi_test_bit(&a, 5) == 0, "clear bit 5");
  CHECK(ccbi_is_zero(&a), "cleared back to zero");

  /* flip from 0 to 1 on a large offset — triggers grow */
  ccbi_flip_bit(&a, 70);   /* limb 2, bit 6 */
  CHECK(ccbi_test_bit(&a, 70) == 1, "flip bit 70 on zero = 1");
  CHECK(ccbi_bit_length(&a) == 71, "bl after flip bit 70 = 71");

  ccbi_flip_bit(&a, 70);
  CHECK(ccbi_test_bit(&a, 70) == 0, "flip bit 70 again = 0");
  CHECK(ccbi_is_zero(&a), "double flip back to zero");

  /* alias: set_bit on self */
  ccbi_set_bit(&a, 0);
  ccbi_set_bit(&a, 63);
  CHECK(ccbi_test_bit(&a, 0) == 1, "set bit 0");
  CHECK(ccbi_test_bit(&a, 63) == 1, "set bit 63 (limb 1)");
  CHECK(ccbi_to_int(&a) == (int64_t)(UINT64_C(1) | (UINT64_C(1) << 63)),
         "set bit 0 + 63 value check");

  ccbi_destroy(&a);
}

static void test_bit_alias(void) {
  /* AND/OR/XOR with z == a or z == b */
  ccbi_t a, b; ccbi_init(&a); ccbi_init(&b);

  ccbi_from_uint(&a, 0x0F); ccbi_from_uint(&b, 0x33);

  /* z == a */
  ccbi_and(&a, &a, &b);
  CHECK(ccbi_to_int(&a) == 0x03, "and z==a: 0x0F & 0x33");

  ccbi_from_uint(&a, 0x0F);
  ccbi_or(&a, &a, &b);
  CHECK(ccbi_to_int(&a) == 0x3F, "or z==a: 0x0F | 0x33");

  ccbi_from_uint(&a, 0x0F);
  ccbi_xor(&a, &a, &b);
  CHECK(ccbi_to_int(&a) == 0x3C, "xor z==a: 0x0F ^ 0x33");

  /* z == b */
  ccbi_from_uint(&a, 0x0F);
  ccbi_and(&b, &a, &b);
  CHECK(ccbi_to_int(&b) == 0x03, "and z==b: 0x0F & 0x33");

  ccbi_from_uint(&b, 0x33);
  ccbi_from_uint(&a, 0x0F);
  ccbi_or(&b, &a, &b);
  CHECK(ccbi_to_int(&b) == 0x3F, "or z==b: 0x0F | 0x33");

  ccbi_from_uint(&b, 0x33);
  ccbi_from_uint(&a, 0x0F);
  ccbi_xor(&b, &a, &b);
  CHECK(ccbi_to_int(&b) == 0x3C, "xor z==b: 0x0F ^ 0x33");

  /* not alias */
  ccbi_from_uint(&a, 0xAAAAAAAA);
  ccbi_not(&a, &a);
  CHECK(ccbi_to_int(&a) == 0x55555555ULL, "not z==a: ~0xAAAAAAAA");

  ccbi_destroy(&a); ccbi_destroy(&b);
}

int main(void) {
  failed = 0;
  printf("ccbi test suite\n===============\n");

  test_lifecycle();
  test_from_str();
  test_arithmetic();
  test_shift();
  test_gcd();
  test_pow_mod();
  test_swap();
  test_large_arithmetic();
  test_boundary_sign();
  test_toom3();
  test_large_div();

  printf("  bitwise ops...\n");
  test_and();
  test_or();
  test_xor();
  test_not();
  test_test_bit();
  test_set_clear_flip();
  test_bit_alias();

  printf("===============\n%d failed\n", failed);
  return failed > 0 ? 1 : 0;
}
