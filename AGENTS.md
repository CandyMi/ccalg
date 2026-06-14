# AGENTS.md

> Canonical reference for AI coding agents working on this project.

---

## Project identity

**ccalg** — a header-only C data-structure library, compatible with C89 / C99 / C++ / MSVC.

- **License:** BSD 3-Clause
- **Author:** CandyMi
- **Repository:** [github.com/CandyMi/ccalg](https://github.com/CandyMi/ccalg)
- **Website:** [ccalg.dev](https://ccalg.dev) (served via `gh-pages` branch)
- **Build:** CMake (primary) or Premake5 (alternative). Headers are self-contained; `#include` and use.
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
| [ccrandom.h](include/ccrandom.h) | `ccrandom128_t` / `ccrandom256_t` | Non-cryptographic PRNG (Xoroshiro128++ / Xoshiro256**) |

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
| `cctreap` | `cctreap_cmp_t`: `int64_t (*)(const cctreap_node_t*, const cctreap_node_t*)` (key) | `#define CCTREAP_COMPARE(a, b)` (key) <br> `#define CCTREAP_RAND(state)` (RNG) |
| `cclist` / `cclink` | none needed | none |

#### Comparison semantics

- Return type `int64_t`; `>0` → `a` ordered before `b`; `<0` → `b` before `a`; `==0` → equal
- For `ccmap` and `cctreap`, equality means duplicate key → insert returns `-1`

### Macro naming convention

Each container owns its namespace. Prefix = container abbreviation. No macros are shared cross-container.

| Container | Prefix | Key macros |
| --- | --- | --- |
| `ccmap` | `CCMAP_` | `CCMAP_INLINE`, `CCMAP_CONTAINER`, `CCMAP_COMPARE` |
| `cchashmap` | `CCHASHMAP_` | `CCHASHMAP_INLINE`, `CCHASHMAP_CONTAINER`, `CCHASHMAP_REALLOC` |
| `cclink` | `CCLINK_` | `CCLINK_INLINE`, `CCLINK_CONTAINER` |
| `cclist` | `CCLIST_` | `CCLIST_INLINE`, `CCLIST_CONTAINER` |
| `ccheap` | `CCHEAP_` | `CCHEAP_INLINE`, `CCHEAP_COMPARE`, `CCHEAP_REALLOC` |
| `ccvector` | `CCVECTOR_` | `CCVECTOR_INLINE`, `CCVECTOR_REALLOC` |
| `ccflatmap` | `CCFLATMAP_` | `CCFLATMAP_INLINE`, `CCFLATMAP_COMPARE`, `CCFLATMAP_REALLOC` |
| `cctreap` | `CCTREAP_` | `CCTREAP_INLINE`, `CCTREAP_CONTAINER`, `CCTREAP_COMPARE`, `CCTREAP_RAND` |
| `ccunicode` | `CCUNICODE_` | `CCUNICODE_INLINE` |
| `ccrandom` | `CCRANDOM_` | `CCRANDOM_INLINE` |

### Allocation hooks

Containers that allocate memory internally expose replaceable allocator macros:

| Macro | Purpose | Default |
| --- | --- | --- |
| `CCXXX_MALLOC(sz)` | Allocate | `CCXXX_REALLOC(NULL, sz)` |
| `CCXXX_REALLOC` | Reallocate | `realloc` |
| `CCXXX_FREE(ptr)` | Free | `free(ptr)` |

- `cchashmap`, `ccheap`, `ccvector`, `ccflatmap` use this mechanism.
- `ccmap`, `cclink`, `cclist`, `cctreap` have zero internal allocation → no allocator macros.

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
| Height | `ccmap_height(m)` / `cctreap_height(m)` — O(log n) estimate from `size` |
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

## Portability

Each header defines its own `CCXXX_INLINE` macro (pattern: `#define CCXXX_INLINE static inline`), user-overridable for C89 (`#define CCXXX_INLINE static` before `#include`).  `cclist.h` provides a C89 `bool` shim.  Each container guards its node typedef with `#ifndef CCXXX_NODE_T`.

---

## Per-Container Details

### ccmap — Red-Black Tree

- **Color encoding**: low bit of parent pointer.  Node 24B (64-bit): `child[2]` (16B) + `pc` (8B).
- Internal helpers prefixed `_rb_` (`_rb_p`, `_rb_c`, `_rb_red`, `_rb_sp`, `_rb_sc`, `_rb_spc`, `_rb_min`, `_rb_transplant`, `_rb_rot_left`, `_rb_rot_right`, `_rb_ins_fix`, `_rb_del_fix`).
- `_rb_p`/`_rb_c`/`_rb_red`/`_rb_sp`/`_rb_sc`/`_rb_spc` are macros — zero-overhead access to `pc` field.
- `_rb_transplant`: setne-based array index, eliminates 3-way branch.
- Rotations: hoisted child pointers + setne-based parent reattachment.

### cchashmap — Hash Map

- Chained hashing: array + singly-linked list.  Node caches hash value.
- Bucket array `cchashmap_node_t **` — lazy-allocated on first insert; 2× resize on load factor > 1.25.
- Power-of-two capacity: index via `hash & (cap - 1)`.
- Seed = container address `(size_t)(uintptr_t)m`.
- `destroy` frees bucket array only — caller must delete all nodes first.

### cclink — Singly-Linked List

- `head` + `tail` + `size`.  No sentinels.  `push` O(1) head, `push_back` O(1) tail, `remove` O(n).

### cclist — Doubly-Linked List

- `head`/`tail` sentinels + `size`.  All push/pop O(1).
- `splice_back`: O(1) bulk move.  `verify`: 6 checks.  `has_cycle`: Floyd.

### ccheap — D-ary Heap

- D-ary branching via `CCHEAP_ARITY` (2/4/8).  Child loops unrolled at compile-time via `#if CCHEAP_ARITY_N > N`.
- Pointer array `ccheap_node_t **` — 2× growth.  `ccheap_node_t` is a fixed 8B union (`priority`/`timeout`); users embed it and recover via `CCHEAP_CONTAINER`.

### ccvector — Dynamic Array

- Value storage in contiguous `CCVECTOR_NODE_T *buckets`.  2× growth from `CCVECTOR_DEFAULT_CAP=8`.
- `ccvector_at(v, i)` returns `NULL` on out-of-bounds (unlike `std::vector::operator[]`).

### ccflatmap — Sorted-Array Map

- Sorted-array map (like C++23 `std::flat_map`).  Binary search O(log n); insert/erase memmove O(n).
- **Bulk insert**: `push_back` (O(1) unsorted) × N + `sort` (O(N log N)).
- **Bulk erase**: `erase_unordered` (swap-with-last O(1)) × N + `sort`.
- `erase_at(m, pos)`: index-based erase, skips binary search.
- Internal sort: in-place quicksort (Hoare, median-of-three, insertion sort for ≤16).
- Default `ccflatmap_node_t`: `{ int64_t key; uint64_t value; }`.

### cctreap — Treap

- BST (key) + max-heap (priority).  Node 32B (64-bit): `child[2]` (16B) + `pc` (8B) + `size` (8B) + `priority` (8B).  Priority internal, xorshift64-generated on insert.
- Key comparison via `CCTREAP_COMPARE` (macro or fn-ptr). Priority is internal `uint64_t` with max-heap invariant, generated on insert via xorshift64 (overridable via `CCTREAP_RAND`).
- **Insert**: BST descent → bubble-up by priority (rotate while `_TP_PRIO_CMP(node, parent) > 0`).
- **Erase**: bubble-down to leaf (rotate toward higher-priority child) → transplant with NULL.
- `kth(m, k)`: 0-indexed k-th smallest; uses node `size` to binary-search.  O(log n) deterministic.
- `rank(m, probe)`: 0-indexed position; accumulates left-subtree sizes during descent.  Returns `-1` if not found.
- `first`/`last` recomputed from root after erase (bubble-down alters topology).
- Internal helpers prefixed `_tp_` (`_tp_p`, `_tp_sp`, `_tp_spc`, `_tp_sz`, `_tp_upd_sz`, `_tp_min`, `_tp_max`, `_tp_transplant`, `_tp_rot_left`, `_tp_rot_right`, `_tp_ins_fix`, `_tp_del_fix`).

### ccunicode — UTF-8 Codec

- **Zero state** — pure functions, no struct/state type to embed.
- `ccunicode_to_codepoint(str, len, &val)` → bytes consumed (1–4), 0 on error.
- `ccunicode_from_codepoint(val, str, &len)` → `bool` success.
- ASCII fast path (~70–90% of typical text) returns without touching lookup table.
- 256-byte `seq_len[]` table classifies first byte in one access — replaces if-else chain.
- Overlong encoding rejection, surrogate-half rejection (U+D800–U+DFFF), >U+10FFFF rejection.
- Branchless byte-count in encoder (compiler emits SETcc/CMOV).
- Fall-through switch for continuation bytes — eliminates loop overhead.
- Unicode 17.0 / RFC 3629 compliant.

### ccrandom — PRNG

- Two engines, value-based (no intrusive node):
  - `ccrandom128_t` — Xoroshiro128++, 2×u64 state, 2^128−1 period, fastest.
  - `ccrandom256_t` — Xoshiro256**, 4×u64 state, 2^256−1 period, higher statistical quality.
- Both pass BigCrush (TestU01) and PractRand ≥ 32 TiB.
- **Seeding**: `ccrandom128_init(&rng, seed)` / `ccrandom256_init(&rng, seed)` — 64-bit seed expanded via SplitMix64.  Any 64-bit value valid (zero included); seed is consumed, not stored.
- **u64 output**: `ccrandom128_next(&rng)` / `ccrandom256_next(&rng)` → [0, 2^64−1].
- **f32 output**: `ccrandom128_f32next(&rng)` / `ccrandom256_f32next(&rng)` → float [0, 1), 24-bit mantissa.
- **f64 output**: `ccrandom128_f64next(&rng)` / `ccrandom256_f64next(&rng)` → double [0, 1), 53-bit mantissa.
- Zero internal allocation.  Bit-identical across platforms for the same seed.
- Not cryptographic — predictable from ~2^64 outputs.
- No thread safety — one instance per thread.

---

Internal constants: `CCMAP_RED=0`, `CCMAP_BLACK=1`, `CCMAP_LEFT=0`, `CCMAP_RIGHT=1`.

---

## Build, Test, Benchmark

### CMake

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target check    # build + test
cmake --build build --target bench    # build + benchmark
cmake --install build --prefix /usr/local
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
| `test_ccrandom.c` | PRNG (Xoroshiro128++ / Xoshiro256**) |
| `test_ccunicode.c` | UTF-8 ↔ UCS-4 codec |

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

---

## New Container Checklist

1. [ ] Single header `include/ccxxx.h` with `#ifndef CCXXX_H` guard
2. [ ] `CCXXX_INLINE` portable-inline macro
3. [ ] Intrusive node `ccxxx_node_t` with `CCXXX_NODE_T` guard (or value-based for vector/flatmap)
4. [ ] `CCXXX_CONTAINER` → `container_of` (if intrusive)
5. [ ] Comparison/hash dual-mode dispatch (fn ptr + macro), `(void)arg` for unused params
6. [ ] `xxx_init` / `xxx_clear` (+ `xxx_destroy` if internal allocation)
7. [ ] `xxx_insert` / `xxx_find` / `xxx_erase`
8. [ ] `xxx_begin` / `xxx_end` / `xxx_next` (+ `xxx_prev` for ordered)
9. [ ] `xxx_size` (+ `xxx_height` if tree)
10. [ ] All public functions NULL-safe
11. [ ] Allocator hooks if the container allocates internally
12. [ ] Internal helpers prefixed `_xxx_`
13. [ ] BSD license header
14. [ ] Matching `tests/test_ccxxx.c` + `bench/bench_ccxxx.cpp`
15. [ ] Update `AGENTS.md`, `docs/api-reference.md`, `docs/algorithms.md`, `docs/index.md`

---

## Standard Operating Procedures

> **Mandatory.** These rules MUST be checked before every create / modify / debug / commit.  
> If uncertain about any rule's applicability, STOP and ask interactively — do NOT proceed without confirmation.

### 1. File header — License

Every source file (`*.h`, `*.c`, `*.cpp`) MUST include the BSD 3-Clause license header:

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

### 3. Language standard & C89 compatibility

- C99+ and C89 syntax may coexist in the project.
- **A single file MUST NOT mix C89 and C99+ syntax styles** (e.g. declarations at block start + mid-block declarations).  
  If mixing is detected, STOP and ask which standard to normalize to.
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
- The test MUST be registered in `CMakeLists.txt` so `cmake --build build --target check` runs it.
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

---

*Generated from `include/*.h`.*
