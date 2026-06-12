# 快速开始

> 每个容器 5 分钟上手，完整可编译示例。按 ccmap → cctreap 顺序排列。

## 1. 引入头文件

ccalg 是 header-only 库，无需编译链接。两种引入方式：

**方式一** — 直接从仓库 include 目录：

```c
#include "ccmap.h"
// gcc -I path/to/ccalg/include main.c
```

**方式二** — 安装后（推荐）：

```bash
cmake --install build --prefix /usr/local   # 或 sh scripts/install.sh
```

```c
#include "ccalg/ccmap.h"      // 红黑树
#include "ccalg/cchashmap.h"  // 哈希表
#include "ccalg/cclink.h"     // 单向链表
#include "ccalg/cclist.h"     // 双向链表
#include "ccalg/ccheap.h"     // 优先队列
#include "ccalg/ccunicode.h"  // UTF-8 编解码
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

## 4. cclink — 单向链表

```c
#include <stdio.h>
#include "cclink.h"

struct entry { int val; cclink_node_t node; };

int main() {
    cclink_t l; cclink_init(&l);

    struct entry e1 = {1}, e2 = {2}, e3 = {3};
    cclink_push_back(&l, &e1.node);
    cclink_push_back(&l, &e2.node);
    cclink_push_back(&l, &e3.node);

    /* 正向遍历: 1, 2, 3 */
    for (cclink_node_t *n = cclink_begin(&l); n != cclink_end(&l); n = cclink_next(n))
        printf("%d\n", CCLINK_CONTAINER(n, struct entry, node)->val);

    /* pop_front: 取出头部 */
    while (!cclink_empty(&l)) {
        cclink_node_t *n = cclink_pop_front(&l);
        printf("popped: %d\n", CCLINK_CONTAINER(n, struct entry, node)->val);
    }

    return 0;
}
```

## 5. cclist — 双向链表

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

## 6. ccheap — 优先队列 / 定时器

`ccheap_node_t` 是一个 8 字节 union，包含 `priority` 和 `timeout`——堆不访问字段，由比较器定义语义。

```c
#include <stdio.h>
#define CCHEAP_COMPARE(a, b) \
    ((int64_t)(b)->priority - (int64_t)(a)->priority)
#include "ccheap.h"

int main() {
    ccheap_t h;
    heap_init(&h, NULL);  // CCHEAP_COMPARE 定义后第二个参数被忽略

    ccheap_node_t n1 = {.priority = 50};
    ccheap_node_t n2 = {.priority = 10};
    ccheap_node_t n3 = {.priority = 30};
    heap_push(&h, &n1);
    heap_push(&h, &n2);
    heap_push(&h, &n3);

    /* 弹出顺序: .priority = 10, 30, 50 */
    while (heap_size(&h) > 0) {
        ccheap_node_t *top = heap_pop(&h);
        printf("%llu\n", (unsigned long long)top->priority);
    }

    heap_destroy(&h);
    return 0;
}
```

### 嵌入自定义结构体（携带额外数据）

```c
struct task { ccheap_node_t node; void (*cb)(void *); void *arg; };

static int64_t cmp(const ccheap_node_t *a, const ccheap_node_t *b) {
    return (int64_t)b->timeout - (int64_t)a->timeout;
}

/* ccheap_t h; heap_init(&h, cmp); */
/* struct task t = {{.timeout = now() + 5000}, my_cb, NULL}; */
/* heap_push(&h, &t.node); */
/* struct task *done = CCHEAP_CONTAINER(heap_pop(&h), struct task, node); */
/* done->cb(done->arg); */
```

### 零开销宏模式

```c
#define CCHEAP_COMPARE(a, b) \
    ((int64_t)(b)->timeout - (int64_t)(a)->timeout)
#include "ccheap.h"
/* heap_init(&h, NULL) — 第二个参数被忽略 */
```

## 7. ccflatmap — 排序数组映射

```c
#include <stdio.h>
#include "ccflatmap.h"

int main() {
    ccflatmap_t m; ccflatmap_init(&m, NULL);

    /* 批量插入：push_back + sort（推荐） */
    ccflatmap_reserve(&m, 3);
    ccflatmap_node_t n1 = {.key = 42, .value = 100};
    ccflatmap_node_t n2 = {.key = 7,  .value = 200};
    ccflatmap_node_t n3 = {.key = 99, .value = 300};
    ccflatmap_push_back(&m, n1);
    ccflatmap_push_back(&m, n2);
    ccflatmap_push_back(&m, n3);
    ccflatmap_sort(&m);

    /* 二分查找 */
    ccflatmap_node_t probe = {.key = 7};
    ccflatmap_node_t *f = ccflatmap_find(&m, &probe);
    if (f) printf("found: %lld\n", (long long)f->value);

    ccflatmap_destroy(&m);
    return 0;
}
```

## 8. cctreap — Treap（排行树）

```c
#include <stdio.h>
#include <stdlib.h>
#include "cctreap.h"

