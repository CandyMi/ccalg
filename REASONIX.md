# REASONIX — Design Spec & API Reference

> Auto-generated from `include/*.h`.  Canonical reference for development and review.

---

## Project Overview

- **Project**: `cclag`
- **Type**: header-only C data-structure library (C89 / C99 / C++ / MSVC)
- **License**: BSD
- **Author**: CandyMi
- **Repository**: [github.com/CandyMi/ccalg](https://github.com/CandyMi/ccalg)
- **Website**: [ccalg.dev](https://ccalg.dev) (served via `gh-pages` branch)
- **Core traits**: intrusive, zero internal node allocation (with exceptions), compile-time zero-overhead callbacks

---

## 1. File Layout

| File | Structures | Description |
| --- | --- | --- |
| [ccmap.h](include/ccmap.h) | `ccmap_t` / `ccmap_node_t` | Intrusive red-black tree (ordered map) |
| [cchashmap.h](include/cchashmap.h) | `cchashmap_t` / `cchashmap_node_t` | Intrusive chained hash map |
| [cclink.h](include/cclink.h) | `cclink_t` / `cclink_node_t` | Intrusive singly-linked list |
| [cclist.h](include/cclist.h) | `cclist_t` / `cclist_node_t` | Intrusive doubly-linked list |
| [ccheap.h](include/ccheap.h) | `ccheap_t` / `ccheap_node_t` | D-ary heap (priority queue) |
| [ccvector.h](include/ccvector.h) | `ccvector_t` / `ccvector_node_t` | Dynamic array (value-based) |
| [ccflatmap.h](include/ccflatmap.h) | `ccflatmap_t` / `ccflatmap_node_t` | Sorted-array map (value-based) |

---

## 2. Intrusive Design

### 2.1 Node Embedding

Users embed `xxx_node_t` in their own structs.  The container only manages node pointers — it **never allocates or frees user struct memory**.

```c
struct my_entry {
    int key;
    ccmap_node_t node;    // embedded node
};
```

**Exceptions**: `cchashmap_t` internally manages a bucket array.  `ccvector_t` and `ccflatmap_t` store elements by **value** (not pointers) — they own element memory and copy on push/insert.

### 2.2 container_of Macro

Every container provides `CCXXX_CONTAINER(ptr, type, member)` to recover the parent struct pointer from a node pointer.  Implementation is standard `offsetof` + pointer arithmetic.

```
CCMAP_CONTAINER(ptr, type, member)
CCHASHMAP_CONTAINER(ptr, type, member)
CCLIST_CONTAINER(ptr, type, member)
CCHEAP_CONTAINER(ptr, type, member)
```

### 2.3 Node Type Guard

Each container's `typedef` is wrapped in `#ifndef CCXXX_NODE_T`, allowing users to pre-define their own node struct before `#include`.  **Exception**: `ccheap` — `CCHEAP_NODE_T` is fixed to `ccheap_node_t`; users embed it via `CCHEAP_CONTAINER`.

---

## 3. Portable Inline Macros

All public API functions are `static inline`, using compiler-detection macros:

```c
#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
  #define CCXXX_INLINE static inline     // C99 / C++
#elif defined(_MSC_VER)
  #define CCXXX_INLINE static __inline    // MSVC
#elif defined(__GNUC__)
  #define CCXXX_INLINE static __inline__  // GNU89
#else
  #define CCXXX_INLINE static             // fallback
#endif
```

Each container uses its own `CCXXX_INLINE` prefix (`CCMAP_INLINE`, `CCHASHMAP_INLINE`, `CCLIST_INLINE`, `CCHEAP_INLINE`).

---

## 4. Comparison / Hash Dispatch (Zero-Overhead Callbacks)

### 4.1 Two Modes

| Mode | Mechanism | Overhead |
| --- | --- | --- |
| **Function pointer** (default) | Pass callback to `init()` | Indirect call |
| **Macro** (opt-in) | `#define` comparison/hash before `#include` | Compile-time inline, zero overhead |

### 4.2 Dispatch Macros by Container

| Container | Function pointer type | Macro override |
| --- | --- | --- |
| `ccmap` | `ccmap_cmp_t`: `int64_t (*)(const ccmap_node_t*, const ccmap_node_t*)` | `#define CCMAP_COMPARE(a, b)` |
| `cchashmap` | `cchashmap_hash_t`: `uint64_t (*)(const cchashmap_node_t*, size_t seed)` <br> `cchashmap_equal_t`: `bool (*)(const cchashmap_node_t*, const cchashmap_node_t*)` | `#define CCHASHMAP_HASH(n, seed)` <br> `#define CCHASHMAP_EQUAL(a, b)` |
| `ccheap` | `ccheap_compare_t`: `int64_t (*)(const ccheap_node_t*, const ccheap_node_t*)` | `#define CCHEAP_COMPARE(a, b)` |
| `ccflatmap` | `ccflatmap_cmp_t`: `int64_t (*)(const CCFLATMAP_NODE_T*, const CCFLATMAP_NODE_T*)` | `#define CCFLATMAP_COMPARE(a, b)` |
| `cclist` | none needed | none |

When a macro is defined, the matching `init()` parameter is suppressed with `(void)arg`.

### 4.3 Comparison Semantics

- Return type `int64_t`
- `> 0` → `a` has higher priority (closer to root / smaller order)
- `< 0` → `b` has higher priority
- `== 0` → equal
- For `ccmap`, equality means duplicate key → insert returns `-1`

---

## 5. Allocator Hooks

Containers that allocate memory internally expose replaceable macros:

| Macro | Purpose | Default |
| --- | --- | --- |
| `CCXXX_MALLOC(sz)` | Allocate | `CCXXX_REALLOC(NULL, sz)` |
| `CCXXX_REALLOC` | Reallocate | `realloc` |
| `CCXXX_FREE(ptr)` | Free | `free(ptr)` |

- `cchashmap`, `ccheap`, `ccvector`, and `ccflatmap` use this mechanism.
- `ccmap`, `cclink`, and `cclist` have zero internal allocation → no allocator macros.

---

## 6. API Naming Conventions

### 6.1 Lifecycle

| Operation | Naming | Notes |
| --- | --- | --- |
| Init | `xxx_init(m, ...)` | Container pointer first, then callbacks |
| Destroy | `xxx_destroy(m)` | Frees internal resources only, not user nodes |
| Clear | `xxx_clear(m)` | Reset to empty, no free |

### 6.2 CRUD

| Operation | Generic | Alias | Returns |
| --- | --- | --- | --- |
| Insert | `xxx_insert(m, node, out)` | hashmap: `xxx_set` | `0` success / `-1` duplicate or fail |
| Find | `xxx_find(m, probe)` | hashmap: `xxx_get` | node pointer / `NULL` |
| Delete | `xxx_erase(m, node)` | hashmap: `xxx_del` | `void` |

**`out` parameter**: `ccmap_insert` and `cchashmap_set` accept `xxx_node_t **out`.  On duplicate, `*out` = existing node; on success, `*out` = inserted node (or `NULL`).

### 6.3 Iteration

| Operation | Naming | Notes |
| --- | --- | --- |
| Forward first | `xxx_begin(m)` / `xxx_first(m)` | First element in order |
| Reverse first | `xxx_rbegin(m)` | Reverse begin |
| End sentinel | `xxx_end(m)` | Always returns `NULL` |
| Successor | `xxx_next(n)` | Node → next |
| Predecessor | `xxx_prev(n)` | Node → previous |

**Convention**: `xxx_end()` always returns `NULL` as the iteration sentinel.  Standard loop:

```c
for (ccmap_node_t *n = ccmap_begin(&m); n != ccmap_end(&m); n = ccmap_next(n)) {
    struct my_entry *e = CCMAP_CONTAINER(n, struct my_entry, node);
}
```

`ccmap` maintains a `first` pointer for O(1) `ccmap_begin()` / `ccmap_first()`, with an insert fast-path when the new key is smaller than `first`.

**Exception**: `ccflatmap_next(m, p)` and `ccflatmap_prev(m, p)` take the container pointer as the first argument — arrays need bounds checking via `m->len`.

### 6.4 List-Specific Operations

| Operation | Naming |
| --- | --- |
| Head / tail push | `push_front` / `push_back` |
| Head / tail pop | `pop_front` / `pop_back` |
| Insert before / after | `insert_before` / `insert_after` |
| Remove | `remove` |
| Splice | `splice_back` (move src → dst tail) |
| Move | `move(to, from)` — splice with error code |

### 6.5 Size / Empty

| Operation | Naming |
| --- | --- |
| Size | `xxx_size(m)` |
| Empty | `xxx_empty(m)` (cclist only) |

---

## 7. Error Handling

### 7.1 Return Value Patterns

| Return | Meaning |
| --- | --- |
| `NULL` | Not found / empty / invalid parameter |
| `0` | Success |
| `-1` | Failure (invalid param / duplicate / out of memory) |
| `false` / `true` | Boolean ops (hashmap insert, etc.) |

### 7.2 NULL Safety

All public functions guard with `if (!m)` or `if (!m || !node)` at the top.  Passing `NULL` never crashes.

### 7.3 List Error Codes

`cclist` provides a detailed error-code enum `cclist_ecode_t` (`UNKNOWN`, `NOERROR`, `ISNULL`, `NOHEAD`, `NONEXT`, `HASCYCLE`, `MISSPREV`, `SIZEERROR`) and `cclist_verify()` for debugging doubly-linked-list invariants.

---

## 8. Configuration Macros

### Pre-Include Macros

| Macro | Container | Purpose | Default |
| --- | --- | --- | --- |
| `CCMAP_COMPARE(a,b)` | ccmap | Inline compare | none (use fn ptr) |
| `CCMAP_NODE_T` | ccmap | Override default node type | none |
| `CCHASHMAP_HASH(n,s)` | cchashmap | Inline hash | none |
| `CCHASHMAP_EQUAL(a,b)` | cchashmap | Inline equality | none |
| `CCHASHMAP_NODE_T` | cchashmap | Override default node type | none |
| `CCHASHMAP_MAX_LOAD` | cchashmap | Max load factor | `1.25` |
| `CCHASHMAP_DEFAULT_SLOT` | cchashmap | Initial bucket count | `64` |
| `CCHASHMAP_MAX_LOAD` | cchashmap | Max load factor | `1.25` |
| `CCHASHMAP_REALLOC` | cchashmap | Realloc function | `realloc` |
| `CCHASHMAP_MALLOC` | cchashmap | Alloc function | `realloc(NULL, sz)` |
| `CCHASHMAP_FREE` | cchashmap | Free function | `free(ptr)` |
| `CCHEAP_COMPARE(a,b)` | ccheap | Inline compare | none |
| `CCHEAP_ARITY` | ccheap | D-ary branching factor | `2` (or `4`/`8`) |
| `CCHEAP_REALLOC` | ccheap | Realloc | `realloc` |
| `CCHEAP_MALLOC` | ccheap | Alloc | `realloc(NULL, sz)` |
| `CCHEAP_FREE` | ccheap | Free | `free` |
| `CCHEAP_DEFAULT_CAP` | ccheap | Initial capacity | `32` |
| `CCHEAP_NODE_T` | ccheap | Node type (fixed, embed into user struct) | `ccheap_node_t` |
| `CCVECTOR_NODE_T` | ccvector | Element type | `ccvector_node_t` |
| `CCVECTOR_REALLOC` | ccvector | Realloc | `realloc` |
| `CCVECTOR_MALLOC` | ccvector | Alloc | `realloc(NULL, sz)` |
| `CCVECTOR_FREE` | ccvector | Free | `free` |
| `CCVECTOR_DEFAULT_CAP` | ccvector | Initial capacity | `8` |
| `CCFLATMAP_COMPARE(a,b)` | ccflatmap | Inline compare | none (use fn ptr) |
| `CCFLATMAP_NODE_T` | ccflatmap | Override default node type | `ccflatmap_node_t` |
| `CCFLATMAP_REALLOC` | ccflatmap | Realloc | `realloc` |
| `CCFLATMAP_MALLOC` | ccflatmap | Alloc | `realloc(NULL, sz)` |
| `CCFLATMAP_FREE` | ccflatmap | Free | `free` |
| `CCFLATMAP_DEFAULT_CAP` | ccflatmap | Initial capacity | `8` |

### Internal Constants

| Constant | Value | Description |
| --- | --- | --- |
| `CCMAP_RED` / `CCMAP_BLACK` | `0` / `1` | Red-black tree colors |
| `CCMAP_LEFT` / `CCMAP_RIGHT` | `0` / `1` | Tree directions |
| `CCHASHMAP_DEFAULT_SLOT` | `64` | Hashmap initial bucket count |
| `CCHEAP_DEFAULT_CAP` | `32` | Heap initial capacity |
| `CCVECTOR_DEFAULT_CAP` | `8` | Vector initial capacity |
| `CCFLATMAP_DEFAULT_CAP` | `8` | Flatmap initial capacity |

---

## 9. Red-Black Tree Details (ccmap)

- **Color encoding**: stored in the low bit of the parent pointer (`parent pointer | color`).  Saves `sizeof(uint32_t)` per node.
- **Node size**: `ccmap_node_t` is 16 bytes (64-bit): `child[2]` (16B) + `pc` (8B).
- **`first` pointer**: additionally tracks the minimum node, making `ccmap_first()` / `ccmap_begin()` O(1) and providing an insert fast-path when the new key is smaller than `first`.
- **Internal helpers**: prefixed `_rb_` (`_rb_p`, `_rb_c`, `_rb_red`, `_rb_sp`, `_rb_sc`, `_rb_spc`, `_rb_min`, `_rb_rot_left`, `_rb_rot_right`, `_rb_ins_fix`, `_rb_del_fix`).
- **Post-erase `first` update**: `ccmap_erase` takes O(log n) to find the new minimum.

---

## 10. Hash Map Details (cchashmap)

- **Chained hashing**: array + singly-linked list.  `cchashmap_node_t` caches the hash value in a `hash` field.
- **Load factor**: default `CCHASHMAP_MAX_LOAD = 1.25`.  Auto-resize (2×) when exceeded.
- **Seed**: container address `(size_t)(uintptr_t)m` is used as the hash seed during init.
- **Power-of-two capacity**: bucket index via `hash & (cap - 1)` bitmask.
- **Aliases**: `insert`=`set`, `find`=`get`, `erase`=`del` provided via `#define`.
- **`destroy`**: caller must delete all nodes before `cchashmap_destroy` — it only frees the bucket array.
- **`out` parameter**: `cchashmap_set`'s `old` parameter — set to existing node on duplicate, `NULL` on success.

---

## 11. Doubly-Linked List Details (cclist)

- **Structure**: `head` / `tail` sentinels + `size` counter.
- **Internal helpers**: `_cclist_link` and `_cclist_unlink` are not exposed; all public APIs call them.
- **Move / splice**: `cclist_move` returns detailed error codes (`-1` NULL, `-2` empty, `-3` self-move); internally calls `cclist_splice_back`.
- **Invariant verification**: `cclist_verify()` runs 6 checks, returns `cclist_ecode_t`.
- **Cycle detection**: Floyd's tortoise-and-hare `cclist_has_cycle()`.
- **bool compatibility**: C89 lacks `<stdbool.h>`; manually `typedef int bool` + `#define true/false`.

---

## 12. Heap Details (ccheap)

### 12.1 D-ary Unrolling

Child comparison loops are unrolled at compile time based on `CCHEAP_ARITY` (2/4/8) via `#if CCHEAP_ARITY_N > N` conditional compilation.  Index macros: `CCHEAP_PARENT(i)`, `CCHEAP_CHILD(i, k)`.

### 12.2 Default Node Structure

`ccheap_node_t` is an 8-byte anonymous union of `uint64_t priority` and `uint64_t timeout`.  Heap never accesses either member — the comparison callback defines the meaning.  The type is fixed; users embed it in their own structs and recover the parent with `CCHEAP_CONTAINER`.

---

## 13. Vector Details (ccvector)

- **Value storage**: elements stored by value in contiguous `CCVECTOR_NODE_T *buckets` array.  `push_back` copies the element into the array.
- **Auto-grow**: 2× on full, starting from `CCVECTOR_DEFAULT_CAP=8`.
- **Access**: `ccvector_at(v, i)` returns `NULL` on out-of-bounds (unlike `std::vector::operator[]`).
- **Element type**: overridable via `CCVECTOR_NODE_T` before `#include`.

---

## 14. Flat Map Details (ccflatmap)

### 14.1 Overview

Sorted-array map — elements stored by value in contiguous memory, maintained in sorted order.  Analogous to C++23 `std::flat_map`.

- **Find**: binary search, O(log n)
- **Insert**: binary search + memmove, O(n)
- **Erase**: binary search + memmove, O(n)
- **Iterate**: pointer arithmetic, O(1) per step, cache-friendly

### 14.2 Bulk Insert / Bulk Erase

For bulk loading, use unsorted append + sort to achieve O(n log n) total instead of O(n²):

**Bulk insert:**
```c
ccflatmap_reserve(&m, N);
for (int i = 0; i < N; i++)
    ccflatmap_push_back(&m, elements[i]);  // O(1) unsorted append
ccflatmap_sort(&m);                        // O(N log N) in-place quicksort
```

**Bulk erase:**
```c
for (int i = 0; i < N; i++)
    ccflatmap_erase_unordered(&m, &probes[i]);  // O(log n) + O(1) swap-pop
ccflatmap_sort(&m);                              // O(N log N) restore order
```

After `sort`, `find` / `iterate` work correctly.  `push_back` and `erase_unordered` do NOT maintain sorted order — the final `sort` restores it.  `erase_unordered` swaps the target with the last element before popping (O(1) removal); the array becomes unsorted after the first call.

### 14.3 `erase_at` — Index-Based Erase

```c
ccflatmap_reserve(&m, N);
for (int i = 0; i < N; i++)
    ccflatmap_push_back(&m, elements[i]);  // O(1) unsorted append
ccflatmap_sort(&m);                        // O(N log N) in-place quicksort
```

After `sort`, `find` / `erase` work correctly.  `push_back` does NOT check duplicates — `sort` does not remove them.  Use `ccflatmap_insert` if duplicate detection is required during loading.

`ccflatmap_erase_at(m, pos)` erases by index — skips the binary search when the position is already known (e.g. from a prior `find`).  O(n) memmove, same as erase-by-key.

### 14.4 Internal Sort

`ccflatmap_sort` uses an in-place quicksort (Hoare partition, median-of-three pivot, insertion sort for ≤16-element partitions).  The comparison is the same `CCFLATMAP_CMP` dispatch used by `find`/`insert`/`erase`.

### 14.5 Default Node Type

```c
typedef struct ccflatmap_node {
    int64_t  key;
    uint64_t value;
} ccflatmap_node_t;
```

Override via `#define CCFLATMAP_NODE_T struct my_pair` before `#include`.

### 14.6 Iteration Note

`ccflatmap_next(m, p)` and `ccflatmap_prev(m, p)` take the container pointer — unlike intrusive containers where `next(node)` suffices, array bounds require `m->len`.

---

## 15. Per-Container Macros (No Cross-Container Sharing)

| Macro | Source | Consumer |
| --- | --- | --- |
| `CCMAP_CONTAINER` (= `container_of`) | ccmap | ccmap only |
| `CCHASHMAP_CONTAINER` (= `container_of`) | cchashmap | cchashmap only |
| `CCMAP_INLINE` | ccmap | ccmap only |
| `CCHASHMAP_INLINE` | cchashmap | cchashmap only |
| `CCVECTOR_INLINE` | ccvector | ccvector only |
| `CCHEAP_CONTAINER` (= `container_of`) | ccheap | ccheap only |
| `CCHASHMAP_REALLOC` / `CCHASHMAP_MALLOC` / `CCHASHMAP_FREE` | cchashmap | hashmap allocator hooks |
| `CCHASHMAP_MAX_LOAD` / `CCHASHMAP_DEFAULT_SLOT` | cchashmap | hashmap config |

**Note**: macro prefixes are per-container (`CCMAP_` → ccmap, `CCHASHMAP_` → cchashmap, `CCLIST_` → cclist, `CCHEAP_` → ccheap).  No cross-container reuse.

---

## 16. New-Container Checklist

When adding a new data structure:

1. [ ] Single header in `include/`, guarded by `#ifndef CCXXX_H` / `#define CCXXX_H` / `#endif`
2. [ ] `CCXXX_INLINE` portable inline macro
3. [ ] Intrusive node `ccxxx_node_t` with `CCXXX_NODE_T` guard
4. [ ] `container_of` → `CCXXX_CONTAINER` macro
5. [ ] Comparison/hash dual-mode dispatch (fn ptr + macro override), `(void)arg` for unused params
6. [ ] `xxx_init` / `xxx_clear` / (optional `xxx_destroy`)
7. [ ] `xxx_insert` / `xxx_find` / `xxx_erase` (aliases `xxx_set` / `xxx_get` / `xxx_del` optional)
8. [ ] `xxx_begin` / `xxx_end` / `xxx_next` iteration trio; ordered containers add `xxx_prev`
9. [ ] `xxx_size`
10. [ ] All public functions NULL-safe
11. [ ] Allocator hooks `CCXXX_REALLOC` / `CCXXX_MALLOC` / `CCXXX_FREE` if the container allocates
12. [ ] Internal helpers prefixed `_xxx_` or `_ccxxx_`
13. [ ] `(void)arg` pattern for macro/function-pointer dual dispatch
14. [ ] BSD license header + brief design comment at top

---

## 17. Build, Test & Benchmark

### 17.1 CMake (Recommended)

```bash
# Configure — all artifacts in build/
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Full build (6 tests + 6 benchmarks)
cmake --build build

# Unit tests (CTest, label filter)
ctest --test-dir build -L unit --output-on-failure

# One-shot build + test
cmake --build build --target check

# One-shot build + benchmarks
cmake --build build --target bench

# Install headers to system
cmake --install build --prefix /usr/local
# → /usr/local/include/cclag/*.h
```

### 17.2 Premake5 (Alternative)

```bash
premake5 gmake2                   # Generate Makefile
make -C build config=release -j4  # Build (6 tests + 6 benchmarks)
```

Outputs: `build/test_*` (tests), `build/bench_*` (benchmarks).  Single target:

```bash
make -C build config=release test_cclink   # build singly-linked list test only
./build/test_cclink                        # run

# Install headers
PREFIX=/usr/local sh scripts/install.sh
```

### 17.3 Manual Compilation

No build system required:

```bash
# Test
gcc -std=c99 -Wall -Wextra -Wno-missing-field-initializers \
    -o test_ccmap tests/test_ccmap.c && ./test_ccmap

# Benchmark
g++ -std=c++11 -O2 -Wall -o bench_ccmap bench/bench_ccmap.cpp && ./bench_ccmap
```

### 17.4 Test Coverage

| Test File | Container | Assertions |
| --- | --- | --- |
| `tests/test_ccmap.c` | ccmap red-black tree | 2058 |
| `tests/test_cchashmap.c` | cchashmap hash map | 2043 |
| `tests/test_cclink.c` | cclink singly-linked list | 54 |
| `tests/test_cclist.c` | cclist doubly-linked list | 78 |
| `tests/test_ccheap.c` | ccheap D-ary heap | 1255 |
| `tests/test_ccvector.c` | ccvector dynamic array | 548 |
| `tests/test_ccflatmap.c` | ccflatmap sorted-array map | — |

### 17.5 Benchmark Comparisons

| Benchmark | Comparison | Scale |
| --- | --- | --- |
| `bench/bench_ccmap.cpp` | ccmap vs `std::map` | 100K |
| `bench/bench_cchashmap.cpp` | cchashmap vs `std::unordered_map` | 100K |
| `bench/bench_cclist.cpp` | cclist vs `std::list` | 200K |
| `bench/bench_cclink.cpp` | cclink vs `std::forward_list` | 200K |
| `bench/bench_ccheap.cpp` | ccheap vs `std::priority_queue` | 200K |
| `bench/bench_ccvector.cpp` | ccvector vs `std::vector` | 500K |
| `bench/bench_ccflatmap.cpp` | ccflatmap vs ccmap vs `std::map` | 50K |

---

## 18. Change Propagation

### 18.1 Cascade

Any change to core headers (`include/*.h`) must sync downstream:

```
include/*.h change
  → tests/test_*.c           (test coverage)
  → bench/bench_*.cpp        (re-collect benchmark data)
  → docs/*.md                (API docs + benchmark data)
  → build/docs-html/*.html   (regenerate HTML)
  → gh-pages branch          (deploy to GitHub Pages)
```

A header change is not complete until gh-pages is deployed.

### 18.2 Full Workflow

```
1. Modify code         include/*.h / tests/ / bench/ / docs/
2. Build + test        cmake --build build --target check
3. Update benchmarks   cmake --build build --target bench
                       → write results to docs/benchmarks.md
4. Update docs         sync docs/*.md with API changes
5. Generate HTML       cmake --build build --target docs-html
6. Commit + push       git add → commit → pull --rebase → push
7. Deploy GitHub Pages sh scripts/deploy-gh-pages.sh
```

### 18.3 Post-Install Include Path

Install commands:

```bash
cmake --install build --prefix /usr/local    # CMake
PREFIX=/usr/local sh scripts/install.sh      # Premake5 / script
```

After installation:

```c
#include "cclag/ccmap.h"
#include "cclag/cchashmap.h"
```

When using the repo directly, include paths are `<name>.h>` (requires `-I include`).

### 18.4 Pre-Push Checklist

```bash
git pull --rebase origin master   # pull + rebase
# Resolve conflicts → git add → git rebase --continue
# Rebuild + re-test to confirm no regressions
# Ask user for confirmation before pushing
```

---

*This document is auto-generated by Reasonix Code from `include/*.h`.*
