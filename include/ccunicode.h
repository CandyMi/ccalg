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
  #define ccunicode_unlikely(x) __builtin_expect(!!(x), 0)
#else
  #define ccunicode_unlikely(x) (x)
#endif

/*
 * Portable fallthrough hint — suppresses -Wimplicit-fallthrough (GCC/Clang)
 * and C5260 (MSVC).
 */
#if defined(_MSC_VER)
  #define ccunicode_fallthrough __fallthrough
#else
  #define ccunicode_fallthrough /* fall through */
#endif

#define CCUNICODE_INLINE static inline

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Lookup tables indexed by (bytes - 1). */
static const uint8_t  lead_mark[4]  = { 0x00, 0xC0, 0xE0, 0xF0 };
static const uint8_t  lead_shift[4] = {    0,    6,   12,   18 };
static const uint32_t cp_ranges[5]  = { 0, 0, 0x80, 0x800, 0x10000 };
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

/**
 * Convert one UTF-8 character to its UCS-4 code point.
 *
 * Performance-oriented implementation:
 *   - ASCII (most common case) returns immediately without touching the lookup table.
 *   - A 256-byte lookup table classifies the first byte in a single access, replacing
 *     the if-else chain and folding in the 0xC0/0xC1 overlong and 0xF5+ out-of-range
 *     rejections.
 *   - Continuation bytes are processed via an unrolled switch, eliminating loop overhead.
 *
 * @param str Pointer to UTF-8 encoded byte sequence (not necessarily null-terminated).
 * @param len Number of bytes available in the buffer pointed to by str.
 * @param val Pointer to a uint32_t variable that will receive the code point on success.
 * @return    Number of UTF-8 bytes consumed (1–4) on success,
 *            0 on failure (invalid encoding, incomplete sequence, or invalid code point).
 */
CCUNICODE_INLINE
int ccunicode_to_codepoint(const char *str, int len, uint32_t *val)
{
  if (ccunicode_unlikely(!str || len <= 0 || !val)) return 0;

  const uint8_t* p = (const uint8_t*)str;

  /* Fast path: ASCII — ~70–90% of typical UTF-8 text (HTML, JSON, source code). */
  if (p[0] < 0x80) {
    *val = p[0];
    return 1;
  }

  int bytes = seq_len[p[0]];
  if (ccunicode_unlikely(bytes < 2)) return 0;  /* invalid first byte (0) or ASCII (1) */
  if (ccunicode_unlikely((size_t)len < (size_t)bytes)) return 0; /* incomplete sequence */

  uint32_t code;
  /*
   * Unrolled continuation-byte processing.
   * Each case processes its continuation bytes and assembles the code point directly,
   * eliminating loop overhead and enabling better instruction-level parallelism.
   */
  switch (bytes) {
    case 4:
      if (ccunicode_unlikely(((p[1] ^ 0x80) | (p[2] ^ 0x80) | (p[3] ^ 0x80)) & 0xC0)) return 0;
      code  = (uint32_t)(p[0] & 0x07) << 18;
      code |= (uint32_t)(p[1] & 0x3F) << 12;
      code |= (uint32_t)(p[2] & 0x3F) << 6;
      code |= (uint32_t)(p[3] & 0x3F);
      break;
    case 3:
      if (ccunicode_unlikely(((p[1] ^ 0x80) | (p[2] ^ 0x80)) & 0xC0)) return 0;
      code  = (uint32_t)(p[0] & 0x0F) << 12;
      code |= (uint32_t)(p[1] & 0x3F) << 6;
      code |= (uint32_t)(p[2] & 0x3F);
      break;
    default: /* 2 */
      if (ccunicode_unlikely((p[1] & 0xC0) != 0x80)) return 0;
      code  = (uint32_t)(p[0] & 0x1F) << 6;
      code |= (uint32_t)(p[1] & 0x3F);
      break;
  }

  /* Overlong encoding check (minima: 2-byte → U+0080, 3-byte → U+0800, 4-byte → U+10000) */
  /* Beyond Unicode maximum (e.g. F4 90 80 80 → U+110000) */
  if (ccunicode_unlikely(code < cp_ranges[bytes] || code > 0x10FFFF)) return 0;

  /* Surrogate halves (U+D800–U+DFFF) are invalid in UTF-8 */
  if (ccunicode_unlikely(code >= 0xD800 && code <= 0xDFFF)) return 0;

  *val = code;
  return bytes;
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
bool ccunicode_from_codepoint(uint32_t val, char str[4], int *len)
{
  if (ccunicode_unlikely(!str || !len)) return false;

  /* Unicode range caps at U+10FFFF. */
  if (ccunicode_unlikely(val > 0x10FFFF)) return false;
  /* Surrogates are not valid Unicode scalar values. */
  if (ccunicode_unlikely(val >= 0xD800 && val <= 0xDFFF)) return false;

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
      ccunicode_fallthrough;
    case 3:
      p[2] = (uint8_t)(0x80 | (val & 0x3F));
      val >>= 6;
      ccunicode_fallthrough;
    case 2:
      p[1] = (uint8_t)(0x80 | (val & 0x3F));
      break;
  }

  *len = bytes;
  return true;
}

#endif /* CCUNICODE_H */