# ccalg

**header-only · 侵入式 · 零开销回调 · C89 兼容**

```
纯C语言编写的 C/C++ 高性能数据结构库。每个容器都是直接 `#include` 开箱即用。
```

## 1. 选择

  为什么选择 ccalg ?

| 特性 | 说明 |
| --- | --- |
| **Header-only · C89** | 单头文件 `#include` 即用，兼容 MSVC / GCC / Clang，可运行在嵌入式平台 |
| **侵入式 · 零分配** | 节点嵌入用户结构体，无 `malloc`/`free`，内存完全可控 |
| **零开销回调** | 宏模式将比较/哈希编译期内联，消除函数指针间接调用 |
| **读写天然分离** | 纯读操作不修改内部状态，外部 rwlock 即可 N 路读并发，无线程竞争 |
| **NULL 安全 · BSD 协议** | 所有 API 守卫 NULL 参数，宽松许可商用/闭源均可 |

## 2. 容器

  当前支持的容器如下:

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

## 3. 上手

  现在给您一分钟阅读代码, 然后我们立刻快速上手使用高性能容器.

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

  是的, 就是这么快上手! 

## 4. 构建

  你问: "既然是 header-only 为什么还要构建?" , 还不是因为让你也能和我一样跑用例测试它 👍🏻 

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target check    # 构建 + 测试
cmake --build build --target bench    # 构建 + 基准
```

## 5. 最后

  希望你能喜欢上它! 😃😃😃😃😃