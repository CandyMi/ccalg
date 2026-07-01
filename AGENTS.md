# AGENTS.md

> Canonical reference for AI coding agents working on this project.

---

## Project identity

**ccalg** — a header-only C data-structure library, compatible with C99 / C++11 / MSVC.

- **License:** BSD 3-Clause
- **Author:** CandyMi
- **Repository:** [github.com/CandyMi/ccalg](https://github.com/CandyMi/ccalg)
- **Website:** [ccalg.dev](https://ccalg.dev) (served via `gh-pages` branch)
- **Build:** CMake (primary). Headers are self-contained; `#include` and use.
- **Core traits:** intrusive, zero internal node allocation (with exceptions), compile-time zero-overhead callbacks.

---

## File map

| File | Structures | Description |
| --- | --- | --- |
| [ccmap.h](include/ccmap.h) | `ccmap_t` / `ccmap_node_t` | Intrusive red-black tree (ordered map) |
| [cchashmap.h](include/cchashmap.h) | `cchashmap_t` / `cchashmap_node_t` | Intrusive chained hash map |
| [cclink.h](include/cclink.h) | `cclink_t` / `cclink_node_t` | Intrusive singly-linked list |
| [cclist.h](include/cclist.h) | `cclist_t` / `cclist_node_t` | Intrusive doubly-linked list |
| [ccheap.h](include/ccheap.h) | `ccheap_t` / `ccheap_node_t` | D-ary heap (priority queue) |
| [ccvector.h](include/ccvector.h) | `ccvector_t` / `ccvector_node_t` | Dynamic array (value-based) |
| [ccflatmap.h](include/ccflatmap.h) | `ccflatmap_t` / `ccflatmap_node_t` | Sorted-array map (value-based) |
| [cctreap.h](include/cctreap.h) | `cctreap_t` / `cctreap_node_t` | Intrusive treap (BST + max-heap, order statistics) |
| [ccunicode.h](include/ccunicode.h) | — | UTF-8 ↔ UCS-4 codec |
| [ccrandom.h](include/ccrandom.h) | `ccrandom128_t` / `ccrandom256_t` / `ccrandom512_t` | Non-cryptographic PRNG (Xoroshiro128++ / Xoshiro256** / Xoshiro512**) |
| [ccshuffle.h](include/ccshuffle.h) | — | Fisher-Yates (Knuth) shuffle for contiguous arrays |
| [ccbits.h](include/ccbits.h) | — | Bit manipulation (popcount, clz, ctz, rotl/rotr, bswap, bitrev, bit_width, parity, mask_low, sign_ext) |

---

## Architecture

### Intrusive containers

Every container manages **user-embedded nodes**, not allocated elements. The user embeds `xxx_node_t` in their struct; the container operates on node pointers.

```c
struct my_entry { int key; ccmap_node_t node; };
```

The container **never allocates or frees user struct memory**.  Exceptions:

- `cchashmap` internally manages an inline bucket array.
- `ccvector` and `ccflatmap` store elements by **value** (not pointers) — they own element memory and copy on push/insert.

### Container-of idiom

`CCXXX_CONTAINER(ptr, type, member)` — macro wrapping `container_of` / `offsetof`.  Standard loop:

```c
for (ccmap_node_t *n = ccmap_begin(&m); n != ccmap_end(&m); n = ccmap_next(n)) {
    struct my_entry *e = CCMAP_CONTAINER(n, struct my_entry, node);
}
```

Available: `CCMAP_CONTAINER`, `CCHASHMAP_CONTAINER`, `CCLINK_CONTAINER`, `CCLIST_CONTAINER`, `CCHEAP_CONTAINER`, `CCTREAP_CONTAINER`.

### Zero-overhead callbacks

| Mode | Mechanism | Overhead |
|---|---|---|
| **Function pointer** (default) | Pass callback to `xxx_init()` | Indirect call |
| **Macro** (opt-in) | `#define` before `#include` | Compile-time inline, zero overhead |

When a macro is defined, the matching `init()` parameter is suppressed with `(void)arg`.

#### Dispatch signatures

| Container | Function pointer type | Macro override |
| --- | --- | --- |
| `ccmap` | `ccmap_cmp_t`: `int64_t (*)(const ccmap_node_t*, const ccmap_node_t*)` | `#define CCMAP_COMPARE(a, b)` |
| `cchashmap` | `cchashmap_hash_t`: `uint64_t (*)(const cchashmap_node_t*, size_t)` <br> `cchashmap_equal_t`: `bool (*)(const cchashmap_node_t*, const cchashmap_node_t*)` | `#define CCHASHMAP_HASH(n, seed)` <br> `#define CCHASHMAP_EQUAL(a, b)` |
| `ccheap` | `ccheap_compare_t`: `int64_t (*)(const ccheap_node_t*, const ccheap_node_t*)` | `#define CCHEAP_COMPARE(a, b)` |
| `ccflatmap` | `ccflatmap_cmp_t`: `int64_t (*)(const CCFLATMAP_NODE_T*, const CCFLATMAP_NODE_T*)` | `#define CCFLATMAP_COMPARE(a, b)` |
| `cctreap` | `cctreap_cmp_t`: `int64_t (*)(const cctreap_node_t*, const cctreap_node_t*)` (key) | `#define CCTREAP_COMPARE(a, b)` (key) <br> `#define CCTREAP_RAND_NEXT(state)` (RNG) |
| `cclist` / `cclink` | none needed | none |

All callback typedefs include `CCXXX_NOEXCEPT` (C++17+ `noexcept`, C < C++17 empty) to enforce
non-throwing callbacks at compile time in C++17+.

#### Comparison semantics

- Return type `int64_t`; `>0` → `a` ordered before `b`; `<0` → `b` before `a`; `==0` → equal
- For `ccmap` and `cctreap`, equality means duplicate key → insert returns `-1`

### Allocation hooks

Containers that allocate memory internally expose replaceable allocator macros:

| Macro | Purpose | Default |
| --- | --- | --- |
| `CCXXX_MALLOC(sz)` | Allocate | `CCXXX_REALLOC(NULL, sz)` |
| `CCXXX_REALLOC` | Reallocate | `realloc` |
| `CCXXX_FREE(ptr)` | Free | `free(ptr)` |

- `cchashmap`, `ccheap`, `ccvector`, `ccflatmap` use this mechanism.
- `ccmap`, `cclink`, `cclist`, `cctreap` have zero internal allocation → no allocator macros.

### C++ interop

Headers are wrapped in `extern "C" { }` for C++ linking.  Callback typedefs carry
`CCXXX_NOEXCEPT` (empty on C / C++ < 17, `noexcept` on C++17+) — this prevents
exception propagation through library callbacks at compile time.

### API naming conventions

#### Lifecycle / CRUD

| Operation | Naming | Returns |
| --- | --- | --- |
| Init | `xxx_init(m, ...)` | void |
| Destroy | `xxx_destroy(m)` | void (frees internal resources only) |
| Clear | `xxx_clear(m)` | void (reset to empty, no free) |
| Insert | `xxx_insert(m, node, out)` | `0` success / `-1` duplicate |
| Find | `xxx_find(m, probe)` | node pointer / `NULL` |
| Delete | `xxx_erase(m, node)` | void |

Aliases: hashmap provides `set`/`get`/`del`.  `out` parameter: on duplicate `*out` = existing node; on success `*out` = inserted node.

#### Iteration

| Operation | Naming | Notes |
| --- | --- | --- |
| Forward first | `xxx_begin(m)` / `xxx_first(m)` | Minimum element |
| Reverse first | `xxx_rbegin(m)` / `xxx_last(m)` | Maximum element |
| End sentinel | `xxx_end(m)` | Always `NULL` |
| Successor | `xxx_next(n)` | Node → next |
| Predecessor | `xxx_prev(n)` | Node → previous |

`ccmap` and `cctreap` cache `first`/`last` pointers for O(1) `begin()` / `rbegin()`.  Insert fast-paths skip BST descent when new key is outside `[first, last]`.

**Erase recomputation:**

| Container | Strategy | Rationale |
| --- | --- | --- |
| `ccmap` | From original `z`'s `child[]` and parent | Tree intact after `_rb_transplant` |
| `cctreap` | From `m->root` after `_tp_transplant` | Bubble-down rotations alter topology |

Both capture `is_first`/`is_last` flags **before** any structural modification.

`cctreap` nodes carry a `size` field enabling O(log n) `cctreap_kth(m, k)` (0-indexed k-th smallest) and `cctreap_rank(m, probe)` (0-indexed rank, `-1` if not found). Priority is internal — a random `uint64_t` generated on insert (overridable via `CCTREAP_RAND_NEXT`).

**Exception**: `ccflatmap_next(m, p)` / `ccflatmap_prev(m, p)` take the container pointer — arrays need bounds via `m->len`.

#### List-specific operations

| Operation | Naming |
| --- | --- |
| Head / tail push | `push_front` / `push_back` |
| Head / tail pop | `pop_front` / `pop_back` |
| Insert before / after | `insert_before` / `insert_after` |
| Remove | `remove` |
| Splice / move | `splice_back` / `move(to, from)` |

#### Size / empty / diagnostic

| Operation | Naming |
| --- | --- |
| Size | `xxx_size(m)` |
| Height | `ccmap_height(m)` / `cctreap_height(m)` — Debug: actual DFS traversal; Release: O(log n) estimate from `size` |
| Empty | `xxx_empty(m)` (cclist / cclink / ccvector / ccflatmap) |

### Error handling

| Return | Meaning |
| --- | --- |
| `NULL` | Not found / empty / invalid parameter |
| `0` | Success |
| `-1` | Failure (invalid param / duplicate / out of memory) |
| `false` / `true` | Boolean ops (hashmap) |

All public functions NULL-safe — `if (!m)` or `if (!m || !node)` guards. No `assert()`, no `abort()`.

`cclist_ecode_t` / `cclink_ecode_t` provide detailed verify error codes with `xxx_get_error()` lookup.

---

## Per-Container Details

### ccmap — Red-Black Tree
- Color in low bit of parent pointer.  Node 24B (64-bit): `child[2]` + `pc`.
- `_rb_transplant`: setne-based array index eliminates 3-way branch.

### cchashmap — Hash Map
- Chained hashing, lazy bucket array, 2× resize at load >1.25.
- Seed = `(uintptr_t)m`; bucket index via `hash & (cap-1)`.

### cclink — Singly-Linked List
- `head`+`tail`+`size`, no sentinels.  `push_front`/`push_back` O(1), `remove` O(n).

### cclist — Doubly-Linked List
- `head`/`tail` ptrs, no sentinels, all push/pop O(1).  `splice_back` O(1), `verify` (6 checks), `has_cycle` (Floyd).

### ccheap — D-ary Heap
- `CCHEAP_ARITY` ∈ {2,4,8}, child loops unrolled at compile time.
- Optional decrease-key: `#define CCHEAP_NODE_INDEX <field>` → `ccheap_update` O(log n).

### ccvector — Dynamic Array
- Value-based, 2× growth from cap 8.  `at()` returns `NULL` on OOB.

### ccflatmap — Sorted-Array Map
- Value-based sorted array.  Bulk: `push_back`×N + `sort`, or `erase_unordered`×N + `sort`.
- Internal QS: Hoare, median-of-3, insertion sort ≤16.

### cctreap — Treap
- BST(key) + max-heap(priority).  Node 40B.  Priority via internal xorshift64.
- `kth(m,k)` / `rank(m,probe)` — O(log n) via cached subtree `size`.

### ccunicode — UTF-8 Codec
- Pure functions, zero state.  256-byte `seq_len[]` table for first-byte classification.
- Overlong/surrogate/>U+10FFFF rejection.  Unicode 17.0 / RFC 3629.

### ccrandom — PRNG
- 3 engines: 128 (Xoroshiro128++), 256 (Xoshiro256**), 512 (Xoshiro512**).
- All pass BigCrush + PractRand ≥32 TiB.  Seed via SplitMix64.
- Not crypto, not thread-safe.

### ccbits — Bit Manipulation
- 12 categories, 8–64 bit widths.  GCC/Clang builtins → MSVC intrinsics → pure C fallback.
- All functions `#ifndef`-guarded — user-overridable before `#include`.

### ccshuffle — Fisher-Yates Shuffle
- O(n) in-place, O(1) stack swap (heap fallback for >64B elements).
- Accepts any PRNG via `ccshuffle_prng_t` callback.

---

## Build, Test, Benchmark

### CMake

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target check    # build + test
cmake --build build --target bench    # build + benchmark
```

### Test coverage

| Test | Container |
| --- | --- |
| `test_ccmap.c` | Red-black tree |
| `test_cchashmap.c` | Hash map |
| `test_cclink.c` | Singly-linked list |
| `test_cclist.c` | Doubly-linked list |
| `test_ccheap.c` | D-ary heap |
| `test_ccvector.c` | Dynamic array |
| `test_ccflatmap.c` | Sorted-array map |
| `test_cctreap.c` | Treap |
| `test_ccrandom.c` | PRNG (Xoroshiro128++ / Xoshiro256** / Xoshiro512**) |
| `test_ccshuffle.c` | Fisher-Yates shuffle |
| `test_ccunicode.c` | UTF-8 ↔ UCS-4 codec |
| `test_ccunicode_verify.c` | UTF-8 edge-case verification |
| `test_ccheap_update.c` | D-ary heap decrease-key |
| `test_ccbi.c` | Big-integer (arbitrary-precision arithmetic) |
| `test_ccbits.c` | Bit manipulation primitives |

### Sanitizer build

Set `-DCCALG_SANITIZE=ON` (default for Debug builds) to enable
AddressSanitizer + UndefinedBehaviorSanitizer on all test targets:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCCALG_SANITIZE=ON
cmake --build build --target check
```

- GCC/Clang: `-fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined`
- MSVC ≥ VS 16.9: `/fsanitize=address`
- Benchmark targets are excluded — ASAN conflicts with `-O2`.
- On macOS ASAN may need `$ENV{ASAN_OPTIONS}=detect_leaks=0` (no malloc-leak detection).

### Benchmarks (C++ vs STL)

| Benchmark | Comparison | Scale |
| --- | --- | --- |
| `bench_ccmap.cpp` | ccmap vs `std::map` | 100K |
| `bench_cchashmap.cpp` | cchashmap vs `std::unordered_map` vs uthash | 200K |
| `bench_cclink.cpp` | cclink vs `std::forward_list` | 200K |
| `bench_cclist.cpp` | cclist vs `std::list` | 200K |
| `bench_ccheap.cpp` | ccheap vs `std::priority_queue` | 200K |
| `bench_ccvector.cpp` | ccvector vs `std::vector` | 500K |
| `bench_ccflatmap.cpp` | ccflatmap vs ccmap vs `std::map` | 50K |
| `bench_cctreap.cpp` | cctreap vs `std::map` (incl. kth/rank) | 100K |
| `bench_ccrandom.cpp` | ccrandom128/ccrandom256 vs `std::mt19937` | 20M draws |
| `bench_ccunicode.cpp` | ccunicode UTF-8 encode/decode throughput | ASCII / CJK / 4-byte |
| `bench_ccmap_prefetch.cpp` | ccmap with/without CCMAP_PREFETCH | 100K |
| `bench_ccshuffle.cpp` | ccshuffle_knuth vs `std::shuffle` | 100K / 1M |
| `bench_ccbits.cpp` | Bit primitives (popcount, clz, ctz, bswap) throughput | 10M |
| `bench_ccbi.cpp` | Big-integer mul / div / pow_mod / to_str throughput | 256/1024/2048-bit |

---

## New Container Checklist

1. [ ] Single header `include/ccxxx.h` with `#ifndef CCXXX_H` guard
2. [ ] `CCXXX_INLINE` portable-inline macro
3. [ ] Intrusive node `ccxxx_node_t` with `CCXXX_NODE_T` guard (or value-based for vector/flatmap)
4. [ ] `CCXXX_CONTAINER` → `container_of` (if intrusive)
5. [ ] Comparison/hash dual-mode dispatch (fn ptr + macro)
6. [ ] `xxx_init` / `xxx_clear` (+ `xxx_destroy` if internal allocation)
7. [ ] `xxx_insert` / `xxx_find` / `xxx_erase`
8. [ ] `xxx_begin` / `xxx_end` / `xxx_next` (+ `xxx_prev` if ordered)
9. [ ] `xxx_size` (+ `xxx_height` if tree)
10. [ ] Internal helpers prefixed `_xxx_`
11. [ ] BSD license header
12. [ ] Matching `tests/test_ccxxx.c` + `bench/bench_ccxxx.cpp`
13. [ ] Update `AGENTS.md`, `docs/api-reference.md`, `docs/algorithms.md`, `docs/index.md`

---

## Standard Operating Procedures

> **Mandatory.** These rules MUST be checked before every create / modify / debug / commit.  
> If uncertain about any rule's applicability, STOP and ask interactively — do NOT proceed without confirmation.

### 1. File header — License

Every source file (`*.h`, `*.c`, `*.cpp`) MUST include the BSD license header:

```c
/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
*/
```

### 2. Comment style

Use `/* */` consistently throughout each file.  
**No mixing** — a single file MUST NOT contain both `/* */` and `//` comments.  
If a file mixes styles, STOP and ask which style to unify to before proceeding.

#### Doxygen annotations

Utility modules (`ccrandom`, `ccunicode`, `ccshuffle`, and similar non-container
helpers) MUST annotate every public function with Doxygen-style doc comments:

```c
/** @brief One-line summary.
 *  @param name  Description.
 *  @return      Description. */
```

Fields:
- `@brief` — required, single-sentence summary of the function.
- `@param name` — one per parameter, describing valid values / NULL semantics.
- `@return` — required for non-void functions; optional for void (document `out`
  parameters if any).

Container headers (`ccmap`, `cclist`, …) MAY use plain `/* */` block comments
at the author's discretion — their public API is documented in `docs/api-reference.md`.
A single file MUST NOT mix `/** */` and plain `/* */` for different doc blocks;
choose one style per file.

### 3. Language standard

- C99+ syntax throughout.  No strict C89 mode — the library requires `<stdint.h>`,
  `for`-loop initial declarations, and other C99 features.
- **A single file MUST NOT mix declaration styles** (e.g. block-start + mid-block
  declarations).  If mixing is detected, STOP and ask which style to normalize to.
- C++ interop: wrap public declarations in `#ifdef __cplusplus extern "C" {` / `}` if the header may be consumed from C++.

### 4. Documentation sync

- New or changed public API → MUST update:
  - `docs/api-reference.md`
  - `docs/algorithms.md` (if algorithmic detail changed)
  - `docs/index.md` (if feature list changed)
  - `AGENTS.md` (architecture, file map, test table, macro table, per-container details)
- **Stale docs MUST be removed** — if an API was removed or renamed, purge its old docs entry.

### 5. Test requirement

- Every new public function or container MUST have a matching test in `tests/test_ccxxx.c`.
- The test MUST be registered in `CMakeLists.txt` (via `add_ccalg_test()`) AND in the `check` target's DEPENDS list.
- **All testing MUST use the cmake/ctest workflow** — writing code → registering in CMakeLists.txt → `cmake --build build --target check`.  
  **Do NOT compile or run individual test files via `gcc`/`clang` command line.**  If single-file compilation is needed for debugging, ask first.
- Benchmarks (`.cpp` under `bench/`) are strongly recommended for new containers.

### 6. Commit message format

Use **Conventional Commits**:

```
<type>(<scope>): <description>

[optional body]
```

Types: `feat`, `fix`, `docs`, `refactor`, `test`, `bench`, `chore`, `cmake`.  
Scope is the component name (e.g. `ccmap`, `ccrandom`, `docs`, `AGENTS`).  
Body explains *why* when the title isn't sufficient.

### 7. NULL safety & error handling

- Every public function MUST guard `NULL` parameters:
  ```c
  if (!m) return 0;   /* or NULL / false, matching the return type */
  ```
- **No `assert()`, no `abort()`** in public functions — errors return a sentinel value.
- Internal helpers may assert for invariant checking in debug builds.

### 8. Formatting

- Line endings: **LF** (`\n`), never CRLF.
- No trailing whitespace on any line.
- Files MUST end with a single newline.
- Build artifacts (`build/`, `*.o`, `*.exe`, etc.) MUST NOT be committed.

### 9. Cascade order

```
header change → tests → benchmarks → docs → gh-pages deploy
```

### 10. API hygiene

- **Macro prefixes are per-container.** No cross-container shared macros.
- Do not use internal helper functions (`_rb_*`, `_tp_*`, `_cclist_*`) outside their owning header — use only the public API.

### 11. Test style convention

All test files MUST use the standard test framework macros:

```c
static int passed, failed;
#define TEST(name) static void test_##name(void)
#define ASSERT(cond) do { \
  if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failed++; } \
  else passed++; \
} while(0)
#define RUN(name) do { printf("  %s\n", #name); test_##name(); } while(0)

int main(void) {
  printf("xxx tests:\n");
  RUN(test_a);
  RUN(test_b);
  printf("\n  %d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
```

- `ASSERT` uses `#cond` (auto-stringification) — **never** supply a manual message string.
- `fprintf(stderr, ...)` is NOT used — all output goes to `stdout`.
- Tests output `"%d passed, %d failed\n"` at exit; ctest matches `"0 failed"` / `"[1-9]* failed"`.
- Custom CHECK-style macros are NOT permitted — use the unified TEST/ASSERT/RUN.

### 12. Encapsulation — public API boundary

- **External code (tests, benchmarks, examples, users) MUST NOT access struct fields directly.** Use only the public API functions (`xxx_init`, `xxx_size`, `xxx_at`, etc.).
- **Internal implementation code SHOULD delegate to the public API** when it already provides the needed operation (e.g. `ccvector_front` delegates to `ccvector_at(v, 0)`) — this avoids duplicating NULL/bounds checks and keeps a single maintenance point.
- Exception: lifecycle functions (`init`, `destroy`, `clear`) and core operations (`push_back`, `pop_back`, `at`, `reserve`) necessarily write to internal fields. All other functions should prefer delegation.
- **Rule of thumb:** if a public getter/accessor exists for a field, use it — don't read `m->len` when `xxx_size(m)` is available.

---

*Generated from `include/*.h`.*
