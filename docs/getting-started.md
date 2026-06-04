# 快速开始

## 1. 引入头文件

alg 是 header-only 库，无需编译链接。将 `include/` 加入头文件搜索路径即可：

```c
#include "ccmap.h"      // 红黑树
#include "cchashmap.h"  // 哈希表
#include "cclist.h"     // 双向链表
#include "ccheap.h"     // 优先队列
```

## 2. ccmap — 红黑树（有序映射）

```c
#include <stdio.h>
#include "ccmap.h"

struct entry { int key; ccmap_node_t node; };

/* 比较函数：按 key 升序 */
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

    /* 有序遍历: 7 → 42 → 99 */
    for (ccmap_node_t *n = ccmap_begin(&m); n != ccmap_end(&m); n = ccmap_next(n))
        printf("%d\n", CCMAP_CONTAINER(n, struct entry, node)->key);

    /* 查找 */
    struct entry probe = {42};
    ccmap_node_t *f = ccmap_find(&m, &probe.node);
    if (f) printf("found: %d\n", CCMAP_CONTAINER(f, struct entry, node)->key);

    return 0;
}
```

**零开销模式** — 用宏替代函数指针，消除间接调用：

```c
#define CCMAP_COMPARE(a, b) \
    ((int64_t)CCMAP_CONTAINER(a, struct entry, node)->key \
   - (int64_t)CCMAP_CONTAINER(b, struct entry, node)->key)
#include "ccmap.h"
/* ccmap_init(&m, NULL) — 第二个参数被忽略 */
```

## 3. cchashmap — 哈希表

```c
#include <stdio.h>
#include "cchashmap.h"

struct entry { int key; cchashmap_node_t node; };

static uint64_t hash(const cchashmap_node_t *n, size_t seed) {
    (void)seed;
    return CCHASHMAP_CONTAINER(n, struct entry, node)->key;
}
static bool equal(const cchashmap_node_t *a, const cchashmap_node_t *b) {
    return CCHASHMAP_CONTAINER(a, struct entry, node)->key
        == CCHASHMAP_CONTAINER(b, struct entry, node)->key;
}

int main() {
    cchashmap_t m; cchashmap_init(&m, hash, equal);

    struct entry e = {42};
    cchashmap_node_t *old;
    if (cchashmap_set(&m, &e.node, &old))
        printf("inserted\n");
    else
        printf("duplicate, existing node: %p\n", (void*)old);

    /* 查找 */
    struct entry probe = {42};
    cchashmap_node_t *f = cchashmap_get(&m, &probe.node);

    /* 删除 + 销毁 */
    cchashmap_del(&m, &e.node);
    cchashmap_destroy(&m);
    return 0;
}
```

## 4. cclist — 双向链表

```c
#include <stdio.h>
#include "cclist.h"

struct entry { int val; cclist_node_t node; };

int main() {
    cclist_t l; cclist_init(&l);

    struct entry e1 = {1}, e2 = {2}, e3 = {3};
    cclist_push_back(&l, &e1.node);
    cclist_push_back(&l, &e2.node);
    cclist_push_back(&l, &e3.node);

    /* 正向遍历: 1, 2, 3 */
    for (cclist_node_t *n = cclist_begin(&l); n != cclist_end(&l); n = cclist_next(n))
        printf("%d\n", CCLIST_CONTAINER(n, struct entry, node)->val);

    /* splice: 合并两个链表 */
    cclist_t src; cclist_init(&src);
    struct entry e4 = {4}, e5 = {5};
    cclist_push_back(&src, &e4.node);
    cclist_push_back(&src, &e5.node);
    cclist_splice_back(&l, &src);  // src 变空, l = [1,2,3,4,5]

    /* 验证双向不变量 */
    if (cclist_verify(&l) == NOERROR)
        printf("list is valid\n");

    return 0;
}
```

## 5. ccheap — 优先队列

```c
#include <stdio.h>
#include "ccheap.h"

struct node { uint32_t conv; uint32_t priority; };

/* min-heap: 值越小优先级越高 */
static int64_t cmp(const struct node *a, const struct node *b) {
    return (int64_t)b->priority - (int64_t)a->priority;
}

int main() {
    ccheap_t h; heap_init(&h, cmp);

    struct node n1 = {0, 50}, n2 = {0, 10}, n3 = {0, 30};
    heap_push(&h, &n1);
    heap_push(&h, &n2);
    heap_push(&h, &n3);

    /* 弹出顺序: 10, 30, 50 */
    while (heap_size(&h) > 0) {
        struct node *top = heap_pop(&h);
        printf("%u\n", top->priority);
    }

    heap_destroy(&h);
    return 0;
}
```

### CCHEAP_VALUE 模式（连续存储，更好缓存局部性）

```c
#define CCHEAP_VALUE
#include "ccheap.h"
/* 节点以值形式存储在连续数组中，堆拥有元素内存 */
/* 注意：节点含指针字段时用默认指针模式 */
```

## 6. 构建与测试

```bash
# CMake 一键构建 + 测试
cmake -S . -B build && cmake --build build --target check

# 基准测试
cmake --build build --target bench
```

详见 [构建指南](building.md)。
