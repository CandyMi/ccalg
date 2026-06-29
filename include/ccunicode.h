/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  UTF-8 ↔ UCS-4 codec.  Self-contained, zero external dependencies.
**
**  Compliant with Unicode 17.0 / RFC 3629.
**    - 1–4 byte sequences, overlong rejection, surrogate rejection
**    - ASCII fast path, 256-byte lookup table, unrolled switch
**    - Branchless encoder, fall-through decoder
*/
#ifndef CCUNICODE_H
#define CCUNICODE_H

/*
 * Branch prediction hint — marks error/edge paths that are almost never
 * taken in valid-UTF-8 input.  Compiler lays out the hot (taken) path
 * inline and moves the error handling out of line.
 */
#if defined(__GNUC__) || defined(__clang__)
  #define CCUNICODE_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
  #define CCUNICODE_UNLIKELY(x) (x)
#endif

/*
 * Portable fallthrough hint — suppresses -Wimplicit-fallthrough (GCC/Clang)
 * and C5260 (MSVC).
 */
#if defined(_MSC_VER)
  #define CCUNICODE_FALLTHROUGH __fallthrough
#else
  #define CCUNICODE_FALLTHROUGH /* fall through */
#endif

#if defined(__cplusplus) && __cplusplus >= 201703L
  #define CCUNICODE_NOEXCEPT noexcept
#else
  #define CCUNICODE_NOEXCEPT
#endif

#define CCUNICODE_INLINE static inline

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Lookup tables indexed by (bytes - 1). */
static const uint8_t  lead_mark[4]  = { 0x00, 0xC0, 0xE0, 0xF0 };
static const uint8_t  lead_shift[4] = {    0,    6,   12,   18 };
static const uint32_t cp_ranges[5]  = { 0, 0, 0x80, 0x800, 0x10000 };

/* Tables for branchless multi-byte assembly: indexed by (bytes - 1). */
static const uint8_t byte_sh[4][4] = {
  { 0,  0,  0,  0},  /* ASCII: unused (goes through fast path) */
  { 6,  0,  0,  0},  /* 2-byte: lead<<6 | cont1 */
  {12,  6,  0,  0},  /* 3-byte: lead<<12 | cont1<<6 | cont2 */
  {18, 12,  6,  0},  /* 4-byte: lead<<18 | cont1<<12 | cont2<<6 | cont3 */
};
static const uint8_t byte_ms[4][4] = {
  {0x7F, 0x00, 0x00, 0x00},
  {0x1F, 0x3F, 0x00, 0x00},
  {0x0F, 0x3F, 0x3F, 0x00},
  {0x07, 0x3F, 0x3F, 0x3F},
};
/*
  * Lookup table: number of bytes in the sequence for each possible first byte.
  * 0   = invalid first byte (continuation bytes 0x80-0xBF,
  *                           overlong 2-byte leaders 0xC0-0xC1,
  *                           out-of-range leaders 0xF5-0xFF).
  * 1   = ASCII (handled above — included for completeness).
  * 2–4 = valid multi-byte sequence length.
  */
static const uint8_t seq_len[256] = {
  1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, /* 00-1F */
  1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, /* 20-3F */
  1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, /* 40-5F */
  1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, /* 60-7F */
  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, /* 80-9F */
  0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, /* A0-BF */
  0,0,                                                                /* C0-C1: overlong */
  2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,     /* C2-DF */
  3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3,                                   /* E0-EF */
  4,4,4,4,4,                                                          /* F0-F4 */
  0,0,0,0,0,0,0,0, 0,0,0,                                             /* F5-FF: >U+10FFFF */
};

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal: validate one UTF-8 sequence and (optionally) return its codepoint.
 *
 * Shared by ccunicode_to_codepoint (code != NULL) and ccunicode_verify
 * (code == NULL).  The assembled codepoint is always computed for
 * range/surrogate checks; writing it back is conditional on `code`.
 *
 * Returns bytes (1–4) on success, 0 on failure.
 */
CCUNICODE_INLINE
int ccunicode_validate_seq(const uint8_t *p, size_t len, uint32_t *code)
{
  if (p[0] < 0x80) {
    if (code) *code = p[0];
    return 1;
  }
  int bytes = seq_len[p[0]];
  if (CCUNICODE_UNLIKELY(bytes < 2)) return 0;
  if (CCUNICODE_UNLIKELY(len < (size_t)bytes)) return 0;

  /* Safe reads: predictable ternary → cmov/csel, no branch */
  uint32_t b1 = p[1];
  uint32_t b2 = (bytes >= 3) ? (uint32_t)p[2] : 0;
  uint32_t b3 = (bytes >= 4) ? (uint32_t)p[3] : 0;

  /* Continuation-byte validity: arithmetic masking, no branch */
  int bad  = ((b1 ^ 0x80) & 0xC0);
  bad |= (((b2 ^ 0x80) & 0xC0) & -(bytes >= 3));
  bad |= (((b3 ^ 0x80) & 0xC0) & -(bytes >= 4));
  if (CCUNICODE_UNLIKELY(bad)) return 0;

  /* Table-driven assembly */
  int idx = bytes - 1;
  uint32_t v = ((uint32_t)(p[0] & byte_ms[idx][0]) << byte_sh[idx][0])
             | ((uint32_t)(b1 & byte_ms[idx][1]) << byte_sh[idx][1])
             | ((uint32_t)(b2 & byte_ms[idx][2]) << byte_sh[idx][2])
             | ((uint32_t)(b3 & byte_ms[idx][3]) << byte_sh[idx][3]);

  /* Combined range/surrogate check */
  if (CCUNICODE_UNLIKELY((v < cp_ranges[bytes]) | (v > 0x10FFFF)
      | ((uint32_t)(v - 0xD800) < 0x0800u))) return 0;

  if (code) *code = v;
  return bytes;
}

