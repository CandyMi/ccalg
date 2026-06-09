# cclag

**header-only · 侵入式 · 零开销回调 · C89 兼容**

C/C++ 高性能数据结构库。每个容器都是单头文件，`#include` 即用。

## 为什么选择 cclag

| 特性 | 说明 |
| --- | --- |
| **Header-only** | 无需编译、无需链接库，一个 `#include` 即用 |
| **侵入式容器** | 节点嵌入用户结构体，零 `malloc`/`free`，内存完全可控 |
| **零开销回调** | 宏模式将比较/哈希内联，消除函数指针间接调用开销 |
| **C89 兼容** | 支持 MSVC / GCC / Clang，可运行在嵌入式和老旧平台 |
| **无外部依赖** | 仅需标准头文件 (`stddef.h` / `stdint.h`)，无 libc 绑定 |
| **NULL 安全** | 所有公共函数守卫 NULL 参数，不会崩溃 |
| **按需取用** | 每个容器是独立单文件，只用需要的即可 |
| **独立命名空间** | 每个容器的宏前缀互不污染 |
| **读写分离** | 侵入式纯读操作不修改内部状态，天然适配外部 rwlock，支持 N 路读并发 |
| **无隐藏锁** | 容器本身不加锁，线程安全策略由调用方完全掌控，无线程竞争开销 |
| **BSD 协议** | 宽松许可，商用/闭源均可 |

## 容器

| 容器 | 结构 | 插入 | 查找 | 删除 | 迭代 |
| --- | --- | --- | --- | --- | --- |
| `ccmap` | 红黑树 | O(log n) | O(log n) | O(log n) | O(n) 有序 |
| `cchashmap` | 链式哈希表 | O(1) 均摊 | O(1) 均摊 | O(1) 均摊 | O(n) 无序 |
| `cclink` | 单向链表 | O(1) 头插 | O(n) | O(n) 任意 / O(1) 头 | O(n) |
| `cclist` | 双向链表 | O(1) 头/尾 | O(n) | O(1) 给定节点 | O(n) |
| `ccheap` | D-ary 堆 | O(log n) | — (peek O1) | O(log n) pop | — |
| `ccvector` | 动态数组 | O(1) 均摊 | O(1) 随机 | O(1) 尾部 | O(n) |
| `ccflatmap` | 排序数组映射 | O(n) | O(log n) 二分 | O(n) | O(n) 有序 |
| `cctreap` | Treap (树堆) | O(log n) 期望 | O(log n) 期望 | O(log n) 期望 | O(n) 有序 |

> `cctreap` 额外支持 O(log n) 的 `kth`（第 k 小）和 `rank`（排名查询）操作。均摊复杂度基于默认配置（ccmap 红黑树保证最坏 O(log n)，cchashmap 负载因子 ≤1.25 保证均摊 O(1)）。

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
| [API 参考](api-reference.md) | 所有容器的完整 API 手册 |
| [算法原理](algorithms.md) | 核心算法图解（红黑树、哈希、堆…） |
| [构建指南](building.md) | CMake / Premake5 / 手动编译 |
| [性能基准](benchmarks.md) | ccmap / cchashmap / cclink / cclist / ccheap / ccvector vs STL |
| [线程安全](thread-safety.md) | 外部读写锁下的操作分类与并行度分析 |
| treap 压测 | [性能基准](benchmarks.md) — cctreap vs std::map, rank 快 141× |

## 设计

- **侵入式** — 节点嵌入用户结构体，无隐藏分配
- **零开销回调** — 宏模式消除函数指针间接调用
- **读写分离** — 纯读操作不修改内部状态，外部 rwlock 即可实现 N 路读并发
- **无隐藏锁** — 容器不加锁，线程安全策略由调用方完全掌控
- **NULL 安全** — 所有公有函数守卫 NULL 参数
- **C89 兼容** — MSVC / GCC / Clang 均可编译

[ccalg.dev](https://ccalg.dev) · BSD 3-Clause
