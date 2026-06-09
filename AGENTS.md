# AGENTS.md

> Canonical reference for AI coding agents working on this project.
> Auto-generated from `include/*.h` by Reasonix Code.

---

## Project identity

**cclag** — a header-only C data-structure library, compatible with C89 / C99 / C++ / MSVC.

- **License:** BSD 3-Clause
- **Author:** CandyMi
- **Repository:** [github.com/CandyMi/ccalg](https://github.com/CandyMi/ccalg)
- **Website:** [ccalg.dev](https://ccalg.dev) (served via `gh-pages` branch)
- **No build system.** Every header is self-contained; `#include` and use.
- **Core traits:** intrusive, zero internal node allocation (with exceptions), compile-time zero-overhead callbacks.

---

## Repository layout

```
cclag/
├── include/
│   ├── ccmap.h       # Intrusive red-black tree (ordered map)
│   ├── cchashmap.h   # Intrusive chained hash map
│   ├── cclink.h      # Intrusive singly-linked list
│   ├── cclist.h      # Intrusive doubly-linked list
│   ├── ccheap.h      # D-ary heap (priority queue)
│   ├── ccvector.h    # Dynamic array (value-based)
│   └── ccflatmap.h   # Sorted-array map (value-based)
├── AGENTS.md         # This file — canonical design spec & API reference
├── README.md
├── LICENSE
├── tests/            # Unit tests (C, one per container)
├── bench/            # Benchmarks (C++ vs STL)
├── docs/             # User-facing documentation
├── scripts/          # Helper scripts (install, deploy, md2html)
├── CMakeLists.txt    # CMake build (primary)
└── premake5.lua      # Premake5 build (alternative)
```

### File map

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

## Architecture

### Intrusive containers

Every container manages **user-embedded nodes**, not allocated elements. The user embeds `xxx_node_t` in their struct; the container operates on node pointers.

```c
struct my_entry {
    int key;
    ccmap_node_t node;    // embedded node
};
```

The container **never allocates or frees user struct memory**.  Exceptions:

- `cchashmap` internally manages an inline bucket array (since v3).
- `ccvector` and `ccflatmap` store elements by **value** (not pointers) — they own element memory and copy on push/insert.

### Container-of idiom

`CCXXX_CONTAINER(ptr, type, member)` — macro wrapping `container_of` / `offsetof` to recover the parent struct from a node pointer.  Standard loop:

```c
for (ccmap_node_t *n = ccmap_begin(&m); n != ccmap_end(&m); n = ccmap_next(n)) {
    struct my_entry *e = CCMAP_CONTAINER(n, struct my_entry, node);
}
```

Available macros:

```
CCMAP_CONTAINER(ptr, type, member)
CCHASHMAP_CONTAINER(ptr, type, member)
CCLIST_CONTAINER(ptr, type, member)
CCHEAP_CONTAINER(ptr, type, member)
```

### Zero-overhead callbacks

Each container supports two comparison/hash dispatch modes:

| Mode | Mechanism | Overhead |
|---|---|---|
| **Function pointer** (default) | Pass callback to `xxx_init()` | Indirect call |
| **Macro** (opt-in) | `#define` comparison/hash before `#include` | Compile-time inline, zero overhead |

#### Dispatch signatures by container

| Container | Function pointer type | Macro override |
| --- | --- | --- |
| `ccmap` | `ccmap_cmp_t`: `int64_t (*)(const ccmap_node_t*, const ccmap_node_t*)` | `#define CCMAP_COMPARE(a, b)` |
| `cchashmap` | `cchashmap_hash_t`: `uint64_t (*)(const cchashmap_node_t*, size_t seed)` <br> `cchashmap_equal_t`: `bool (*)(const cchashmap_node_t*, const cchashmap_node_t*)` | `#define CCHASHMAP_HASH(n, seed)` <br> `#define CCHASHMAP_EQUAL(a, b)` |
| `ccheap` | `ccheap_compare_t`: `int64_t (*)(const ccheap_node_t*, const ccheap_node_t*)` | `#define CCHEAP_COMPARE(a, b)` |
| `ccflatmap` | `ccflatmap_cmp_t`: `int64_t (*)(const CCFLATMAP_NODE_T*, const CCFLATMAP_NODE_T*)` | `#define CCFLATMAP_COMPARE(a, b)` |
| `cclist` | none needed | none |

When a macro is defined, the matching `init()` parameter is suppressed with `(void)arg`.

#### Comparison semantics

- Return type `int64_t`
- `> 0` → `a` has higher priority (closer to root / smaller order)
- `< 0` → `b` has higher priority
- `== 0` → equal
- For `ccmap`, equality means duplicate key → insert returns `-1`

### Macro naming convention

Every container **owns its namespace**: prefix is always the container abbreviation.

| Container | Prefix | Example macros |
|---|---|---|
| `ccmap` | `CCMAP_` | `CCMAP_INLINE`, `CCMAP_CONTAINER`, `CCMAP_COMPARE`, `CCMAP_RED`, `CCMAP_BLACK` |
| `cchashmap` | `CCHASHMAP_` | `CCHASHMAP_INLINE`, `CCHASHMAP_CONTAINER`, `CCHASHMAP_REALLOC`, `CCHASHMAP_HASH` |
| `cclink` | `CCLINK_` | `CCLINK_INLINE`, `CCLINK_CONTAINER` |
| `cclist` | `CCLIST_` | `CCLIST_INLINE`, `CCLIST_CONTAINER` |
| `ccheap` | `CCHEAP_` | `CCHEAP_INLINE`, `CCHEAP_COMPARE`, `CCHEAP_REALLOC`, `CCHEAP_NODE_T` |
| `ccvector` | `CCVECTOR_` | `CCVECTOR_INLINE`, `CCVECTOR_REALLOC`, `CCVECTOR_NODE_T` |
| `ccflatmap` | `CCFLATMAP_` | `CCFLATMAP_INLINE`, `CCFLATMAP_COMPARE`, `CCFLATMAP_REALLOC`, `CCFLATMAP_NODE_T` |

**No macro is shared across containers.** Each header defines its own `CCXXX_INLINE` and `CCXXX_CONTAINER` independently.

### Allocation hooks

Containers that allocate memory internally expose replaceable allocator macros:

| Macro | Purpose | Default |
| --- | --- | --- |
| `CCXXX_MALLOC(sz)` | Allocate | `CCXXX_REALLOC(NULL, sz)` |
| `CCXXX_REALLOC` | Reallocate | `realloc` |
| `CCXXX_FREE(ptr)` | Free | `free(ptr)` |

- `cchashmap`, `ccheap`, `ccvector`, and `ccflatmap` use this mechanism.
- `ccmap`, `cclink`, and `cclist` have zero internal allocation → no allocator macros.

### API naming conventions

#### Lifecycle

| Operation | Naming | Notes |
| --- | --- | --- |
| Init | `xxx_init(m, ...)` | Container pointer first, then callbacks |
| Destroy | `xxx_destroy(m)` | Frees internal resources only, not user nodes |
| Clear | `xxx_clear(m)` | Reset to empty, no free |

#### CRUD

| Operation | Generic | Alias | Returns |
| --- | --- | --- | --- |
| Insert | `xxx_insert(m, node, out)` | hashmap: `xxx_set` | `0` success / `-1` duplicate or fail |
| Find | `xxx_find(m, probe)` | hashmap: `xxx_get` | node pointer / `NULL` |
| Delete | `xxx_erase(m, node)` | hashmap: `xxx_del` | `void` |

**`out` parameter**: `ccmap_insert` and `cchashmap_set` accept `xxx_node_t **out`.  On duplicate, `*out` = existing node; on success, `*out` = inserted node (or `NULL`).

#### Iteration

| Operation | Naming | Notes |
| --- | --- | --- |
| Forward first | `xxx_begin(m)` / `xxx_first(m)` | First element in order |
| Reverse first | `xxx_rbegin(m)` | Reverse begin |
| End sentinel | `xxx_end(m)` | Always returns `NULL` |
| Successor | `xxx_next(n)` | Node → next |
| Predecessor | `xxx_prev(n)` | Node → previous |

`ccmap` maintains a `first` pointer for O(1) `ccmap_begin()` / `ccmap_first()`, with an insert fast-path when the new key is smaller than `first`.

**Exception**: `ccflatmap_next(m, p)` and `ccflatmap_prev(m, p)` take the container pointer as the first argument — arrays need bounds checking via `m->len`.

#### List-specific operations

| Operation | Naming |
| --- | --- |
| Head / tail push | `push_front` / `push_back` |
| Head / tail pop | `pop_front` / `pop_back` |
| Insert before / after | `insert_before` / `insert_after` |
| Remove | `remove` |
| Splice | `splice_back` (move src → dst tail) |
| Move | `move(to, from)` — splice with error code |

#### Size / empty

| Operation | Naming |
| --- | --- |
| Size | `xxx_size(m)` |
| Empty | `xxx_empty(m)` (cclist only) |

### Error handling

#### Return value patterns

| Return | Meaning |
| --- | --- |
| `NULL` | Not found / empty / invalid parameter |
| `0` | Success |
| `-1` | Failure (invalid param / duplicate / out of memory) |
| `false` / `true` | Boolean ops (hashmap insert, etc.) |

#### NULL safety

All public functions guard with `if (!m)` or `if (!m || !node)` at the top.  Passing `NULL` never crashes.  No `assert()`, no `abort()` — callers decide.

#### List error codes

`cclist` provides a detailed error-code enum `cclist_ecode_t` (`UNKNOWN`, `NOERROR`, `ISNULL`, `NOHEAD`, `NONEXT`, `HASCYCLE`, `MISSPREV`, `SIZEERROR`) and `cclist_verify()` for debugging doubly-linked-list invariants.

---

## Portability

Each header starts with a compiler-detection block for `static inline`:

```c
#if defined(__cplusplus) || (__STDC_VERSION__ >= 199901L)
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

`cclist.h` additionally provides a C89 `bool` shim (typedef + `true`/`false`).

### Node-type override

Each container guards its `typedef` with `#ifndef CCXXX_NODE_T`. Users can pre-define the node struct and then `#define CCXXX_NODE_T` before `#include` to suppress the default.  **Exception**: `ccheap` — `CCHEAP_NODE_T` is fixed to `ccheap_node_t`; users always embed it and recover their parent struct with `CCHEAP_CONTAINER`.

---

## Per-Container Details

### Red-Black Tree (`ccmap`)

- **Color encoding**: stored in the low bit of the parent pointer (`parent pointer | color`).  Saves `sizeof(uint32_t)` per node.
- **Node size**: `ccmap_node_t` is 16 bytes (64-bit): `child[2]` (16B) + `pc` (8B).
- **`first` pointer**: additionally tracks the minimum node, making `ccmap_first()` / `ccmap_begin()` O(1) and providing an insert fast-path when the new key is smaller than `first`.
- **Internal helpers**: prefixed `_rb_` (`_rb_p`, `_rb_c`, `_rb_red`, `_rb_sp`, `_rb_sc`, `_rb_spc`, `_rb_min`, `_rb_transplant`, `_rb_rot_left`, `_rb_rot_right`, `_rb_ins_fix`, `_rb_del_fix`).
- **`_rb_transplant`**: replaces node `z` with `c` (may be NULL) — updates parent's child link and root.  Uses `setne`-based array index for the left-vs-right child link, eliminating one branch.
- **Accessor macros**: `_rb_p`/`_rb_c`/`_rb_red`/`_rb_sp`/`_rb_sc`/`_rb_spc` are macros (not `static inline`), guaranteeing zero-overhead pc field access.
- **Macro vs function-pointer mode**: enabling `CCMAP_COMPARE` inlines comparisons and eliminates indirect-call overhead — find becomes ~17% faster, matching or beating `std::map`; erase remains 8-11× faster regardless of mode.
- **Post-erase `first` update**: `ccmap_erase` takes O(log n) to find the new minimum.

### Hash Map (`cchashmap`)

- **Chained hashing**: array + singly-linked list.  `cchashmap_node_t` caches the hash value in a `hash` field.
- **Bucket array**: inline `cchashmap_node_t **` pointer array (since v3).  Lazy-allocated on first insert; resize doubles capacity, rehashing all chains.
- **Load factor**: default `CCHASHMAP_MAX_LOAD = 1.25`.  Auto-resize (2×) when exceeded.
- **Seed**: container address `(size_t)(uintptr_t)m` is used as the hash seed during init.
- **Power-of-two capacity**: bucket index via `hash & (cap - 1)` bitmask.
- **Aliases**: `insert`=`set`, `find`=`get`, `erase`=`del` provided via `#define`.
- **`destroy`**: caller must delete all nodes before `cchashmap_destroy` — it only frees the bucket array.
- **`out` parameter**: `cchashmap_set`'s `old` parameter — set to existing node on duplicate, `NULL` on success.

### Doubly-Linked List (`cclist`)

- **Structure**: `head` / `tail` sentinels + `size` counter.
- **Internal helpers**: `_cclist_link` and `_cclist_unlink` are not exposed; all public APIs call them.
- **Move / splice**: `cclist_move` returns detailed error codes (`-1` NULL, `-2` empty, `-3` self-move); internally calls `cclist_splice_back`.
- **Invariant verification**: `cclist_verify()` runs 6 checks, returns `cclist_ecode_t`.
- **Cycle detection**: Floyd's tortoise-and-hare `cclist_has_cycle()`.
- **bool compatibility**: C89 lacks `<stdbool.h>`; manually `typedef int bool` + `#define true/false`.

### D-ary Heap (`ccheap`)

#### D-ary unrolling

Child comparison loops are unrolled at compile time based on `CCHEAP_ARITY` (2/4/8) via `#if CCHEAP_ARITY_N > N` conditional compilation.  Index macros: `CCHEAP_PARENT(i)`, `CCHEAP_CHILD(i, k)`.

#### Pointer array (inline)

Internal pointer array is an inline `ccheap_node_t **` (since v3).  Grows 2× via `CCHEAP_REALLOC` on push; `heap->len` tracks element count.  Hot-path macros (`CCHEAP_AT`, `CCHEAP_PEEK`) index directly into `heap->data`.

#### Default node structure

`ccheap_node_t` is an 8-byte anonymous union of `uint64_t priority` and `uint64_t timeout`.  Heap never accesses either member — the comparison callback defines the meaning.  The type is fixed; users embed it in their own structs and recover the parent with `CCHEAP_CONTAINER`.

### Dynamic Array (`ccvector`)

- **Value storage**: elements stored by value in contiguous `CCVECTOR_NODE_T *buckets` array.  `push_back` copies the element into the array.
- **Auto-grow**: 2× on full, starting from `CCVECTOR_DEFAULT_CAP=8`.
- **Access**: `ccvector_at(v, i)` returns `NULL` on out-of-bounds (unlike `std::vector::operator[]`).
- **Element type**: overridable via `CCVECTOR_NODE_T` before `#include`.

### Sorted-Array Map (`ccflatmap`)

#### Overview

Sorted-array map — elements stored by value in contiguous memory, maintained in sorted order.  Analogous to C++23 `std::flat_map`.  Find: binary search O(log n).  Insert/erase: binary search + memmove O(n).  Iterate: pointer arithmetic O(1), cache-friendly.

#### Bulk insert / bulk erase

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

#### `erase_at` — index-based erase

`ccflatmap_erase_at(m, pos)` erases by index — skips the binary search when the position is already known (e.g. from a prior `find`).  O(n) memmove, same as erase-by-key.

#### Internal sort

`ccflatmap_sort` uses an in-place quicksort (Hoare partition, median-of-three pivot, insertion sort for ≤16-element partitions).  The comparison is the same `CCFLATMAP_CMP` dispatch used by `find`/`insert`/`erase`.

#### Default node type

```c
typedef struct ccflatmap_node {
    int64_t  key;
    uint64_t value;
} ccflatmap_node_t;
```

Override via `#define CCFLATMAP_NODE_T struct my_pair` before `#include`.

#### Iteration note

`ccflatmap_next(m, p)` and `ccflatmap_prev(m, p)` take the container pointer — unlike intrusive containers where `next(node)` suffices, array bounds require `m->len`.

---

## Configuration Macros

### Pre-include macros

| Macro | Container | Purpose | Default |
| --- | --- | --- | --- |
| `CCMAP_COMPARE(a,b)` | ccmap | Inline compare | none (use fn ptr) |
| `CCMAP_NODE_T` | ccmap | Override default node type | none |
| `CCHASHMAP_HASH(n,s)` | cchashmap | Inline hash | none |
| `CCHASHMAP_EQUAL(a,b)` | cchashmap | Inline equality | none |
| `CCHASHMAP_NODE_T` | cchashmap | Override default node type | none |
| `CCHASHMAP_MAX_LOAD` | cchashmap | Max load factor | `1.25` |
| `CCHASHMAP_DEFAULT_SLOT` | cchashmap | Initial bucket count | `64` |
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
| `CCVECTOR_INIT_CAP(v,c)` | ccvector | Init with explicit capacity (alias `ccvector_init_cap`) | none |
| `CCVECTOR_DEFAULT_CAP` | ccvector | Initial capacity | `8` |
| `CCFLATMAP_COMPARE(a,b)` | ccflatmap | Inline compare | none (use fn ptr) |
| `CCFLATMAP_NODE_T` | ccflatmap | Override default node type | `ccflatmap_node_t` |
| `CCFLATMAP_REALLOC` | ccflatmap | Realloc | `realloc` |
| `CCFLATMAP_MALLOC` | ccflatmap | Alloc | `realloc(NULL, sz)` |
| `CCFLATMAP_FREE` | ccflatmap | Free | `free` |
| `CCFLATMAP_DEFAULT_CAP` | ccflatmap | Initial capacity | `8` |

### Internal constants

| Constant | Value | Description |
| --- | --- | --- |
| `CCMAP_RED` / `CCMAP_BLACK` | `0` / `1` | Red-black tree colors |
| `CCMAP_LEFT` / `CCMAP_RIGHT` | `0` / `1` | Tree directions |
| `CCHASHMAP_DEFAULT_SLOT` | `64` | Hashmap initial bucket count |
| `CCHEAP_DEFAULT_CAP` | `32` | Heap initial capacity |
| `CCVECTOR_DEFAULT_CAP` | `8` | Vector initial capacity |
| `CCFLATMAP_DEFAULT_CAP` | `8` | Flatmap initial capacity |

---

## Per-Container Macro Independence

No cross-container macro sharing.  Each header is fully self-contained:

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

---

## Adding a New Container — Checklist

1. [ ] Single header in `include/`, guarded by `#ifndef CCXXX_H` / `#define CCXXX_H` / `#endif`
2. [ ] `CCXXX_INLINE` portable-inline macro
3. [ ] Intrusive node `ccxxx_node_t` with `CCXXX_NODE_T` guard
4. [ ] `CCXXX_CONTAINER` → `container_of`
5. [ ] Comparison/hash dual-mode dispatch (function pointer + optional macro), `(void)arg` for unused params
6. [ ] `xxx_init` / `xxx_clear` / (optional `xxx_destroy`)
7. [ ] `xxx_insert` / `xxx_find` / `xxx_erase` (+ optional `set`/`get`/`del` aliases)
8. [ ] `xxx_begin` / `xxx_end` / `xxx_next`; ordered containers add `xxx_prev`
9. [ ] `xxx_size`
10. [ ] All public functions NULL-safe
11. [ ] Allocator hooks `CCXXX_REALLOC` / `CCXXX_MALLOC` / `CCXXX_FREE` if the container allocates
12. [ ] Internal helpers prefixed `_xxx_` or `_ccxxx_`
13. [ ] `(void)arg` pattern for macro/function-pointer dual dispatch
14. [ ] BSD license header + brief design comment at top
15. [ ] Update `AGENTS.md` to reflect the new container

---

## Build, Test, and Benchmark

### CMake (primary)

```bash
# Configure (all artifacts isolated in build/)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build everything
cmake --build build

# Run unit tests (CTest with labels)
ctest --test-dir build -L unit --output-on-failure

# One-shot: build + test
cmake --build build --target check

# Run benchmarks
cmake --build build --target bench

# Install headers to system
cmake --install build --prefix /usr/local
# → /usr/local/include/cclag/*.h
```

### Premake5 (alternative)

```bash
premake5 gmake2                   # Generate Makefiles in build/
make -C build config=release -j4  # Build (6 tests + 6 benchmarks)

# Single target
make -C build config=release test_cclink
./build/test_cclink

# Install headers
PREFIX=/usr/local sh scripts/install.sh
```

### Manual compilation

No build system required:

```bash
# Test
gcc -std=c99 -Wall -Wextra -Wno-missing-field-initializers \
    -o test_ccmap tests/test_ccmap.c && ./test_ccmap

# Benchmark
g++ -std=c++11 -O2 -Wall -o bench_ccmap bench/bench_ccmap.cpp && ./bench_ccmap
```

### Test coverage

| Test File | Container | Assertions |
|---|---|---|
| `tests/test_ccmap.c` | Red-black tree | 2058 |
| `tests/test_cchashmap.c` | Hash map | 2043 |
| `tests/test_cclink.c` | Singly-linked list | 54 |
| `tests/test_cclist.c` | Doubly-linked list | 78 |
| `tests/test_ccheap.c` | D-ary heap | 1255 |
| `tests/test_ccvector.c` | Dynamic array | 548 |
| `tests/test_ccflatmap.c` | Sorted-array map | — |

### Benchmarks (C++ vs STL)

| Benchmark | Comparison | Scale |
|---|---|---|
| `bench/bench_ccmap.cpp` | `ccmap` vs `std::map` | 100K ops |
| `bench/bench_cchashmap.cpp` | `cchashmap` vs `std::unordered_map` | 100K ops |
| `bench/bench_cclink.cpp` | `cclink` vs `std::forward_list` | 200K ops |
| `bench/bench_cclist.cpp` | `cclist` vs `std::list` | 200K ops |
| `bench/bench_ccheap.cpp` | `ccheap` vs `std::priority_queue` | 200K ops |
| `bench/bench_ccvector.cpp` | `ccvector` vs `std::vector` | 500K ops |
| `bench/bench_ccflatmap.cpp` | `ccflatmap` vs `ccmap` vs `std::map` | 50K ops |

---

## Git Commit Conventions

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <description>

[optional body]
```

### Types

| Type | Use |
|---|---|
| `feat` | New container, new API, new feature |
| `fix` | Bug fix |
| `refactor` | Macro rename, internal restructure (no API change) |
| `test` | Add/update tests or benchmarks |
| `docs` | Documentation changes (docs/, AGENTS.md, README.md) |
| `build` | Build system (CMakeLists.txt, premake5.lua, scripts) |
| `style` | Formatting, comment changes (no code change) |
| `chore` | .gitignore, license, repo maintenance |

### Scopes

Use the container name where applicable: `ccmap`, `cchashmap`, `cclink`, `cclist`, `ccheap`, `ccflatmap`.  
Use `all` for cross-container changes, `ci` for CI/build, `docs` for documentation.

### Examples

```
feat(ccheap): add D-ary heap with inline comparator
fix(cchashmap): replace realloc(ptr,0) with free(ptr) for portability
refactor(cchashmap): rename CCMAP_ macros to CCHASHMAP_ prefix
test(all): add full test suite and STL benchmarks
docs: fill docs/ with getting-started, API reference, and build guide
build: add CMake + premake5, CTest integration, docs-html target
```

### Rules

- **One commit per logical change.** Don't mix a bugfix and a new feature.
- **First line ≤ 72 chars.** Wrap body at 72.
- **Use imperative mood** ("add" not "added").
- **Breaking changes** get `!` after type/scope: `feat(ccmap)!: change insert return type`
- **Pull before push.** Run `git pull --rebase` first; resolve conflicts, re-test, then ask user to confirm before pushing.
- **Sync gh-pages.** After pushing master, regenerate HTML and deploy to gh-pages: `sh scripts/deploy-gh-pages.sh`

---

## Docs → HTML

Generate browsable HTML from `docs/*.md`:

```bash
cmake --build build --target docs-html
```

Output goes to `build/docs-html/`. Open `build/docs-html/index.html` in a browser.

Requires Python 3 with `markdown` package (`pip install markdown`). See `scripts/md2html.py` for the conversion script.

---

## Change Propagation

### Cascade

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

### Full workflow

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

### Post-install include path

After installation:

```c
#include "cclag/ccmap.h"
#include "cclag/cchashmap.h"
```

When using the repo directly, include paths are `<name.h>` (requires `-I include`).

### Pre-push checklist

```bash
git pull --rebase origin master   # pull + rebase
# Resolve conflicts → git add → git rebase --continue
# Rebuild + re-test to confirm no regressions
# Ask user for confirmation before pushing
```

---

## Editing Conventions for This Repo

### Language rules (mandatory)

- **README.md and `docs/*.md`** — always Chinese (中文).
- **AGENTS.md** — always English.
- Unless explicitly requested otherwise, all future additions and updates must follow these rules.

### Other conventions

- **AGENTS.md** is the canonical reference — keep it in sync with header changes.
- **Build artifacts** go in `build/` (gitignored). Do not commit binaries in `tests/` or `bench/`.
- **Macro prefixes are per-container.** Do NOT add cross-container shared macros; each header is self-contained.
- **New containers** need matching test (`tests/test_ccxxx.c`) and benchmark (`bench/bench_ccxxx.cpp`).
- **Docs** live in `docs/` — keep `docs/index.md` updated when adding files.
- **Cascade updates.** When core code (include/*.h) changes, also update: tests → benchmarks/data → docs/md → docs-html → gh-pages. A header change is not complete until the HTML is deployed.

---

*This document is auto-generated by Reasonix Code from `include/*.h`.*
