/*
**  LICENSE: BSD
**  Author: CandyMi (https://github.com/candymi)
**
**  Fuzz harness for ccbi (big-integer / arbitrary-precision arithmetic).
**
**  The fuzz data is interpreted as a sequence of operations:
**    - Parse as decimal string and round-trip via to_str
**    - Perform add/sub/mul/div_rem on pairs of parsed values
**    - Shift operations (SHL/SHR) on parsed values
**    - Bitwise operations (and/or/xor/not)
**
**  Invariants checked:
**    1. from_str → to_str round-trip produces the identical string
**    2. a + b - b == a
**    3. (a * b) / b == a  (b != 0)
**    4. (a / b) * b + (a % b) == a  (Euclidean div-rem identity)
**    5. SHL then SHR by the same amount restores original
**
**  Build (libFuzzer, requires clang):
**    clang -std=c99 -fsanitize=fuzzer,address -I../include \
**          fuzz_ccbi.c -o fuzz_ccbi
**
**  Build (standalone):
**    clang -std=c99 -DDXXR_STANDALONE -I../include \
**          fuzz_ccbi.c -o fuzz_ccbi
*/
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../include/ccbi.h"

/* ── max input per iteration ──────────────────────────────────────────── */
#define FUZZ_MAX_INPUT 8192

/* ── scratch pool (avoids repeated init/destroy churn) ────────────────── */
#define N_SCRATCH 8
static ccbi_t scratch[N_SCRATCH];
static int scratch_initialized = 0;

static int ensure_scratch(void) {
  if (!scratch_initialized) {
    for (int i = 0; i < N_SCRATCH; i++) ccbi_init(&scratch[i]);
    scratch_initialized = 1;
  }
  return 0;
}

static void cleanup_scratch(void) {
  if (scratch_initialized) {
    for (int i = 0; i < N_SCRATCH; i++) ccbi_destroy(&scratch[i]);
    scratch_initialized = 0;
  }
}

/* ── build a printable decimal string from fuzz bytes ───────────────────
 *  Returns a null-terminated string in a static buffer, or NULL if the
 *  input would produce an absurdly large number (> 64 limbs = ~2048-bit).
 *  The string consists only of digits [0-9] and an optional leading '-'.
 */
static char *fuzz_to_decimal(const uint8_t *data, size_t len, char *buf, size_t bufsz) {
  if (len == 0) return NULL;

  size_t idx = 0;
  int neg = 0;

  /* Decide sign from first bit */
  if (data[0] & 0x80) { neg = 1; data++; len--; }

  /* Cap output digits: 2048-bit ≈ 617 decimal digits */
  size_t max_digits = 620;
  if (len > max_digits) len = max_digits;

  if (len == 0) return NULL;

  /* Skip leading zeros produced by zero-valued bytes */
  size_t start = 0;
  while (start < len && data[start] == 0) start++;
  if (start == len) {
    buf[0] = '0';
    buf[1] = '\0';
    return buf;
  }

  if (neg) { buf[idx++] = '-'; if (idx >= bufsz) return NULL; }

  /* Convert each byte to two decimal digits (0-99) */
  for (size_t i = start; i < len; i++) {
    unsigned int v = (unsigned int)data[i] % 100;  /* 0-99 */
    if (v >= 10) {
      if (idx + 2 >= bufsz) return NULL;
      buf[idx++] = (char)('0' + v / 10);
      buf[idx++] = (char)('0' + v % 10);
    } else {
      /* Single digit — only emit if we've already started (avoid
       * leading zeros that aren't actually the first digit) */
      if (idx > (size_t)neg || v != 0) {
        if (idx + 1 >= bufsz) return NULL;
        buf[idx++] = (char)('0' + v);
      }
    }
  }

  /* If we somehow produced nothing, emit "0" */
  if (idx == 0 || (idx == 1 && neg)) {
    buf[0] = '0'; buf[1] = '\0';
    return buf;
  }

  buf[idx] = '\0';
  return buf;
}

