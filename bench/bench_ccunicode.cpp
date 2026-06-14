/*
**  Benchmark: ccunicode UTF-8 encode/decode throughput
**  Build:  g++ -std=c++11 -O2 -o bench_ccunicode bench_ccunicode.cpp && ./bench_ccunicode
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <chrono>

#include "../include/ccunicode.h"

/* ── timer ─────────────────────────────────────────────────────────────── */
using clk = std::chrono::high_resolution_clock;
static double ms(clk::time_point s, clk::time_point e) {
  return std::chrono::duration<double, std::milli>(e - s).count();
}

/* anti-optimisation sink */
static volatile uint32_t sink_cp;
static volatile int      sink_bytes;

/* ── test data ─────────────────────────────────────────────────────────── */

/* ASCII-only text: 1000 chars */
static const char *ascii_text =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\n"
  "The quick brown fox jumps over the lazy dog. 0123456789 times!\n"
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\n"
  "The quick brown fox jumps over the lazy dog. 0123456789 times!\n"
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\n"
  "The quick brown fox jumps over the lazy dog. 0123456789 times!\n"
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\n"
  "The quick brown fox jumps over the lazy dog. 0123456789 times!\n"
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\n"
  "The quick brown fox jumps over the lazy dog. 0123456789 times!\n"
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\n"
  "The quick brown fox jumps over the lazy dog. 0123456789 times!\n"
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\n"
  "The quick brown fox jumps over the lazy dog. 0123456789 times!\n"
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\n"
  "The quick brown fox jumps over the lazy dog. 0123456789 times!\n";

/* CJK-heavy text: 500 CJK chars × 3 bytes each */
static const unsigned char cjk_utf8[] =
  "\xE4\xB8\xAD\xE5\x9B\xBD\xE4\xBA\xBA\xE6\xB0\x91\xE5\x85\xB1\xE5\x92\x8C\xE5\x9B\xBD" /* 中华人民共和国 */
  "\xE7\xA2\xBC\xE5\xAD\x97\xE7\xAC\xA6\xE7\xB7\xA8\xE7\xA2\xBC\xE8\xA7\xA3\xE7\xA2\xBC" /* 码字编码解码 */
  "\xE6\x95\xB0\xE6\x8D\xAE\xE7\xBB\x93\xE6\x9E\x84\xE7\xAE\x97\xE6\xB3\x95\xE5\xBA\x93" /* 数据结构算法库 */
  "\xE5\xBC\x80\xE6\xBA\x90\xE8\xBD\xAF\xE4\xBB\xB6\xE5\xA4\xB4\xE6\x96\x87\xE4\xBB\xB6" /* 开源软件头文件 */
  "\xE6\x80\xA7\xE8\x83\xBD\xE6\xB5\x8B\xE8\xAF\x95\xE5\x8F\x82\xE8\x80\x83\xE6\x96\x87\xE6\xA1\xA3" /* 性能测试参考文献 */
  "\xE4\xB8\xAD\xE5\x9B\xBD\xE4\xBA\xBA\xE6\xB0\x91\xE5\x85\xB1\xE5\x92\x8C\xE5\x9B\xBD" /* 中华人民共和国 */
  "\xE7\xA2\xBC\xE5\xAD\x97\xE7\xAC\xA6\xE7\xB7\xA8\xE7\xA2\xBC\xE8\xA7\xA3\xE7\xA2\xBC" /* 码字编码解码 */
  "\xE6\x95\xB0\xE6\x8D\xAE\xE7\xBB\x93\xE6\x9E\x84\xE7\xAE\x97\xE6\xB3\x95\xE5\xBA\x93" /* 数据结构算法库 */
  "\xE5\xBC\x80\xE6\xBA\x90\xE8\xBD\xAF\xE4\xBB\xB6\xE5\xA4\xB4\xE6\x96\x87\xE4\xBB\xB6" /* 开源软件头文件 */
  "\xE6\x80\xA7\xE8\x83\xBD\xE6\xB5\x8B\xE8\xAF\x95\xE5\x8F\x82\xE8\x80\x83\xE6\x96\x87\xE6\xA1\xA3"; /* 性能测试参考文献 */

/* Emoji/musical 4-byte text */
static const unsigned char four_byte_utf8[] =
  "\xF0\x9F\x98\x80\xF0\x9F\x98\x81\xF0\x9F\x98\x82\xF0\x9F\x98\x83" /* 😀😁😂😃 */
  "\xF0\x9F\x98\x84\xF0\x9F\x98\x85\xF0\x9F\x98\x86\xF0\x9F\x98\x87" /* 😄😅😆😇 */
  "\xF0\x9D\x84\x9E\xF0\x9D\x84\xA2\xF0\x9D\x84\xA6\xF0\x9D\x84\xAA" /* 𝄞𝄢𝄦𝄪 */
  "\xF0\x9F\x90\xB1\xF0\x9F\x90\xB2\xF0\x9F\x90\xB3\xF0\x9F\x90\xB4"; /* 🐱🐲🐳🐴 */

