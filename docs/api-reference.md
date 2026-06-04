# API 参考

## ccmap — 侵入式红黑树

头文件: [`include/ccmap.h`](../include/ccmap.h)

有序映射，基于红黑树。节点 16 字节（64-bit），颜色编码在父指针低位。

### 类型

```c
typedef struct ccmap_node {
    struct ccmap_node *child[2];  // [CCMAP_LEFT]=left, [CCMAP_RIGHT]=right
    uintptr_t           pc;       // parent | color (low bit)
} ccmap_node_t;

typedef int64_t (*ccmap_cmp_t)(const ccmap_node_t *a, const ccmap_node_t *b);
// 返回 >0: a 序更小  <0: b 序更小  ==0: 相等

typedef struct ccmap {
    ccmap_node_t *root;
    ccmap_node_t *first;        // 最小节点 (O(1) begin)
    size_t        size;
#ifndef CCMAP_COMPARE
    ccmap_cmp_t   cmp;
#endif
} ccmap_t;
```

### 编译期配置

| 宏 | 作用 | 默认 |
| --- | --- | --- |
| `CCMAP_COMPARE(a, b)` | 内联比较（替代函数指针） | 未定义 |
| `CCMAP_NODE_T` | 阻止默认节点 typedef | 未定义 |

### 生命周期

```c
void ccmap_init(ccmap_t *m, ccmap_cmp_t cmp);
// 初始化。若定义了 CCMAP_COMPARE，cmp 被忽略。
// NULL 安全。

void ccmap_clear(ccmap_t *m);
// 重置为空。不释放节点内存。
// NULL 安全。
```

### 增删查

```c
int ccmap_insert(ccmap_t *m, ccmap_node_t *node, ccmap_node_t **out);
// 插入节点。返回 0 成功，-1 重复（*out = 已存在节点）。
// 插入成功时 *out = node（或 NULL 如果 out==NULL）。
// m/node NULL 返回 -1。

ccmap_node_t *ccmap_find(ccmap_t *m, const ccmap_node_t *probe);
// 按 key 查找。返回节点指针或 NULL。

void ccmap_erase(ccmap_t *m, ccmap_node_t *z);
// 删除节点。O(log n)。自动更新 first 指针。
// NULL 安全。
```

### 迭代器

```c
ccmap_node_t *ccmap_begin(ccmap_t *m);   // → 最小节点
ccmap_node_t *ccmap_first(ccmap_t *m);   // begin 的同义词
ccmap_node_t *ccmap_end(ccmap_t *m);     // 总是返回 NULL
ccmap_node_t *ccmap_rbegin(ccmap_t *m);  // → 最大节点
ccmap_node_t *ccmap_next(ccmap_node_t *n); // 后继，末尾返回 NULL
ccmap_node_t *ccmap_prev(ccmap_node_t *n); // 前驱，首元素返回 NULL
```

迭代模式：

```c
for (ccmap_node_t *n = ccmap_begin(&m); n != ccmap_end(&m); n = ccmap_next(n)) {
    struct my_entry *e = CCMAP_CONTAINER(n, struct my_entry, node);
}
```

### 工具

```c
size_t ccmap_size(ccmap_t *m);
// 元素数量。NULL 返回 0。
```

---

## cchashmap — 侵入式链式哈希表

头文件: [`include/cchashmap.h`](../include/cchashmap.h)

链式哈希，内部管理桶数组。容量为 2 的幂，位掩码索引。

### 类型

```c
typedef struct cchashmap_node {
    struct cchashmap_node *next;
    uint64_t               hash;  // 缓存的哈希值
} cchashmap_node_t;

typedef uint64_t (*cchashmap_hash_t)(const cchashmap_node_t *n, size_t seed);
typedef bool     (*cchashmap_equal_t)(const cchashmap_node_t *a, const cchashmap_node_t *b);

typedef struct cchashmap {
    cchashmap_node_t **buckets;
    size_t             size;
    size_t             cap;
    size_t             seed;
#ifndef CCHASHMAP_HASH
    cchashmap_hash_t   hash_fn;
    cchashmap_equal_t  equal_fn;
#endif
} cchashmap_t;
```

### 编译期配置

