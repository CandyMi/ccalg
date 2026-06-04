# alg

**header-only · 侵入式 · 零开销回调 · C89 兼容**

C/C++ 高性能数据结构库。每个容器都是单头文件，`#include` 即用。

## 容器

| 容器 | 结构 | 插入 | 查找 | 删除 | 迭代 |
| --- | --- | --- | --- | --- | --- |
| `ccmap` | 红黑树 | O(log n) | O(log n) | O(log n) | O(n) 有序 |
| `cchashmap` | 链式哈希表 | O(1) 均摊 | O(1) 均摊 | O(1) 均摊 | O(n) 无序 |
| `cclink` | 单向链表 | O(1) 头插 | O(n) | O(n) 任意 / O(1) 头 | O(n) |
| `cclist` | 双向链表 | O(1) 头/尾 | O(n) | O(1) 给定节点 | O(n) |
| `ccheap` | D-ary 堆 | O(log n) | — (peek O1) | O(log n) pop | — |

> 均摊复杂度基于默认配置（ccmap 红黑树保证最坏 O(log n)，cchashmap 负载因子 ≤1.25 保证均摊 O(1)）。

## 一分钟上手

```c
#include "ccmap.h"

struct entry { int key; ccmap_node_t node; };

static int64_t cmp(const ccmap_node_t *a, const ccmap_node_t *b) {
    return (int64_t)CCMAP_CONTAINER(a, struct entry, node)->key
         - (int64_t)CCMAP_CONTAINER(b, struct entry, node)->key;
}

int main() {
    ccmap_t m; ccmap_init(&m, cmp);
    struct entry e1 = {42}, e2 = {7}, e3 = {99};
    ccmap_insert(&m, &e1.node, NULL);
    ccmap_insert(&m, &e2.node, NULL);
    ccmap_insert(&m, &e3.node, NULL);
    // 有序遍历: 7 → 42 → 99
    for (ccmap_node_t *n = ccmap_begin(&m); n; n = ccmap_next(n))
        printf("%d\n", CCMAP_CONTAINER(n, struct entry, node)->key);
}
```

## 构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target check    # 构建 + 测试
cmake --build build --target bench    # 构建 + 基准
```

## 文档

| 文档 | 说明 |
| --- | --- |
| [快速开始](getting-started.md) | 5 分钟上手，完整示例 |
| [API 参考](api-reference.md) | 四个容器的完整 API 手册 |
| [构建指南](building.md) | CMake / Premake5 / 手动编译 |
| [性能基准](benchmarks.md) | ccmap / cchashmap / cclist / ccheap vs STL |

## 设计

- **侵入式** — 节点嵌入用户结构体，无隐藏分配
- **零开销回调** — 宏模式消除函数指针间接调用
- **NULL 安全** — 所有公有函数守卫 NULL 参数
- **C89 兼容** — MSVC / GCC / Clang 均可编译

[REASONIC.md](../REASONIC.md) · [AGENTS.md](../AGENTS.md) · BSD 3-Clause
