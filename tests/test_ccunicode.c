/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Test suite for ccunicode (UTF-8 ↔ UCS-4 codec)
**  Build:  gcc -std=c99 -Wall -Wextra -o test_ccunicode test_ccunicode.c && ./test_ccunicode
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../include/ccunicode.h"

/* ── test harness ─────────────────────────────────────────────────────── */
static int passed, failed;
#define TEST(name) static void test_##name(void)
#define ASSERT(cond) do { \
  if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failed++; } \
  else passed++; \
} while(0)
#define RUN(name) do { printf("  %s\n", #name); test_##name(); } while(0)

/* ── helpers ──────────────────────────────────────────────────────────── */

/* Encode a codepoint, then decode back and verify round-trip. */
static int roundtrip(uint32_t cp) {
  char buf[8];
  int n;
  if (!ccunicode_from_codepoint(cp, buf, &n)) return -1;
  if (n < 1 || n > 4) return -2;
  uint32_t out;
  int m = ccunicode_to_codepoint(buf, n, &out);
  if (m != n) return -3;
  if (out != cp) return -4;
  return 0;
}

/* ── basic ASCII ──────────────────────────────────────────────────────── */

TEST(ascii_null) {
  uint32_t cp;
  /* null terminator */
  int n = ccunicode_to_codepoint("\x00", 1, &cp);
  ASSERT(n == 1);
  ASSERT(cp == 0x00);
}

TEST(ascii_basic) {
  uint32_t cp;
  /* 'A' = U+0041 */
  int n = ccunicode_to_codepoint("A", 1, &cp);
  ASSERT(n == 1);
  ASSERT(cp == 0x41);
}

TEST(ascii_delim) {
  uint32_t cp;
  /* 0x7F is the highest ASCII value */
  int n = ccunicode_to_codepoint("\x7F", 1, &cp);
  ASSERT(n == 1);
  ASSERT(cp == 0x7F);
}

/* ── multi-byte encode/decode ─────────────────────────────────────────── */

TEST(two_byte_min) {
  /* U+0080 — minimum 2-byte sequence: 0xC2 0x80 */
  uint32_t cp;
  int n = ccunicode_to_codepoint("\xC2\x80", 2, &cp);
  ASSERT(n == 2);
  ASSERT(cp == 0x80);
}

TEST(two_byte_max) {
  /* U+07FF — maximum 2-byte sequence: 0xDF 0xBF */
  uint32_t cp;
  int n = ccunicode_to_codepoint("\xDF\xBF", 2, &cp);
  ASSERT(n == 2);
  ASSERT(cp == 0x7FF);
}

TEST(three_byte_min) {
  /* U+0800 — minimum 3-byte sequence: 0xE0 0xA0 0x80 */
  uint32_t cp;
  int n = ccunicode_to_codepoint("\xE0\xA0\x80", 3, &cp);
  ASSERT(n == 3);
  ASSERT(cp == 0x800);
}

TEST(three_byte_max) {
  /* U+FFFF — maximum 3-byte sequence: 0xEF 0xBF 0xBF */
  uint32_t cp;
  int n = ccunicode_to_codepoint("\xEF\xBF\xBF", 3, &cp);
  ASSERT(n == 3);
  ASSERT(cp == 0xFFFF);
}

TEST(four_byte_min) {
  /* U+10000 — minimum 4-byte sequence: 0xF0 0x90 0x80 0x80 */
  uint32_t cp;
  int n = ccunicode_to_codepoint("\xF0\x90\x80\x80", 4, &cp);
  ASSERT(n == 4);
  ASSERT(cp == 0x10000);
}

TEST(four_byte_max) {
  /* U+10FFFF — maximum valid 4-byte: 0xF4 0x8F 0xBF 0xBF */
  uint32_t cp;
  int n = ccunicode_to_codepoint("\xF4\x8F\xBF\xBF", 4, &cp);
  ASSERT(n == 4);
  ASSERT(cp == 0x10FFFF);
}

/* ── from_codepoint (encode) ──────────────────────────────────────────── */

TEST(encode_ascii) {
  char buf[8];
  int n;
  ASSERT(ccunicode_from_codepoint(0x41, buf, &n));
  ASSERT(n == 1);
  ASSERT((unsigned char)buf[0] == 0x41);
}

TEST(encode_two_byte) {
  char buf[8];
  int n;
  ASSERT(ccunicode_from_codepoint(0x80, buf, &n));
  ASSERT(n == 2);
  ASSERT((unsigned char)buf[0] == 0xC2);
  ASSERT((unsigned char)buf[1] == 0x80);
}

TEST(encode_three_byte) {
  char buf[8];
  int n;
  ASSERT(ccunicode_from_codepoint(0x800, buf, &n));
  ASSERT(n == 3);
  ASSERT((unsigned char)buf[0] == 0xE0);
  ASSERT((unsigned char)buf[1] == 0xA0);
  ASSERT((unsigned char)buf[2] == 0x80);
}