/**
 * Convert one UTF-8 character to its UCS-4 code point.
 *
 * @param str Pointer to UTF-8 encoded byte sequence (not necessarily null-terminated).
 * @param len Number of bytes available in the buffer (must be > 0).
 * @param val Pointer to a uint32_t variable that will receive the code point on success.
 * @return    Number of UTF-8 bytes consumed (1–4) on success,
 *            0 on failure (invalid encoding, incomplete sequence, or invalid code point).
 */
CCUNICODE_INLINE
int ccunicode_to_codepoint(const char *str, size_t len, uint32_t *val) CCUNICODE_NOEXCEPT
{
  if (CCUNICODE_UNLIKELY(!str || len == 0 || !val)) return 0;
  return ccunicode_validate_seq((const uint8_t *)str, len, val);
}

/**
 * Encode a UCS-4 code point into UTF-8.
 *
 * Performance-oriented implementation:
 *     - Branchless byte-count computation (compiler emits SETcc/CMOV, not jumps).
 *     - Table-driven first byte eliminates per-case prefix logic.
 *     - Fall-through switch unrolls continuation-byte writes with a single shift
 *       chain — same pattern as the decoder.
 *
 * @param val  UCS-4 code point to encode.
 * @param str  Destination buffer (at least 4 bytes).
 * @param len  [out] Number of UTF-8 bytes written (1–4).
 * @return     true on success, false for invalid code points
 *             (surrogates, > U+10FFFF, or null parameters).
 */
CCUNICODE_INLINE
bool ccunicode_from_codepoint(uint32_t val, char str[4], int *len) CCUNICODE_NOEXCEPT
{
  if (CCUNICODE_UNLIKELY(!str || !len)) return false;

  /* Unicode range caps at U+10FFFF. */
  if (CCUNICODE_UNLIKELY(val > 0x10FFFF)) return false;
  /* Surrogates are not valid Unicode scalar values. */
  if (CCUNICODE_UNLIKELY(val >= 0xD800 && val <= 0xDFFF)) return false;

  uint8_t *p = (uint8_t *)str;

  /*
   * Branchless byte count: 1 + (val > 0x7F) + (val > 0x7FF) + (val > 0xFFFF).
   * The compiler lowers this to SETcc/arithmetic — no mispredict penalty.
   */
  int bytes = 1 + (val > 0x7F) + (val > 0x7FF) + (val > 0xFFFF);

  int idx = bytes - 1;
  p[0] = (uint8_t)(lead_mark[idx] | (val >> lead_shift[idx]));

  /*
   * Continuation bytes via fall-through switch — same unrolled pattern
   * as the decoder.  Writes LSB first, shifts val right 6 bits at each
   * step so the next byte picks up the next 6-bit window.
   */
  switch (bytes) {
    case 4:
      p[3] = (uint8_t)(0x80 | (val & 0x3F));
      val >>= 6;
      CCUNICODE_FALLTHROUGH;
    case 3:
      p[2] = (uint8_t)(0x80 | (val & 0x3F));
      val >>= 6;
      CCUNICODE_FALLTHROUGH;
    case 2:
      p[1] = (uint8_t)(0x80 | (val & 0x3F));
      break;
  }

  *len = bytes;
  return true;
}

/**
 * Validate that a buffer contains well-formed UTF-8.
 *
 * Scans the input linearly, checking each sequence for:
 *   - valid lead byte
 *   - correctly formatted continuation bytes
 *   - no overlong encodings
 *   - no surrogate halves (U+D800–U+DFFF)
 *   - no code points beyond U+10FFFF
 *
 * @param str  Input buffer (not necessarily null-terminated).
 * @param len  Number of bytes in the buffer.
 * @return     true if the entire buffer is valid UTF-8, false otherwise.
 */
CCUNICODE_INLINE
bool ccunicode_verify(const char *str, size_t len) CCUNICODE_NOEXCEPT
{
  size_t offset = 0;

  /* 4× unrolled loop: process 4 sequences per iteration.
   * All 4 decoded before any error check — eliminates 3 of 4 branches
   * from the hot path.  A single multiply-and-test catches any failure. */
  while (len - offset >= 16) {
    int n1 = ccunicode_validate_seq((const uint8_t *)str + offset,
                                    len - offset, NULL);
    int n2 = ccunicode_validate_seq((const uint8_t *)str + offset + n1,
                                    len - offset - n1, NULL);
    int n3 = ccunicode_validate_seq((const uint8_t *)str + offset + n1 + n2,
                                    len - offset - n1 - n2, NULL);
    int n4 = ccunicode_validate_seq((const uint8_t *)str + offset + n1 + n2 + n3,
                                    len - offset - n1 - n2 - n3, NULL);
    if (CCUNICODE_UNLIKELY(!(n1 * n2 * n3 * n4))) return false;
    offset += (size_t)(n1 + n2 + n3 + n4);
  }

  /* Tail: process remaining bytes one by one */
  const uint8_t *p = (const uint8_t *)str + offset;
  size_t remain = len - offset;
  while (remain) {
    if (p[0] < 0x80) {
      p++; remain--;            /* ASCII fast path: inline, no function call */
      continue;
    }
    int n = ccunicode_validate_seq(p, remain, NULL);
    if (CCUNICODE_UNLIKELY(!n)) return false;
    p += n; remain -= (size_t)n;
  }

  return true;
}

#ifdef __cplusplus
}
#endif

#endif /* CCUNICODE_H */