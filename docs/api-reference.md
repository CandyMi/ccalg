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
    ccmap_node_t *last;         // 最大节点 (O(1) rbegin)
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
// 删除节点。O(log n)。自动更新 first/last 指针。
// NULL 安全。
```

### 迭代器

```c
ccmap_node_t *ccmap_begin(ccmap_t *m);   // → 最小节点
ccmap_node_t *ccmap_first(ccmap_t *m);   // begin 的同义词
ccmap_node_t *ccmap_end(ccmap_t *m);     // 总是返回 NULL
ccmap_node_t *ccmap_rbegin(ccmap_t *m);  // → 最大节点 (O(1) via last 指针)
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

int ccmap_height(const ccmap_t *m);
// 树高估值。红黑树上界 ≤ 2·⌊log₂(n+1)⌋，由 size 推算 O(1)。
// 空树返回 0。
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
    cchashmap_node_t **buckets;  // bucket 指针数组
    size_t              cap;
    size_t              size;
    size_t              seed;
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
| `CCHASHMAP_DEFAULT_SLOT` | 初始桶数 | `64` |
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

---

## cclink — 侵入式单向链表

头文件: [`include/cclink.h`](../include/cclink.h)

轻量单向链表，无内部分配。可作为哈希桶链等场景的构建块。

### 类型

```c
typedef struct cclink_node {
    struct cclink_node *next;
} cclink_node_t;

typedef struct cclink {
    cclink_node_t *head;
    cclink_node_t *tail;
    size_t         size;
} cclink_t;
```

### 编译期配置

| 宏 | 作用 | 默认 |
| --- | --- | --- |
| `CCLINK_NODE_T` | 阻止默认节点 typedef | 未定义 |

### 生命周期

```c
void cclink_init(cclink_t *l);
void cclink_clear(cclink_t *l);  // 同 init
```

### 增删

```c
void cclink_push(cclink_t *l, cclink_node_t *n);
// O(1) 头部插入。

void cclink_push_back(cclink_t *l, cclink_node_t *n);
// O(1) 尾部追加。

void cclink_remove(cclink_t *l, cclink_node_t *n);
// O(n) 删除。节点不在链表中则无操作。

cclink_node_t *cclink_pop_front(cclink_t *l);
// O(1) 弹出头部。空返回 NULL。
```

### 迭代器 / 访问器

```c
cclink_node_t *cclink_begin(const cclink_t *l);   // → head
cclink_node_t *cclink_end(const cclink_t *l);     // 总是 NULL
cclink_node_t *cclink_next(const cclink_node_t *n);
cclink_node_t *cclink_front(const cclink_t *l);   // → head
cclink_node_t *cclink_back(const cclink_t *l);    // → tail
```

### 查询

```c
size_t cclink_size(const cclink_t *l);   // NULL 返回 0
bool   cclink_empty(const cclink_t *l);  // NULL 返回 true
```

### 调试

```c
bool cclink_has_cycle(const cclink_t *l);
// Floyd 龟兔算法，O(N) O(1)。

cclink_ecode_t cclink_verify(const cclink_t *l);
// 返回 0 或: ISNULL(1) / HASCYCLE(2) / SIZEERROR(3)
```

---

## ccheap — D-ary 堆（优先队列）

头文件: [`include/ccheap.h`](../include/ccheap.h)

指针数组存储，调用方持有节点内存。默认二叉堆，可选 4-ary / 8-ary。

### 类型

```c
typedef struct ccheap_node {
    union {
        uint64_t priority;  // 优先队列
        uint64_t timeout;   // 定时器
    };
} ccheap_node_t;
// 8 字节 union，堆不访问字段——由比较器决定语义。
// 嵌入到自定义结构体中（编译期不可替换）。

typedef int64_t (*ccheap_compare_t)(const ccheap_node_t *, const ccheap_node_t *);

typedef struct ccheap {
    ccheap_node_t   **data;   // 指针数组
    size_t            len;
    size_t            cap;
#ifndef CCHEAP_COMPARE
    ccheap_compare_t f;
#endif
} ccheap_t;

#define CCHEAP_CONTAINER(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
// 从节点指针恢复父结构体
```