int main() {
  printf("=== ccunicode UTF-8 encode/decode throughput ===\n\n");

  /* ── ASCII decode ────────────────────────────────────────── */
  {
    int len = (int)strlen(ascii_text);
    int iterations = 50000;
    uint32_t cp;
    auto t0 = clk::now();
    for (int i = 0; i < iterations; i++) {
      int off = 0;
      while (off < len) {
        int n = ccunicode_to_codepoint(ascii_text + off, len - off, &cp);
        off += n;
      }
    }
    auto t1 = clk::now();
    double total_chars = (double)iterations * len;
    printf("ASCII decode         : %8.2f ms  (%5.0f MB/s, %4.0f M chars/s)\n",
           ms(t0, t1), total_chars / ms(t0, t1) * 1000 / 1e6, (double)iterations * len / ms(t0, t1) * 1000 / 1e6);
    sink_cp = cp;
  }

  /* ── CJK (3-byte) decode ─────────────────────────────────── */
  {
    int len = (int)strlen((const char *)cjk_utf8);
    int iterations = 50000;
    uint32_t cp;
    auto t0 = clk::now();
    for (int i = 0; i < iterations; i++) {
      int off = 0;
      while (off < len) {
        int n = ccunicode_to_codepoint((const char *)cjk_utf8 + off, len - off, &cp);
        off += n;
      }
    }
    auto t1 = clk::now();
    double total_bytes = (double)iterations * len;
    printf("CJK (3-byte) decode  : %8.2f ms  (%5.0f MB/s, %4.0f M chars/s)\n",
           ms(t0, t1), total_bytes / ms(t0, t1) * 1000 / 1e6, (double)iterations * (len / 3) / ms(t0, t1) * 1000 / 1e6);
    sink_cp = cp;
  }

  /* ── 4-byte decode ───────────────────────────────────────── */
  {
    int len = (int)strlen((const char *)four_byte_utf8);
    int iterations = 50000;
    uint32_t cp;
    auto t0 = clk::now();
    for (int i = 0; i < iterations; i++) {
      int off = 0;
      while (off < len) {
        int n = ccunicode_to_codepoint((const char *)four_byte_utf8 + off, len - off, &cp);
        off += n;
      }
    }
    auto t1 = clk::now();
    double total_bytes = (double)iterations * len;
    printf("4-byte decode        : %8.2f ms  (%5.0f MB/s, %4.0f M chars/s)\n",
           ms(t0, t1), total_bytes / ms(t0, t1) * 1000 / 1e6, (double)iterations * (len / 4) / ms(t0, t1) * 1000 / 1e6);
    sink_cp = cp;
  }

  /* ── ASCII encode ────────────────────────────────────────── */
  {
    int iterations = 10000000;
    char buf[8];
    int n;
    auto t0 = clk::now();
    for (int i = 0; i < iterations; i++) {
      ccunicode_from_codepoint((uint32_t)('A' + (i & 63)), buf, &n);
    }
    auto t1 = clk::now();
    printf("ASCII encode         : %8.2f ms  (%4.0f M/s)\n",
           ms(t0, t1), (double)iterations / ms(t0, t1) * 1000 / 1e6);
    sink_bytes = n;
  }

  /* ── CJK (3-byte) encode ─────────────────────────────────── */
  {
    int iterations = 5000000;
    char buf[8];
    int n;
    auto t0 = clk::now();
    for (int i = 0; i < iterations; i++) {
      ccunicode_from_codepoint(0x4E2D + (i & 0xFF), buf, &n);
    }
    auto t1 = clk::now();
    printf("CJK (3-byte) encode  : %8.2f ms  (%4.0f M/s)\n",
           ms(t0, t1), (double)iterations / ms(t0, t1) * 1000 / 1e6);
    sink_bytes = n;
  }

  /* ── 4-byte encode ───────────────────────────────────────── */
  {
    int iterations = 5000000;
    char buf[8];
    int n;
    auto t0 = clk::now();
    for (int i = 0; i < iterations; i++) {
      ccunicode_from_codepoint(0x1F600 + (i & 0xFF), buf, &n);
    }
    auto t1 = clk::now();
    printf("4-byte encode        : %8.2f ms  (%4.0f M/s)\n",
           ms(t0, t1), (double)iterations / ms(t0, t1) * 1000 / 1e6);
    sink_bytes = n;
  }

  /* ── reject path (overlong) ──────────────────────────────── */
  {
    int iterations = 5000000;
    uint32_t cp;
    auto t0 = clk::now();
    for (int i = 0; i < iterations; i++) {
      /* each call hits the overlong rejection path */
      cp = 0;
      ccunicode_to_codepoint("\xC0\x80", 2, &cp);
    }
    auto t1 = clk::now();
    printf("Reject (overlong)    : %8.2f ms  (%4.0f M/s)\n",
           ms(t0, t1), (double)iterations / ms(t0, t1) * 1000 / 1e6);
    sink_cp = cp;
  }

  printf("\n");
  (void)sink_cp;
  (void)sink_bytes;
  return 0;
}
