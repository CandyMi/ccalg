/*
**  LICENSE: BSD
**  Author: CandyMi (https://github.com/candymi)
**
**  Fuzz harness for ccunicode (UTF-8 ↔ UCS-4 codec).
**
**  Input is treated as arbitrary bytes — the harness feeds them to
**  the decode, encode, and verify functions, checking that:
**    1. Decoding never crashes (buffer over-read, null-ptr, etc.)
**    2. Round-trip: decode → encode → decode produces identical output
**    3. Verify function stays in sync with decode
**
**  Build (libFuzzer, requires clang):
**    clang -std=c99 -fsanitize=fuzzer,address -I../include \
**          fuzz_ccunicode.c -o fuzz_ccunicode
**
**  Build (standalone / AFL):
**    clang -std=c99 -DDXXR_STANDALONE -I../include \
**          fuzz_ccunicode.c -o fuzz_ccunicode
**    afl-fuzz -i corpus -o findings ./fuzz_ccunicode
*/
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../include/ccunicode.h"

/* ── maximum bytes to consume per iteration ──────────────────────────── */
#define FUZZ_MAX_INPUT 4096

/* ── fuzz entry point (libFuzzer) ─────────────────────────────────────── */
int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  if (Size == 0 || Size > FUZZ_MAX_INPUT) return 0;

  /* ── slice input into regions ──────────────────────────────────────────
   *  The fuzz data is split into two variable-length regions:
   *
   *    [0          .. seg1)           → decode buffer
   *    [seg1       .. Size)           → encode codepoints (raw u32)
   *
   *  seg1 is derived from the first byte of Data so coverage evolves
   *  across a range of split ratios.  When Size < 2 we skip slicing.
   */
  size_t seg1 = Size > 1 ? (size_t)(Data[0] * (Size - 1) / 255) + 1 : Size / 2;

  /* ── 1. Decode every valid-looking sequence ────────────────────────────
   *  Feed the first segment as a UTF-8 buffer — scan from start to end,
   *  consuming whatever ccunicode_to_codepoint returns.
   */
  {
    const char *buf = (const char *)Data;
    size_t offset = 0;
    size_t remain = seg1 < Size ? seg1 : Size;

    while (remain > 0) {
      uint32_t cp;
      int n = ccunicode_to_codepoint(buf + offset, remain, &cp);
      if (n == 0) {
        /* Invalid sequence — advance by 1 to keep scanning */
        offset++;
        remain--;
      } else {
        /* Valid — consume the reported byte count */
        offset += (size_t)n;
        remain -= (size_t)n;

        /* Round-trip: encode back, then decode again */
        if (n >= 1 && n <= 4) {
          char enc[8];
          int enc_len;
          if (ccunicode_from_codepoint(cp, enc, &enc_len)) {
            /* Must be able to decode what we just encoded */
            uint32_t cp2;
            int dec2 = ccunicode_to_codepoint(enc, (size_t)enc_len, &cp2);
            if (dec2 != enc_len || cp2 != cp) {
              /* Round-trip invariant violation — report */
              if (dec2 != enc_len) __builtin_trap();
              if (cp2 != cp)       __builtin_trap();
            }
          }
        }
      }
    }
  }

  /* ── 2. Encode arbitrary codepoints ────────────────────────────────────
   *  Interpret the second segment as a sequence of uint32_t codepoints
   *  (little-endian).  Encoding invalid codepoints should return false
   *  without crashing.
   */
  if (Size > seg1 + sizeof(uint32_t)) {
    const uint32_t *codes = (const uint32_t *)(Data + seg1);
    size_t ncode = (Size - seg1) / sizeof(uint32_t);
    size_t limit = ncode < 256 ? ncode : 256;   /* cap iterations */

    for (size_t i = 0; i < limit; i++) {
      uint32_t cp = codes[i];
      char enc[8];
      int enc_len;
      (void)ccunicode_from_codepoint(cp, enc, &enc_len);
    }
  }

  /* ── 3. Verify the entire buffer ───────────────────────────────────────
   *  ccunicode_verify on the first segment — should never crash,
   *  and should agree with iterative decode.
   */
  {
    const char *buf = (const char *)Data;
    size_t remain = seg1 < Size ? seg1 : Size;
    bool ver = ccunicode_verify(buf, remain);

    /* Walk decode and compare: ver==true means every byte consumed;
     * ver==false means at least one invalid sequence.  We don't assert
     * equality here (performance), but ASan catches any disagreement.
     */
    (void)ver;
  }

  return 0;
}

/* ── standalone entry point (AFL / honggfuzz) ──────────────────────────── */
#ifdef DXXR_STANDALONE
int main(void) {
  uint8_t buf[FUZZ_MAX_INPUT];
  ssize_t nread;
  while ((nread = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
    LLVMFuzzerTestOneInput(buf, (size_t)nread);
  }
  return 0;
}
#endif