### 编译期配置

| 宏 | 作用 | 默认 |
| --- | --- | --- |
| `CCHEAP_COMPARE(a, b)` | 内联比较（替代函数指针） | 未定义 |
| `CCHEAP_ARITY` | 分叉数 | `2`（可选 `4`/`8`） |
| `CCHEAP_NODE_T` | 节点类型（固定，嵌入用） | `ccheap_node_t` |
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
int  heap_insert(ccheap_t *heap, ccheap_node_t *n);
// (别名: heap_push)
// 返回 0 成功，-1 失败（NULL 或分配失败）。
// 自动 2x 扩容。传入嵌入节点的指针。

ccheap_node_t *heap_pop(ccheap_t *heap);
// 弹出根节点。空返回 NULL。
// 用 CCHEAP_CONTAINER 恢复父结构体。

ccheap_node_t *heap_peek(ccheap_t *heap);
// 查看根节点（不弹出）。空/NULL 返回 NULL。
```

### 工具

```c
size_t heap_size(ccheap_t *heap);
// NULL 返回 0。
```

---

## ccvector — 动态数组

头文件: [`include/ccvector.h`](../include/ccvector.h)

值存储的动态数组，元素以值形式存入连续内存。自动 2x 扩容。ccvector 不是侵入式容器——元素通过浅拷贝存入内部数组。

### 类型

```c
typedef struct ccvector_node {
    union { uint32_t value; };
} ccvector_node_t;
// 可通过 #define CCVECTOR_NODE_T 自定义元素类型

typedef struct ccvector {
    CCVECTOR_NODE_T *buckets;  // 连续数组
    size_t           len;      // 元素数量
    size_t           cap;      // 当前容量
} ccvector_t;
```

### 编译期配置

| 宏 | 作用 | 默认 |
| --- | --- | --- |
| `CCVECTOR_NODE_T` | 元素类型 | `ccvector_node_t` |
| `CCVECTOR_REALLOC` | 重分配函数 | `realloc` |
| `CCVECTOR_MALLOC(sz)` | 分配函数 | `realloc(NULL, sz)` |
| `CCVECTOR_FREE(ptr)` | 释放函数 | `free(ptr)` |
| `CCVECTOR_DEFAULT_CAP` | 初始容量 | `8` |

### 生命周期

```c
int  ccvector_init(ccvector_t *v);
// 返回 0 成功，-1 失败（v NULL 或分配失败）。初始容量 8。

int  ccvector_init_cap(ccvector_t *v, size_t cap);
// 以指定容量初始化。cap=0 回退到 CCVECTOR_DEFAULT_CAP。
// 无需修改宏即可控制初始容量。

void ccvector_destroy(ccvector_t *v);
// 释放内部数组。NULL 安全。

void ccvector_clear(ccvector_t *v);
// 重置 len=0，不释放内存。NULL 安全。
```

### 增删

```c
int  ccvector_push_back(ccvector_t *v, CCVECTOR_NODE_T elem);
// 尾部追加，按值拷贝。返回 0 成功，-1 失败。
// 容量满时自动 2x 扩容。

int  ccvector_pop_back(ccvector_t *v);
// 尾部弹出（len--）。返回 0 成功，-1 空。
```

### 访问

```c
CCVECTOR_NODE_T *ccvector_at(ccvector_t *v, size_t i);
// 索引访问。越界返回 NULL。

CCVECTOR_NODE_T *ccvector_front(ccvector_t *v);
// 首元素。空返回 NULL。

CCVECTOR_NODE_T *ccvector_back(ccvector_t *v);
// 尾元素。空返回 NULL。
```

### 查询

```c
size_t ccvector_size(ccvector_t *v);
// 元素数量。NULL 返回 0。

size_t ccvector_capacity(ccvector_t *v);
// 当前容量。NULL 返回 0。