| 宏 | 作用 | 默认 |
| --- | --- | --- |
| `CCHASHMAP_HASH(n, seed)` | 内联哈希（替代函数指针） | 未定义 |
| `CCHASHMAP_EQUAL(a, b)` | 内联判等（替代函数指针） | 未定义 |
| `CCHASHMAP_NODE_T` | 阻止默认节点 typedef | 未定义 |
| `CCHASHMAP_REALLOC` | 重分配函数 | `realloc` |
| `CCHASHMAP_MALLOC(sz)` | 分配函数 | `realloc(NULL, sz)` |
| `CCHASHMAP_FREE(ptr)` | 释放函数 | `free(ptr)` |
| `CCHASHMAP_MAX_LOAD` | 最大负载因子 | `1.25` |

### 生命周期

```c
void cchashmap_init(cchashmap_t *m, cchashmap_hash_t hfn, cchashmap_equal_t efn);
// 初始化。若定义了 CCHASHMAP_HASH/CCHASHMAP_EQUAL，hfn/efn 被忽略。

void cchashmap_destroy(cchashmap_t *m);
// 释放桶数组。不释放节点。调用前须先删除所有节点。

void cchashmap_clear(cchashmap_t *m);
// 重置桶数组为零，size=0。不释放内存。
```

### 增删查

```c
bool cchashmap_set(cchashmap_t *m, cchashmap_node_t *node, cchashmap_node_t **old);
// (别名: cchashmap_insert)
// 插入节点。true=成功 false=重复（*old=已存在节点）。
// 自动 resize（负载因子 > CCHASHMAP_MAX_LOAD 时 2x 扩容）。

cchashmap_node_t *cchashmap_get(cchashmap_t *m, cchashmap_node_t *probe);
// (别名: cchashmap_find)
// 按 key 查找。返回节点或 NULL。

void cchashmap_del(cchashmap_t *m, cchashmap_node_t *node);
// (别名: cchashmap_erase)
// 删除节点。O(链长)。
```

### 工具

```c
size_t cchashmap_size(cchashmap_t *m);
// NULL 返回 0。
```

---

## cclist — 侵入式双向链表

头文件: [`include/cclist.h`](../include/cclist.h)

标准双向链表，head/tail 哨兵 + size 计数。无内部分配。

### 类型

```c
typedef struct cclist_node {
    struct cclist_node *next;
    struct cclist_node *prev;
} cclist_node_t;

typedef struct cclist {
    cclist_node_t *head;
    cclist_node_t *tail;
    size_t         size;
} cclist_t;
```

### 编译期配置

无需配置宏。

### 生命周期

```c
void cclist_init(cclist_t *l);
void cclist_clear(cclist_t *l);  // 同 init，不释放节点
```

### 增删

```c
// push / pop
void            cclist_push_front(cclist_t *l, cclist_node_t *n);
void            cclist_push_back(cclist_t *l, cclist_node_t *n);
cclist_node_t  *cclist_pop_front(cclist_t *l);  // 空返回 NULL
cclist_node_t  *cclist_pop_back(cclist_t *l);   // 空返回 NULL

// insert / remove
void cclist_insert_before(cclist_t *l, cclist_node_t *pos, cclist_node_t *n);
void cclist_insert_after(cclist_t *l, cclist_node_t *pos, cclist_node_t *n);
void cclist_remove(cclist_t *l, cclist_node_t *n);

// splice / move
void cclist_splice_back(cclist_t *dst, cclist_t *src);
// 将 src 所有节点移到 dst 尾部，src 变空。
int  cclist_move(cclist_t *to, cclist_t *from);
// 返回: 0 成功, -1 NULL, -2 from 空, -3 自移动
```

### 迭代器 / 访问器

```c
cclist_node_t *cclist_begin(const cclist_t *l);   // → head
cclist_node_t *cclist_end(const cclist_t *l);     // 总是 NULL
cclist_node_t *cclist_rbegin(const cclist_t *l);  // → tail
cclist_node_t *cclist_next(const cclist_node_t *n);
cclist_node_t *cclist_prev(const cclist_node_t *n);
cclist_node_t *cclist_front(const cclist_t *l);   // → head
cclist_node_t *cclist_back(const cclist_t *l);    // → tail
```

