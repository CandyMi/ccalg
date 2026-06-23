# ccalg

[![CI](https://img.shields.io/github/actions/workflow/status/CandyMi/ccalg/ci.yml?branch=master&logo=github)](https://github.com/CandyMi/ccalg/actions)
[![License](https://img.shields.io/badge/license-BSD%203--Clause-blue.svg)](LICENSE)
[![C/C++](https://img.shields.io/badge/language-C%20/%20C%2B%2B-00599c.svg)]()
[![Header-only](https://img.shields.io/badge/header--only-true-brightgreen.svg)]()

A header-only library for high-performance data structures in C/C++.

Compatible with C89 / C99 / C++ / MSVC.  BSD 3-Clause.

## Containers

| Container | File | Structure | Allocates? |
|---|---|---|---|
| `ccmap` | [`include/ccmap.h`](include/ccmap.h) | Intrusive red-black tree (ordered map) | No |
| `cchashmap` | [`include/cchashmap.h`](include/cchashmap.h) | Intrusive chained hash map | Buckets |
| `cclink` | [`include/cclink.h`](include/cclink.h) | Intrusive singly-linked list | No |
| `cclist` | [`include/cclist.h`](include/cclist.h) | Intrusive doubly-linked list | No |
| `ccheap` | [`include/ccheap.h`](include/ccheap.h) | D-ary heap (priority queue) | Pointer array |
| `ccvector` | [`include/ccvector.h`](include/ccvector.h) | Dynamic array | Yes |
| `ccflatmap` | [`include/ccflatmap.h`](include/ccflatmap.h) | Sorted-array map (flat map) | Yes |
| `cctreap` | [`include/cctreap.h`](include/cctreap.h) | Intrusive treap (order statistics) | No |
| `ccrandom` | [`include/ccrandom.h`](include/ccrandom.h) | PRNG (Xoroshiro128++ / Xoshiro256** / Xoshiro512**) | No |
| `ccunicode` | [`include/ccunicode.h`](include/ccunicode.h) | UTF-8 ↔ UCS-4 codec | No |
| `ccshuffle` | [`include/ccshuffle.h`](include/ccshuffle.h) | Fisher-Yates shuffle | No |

Most containers are **intrusive** — nodes are embedded in user structs, no hidden allocations. `ccvector` and `ccflatmap` store elements by **value**.

## Quick start

```c
#include <stdio.h>
#include "ccmap.h"

struct entry { int key; ccmap_node_t node; };

static int64_t cmp(const ccmap_node_t *a, const ccmap_node_t *b) {
    return (int64_t)CCMAP_CONTAINER(a, struct entry, node)->key
         - (int64_t)CCMAP_CONTAINER(b, struct entry, node)->key;
}

int main() {
    ccmap_t m; ccmap_init(&m, cmp);
    struct entry e = {42};
    ccmap_insert(&m, &e.node, NULL);

    for (ccmap_node_t *n = ccmap_begin(&m); n; n = ccmap_next(n))
        printf("%d\n", CCMAP_CONTAINER(n, struct entry, node)->key);
}
```

More examples → [docs/getting-started.md](docs/getting-started.md).  
Full API → [docs/api-reference.md](docs/api-reference.md).

## Build, test, benchmark

```bash
# Configure (artifacts in build/)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build everything
cmake --build build

# Run unit tests
ctest --test-dir build -L unit --output-on-failure

# One-shot build + test
cmake --build build --target check

# Run benchmarks (C++ STL comparison)
cmake --build build --target bench
```

See [docs/building.md](docs/building.md) for details and manual compilation.

## Documents

[Online documentation (only chinease)](https://ccalg.dev)

## License

BSD 3-Clause © CandyMi
