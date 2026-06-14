/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Test suite for ccunicode_verify
**  Build:  gcc -std=c99 -Wall -Wextra -o test_ccunicode_verify test_ccunicode_verify.c && ./test_ccunicode_verify
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

/* ── valid inputs ─────────────────────────────────────────────────────── */

TEST(verify_empty) {
  ASSERT(ccunicode_verify("", 0) == true);
}

TEST(verify_ascii) {
  ASSERT(ccunicode_verify("Hello, World!", 13) == true);
}

TEST(verify_null_byte) {
  /* embedded null is valid UTF-8 (U+0000) */
  const char s[] = { 'A', '\0', 'B' };
  ASSERT(ccunicode_verify(s, 3) == true);
}

TEST(verify_ascii_7f) {
  /* 0x7F is DEL, valid ASCII */
  ASSERT(ccunicode_verify("\x7F", 1) == true);
}

TEST(verify_2byte) {
  /* U+00A9 © = 0xC2 0xA9 */
  ASSERT(ccunicode_verify("\xC2\xA9", 2) == true);
}

TEST(verify_3byte) {
  /* U+4E2D 中 = 0xE4 0xB8 0xAD */
  ASSERT(ccunicode_verify("\xE4\xB8\xAD", 3) == true);
}

TEST(verify_4byte) {
  /* U+1F600 😀 = 0xF0 0x9F 0x98 0x80 */
  ASSERT(ccunicode_verify("\xF0\x9F\x98\x80", 4) == true);
}

TEST(verify_max_valid) {
  /* U+10FFFF = 0xF4 0x8F 0xBF 0xBF */
  ASSERT(ccunicode_verify("\xF4\x8F\xBF\xBF", 4) == true);
}

TEST(verify_mixed) {
  /* "A中😀" mixed ASCII + CJK + emoji */
  const unsigned char utf8[] = {
    0x41, 0xE4, 0xB8, 0xAD, 0xF0, 0x9F, 0x98, 0x80
  };
  ASSERT(ccunicode_verify((const char *)utf8, 8) == true);
}

TEST(verify_long_ascii) {
  /* 1000 bytes of ASCII — exercises the 4× unrolled loop */
  char buf[1000];
  memset(buf, 'x', sizeof(buf));
  ASSERT(ccunicode_verify(buf, sizeof(buf)) == true);
}

TEST(verify_long_mixed) {
  /* 1000 bytes of mixed CJK — exercises the 4× unrolled loop */
  unsigned char buf[1000];
  /* fill with "中" = E4 B8 AD (3 bytes) */
  for (int i = 0; i + 3 <= 1000; i += 3) {
    buf[i] = 0xE4; buf[i+1] = 0xB8; buf[i+2] = 0xAD;
  }
  ASSERT(ccunicode_verify((const char *)buf, 999) == true);
}

/* ── invalid inputs ───────────────────────────────────────────────────── */

TEST(reject_overlong_2byte) {
  /* 0xC0 0x80 would encode U+0000 as 2 bytes */
  ASSERT(ccunicode_verify("\xC0\x80", 2) == false);
}

TEST(reject_overlong_3byte) {
  /* 0xE0 0x80 0x80 would encode U+0000 as 3 bytes */
  ASSERT(ccunicode_verify("\xE0\x80\x80", 3) == false);
}

TEST(reject_overlong_4byte) {
  /* 0xF0 0x80 0x80 0x80 would encode U+0000 as 4 bytes */
  ASSERT(ccunicode_verify("\xF0\x80\x80\x80", 4) == false);
}

TEST(reject_surrogate_high) {
  /* U+D800 = 0xED 0xA0 0x80 */
  ASSERT(ccunicode_verify("\xED\xA0\x80", 3) == false);
}

TEST(reject_surrogate_low) {
  /* U+DFFF = 0xED 0xBF 0xBF */
  ASSERT(ccunicode_verify("\xED\xBF\xBF", 3) == false);
}

TEST(reject_above_max) {
  /* U+110000 encoded as UTF-8: 0xF4 0x90 0x80 0x80 */
  ASSERT(ccunicode_verify("\xF4\x90\x80\x80", 4) == false);
}

TEST(reject_invalid_lead) {
  /* 0xFF is not a valid lead byte */
  ASSERT(ccunicode_verify("\xFF", 1) == false);
  ASSERT(ccunicode_verify("\xF8\x80\x80\x80", 4) == false);
  ASSERT(ccunicode_verify("\xFE\x80\x80\x80", 4) == false);
}

TEST(reject_lone_continuation) {
  /* continuation byte without a lead */
  ASSERT(ccunicode_verify("\x80", 1) == false);
  ASSERT(ccunicode_verify("\xBF", 1) == false);
}

TEST(reject_missing_continuation) {
  /* 2-byte lead without continuation */
  ASSERT(ccunicode_verify("\xC2", 1) == false);
}

TEST(reject_bad_continuation) {
  /* continuation byte must be 10xxxxxx, 0xC0 is 11000000 */
  ASSERT(ccunicode_verify("\xC2\xC0", 2) == false);
}

TEST(reject_truncated_3byte) {
  /* 3-byte lead with only 1 continuation */
  ASSERT(ccunicode_verify("\xE4\xB8", 2) == false);
}

TEST(reject_truncated_4byte) {
  /* 4-byte lead with only 2 continuations */
  ASSERT(ccunicode_verify("\xF0\x9F\x98", 3) == false);
}

TEST(reject_mixed_valid_until_invalid) {
  /* valid text followed by invalid */
  const unsigned char utf8[] = {
    0x41, 0xE4, 0xB8, 0xAD,  /* A中 */
    0xC0, 0x80               /* overlong null — invalid */
  };
  ASSERT(ccunicode_verify((const char *)utf8, 7) == false);
}

/* ── main ─────────────────────────────────────────────────────────────── */

int main(void) {
  printf("=== test_ccunicode_verify ===\n");

  RUN(verify_empty);
  RUN(verify_ascii);
  RUN(verify_null_byte);
  RUN(verify_ascii_7f);
  RUN(verify_2byte);
  RUN(verify_3byte);
  RUN(verify_4byte);
  RUN(verify_max_valid);
  RUN(verify_mixed);
  RUN(verify_long_ascii);
  RUN(verify_long_mixed);

  RUN(reject_overlong_2byte);
  RUN(reject_overlong_3byte);
  RUN(reject_overlong_4byte);
  RUN(reject_surrogate_high);
  RUN(reject_surrogate_low);
  RUN(reject_above_max);
  RUN(reject_invalid_lead);
  RUN(reject_lone_continuation);
  RUN(reject_missing_continuation);
  RUN(reject_bad_continuation);
  RUN(reject_truncated_3byte);
  RUN(reject_truncated_4byte);
  RUN(reject_mixed_valid_until_invalid);

  printf("\n%d passed, %d failed\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