TEST(encode_four_byte) {
  char buf[8];
  int n;
  ASSERT(ccunicode_from_codepoint(0x10000, buf, &n));
  ASSERT(n == 4);
  ASSERT((unsigned char)buf[0] == 0xF0);
  ASSERT((unsigned char)buf[1] == 0x90);
  ASSERT((unsigned char)buf[2] == 0x80);
  ASSERT((unsigned char)buf[3] == 0x80);
}

TEST(encode_emoji) {
  /* U+1F600 = 😀 */
  char buf[8];
  int n;
  ASSERT(ccunicode_from_codepoint(0x1F600, buf, &n));
  ASSERT(n == 4);
  uint32_t cp;
  int m = ccunicode_to_codepoint(buf, n, &cp);
  ASSERT(m == 4);
  ASSERT(cp == 0x1F600);
}

/* ── round-trip ───────────────────────────────────────────────────────── */

TEST(roundtrip_boundaries) {
  /* Test key boundary values across all ranges */
  ASSERT(roundtrip(0x00) == 0);
  ASSERT(roundtrip(0x41) == 0);
  ASSERT(roundtrip(0x7F) == 0);
  ASSERT(roundtrip(0x80) == 0);
  ASSERT(roundtrip(0x7FF) == 0);
  ASSERT(roundtrip(0x800) == 0);
  ASSERT(roundtrip(0xFFFF) == 0);
  ASSERT(roundtrip(0x10000) == 0);
  ASSERT(roundtrip(0x10FFFF) == 0);
}

TEST(roundtrip_cjk) {
  /* U+4E2D = 中, U+6587 = 文, U+5B57 = 字 */
  ASSERT(roundtrip(0x4E2D) == 0);
  ASSERT(roundtrip(0x6587) == 0);
  ASSERT(roundtrip(0x5B57) == 0);
}

TEST(roundtrip_musical) {
  /* U+1D11E = 𝄞 musical symbol G clef */
  ASSERT(roundtrip(0x1D11E) == 0);
}

/* ── error handling ───────────────────────────────────────────────────── */

TEST(reject_null_str) {
  uint32_t cp;
  int n = ccunicode_to_codepoint(NULL, 4, &cp);
  ASSERT(n == 0);
}

TEST(reject_null_val) {
  int n = ccunicode_to_codepoint("A", 1, NULL);
  ASSERT(n == 0);
}

TEST(reject_zero_len) {
  uint32_t cp;
  int n = ccunicode_to_codepoint("A", 0, &cp);
  ASSERT(n == 0);
}

TEST(reject_encode_null_str) {
  int n;
  ASSERT(!ccunicode_from_codepoint(0x41, NULL, &n));
}

TEST(reject_encode_null_len) {
  char buf[8];
  ASSERT(!ccunicode_from_codepoint(0x41, buf, NULL));
}

TEST(reject_overlong_2byte) {
  /* 0xC0 0x80 would decode to U+0000 but is overlong */
  uint32_t cp;
  int n = ccunicode_to_codepoint("\xC0\x80", 2, &cp);
  ASSERT(n == 0);
}

TEST(reject_overlong_3byte) {
  /* 0xE0 0x80 0x80 → U+0000 overlong 3-byte */
  uint32_t cp;
  int n = ccunicode_to_codepoint("\xE0\x80\x80", 3, &cp);
  ASSERT(n == 0);
}

TEST(reject_overlong_4byte) {
  /* 0xF0 0x80 0x80 0x80 → U+0000 overlong 4-byte */
  uint32_t cp;
  int n = ccunicode_to_codepoint("\xF0\x80\x80\x80", 4, &cp);
  ASSERT(n == 0);
}

TEST(reject_surrogate_high) {
  /* U+D800 encoded as UTF-8: 0xED 0xA0 0x80 */
  uint32_t cp;
  int n = ccunicode_to_codepoint("\xED\xA0\x80", 3, &cp);
  ASSERT(n == 0);
}

TEST(reject_surrogate_low) {
  /* U+DFFF encoded as UTF-8: 0xED 0xBF 0xBF */
  uint32_t cp;
  int n = ccunicode_to_codepoint("\xED\xBF\xBF", 3, &cp);
  ASSERT(n == 0);
}

TEST(reject_encode_surrogate) {
  char buf[8];
  int n;
  ASSERT(!ccunicode_from_codepoint(0xD800, buf, &n));
  ASSERT(!ccunicode_from_codepoint(0xDFFF, buf, &n));
}

TEST(reject_encode_above_max) {
  char buf[8];
  int n;
  ASSERT(!ccunicode_from_codepoint(0x110000, buf, &n));
  ASSERT(!ccunicode_from_codepoint(0xFFFFFFFF, buf, &n));
}

