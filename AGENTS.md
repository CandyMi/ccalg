# AGENTS.md

## Project identity

**cclag** — a header-only C data-structure library, compatible with C89 / C99 / C++ / MSVC.

- **License:** BSD 3-Clause
- **Author:** CandyMi
- **Repository:** [github.com/CandyMi/ccalg](https://github.com/CandyMi/ccalg)
- **Website:** [ccalg.dev](https://ccalg.dev) (served via `gh-pages` branch)
- **No build system.** Every header is self-contained; `#include` and use.

## Repository layout

```
cclag/
├── include/
│   ├── ccmap.h       # Intrusive red-black tree (ordered map)
│   ├── cchashmap.h   # Intrusive chained hash map
│   ├── cclink.h      # Intrusive singly-linked list
│   ├── cclist.h      # Intrusive doubly-linked list
│   ├── ccheap.h      # D-ary heap (priority queue)
│   └── ccvector.h    # Dynamic array (value-based)
├── REASONIX.md       # Auto-generated design spec & API reference
├── README.md
├── LICENSE
├── tests/            # Unit tests (C, one per container)
├── bench/            # Benchmarks (C++ vs STL)
├── docs/             # User-facing documentation
├── scripts/          # Helper scripts (install, deploy, md2html)
├── CMakeLists.txt    # CMake build (primary)
└── premake5.lua      # Premake5 build (alternative)
```

## Architecture

### Intrusive containers

Every container manages **user-embedded nodes**, not allocated elements. The user embeds `xxx_node_t` in their struct; the container operates on node pointers. No `malloc`/`free` on user data — exceptions:

- `cchashmap` internally manages a bucket array.

### Container-of idiom

`CCXXX_CONTAINER(ptr, type, member)` — macro wrapping `container_of` / `offsetof` to recover the parent struct from a node pointer.

### Zero-overhead callbacks

Each container supports two comparison/hash dispatch modes:

| Mode | Mechanism | Cost |
|---|---|---|
| **Function pointer** (default) | Pass callback to `xxx_init()` | Indirect call |
| **Macro** (opt-in) | `#define CCXXX_COMPARE` / `CCXXX_HASH` / `CCXXX_EQUAL` before `#include` | Inlined, zero overhead |

When a macro is defined, the corresponding function-pointer parameter in `xxx_init()` is suppressed with `(void)arg`.

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

**No macro is shared across containers.** Each header defines its own `CCXXX_INLINE` and `CCXXX_CONTAINER` independently.

### Allocation hooks

Containers that allocate memory internally expose replaceable allocator macros:

```c
#define CCXXX_REALLOC  realloc          // default
#define CCXXX_MALLOC(sz) CCXXX_REALLOC(NULL, sz)
#define CCXXX_FREE(ptr)  CCXXX_REALLOC(ptr, 0)   // or free(ptr) for ccheap
```

- `cchashmap`, `ccheap`, and `ccvector` provide these.
- `ccmap`, `cclink`, and `cclist` have no internal allocation → no allocator macros.

### API naming conventions

| Operation | Naming | Notes |
|---|---|---|
| Init | `xxx_init(m, …)` | Container pointer first, then callbacks |
| Destroy | `xxx_destroy(m)` | Frees only internal resources, not user nodes |
| Clear | `xxx_clear(m)` | Resets to empty, no free |
| Insert | `xxx_insert(m, node, out)` | Alias `xxx_set` for hashmap |
| Find | `xxx_find(m, probe)` | Alias `xxx_get` for hashmap |
| Delete | `xxx_erase(m, node)` | Alias `xxx_del` for hashmap |
| Size | `xxx_size(m)` | |
| Iterator begin | `xxx_begin(m)` / `xxx_first(m)` | |
| Iterator end | `xxx_end(m)` | Always returns `NULL` |
| Next / Prev | `xxx_next(n)` / `xxx_prev(n)` | |

**`out` parameter:** `ccmap_insert` and `cchashmap_set` accept `xxx_node_t **out`. On duplicate key, `*out` is the existing node; on success, `*out` is the inserted node (or `NULL`).

### Error handling

- All public functions guard against `NULL` container / node.
- Return values: `0` = success, `-1` = failure/duplicate, `NULL` = not found, `bool` where appropriate.
- No `assert()`, no `abort()` — callers decide.

## Portability

Each header starts with a compiler-detection block for `static inline`:

```c
#if defined(__cplusplus) || (__STDC_VERSION__ >= 199901L)
  #define CCXXX_INLINE static inline
#elif defined(_MSC_VER)
  #define CCXXX_INLINE static __inline
#elif defined(__GNUC__)
  #define CCXXX_INLINE static __inline__
#else
  #define CCXXX_INLINE static
#endif
```

`cclist.h` additionally provides a C89 `bool` shim (typedef + `true`/`false`).

## Node-type override

Each container guards its `typedef` with `#ifndef CCXXX_NODE_T`. Users can pre-define the node struct and then `#define CCXXX_NODE_T` before `#include` to suppress the default.  **Exception**: `ccheap` — `CCHEAP_NODE_T` is fixed to `ccheap_node_t`; users always embed it and recover their parent struct with `CCHEAP_CONTAINER`.

## Adding a new container — checklist

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
13. [ ] BSD license header + brief design comment at top
14. [ ] Update `REASONIX.md` to reflect the new container

## Build, test, and benchmark

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
```

### Premake5 (alternative)

```bash
premake5 gmake2               # Generate Makefiles in build/
make -C build                 # Build
./build/bin/Release/test_*    # Run tests
```

### Test coverage

| File | Container | Assertions |
|---|---|---|
| `tests/test_ccmap.c` | Red-black tree | 2058 |
| `tests/test_cchashmap.c` | Hash map | 2043 |
| `tests/test_cclink.c` | Singly-linked list | 54 |
| `tests/test_cclist.c` | Doubly-linked list | 78 |
| `tests/test_ccheap.c` | D-ary heap | 1255 |
| `tests/test_ccvector.c` | Dynamic array | 548 |

### Benchmarks (C++ vs STL)

| File | Comparison | Scale |
|---|---|---|
| `bench/bench_ccmap.cpp` | `ccmap` vs `std::map` | 100K ops |
| `bench/bench_cchashmap.cpp` | `cchashmap` vs `std::unordered_map` | 100K ops |
| `bench/bench_cclink.cpp` | `cclink` vs `std::forward_list` | 200K ops |
| `bench/bench_cclist.cpp` | `cclist` vs `std::list` | 200K ops |
| `bench/bench_ccheap.cpp` | `ccheap` vs `std::priority_queue` | 200K ops |
| `bench/bench_ccvector.cpp` | `ccvector` vs `std::vector` | 500K ops |

## Git commit conventions

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
| `docs` | Documentation changes (docs/, REASONIX.md, AGENTS.md, README.md) |
| `build` | Build system (CMakeLists.txt, premake5.lua, scripts) |
| `style` | Formatting, comment changes (no code change) |
| `chore` | .gitignore, license, repo maintenance |

### Scopes

Use the container name where applicable: `ccmap`, `cchashmap`, `cclink`, `cclist`, `ccheap`.  
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

## Docs → HTML

Generate browsable HTML from `docs/*.md`:

```bash
cmake --build build --target docs-html
```

Output goes to `build/docs-html/`. Open `build/docs-html/index.html` in a browser.

Requires Python 3 with `markdown` package (`pip install markdown`). See `scripts/md2html.py` for the conversion script.

## Full development workflow

After modifying headers or docs, follow this sequence:

```
1. 修改代码           include/*.h 或 tests/或 docs/
2. 构建 + 测试        cmake --build build --target check
3. 更新基准数据        cmake --build build --target bench  → 更新 docs/benchmarks.md
4. 更新文档           docs/*.md 同步
5. 生成 HTML          cmake --build build --target docs-html
6. 提交 master        git add → commit → pull --rebase → push origin master
7. 部署 gh-pages      sh scripts/deploy-gh-pages.sh
```

### Install → include path

安装后头文件路径为 `cclag/<name>.h`：

```c
#include "cclag/ccmap.h"       // cmake --install 或 sh scripts/install.sh 后
#include "cclag/cchashmap.h"
```

直接使用仓库源码时路径为 `<name>.h`（需 `-I include`）。

## Editing conventions for this repo

### Language rules (mandatory)

- **README.md and `docs/*.md`** — always Chinese (中文).
- **REASONIX.md and AGENTS.md** — always English.
- Unless explicitly requested otherwise, all future additions and updates must follow these rules.

### Other conventions

- **REASONIX.md** is the canonical design reference — keep it in sync with header changes.
- **Build artifacts** go in `build/` (gitignored). Do not commit binaries in `tests/` or `bench/`.
- **Macro prefixes are per-container.** Do NOT add cross-container shared macros; each header is self-contained.
- **New containers** need matching test (`tests/test_ccxxx.c`) and benchmark (`bench/bench_ccxxx.cpp`).
- **Docs** live in `docs/` — keep `docs/index.md` updated when adding files.
- **Cascade updates.** When core code (include/*.h) changes, also update: tests → benchmarks/data → docs/md → docs-html → gh-pages. A header change is not complete until the HTML is deployed.
