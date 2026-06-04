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
| [ccheap.h](include/ccheap.h) | `ccheap_t` / `ccheap_node_t` | D-ary heap (priority queue; pointer + value dual-mode) |

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

**Exceptions**: `cchashmap_t` internally manages a bucket array.  `ccheap_t` in `CCHEAP_VALUE` mode owns element memory.

### 2.2 container_of Macro

Every container provides `CCXXX_CONTAINER(ptr, type, member)` to recover the parent struct pointer from a node pointer.  Implementation is standard `offsetof` + pointer arithmetic.

```
CCMAP_CONTAINER(ptr, type, member)
CCHASHMAP_CONTAINER(ptr, type, member)
CCLIST_CONTAINER(ptr, type, member)
(ccheap has no CONTAINER; access via direct fields of ccheap_node_t)
```

### 2.3 Node Type Guard

Each container's `typedef` is wrapped in `#ifndef CCXXX_NODE_T`, allowing users to pre-define their own node struct before `#include`.

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

- `cchashmap` and `ccheap` use this mechanism.
- `ccmap` and `cclist` have zero internal allocation → no allocator macros.

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
| `CCHASHMAP_REALLOC` | cchashmap | Realloc function | `realloc` |
| `CCHASHMAP_MALLOC` | cchashmap | Alloc function | `realloc(NULL, sz)` |
| `CCHASHMAP_FREE` | cchashmap | Free function | `free(ptr)` |
| `CCHEAP_COMPARE(a,b)` | ccheap | Inline compare | none |
| `CCHEAP_VALUE` | ccheap | Value mode (store elements directly) | none (ptr mode) |
| `CCHEAP_ARITY` | ccheap | D-ary branching factor | `2` (or `4`/`8`) |
| `CCHEAP_REALLOC` | ccheap | Realloc | `realloc` |
| `CCHEAP_MALLOC` | ccheap | Alloc | `realloc(NULL, sz)` |
| `CCHEAP_FREE` | ccheap | Free | `free` |
| `CCHEAP_DEFAULT_CAP` | ccheap | Initial capacity | `32` |
| `CCNODE_T` | ccheap | Override default node type | `struct ccheap_node` |

### Internal Constants

| Constant | Value | Description |
| --- | --- | --- |
| `CCMAP_RED` / `CCMAP_BLACK` | `0` / `1` | Red-black tree colors |
| `CCMAP_LEFT` / `CCMAP_RIGHT` | `0` / `1` | Tree directions |
| `CCHASHMAP_DEFAULT_SLOT` | `8` | Hashmap initial bucket count |
| `CCHEAP_DEFAULT_CAP` | `32` | Heap initial capacity |

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

### 12.1 Dual Storage Modes

| Mode | Definition | Data layout | Ownership |
| --- | --- | --- | --- |
| **Pointer mode** (default) | none | `CCNODE_T **data` (pointer array) | Caller owns nodes |
| **Value mode** | `#define CCHEAP_VALUE` | `CCNODE_T *data` (contiguous values) | Heap owns elements |

Value mode note (from header):
> IMPORTANT: value mode uses struct assignment (shallow copy). If your node_t contains pointers (e.g. `char *name`), use pointer mode instead.

### 12.2 Value-Mode Pop Trap

`heap_pop` in value mode returns a pointer to an internal buffer `heap->popped`.  **The next `heap_pop` call overwrites this buffer.**  Callers must consume or copy the return value before the next pop.

### 12.3 D-ary Unrolling

Child comparison loops are unrolled at compile time based on `CCHEAP_ARITY` (2/4/8) via `#if CCHEAP_ARITY_N > N` conditional compilation.  Index macros: `CCHEAP_PARENT(i)`, `CCHEAP_CHILD(i, k)`.

### 12.4 Default Node Structure

`ccheap_node_t` provides `conv` and `timestamp` fields by default.  Override with `#define CCNODE_T`.

---

## 13. Per-Container Macros (No Cross-Container Sharing)

| Macro | Source | Consumer |
| --- | --- | --- |
| `CCMAP_CONTAINER` (= `container_of`) | ccmap | ccmap only |
| `CCHASHMAP_CONTAINER` (= `container_of`) | cchashmap | cchashmap only |
| `CCMAP_INLINE` | ccmap | ccmap only |
| `CCHASHMAP_INLINE` | cchashmap | cchashmap only |
| `CCHASHMAP_REALLOC` / `CCHASHMAP_MALLOC` / `CCHASHMAP_FREE` | cchashmap | hashmap allocator hooks |
| `CCHASHMAP_MAX_LOAD` / `CCHASHMAP_DEFAULT_SLOT` | cchashmap | hashmap config |

**Note**: macro prefixes are per-container (`CCMAP_` → ccmap, `CCHASHMAP_` → cchashmap, `CCLIST_` → cclist, `CCHEAP_` → ccheap).  No cross-container reuse.

---

## 14. New-Container Checklist

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

## 15. Build, Test & Benchmark

### 15.1 CMake (Recommended)

```bash
# Configure — all artifacts in build/
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Full build (5 tests + 5 benchmarks)
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

### 15.2 Premake5 (Alternative)

```bash
premake5 gmake2                   # Generate Makefile
make -C build config=release -j4  # Build (5 tests + 5 benchmarks)
```

Outputs: `build/test_*` (tests), `build/bench_*` (benchmarks).  Single target:

```bash
make -C build config=release test_cclink   # build singly-linked list test only
./build/test_cclink                        # run

# Install headers
PREFIX=/usr/local sh scripts/install.sh
```

### 15.3 Manual Compilation

No build system required:

```bash
# Test
gcc -std=c99 -Wall -Wextra -Wno-missing-field-initializers \
    -o test_ccmap tests/test_ccmap.c && ./test_ccmap

# Benchmark
g++ -std=c++11 -O2 -Wall -o bench_ccmap bench/bench_ccmap.cpp && ./bench_ccmap
```

### 15.4 Test Coverage

| Test File | Container | Assertions |
| --- | --- | --- |
| `tests/test_ccmap.c` | ccmap red-black tree | 2058 |
| `tests/test_cchashmap.c` | cchashmap hash map | 2043 |
| `tests/test_cclink.c` | cclink singly-linked list | 54 |
| `tests/test_cclist.c` | cclist doubly-linked list | 78 |
| `tests/test_ccheap.c` | ccheap D-ary heap | 1255 |

### 15.5 Benchmark Comparisons

| Benchmark | Comparison | Scale |
| --- | --- | --- |
| `bench/bench_ccmap.cpp` | ccmap vs `std::map` | 100K |
| `bench/bench_cchashmap.cpp` | cchashmap vs `std::unordered_map` | 100K |
| `bench/bench_cclist.cpp` | cclist vs `std::list` | 200K |
| `bench/bench_cclink.cpp` | cclink vs `std::forward_list` | 200K |
| `bench/bench_ccheap.cpp` | ccheap vs `std::priority_queue` | 200K |

---

## 16. Change Propagation

### 16.1 Cascade

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

### 16.2 Full Workflow

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

### 16.3 Post-Install Include Path

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

### 16.4 Pre-Push Checklist

```bash
git pull --rebase origin master   # pull + rebase
# Resolve conflicts → git add → git rebase --continue
# Rebuild + re-test to confirm no regressions
# Ask user for confirmation before pushing
```

---

*This document is auto-generated by Reasonix Code from `include/*.h`.*