TEST(reject_decode_above_max) {
  /* 0xF4 0x90 0x80 0x80 → U+110000 > U+10FFFF */
  uint32_t cp;
  int n = ccunicode_to_codepoint("\xF4\x90\x80\x80", 4, &cp);
  ASSERT(n == 0);
}

TEST(reject_invalid_first_byte) {
  uint32_t cp;
  /* 0xFF is not a valid lead byte */
  int n = ccunicode_to_codepoint("\xFF\x80\x80\x80", 4, &cp);
  ASSERT(n == 0);
  /* 0xF8 is out of range */
  n = ccunicode_to_codepoint("\xF8\x80\x80\x80", 4, &cp);
  ASSERT(n == 0);
}

TEST(reject_continuation_missing) {
  uint32_t cp;
  /* incomplete 2-byte sequence (missing second byte) */
  int n = ccunicode_to_codepoint("\xC2", 1, &cp);
  ASSERT(n == 0);
}

TEST(reject_bad_continuation) {
  uint32_t cp;
  /* continuation byte must start with 10xxxxxx; 0xC0 is 11000000 */
  int n = ccunicode_to_codepoint("\xC2\xC0", 2, &cp);
  ASSERT(n == 0);
}

/* ── multiple codepoints (successive decode) ──────────────────────────── */

TEST(decode_ascii_string) {
  const char *s = "Hello";
  uint32_t cp;
  int offset = 0;
  int len = (int)strlen(s);
  int n = ccunicode_to_codepoint(s + offset, len - offset, &cp);
  ASSERT(n == 1); ASSERT(cp == 'H'); offset += n;
  n = ccunicode_to_codepoint(s + offset, len - offset, &cp);
  ASSERT(n == 1); ASSERT(cp == 'e'); offset += n;
  n = ccunicode_to_codepoint(s + offset, len - offset, &cp);
  ASSERT(n == 1); ASSERT(cp == 'l'); offset += n;
  n = ccunicode_to_codepoint(s + offset, len - offset, &cp);
  ASSERT(n == 1); ASSERT(cp == 'l'); offset += n;
  n = ccunicode_to_codepoint(s + offset, len - offset, &cp);
  ASSERT(n == 1); ASSERT(cp == 'o');
}

TEST(decode_mixed_string) {
  /* "A中😀" = U+0041 + U+4E2D + U+1F600 */
  const unsigned char utf8[] = {
    0x41,                   /* 'A' */
    0xE4, 0xB8, 0xAD,       /* 中 */
    0xF0, 0x9F, 0x98, 0x80, /* 😀 */
    0x00
  };
  uint32_t cp;
  int offset = 0;
  int len = (int)strlen((const char *)utf8) + 1; /* include null terminator */
  int n;
  n = ccunicode_to_codepoint((const char *)utf8 + offset, len - offset, &cp);
  ASSERT(n == 1); ASSERT(cp == 0x41); offset += n;
  n = ccunicode_to_codepoint((const char *)utf8 + offset, len - offset, &cp);
  ASSERT(n == 3); ASSERT(cp == 0x4E2D); offset += n;
  n = ccunicode_to_codepoint((const char *)utf8 + offset, len - offset, &cp);
  ASSERT(n == 4); ASSERT(cp == 0x1F600);
}

/* ── main ─────────────────────────────────────────────────────────────── */

int main(void) {
  printf("=== test_ccunicode ===\n");

  RUN(ascii_null);
  RUN(ascii_basic);
  RUN(ascii_delim);

  RUN(two_byte_min);
  RUN(two_byte_max);
  RUN(three_byte_min);
  RUN(three_byte_max);
  RUN(four_byte_min);
  RUN(four_byte_max);

  RUN(encode_ascii);
  RUN(encode_two_byte);
  RUN(encode_three_byte);
  RUN(encode_four_byte);
  RUN(encode_emoji);

  RUN(roundtrip_boundaries);
  RUN(roundtrip_cjk);
  RUN(roundtrip_musical);

  RUN(reject_null_str);
  RUN(reject_null_val);
  RUN(reject_zero_len);
  RUN(reject_encode_null_str);
  RUN(reject_encode_null_len);
  RUN(reject_overlong_2byte);
  RUN(reject_overlong_3byte);
  RUN(reject_overlong_4byte);
  RUN(reject_surrogate_high);
  RUN(reject_surrogate_low);
  RUN(reject_encode_surrogate);
  RUN(reject_encode_above_max);
  RUN(reject_decode_above_max);
  RUN(reject_invalid_first_byte);
  RUN(reject_continuation_missing);
  RUN(reject_bad_continuation);

  RUN(decode_ascii_string);
  RUN(decode_mixed_string);

  printf("\n%d passed, %d failed\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