### 查询

```c
size_t cclist_size(const cclist_t *l);   // NULL 返回 0
bool   cclist_empty(const cclist_t *l);  // NULL 返回 true
```

### 调试

```c
bool cclist_has_cycle(const cclist_t *l);
// Floyd 龟兔算法，O(N) 时间 O(1) 空间。

cclist_ecode_t cclist_verify(const cclist_t *l);
// 返回 0 (NOERROR) 或错误码:
//   ISNULL(1) — l 为 NULL 或 head/tail 不一致
//   NOHEAD(2) — head->prev != NULL
//   NONEXT(3) — tail->next != NULL
//   HASCYCLE(4) — 检测到环
//   MISSPREV(5) — 前向指针不一致
//   SIZEERROR(6) — size 与实际节点数不符
```

---

## ccheap — D-ary 堆（优先队列）

头文件: [`include/ccheap.h`](../include/ccheap.h)

支持指针模式和值模式（`CCHEAP_VALUE`）。默认二叉堆，可选 4-ary / 8-ary。

### 类型

```c
typedef struct ccheap_node {
    uint32_t conv;
    uint32_t timestamp;
} ccheap_node_t;
// 可通过 #define CCNODE_T 覆盖

typedef int64_t (*ccheap_compare_t)(const CCNODE_T *, const CCNODE_T *);

typedef struct ccheap {
#ifdef CCHEAP_VALUE
    CCNODE_T   *data;     // 连续值数组
    CCNODE_T    popped;   // pop 返回值缓冲区
#else
    CCNODE_T  **data;     // 指针数组
#endif
    size_t      size;
    size_t      cap;
#ifndef CCHEAP_COMPARE
    ccheap_compare_t f;
#endif
} ccheap_t;
```

### 编译期配置

| 宏 | 作用 | 默认 |
| --- | --- | --- |
| `CCHEAP_COMPARE(a, b)` | 内联比较（替代函数指针） | 未定义 |
| `CCHEAP_VALUE` | 值模式（连续存储） | 未定义（指针模式） |
| `CCHEAP_ARITY` | 分叉数 | `2`（可选 `4`/`8`） |
| `CCNODE_T` | 自定义节点类型 | `struct ccheap_node` |
| `CCHEAP_REALLOC` | 重分配函数 | `realloc` |
| `CCHEAP_MALLOC(sz)` | 分配函数 | `realloc(NULL, sz)` |
| `CCHEAP_FREE(ptr)` | 释放函数 | `free(ptr)` |
| `CCHEAP_DEFAULT_CAP` | 初始容量 | `32` |

### 比较语义

```c
f(a, b) > 0  →  a 距根更近（优先级更高）
f(a, b) < 0  →  b 距根更近
f(a, b) == 0 →  相等
```

### 生命周期

```c
int  heap_init(ccheap_t *heap, ccheap_compare_t f);
// 返回 0 成功，-1 失败（heap NULL 或分配失败）。
// 若定义了 CCHEAP_COMPARE，f 被忽略。

void heap_destroy(ccheap_t *heap);
// 释放内部数组。不释放节点（指针模式）。
// NULL 安全。
```

### 增删查

```c
int  heap_insert(ccheap_t *heap, CCNODE_T *n);
// (别名: heap_push)
// 返回 0 成功，-1 失败（NULL 或分配失败）。
// 自动 2x 扩容。

CCNODE_T *heap_pop(ccheap_t *heap);
// 弹出根节点。空返回 NULL。
// ⚠️ 值模式: 返回指针指向内部缓冲区 heap->popped，
//    下次 pop 会覆盖。须立即消费或拷贝。

CCNODE_T *heap_peek(ccheap_t *heap);
// 查看根节点（不弹出）。空/NULL 返回 NULL。
```

### 工具

```c
size_t heap_size(ccheap_t *heap);
// NULL 返回 0。
```

### 值模式注意事项

`CCHEAP_VALUE` 模式下，堆使用结构体赋值（浅拷贝）在连续数组中存储元素。如果 `CCNODE_T` 包含指针成员（如 `char *name`），请使用默认指针模式——堆只存指针，调用方保有指向内存的所有权。