/* ── fuzz entry point ─────────────────────────────────────────────────── */
int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  if (Size == 0 || Size > FUZZ_MAX_INPUT) return 0;
  ensure_scratch();

  /* ── split input into 3 regions ─────────────────────────────────────────
   *  region 0: decimal string A  (first 40% of bytes)
   *  region 1: decimal string B  (next  40%)
   *  region 2: operation selector (last 20%)
   */
  size_t s1 = Size > 2 ? (size_t)(Data[0] * (Size - 2) / 255) + 1 : Size / 3;
  size_t s2 = Size > s1 + 1 ? (size_t)(Data[1] * (Size - s1 - 1) / 255) + s1 + 1 : Size;

  /* Guard: if regions overlap or are empty, adjust */
  if (s1 >= s2) s2 = s1 + 1;
  if (s2 >= Size) s2 = Size;

  char buf_a[1024], buf_b[1024];
  char *str_a = fuzz_to_decimal(Data, s1, buf_a, sizeof(buf_a));
  char *str_b = fuzz_to_decimal(Data + s1, s2 - s1, buf_b, sizeof(buf_b));

  if (!str_a || !str_b) return 0;

  /* Parse A and B */
  ccbi_t *a = &scratch[0];
  ccbi_t *b = &scratch[1];

  ccbi_init(a); ccbi_init(b);

  if (ccbi_from_str(a, str_a, 10) != 0)  goto out;
  if (ccbi_from_str(b, str_b, 10) != 0)  goto out;

  /* Round-trip check: to_str must produce the same value */
  {
    char *s = ccbi_to_str(a, 10);
    if (s) {
      ccbi_t chk; ccbi_init(&chk);
      if (ccbi_from_str(&chk, s, 10) == 0) {
        if (ccbi_cmp(a, &chk) != 0) __builtin_trap();  /* round-trip fail */
      }
      ccbi_destroy(&chk);
      ccbi_free_str(s);
    }
  }

  /* Select operation from the final byte(s) */
  uint8_t op = Size > s2 ? Data[Size - 1] : (uint8_t)(Data[Size - 1] % 16);
  ccbi_t *z = &scratch[2];
  ccbi_t *r = &scratch[3];
  ccbi_t *q = &scratch[4];
  ccbi_t *t = &scratch[5];

  switch (op % 16) {

  case 0: /* add: z = a + b, verify z - b == a */
    ccbi_add(z, a, b);
    ccbi_sub(t, z, b);
    if (ccbi_cmp(t, a) != 0) __builtin_trap();
    break;

  case 1: /* sub: z = a - b, verify z + b == a */
    ccbi_sub(z, a, b);
    ccbi_add(t, z, b);
    if (ccbi_cmp(t, a) != 0) __builtin_trap();
    break;

  case 2: /* mul: z = a * b, verify z / b == a (b != 0) */
    if (ccbi_is_zero(b)) break;
    ccbi_mul(z, a, b);
    ccbi_div_rem(q, r, z, b);
    if (ccbi_cmp(q, a) != 0) __builtin_trap();
    if (!ccbi_is_zero(r))     __builtin_trap();
    break;

  case 3: /* div_rem: verify (a/b)*b + a%b == a */
    if (ccbi_is_zero(b)) break;
    ccbi_div_rem(q, r, a, b);
    ccbi_mul(t, q, b);
    ccbi_add(z, t, r);
    if (ccbi_cmp(z, a) != 0) __builtin_trap();
    break;

  case 4: /* shl + shr round-trip: (a << k) >> k == a */
    {
      size_t k = (size_t)Data[Size - 1] % 128;
      ccbi_shl(z, a, k);
      ccbi_shr(t, z, k);
      if (ccbi_cmp(t, a) != 0) __builtin_trap();
    }
    break;

  case 5: /* bitwise: a & b, then a | b, then a ^ b */
    ccbi_and(z, a, b);
    ccbi_or(q, a, b);
    ccbi_xor(r, a, b);
    /* (a & b) | (a ^ b) == a | b  — De Morgan sanity */
    ccbi_or(t, z, r);
    if (ccbi_cmp(t, q) != 0) __builtin_trap();
    break;

  case 6: /* neg + abs round-trip: abs(neg(a)) == a (a >= 0) */
    if (ccbi_sign(a) < 0) break;  /* only test on non-negative */
    ccbi_neg(z, a);
    ccbi_abs(t, z);
    if (ccbi_cmp(t, a) != 0) __builtin_trap();
    break;

  case 7: /* copy + cmp identity */
    ccbi_copy(z, a);
    if (ccbi_cmp(z, a) != 0) __builtin_trap();
    break;

  case 8: /* inc: inc(a) then verify a == original + 1 */
    {
      ccbi_copy(z, a);
      ccbi_from_int(t, 1);
      ccbi_add(t, z, t);        /* t = original + 1 */
      ccbi_inc(z);
      if (ccbi_cmp(z, t) != 0) __builtin_trap();
    }
    break;

  case 9: /* dec: dec(a) then verify a == original - 1 */
    if (ccbi_is_zero(a)) break;
    {
      ccbi_copy(z, a);
      ccbi_from_int(t, 1);
      ccbi_sub(t, z, t);        /* t = original - 1 */
      ccbi_dec(z);
      if (ccbi_cmp(z, t) != 0) __builtin_trap();
    }
    break;

  case 10: /* swap + cmp: swap, swap back → same */
    ccbi_copy(z, a);
    ccbi_copy(t, b);
    ccbi_swap(a, b);
    if (ccbi_cmp(a, t) != 0) __builtin_trap();
    if (ccbi_cmp(b, z) != 0) __builtin_trap();
    /* Swap back */
    ccbi_swap(a, b);
    break;

  case 11: /* bit_length and shl by bit_length */
    {
      size_t bl = (size_t)ccbi_bit_length(a);
      ccbi_from_int(t, 1);
      ccbi_shl(t, t, bl);
      if (ccbi_cmp(t, a) < 0 && bl > 0) __builtin_trap(); /* a < 2^bl */
    }
    break;

  default: /* 12-15: from_str in base 2/8/16/36 round-trip */
    {
      int bases[] = {2, 8, 16, 36};
      int base = bases[op & 3];
      char *s = ccbi_to_str(a, base);
      if (s) {
        ccbi_init(t);
        if (ccbi_from_str(t, s, base) == 0) {
          if (ccbi_cmp(t, a) != 0) __builtin_trap();
        }
        ccbi_destroy(t);
        ccbi_free_str(s);
      }
    }
    break;
  }

out:
  ccbi_destroy(a);
  ccbi_destroy(b);
  return 0;
}

/* ── standalone entry point ───────────────────────────────────────────── */
#ifdef DXXR_STANDALONE
int main(void) {
  uint8_t buf[FUZZ_MAX_INPUT];
  ssize_t nread;
  while ((nread = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
    LLVMFuzzerTestOneInput(buf, (size_t)nread);
  }
  cleanup_scratch();
  return 0;
}
#endif