int    ccvector_empty(ccvector_t *v);
// NULL 或空返回非零。
```

### 预留

```c
int  ccvector_reserve(ccvector_t *v, size_t new_cap);
// 预分配容量。new_cap > 当前 cap 时扩容。
```

---

## ccflatmap — 排序数组映射

头文件: [`include/ccflatmap.h`](../include/ccflatmap.h)

值存储的排序数组映射——连续内存 + 二分查找。类似 C++23 `std::flat_map`。

### 类型

```c
typedef struct ccflatmap_node {
    int64_t  key;
    uint64_t value;
} ccflatmap_node_t;

typedef int64_t (*ccflatmap_cmp_t)(const CCFLATMAP_NODE_T *a,
                                   const CCFLATMAP_NODE_T *b);

typedef struct ccflatmap {
    CCFLATMAP_NODE_T *buckets;
    size_t            len;
    size_t            cap;
#ifndef CCFLATMAP_COMPARE
    ccflatmap_cmp_t   cmp;
#endif
} ccflatmap_t;
```

### 编译期配置

| 宏 | 作用 | 默认 |
| --- | --- | --- |
| `CCFLATMAP_COMPARE(a, b)` | 内联比较 | 未定义 |
| `CCFLATMAP_NODE_T` | 元素类型 | `ccflatmap_node_t` |
| `CCFLATMAP_REALLOC` / `MALLOC` / `FREE` | 分配器 | `realloc` / `free` |
| `CCFLATMAP_DEFAULT_CAP` | 初始容量 | `8` |

### 生命周期

```c
int  ccflatmap_init(ccflatmap_t *m, ccflatmap_cmp_t cmp);
void ccflatmap_destroy(ccflatmap_t *m);
void ccflatmap_clear(ccflatmap_t *m);
```

### 增删查

```c
int ccflatmap_insert(ccflatmap_t *m, CCFLATMAP_NODE_T elem,
                     CCFLATMAP_NODE_T **out);
// 排序插入。O(n)。0 成功，-1 重复。

CCFLATMAP_NODE_T *ccflatmap_find(ccflatmap_t *m,
                                  const CCFLATMAP_NODE_T *probe);
// 二分查找。O(log n)。

void ccflatmap_erase(ccflatmap_t *m, const CCFLATMAP_NODE_T *probe);
// 二分查找 + memmove 删除。O(n)。

void ccflatmap_erase_at(ccflatmap_t *m, size_t pos);
// 按索引删除。O(n)。pos >= len 无操作。

void ccflatmap_erase_unordered(ccflatmap_t *m,
                                const CCFLATMAP_NODE_T *probe);
// swap-with-last 删除，O(log n) + O(1)。破坏排序——用 sort() 恢复。
```

批量删除推荐：

```c
for (int i = 0; i < n; i++)
    ccflatmap_erase_unordered(&m, &probes[i]);
ccflatmap_sort(&m);  // 一次性 O(n log n) 恢复排序
```

### 批量插入

```c
int  ccflatmap_push_back(ccflatmap_t *m, CCFLATMAP_NODE_T elem);
// 无序尾部追加，O(1)。不检测重复。

void ccflatmap_sort(ccflatmap_t *m);
// 原地快速排序，O(n log n)。
```

推荐：`reserve(N)` → `push_back` 循环 → `sort()`。

### 迭代器

```c
CCFLATMAP_NODE_T *ccflatmap_begin(ccflatmap_t *m);
CCFLATMAP_NODE_T *ccflatmap_end(ccflatmap_t *m);      // 总是 NULL
CCFLATMAP_NODE_T *ccflatmap_rbegin(ccflatmap_t *m);
CCFLATMAP_NODE_T *ccflatmap_next(ccflatmap_t *m, CCFLATMAP_NODE_T *p);
CCFLATMAP_NODE_T *ccflatmap_prev(ccflatmap_t *m, CCFLATMAP_NODE_T *p);
CCFLATMAP_NODE_T *ccflatmap_at(ccflatmap_t *m, size_t i);
```

### 工具

```c
size_t ccflatmap_size(ccflatmap_t *m);
int    ccflatmap_empty(ccflatmap_t *m);
size_t ccflatmap_capacity(ccflatmap_t *m);
int    ccflatmap_reserve(ccflatmap_t *m, size_t new_cap);
```

---

## cctreap — 侵入式 Treap

头文件: [`include/cctreap.h`](../include/cctreap.h)

有序映射，基于 treap（BST + max-heap）。节点 24 字节（64-bit），优先级外置。

### 类型

```c
typedef struct cctreap_node {
    struct cctreap_node *child[2];  // [0]=left, [1]=right
    uintptr_t            pc;        // parent pointer
    size_t               size;      // subtree node count
} cctreap_node_t;