struct entry { int key; cctreap_node_t node; };

static int64_t key_cmp(const cctreap_node_t *a, const cctreap_node_t *b) {
    return CCTREAP_CONTAINER(a, struct entry, node)->key
         - CCTREAP_CONTAINER(b, struct entry, node)->key;
}

int main() {
    cctreap_t t; cctreap_init(&t, key_cmp);

    struct entry e[5];
    for (int i = 0; i < 5; i++) {
        e[i].key = i * 10;
        cctreap_insert(&t, &e[i].node, NULL);
    }

    /* 第 k 小（0-indexed） */
    cctreap_node_t *k2 = cctreap_kth(&t, 2);
    printf("kth(2) = %d\n", CCTREAP_CONTAINER(k2, struct entry, node)->key); // 20

    /* 排名 */
    int64_t r = cctreap_rank(&t, &e[4].node);
    printf("rank(40) = %lld\n", (long long)r); // 4

    return 0;
}
```

## 9. ccvector — 动态数组

```c
#include <stdio.h>
#include "ccvector.h"

int main() {
    ccvector_t v; ccvector_init(&v);

    for (int i = 0; i < 5; i++) {
        ccvector_node_t e = {.value = (uint32_t)i};
        ccvector_push_back(&v, e);
    }

    for (size_t i = 0; i < ccvector_size(&v); i++)
        printf("%u\n", ccvector_at(&v, i)->value);
    /* 输出: 0, 1, 2, 3, 4 */

    ccvector_pop_back(&v);  /* 移除 4 */
    ccvector_destroy(&v);
    return 0;
}
```

### 自定义元素类型

```c
#define CCVECTOR_NODE_T struct point
struct point { float x, y; };
#include "ccvector.h"
/* 现在 ccvector 存储 struct point 值 */
```

## 10. ccunicode — UTF-8 编解码

```c
#include <stdio.h>
#include <string.h>
#include "ccunicode.h"

int main() {
    /* ── 解码：UTF-8 → UCS-4 ── */
    const char *utf8 = "A©你";
    int offset = 0, len = (int)strlen(utf8);

    while (offset < len) {
        uint32_t cp;
        int n = ccunicode_to_codepoint(utf8 + offset, len - offset, &cp);
        if (n == 0) { printf("invalid UTF-8\n"); break; }
        printf("U+%04X  (%d bytes)\n", cp, n);
        offset += n;
    }
    /* 输出:
       U+0041  (1 byte)   — ASCII 'A'
       U+00A9  (2 bytes)  — ©
       U+4F60  (3 bytes)  — 你
    */

    /* ── 编码：UCS-4 → UTF-8 ── */
    uint32_t codepoints[] = { 0x41, 0xA9, 0x4F60, 0x1F600 };  /* A, ©, 你, 😀 */
    char buf[4]; int n;

    for (size_t i = 0; i < sizeof(codepoints)/sizeof(codepoints[0]); i++) {
        if (ccunicode_from_codepoint(codepoints[i], buf, &n)) {
            printf("U+%05X → ", codepoints[i]);
            for (int j = 0; j < n; j++) printf("%02X ", (unsigned char)buf[j]);
            printf(" (%d bytes)\n", n);
        } else {
            printf("U+%05X → invalid\n", codepoints[i]);
        }
    }
    /* 输出:
       U+00041 → 41        (1 byte)
       U+000A9 → C2 A9     (2 bytes)
       U+04F60 → E4 BD A0  (3 bytes)
       U+1F600 → F0 9F 98 80 (4 bytes)
    */

    /* ── 非法输入 ── */
    uint32_t cp;
    printf("%d\n", ccunicode_to_codepoint("\xC0\x80", 2, &cp));  // 0 — 超长编码
    printf("%d\n", ccunicode_to_codepoint("\xED\xA0\x80", 3, &cp)); // 0 — 代理对
    printf("%d\n", ccunicode_from_codepoint(0xD800, buf, &n));       // false — 代理对
    printf("%d\n", ccunicode_from_codepoint(0x110000, buf, &n));     // false — 超范围

    return 0;
}
```

## 11. 构建与测试

```bash
# CMake 一键构建 + 测试
cmake -S . -B build && cmake --build build --target check

# 基准测试
cmake --build build --target bench
```

详见 [构建指南](building.html)。