typedef int64_t (*cctreap_cmp_t)      (const cctreap_node_t *a, const cctreap_node_t *b);
typedef int64_t (*cctreap_prio_cmp_t) (const cctreap_node_t *a, const cctreap_node_t *b);

typedef struct cctreap {
    cctreap_node_t    *root;
    cctreap_node_t    *first;        // 最小节点 (O(1) begin)
    cctreap_node_t    *last;         // 最大节点 (O(1) rbegin)
    size_t             size;
    cctreap_cmp_t       key_cmp;
    cctreap_prio_cmp_t  prio_cmp;
} cctreap_t;
```

### 编译期配置

| 宏 | 作用 | 默认 |
| --- | --- | --- |
| `CCTREAP_COMPARE(a, b)` | 内联 key 比较 | 未定义 |
| `CCTREAP_PRIORITY(a, b)` | 内联 priority 比较 | 未定义 |
| `CCTREAP_NODE_T` | 阻止默认节点 typedef | 未定义 |

### 生命周期

```c
void cctreap_init(cctreap_t *m, cctreap_cmp_t key_cmp, cctreap_prio_cmp_t prio_cmp);
// 初始化。若定义了 CCTREAP_COMPARE / CCTREAP_PRIORITY，对应参数被忽略。

void cctreap_clear(cctreap_t *m);
// 重置为空。不释放节点内存。
```

### 增删查

```c
int cctreap_insert(cctreap_t *m, cctreap_node_t *node, cctreap_node_t **out);
// 插入节点。返回 0 成功，-1 重复（*out = 已存在节点）。
// 用户须提前设置 priority（通过 CCTREAP_PRIORITY 或 prio_cmp 访问）。

cctreap_node_t *cctreap_find(cctreap_t *m, const cctreap_node_t *probe);
// 按 key 查找。返回节点指针或 NULL。

void cctreap_erase(cctreap_t *m, cctreap_node_t *z);
// 删除节点。O(log n)。自动更新 first/last 指针。
```

### 顺序统计

```c
cctreap_node_t *cctreap_kth(cctreap_t *m, size_t k);
// 第 k 小元素（0-indexed）。k >= size 返回 NULL。O(log n)。

int64_t cctreap_rank(cctreap_t *m, const cctreap_node_t *probe);
// key 的排名（0-indexed）。未找到返回 -1。O(log n)。
```

### 迭代器

```c
cctreap_node_t *cctreap_begin(cctreap_t *m);   // → 最小节点 (O(1))
cctreap_node_t *cctreap_first(cctreap_t *m);   // begin 的同义词
cctreap_node_t *cctreap_end(cctreap_t *m);     // 总是返回 NULL
cctreap_node_t *cctreap_rbegin(cctreap_t *m);  // → 最大节点 (O(1))
cctreap_node_t *cctreap_last(cctreap_t *m);    // rbegin 的同义词
cctreap_node_t *cctreap_next(cctreap_node_t *n); // 后继
cctreap_node_t *cctreap_prev(cctreap_node_t *n); // 前驱
```

### 工具

```c
size_t cctreap_size(cctreap_t *m);
// 元素数量。NULL 返回 0。

int cctreap_height(const cctreap_t *m);
// 树高估值。O(1)，由 size 推算。
```
