# API 参考

> **C++ 互操作：** 所有容器类型和函数包裹在 `extern "C" { }` 中。每个容器定义 `CCXXX_NOEXCEPT` 宏
> （C++17+ 展开为 `noexcept`，C/<C++17 展开为空），标注在所有 public 函数和回调 typedef 上。
> C++17+ 编译时，回调函数指针类型强制要求 `noexcept`，从编译期阻止异常通过库回调传播。

## ccmap — 侵入式红黑树

头文件: [`include/ccmap.h`](https://github.com/CandyMi/ccalg/blob/master/include/ccmap.h)

有序映射，基于红黑树。节点 16 字节（64-bit），颜色编码在父指针低位。

### 类型

```c
typedef struct ccmap_node {
    struct ccmap_node *child[2];  // [CCMAP_LEFT]=left, [CCMAP_RIGHT]=right
    uintptr_t           pc;       // parent | color (low bit)
} ccmap_node_t;

typedef int64_t (*ccmap_cmp_t)(const ccmap_node_t *a, const ccmap_node_t *b) CCMAP_NOEXCEPT;
// 返回 >0: a 序更小  <0: b 序更小  ==0: 相等

typedef struct map {
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
| `CCMAP_NOEXCEPT` | C++17 noexcept 标注（C/<C++17 展开为空） | 未定义 |
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
// 树高。Debug 模式（NDEBUG 未定义）遍历树返回实际高度；
// Release 模式（NDEBUG 已定义）返回 ≤ 2·⌊log₂(n+1)⌋ 估值，O(1)。
// 空树返回 0。
```

---

## cchashmap — 侵入式链式哈希表

头文件: [`include/cchashmap.h`](https://github.com/CandyMi/ccalg/blob/master/include/cchashmap.h)

链式哈希，内部管理桶数组。容量为 2 的幂，位掩码索引。

### 类型

```c
typedef struct cchashmap_node {
    struct cchashmap_node *next;
    uint64_t               hash;  // 缓存的哈希值
} cchashmap_node_t;

typedef uint64_t (*cchashmap_hash_t)(const cchashmap_node_t *n, size_t seed) CCHASHMAP_NOEXCEPT;
typedef bool     (*cchashmap_equal_t)(const cchashmap_node_t *a, const cchashmap_node_t *b) CCHASHMAP_NOEXCEPT;

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
| `CCHASHMAP_NOEXCEPT` | C++17 noexcept 标注 | 未定义 |
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

头文件: [`include/cclist.h`](https://github.com/CandyMi/ccalg/blob/master/include/cclist.h)

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

| 宏 | 作用 | 默认 |
| --- | --- | --- |
| `CCLIST_NOEXCEPT` | C++17 noexcept 标注 | 未定义 |

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

### 错误信息

```c
const cclist_error_t *cclist_get_error(cclist_ecode_t code);
// 将 cclist_verify() 返回的错误码转为可读描述。
```

### 调试

```c
bool cclist_has_cycle(const cclist_t *l);
// Floyd 龟兔算法，O(N) 时间 O(1) 空间。

cclist_ecode_t cclist_verify(const cclist_t *l);
// 返回 0 (CCLIST_NOERROR) 或错误码:
//   CCLIST_ISNULL(1) — l 为 NULL 或 head/tail 不一致
//   CCLIST_NOHEAD(2) — head->prev != NULL
//   CCLIST_NONEXT(3) — tail->next != NULL
//   CCLIST_HASCYCLE(4) — 检测到环
//   CCLIST_MISSPREV(5) — 前向指针不一致
//   CCLIST_SIZEERROR(6) — size 与实际节点数不符
```

---

---

## cclink — 侵入式单向链表

头文件: [`include/cclink.h`](https://github.com/CandyMi/ccalg/blob/master/include/cclink.h)

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
| `CCLINK_NOEXCEPT` | C++17 noexcept 标注 | 未定义 |
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

### 错误信息

```c
const cclink_error_t *cclink_get_error(cclink_ecode_t code);
// 将 cclink_verify() 返回的错误码转为可读描述。
```

### 调试

```c
bool cclink_has_cycle(const cclink_t *l);
// Floyd 龟兔算法，O(N) O(1)。

cclink_ecode_t cclink_verify(const cclink_t *l);
// 返回 0 或: CCLIST_ISNULL(1) / CCLIST_HASCYCLE(2) / CCLIST_SIZEERROR(3)
```

---

## ccheap — D-ary 堆（优先队列）

头文件: [`include/ccheap.h`](https://github.com/CandyMi/ccalg/blob/master/include/ccheap.h)

指针数组存储，调用方持有节点内存。默认二叉堆，可选 4-ary / 8-ary。

### 类型

```c
typedef struct ccheap_node {
    union {
        uint64_t priority;  // 优先队列
        uint64_t timeout;   // 定时器
        double   cost;      // 浮点代價 (A*, 路径搜索)
    };
#ifdef CCHEAP_NODE_INDEX
    size_t CCHEAP_NODE_INDEX;  // 堆数组位置 (ccheap_update 用)
#endif
} ccheap_node_t;
// 8 字节 union (无 CCHEAP_NODE_INDEX 时)，堆不访问字段——由比较器决定语义。
// CCHEAP_NODE_INDEX 开启后节点变为 16B（64-bit），支持 O(log n) decrease-key。

typedef int64_t (*ccheap_compare_t)(const ccheap_node_t *, const ccheap_node_t *) CCHEAP_NOEXCEPT;

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
| `CCHEAP_NOEXCEPT` | C++17 noexcept 标注 | 未定义 |
| `CCHEAP_ARITY` | 分叉数 | `2`（可选 `4`/`8`） |
| `CCHEAP_NODE_T` | 节点类型（固定，嵌入用） | `ccheap_node_t` |
| `CCHEAP_REALLOC` | 重分配函数 | `realloc` |
| `CCHEAP_MALLOC(sz)` | 分配函数 | `realloc(NULL, sz)` |
| `CCHEAP_FREE(ptr)` | 释放函数 | `free(ptr)` |
| `CCHEAP_DEFAULT_CAP` | 初始容量 | `32` |
| `CCHEAP_NODE_INDEX` | 开启索引追踪（字段名自定义），启用 `ccheap_update` | 未定义 |

### 比较语义

```c
f(a, b) > 0  →  a 距根更近（优先级更高）
f(a, b) < 0  →  b 距根更近
f(a, b) == 0 →  相等
```

### 生命周期

```c
int  ccheap_init(ccheap_t *heap, ccheap_compare_t f);
// 返回 0 成功，-1 失败（heap NULL 或分配失败）。
// 若定义了 CCHEAP_COMPARE，f 被忽略。

void ccheap_clear(ccheap_t *heap);
// 重置为空堆（len=0）。保留内部数组，不释放内存。
// NULL 安全。

void ccheap_destroy(ccheap_t *heap);
// 释放内部数组。不释放节点（指针模式）。
// NULL 安全。
```

### 增删查

```c
int  ccheap_insert(ccheap_t *heap, ccheap_node_t *n);
// (别名: ccheap_push)
// 返回 0 成功，-1 失败（NULL 或分配失败）。
// 自动 2x 扩容。传入嵌入节点的指针。

ccheap_node_t *ccheap_pop(ccheap_t *heap);
// 弹出根节点。空返回 NULL。
// 用 CCHEAP_CONTAINER 恢复父结构体。

ccheap_node_t *ccheap_peek(ccheap_t *heap);
// 查看根节点（不弹出）。空/NULL 返回 NULL。

#ifdef CCHEAP_NODE_INDEX
int  ccheap_update(ccheap_t *heap, ccheap_node_t *n);
// 对已在堆中的节点重新评估位置（decrease-key / increase-key）。
// 返回 0 成功，-1 失败（NULL / n 不在堆中 / _idx 过期）。
// 需要 #define CCHEAP_NODE_INDEX <字段名> 开启。
#endif
```

### 工具

```c
size_t ccheap_size(ccheap_t *heap);
// NULL 返回 0。
```

---

## ccvector — 动态数组

头文件: [`include/ccvector.h`](https://github.com/CandyMi/ccalg/blob/master/include/ccvector.h)

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
| `CCVECTOR_NOEXCEPT` | C++17 noexcept 标注 | 未定义 |
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

### 排序

```c
void ccvector_sort(ccvector_t *v, int (*cmp)(const void *, const void *));
// 对元素进行原地排序。使用 C 标准库 qsort。
// cmp 签名与 qsort 一致：返回负数 → a 在前，正数 → b 在前，0 → 相等。
// NULL / 空 / 单元素 均为安全无操作。
```

### 二分查找

```c
void *ccvector_bsearch(ccvector_t *v, const void *key,
                       int (*cmp)(const void *, const void *));
// 在已排序的 vector 上执行二分查找。封装 C 标准库 bsearch。
// key — 搜索键的指针（作为比较器的第一个参数传入）
// cmp — 必须与 ccvector_sort 所用的比较器一致
// 返回匹配元素的指针，或 NULL（未找到）。
// 若有多个相等元素，返回哪个未定义（标准 bsearch 约定）。
// NULL 安全。
```

### 预留

```c
int  ccvector_reserve(ccvector_t *v, size_t new_cap);
// 预分配容量。new_cap > 当前 cap 时扩容。
```

---

## ccflatmap — 排序数组映射

头文件: [`include/ccflatmap.h`](https://github.com/CandyMi/ccalg/blob/master/include/ccflatmap.h)

值存储的排序数组映射——连续内存 + 二分查找。类似 C++23 `std::flat_map`。

### 类型

```c
typedef struct ccflatmap_node {
    int64_t  key;
    uint64_t value;
} ccflatmap_node_t;

typedef int64_t (*ccflatmap_cmp_t)(const CCFLATMAP_NODE_T *a,
                                   const CCFLATMAP_NODE_T *b) CCFLATMAP_NOEXCEPT;

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
| `CCFLATMAP_NOEXCEPT` | C++17 noexcept 标注 | 未定义 |
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

头文件: [`include/cctreap.h`](https://github.com/CandyMi/ccalg/blob/master/include/cctreap.h)

有序映射，基于 treap（BST + max-heap）。节点 32 字节（64-bit），优先级内置（插入时 xorshift64 自动生成）。

### 类型

```c
typedef struct cctreap_node {
    struct cctreap_node *child[2];  // [0]=left, [1]=right
    uintptr_t            pc;        // parent pointer
    size_t               size;      // subtree node count
    uint64_t             priority;  // internal random priority (max-heap)
} cctreap_node_t;

typedef int64_t (*cctreap_cmp_t) (const cctreap_node_t *a, const cctreap_node_t *b) CCTREAP_NOEXCEPT;

typedef struct cctreap {
    cctreap_node_t    *root;
    cctreap_node_t    *first;        // 最小节点 (O(1) begin)
    cctreap_node_t    *last;         // 最大节点 (O(1) rbegin)
    size_t             size;
    cctreap_cmp_t       key_cmp;
    CCTREAP_RAND_T      state;       // RNG state (默认 uint64_t, 可替换)
} cctreap_t;
```

### 编译期配置

| 宏 | 作用 | 默认 |
| --- | --- | --- |
| `CCTREAP_COMPARE(a, b)` | 内联 key 比较 | 未定义 |
| `CCTREAP_RAND_T` | RNG 状态类型 | `uint64_t`（可替换为 `ccrandom128_t` / `ccrandom256_t`）|
| `CCTREAP_NOEXCEPT` | C++17 noexcept 标注 | 未定义 |
| `CCTREAP_RAND_INIT(m, seed)` | 播种：接收容器指针 + 种子值初始化 RNG 状态 | `(m)->state = (seed)` |
| `CCTREAP_RAND_NEXT(state)` | 步进：生成下一个随机值 | `_tp_xorshift64` |
| `CCTREAP_RAND(state)` | （兼容别名，等价于 `CCTREAP_RAND_NEXT`） | — |
| `CCTREAP_NODE_T` | 阻止默认节点 typedef | 未定义 |

### 生命周期

```c
void cctreap_init(cctreap_t *m, cctreap_cmp_t key_cmp);
// 初始化。若定义了 CCTREAP_COMPARE，key_cmp 被忽略。
// RNG 状态由 CCTREAP_RAND_INIT 负责播种（默认：m 的指针值）。

void cctreap_clear(cctreap_t *m);
// 重置为空。不释放节点内存。
```

### 增删查

```c
int cctreap_insert(cctreap_t *m, cctreap_node_t *node, cctreap_node_t **out);
// 插入节点。返回 0 成功，-1 重复（*out = 已存在节点）。
// priority 由内部 CCTREAP_RAND_NEXT 自动生成，无需用户设置。

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
// 树高。Debug 模式（NDEBUG 未定义）遍历树返回实际高度；
// Release 模式（NDEBUG 已定义）返回 O(log n) 估值，O(1)。
```

---

## ccunicode — UTF-8 ↔ UCS-4 编解码

头文件: [`include/ccunicode.h`](https://github.com/CandyMi/ccalg/blob/master/include/ccunicode.h)

自包含的 UTF-8 与 UCS-4 互转编解码器，遵循 Unicode 17.0 / RFC 3629。

- 支持 1–4 字节序列，拒绝超长编码、代理对（U+D800–U+DFFF）
- ASCII 快速路径，256 字节查找表分类首字节，无分支编码器
- 解码：展开 switch 处理连续字节，消除循环开销

### 函数

```c
int ccunicode_to_codepoint(const char *str, int len, uint32_t *val);
// UTF-8 → UCS-4。返回消耗字节数 1–4（成功）或 0（失败）。

bool ccunicode_from_codepoint(uint32_t val, char str[4], int *len);
// UCS-4 → UTF-8。*len 输出写入字节数。true 成功，false 非法码点。
```

### 编译期配置

| 宏 | 作用 | 默认 |
| --- | --- | --- |
| `CCUNICODE_INLINE` | 函数内联关键字，可覆盖为 `static`（C99）| `static inline` |
| `CCUNICODE_NOEXCEPT` | C++17 noexcept 标注 | 未定义 |

### 性能说明

- 解码器：首字节查表（256B `seq_len`）→ 展开 switch 逐 case 处理，无循环、无跳转表开销
- 编码器：无分支计算字节数（`1 + (val > 0x7F) + ...`，编译器降级为 SETcc/CMOV）→ 首字节查表 → fall-through switch 写连续字节
- ASCII 路径（占比 70–90%）立即返回，不触碰查找表

---

## ccrandom — 非加密伪随机数生成器

头文件: [`include/ccrandom.h`](https://github.com/CandyMi/ccalg/blob/master/include/ccrandom.h)

轻量、可复现、跨平台的高性能 PRNG，提供 128-bit、256-bit 与 512-bit 三个引擎。专为**非加密场景**（模拟、游戏、测试、采样、蒙特卡洛）设计。

三个引擎均通过 BigCrush (TestU01) 和 PractRand ≥ 32 TiB 统计测试。

| 引擎 | 算法 | 状态 | 周期 | 特点 |
| --- | --- | --- | --- | --- |
| `ccrandom128` | Xoroshiro128++ | 2×u64 | 2¹²⁸−1 | 最快，适合吞吐敏感场景 |
| `ccrandom256` | Xoshiro256** | 4×u64 | 2²⁵⁶−1 | 统计质量更高 |
| `ccrandom512` | Xoshiro512** | 8×u64 | 2⁵¹²−1 | 最高统计质量，适合大规模模拟 |

### 初始化

```c
void ccrandom128_init(ccrandom128_t *s, uint64_t seed);
void ccrandom256_init(ccrandom256_t *s, uint64_t seed);
void ccrandom512_init(ccrandom512_t *s, uint64_t seed);
// SplitMix64 扩展种子至内部状态。任何 64 位值均可（含 0）。
// ccrandom512 运行 SplitMix64 八次，填充 8×u64 = 512-bit 状态。
```

### 整数输出

```c
uint64_t ccrandom128_next(ccrandom128_t *s);
uint64_t ccrandom256_next(ccrandom256_t *s);
uint64_t ccrandom512_next(ccrandom512_t *s);
// 返回 [0, 2^64−1] 均匀分布 64 位无符号整数。
```

### 浮点输出

```c
float  ccrandom128_f32next(ccrandom128_t *s);
float  ccrandom256_f32next(ccrandom256_t *s);
float  ccrandom512_f32next(ccrandom512_t *s);
// 返回 [0, 1) 范围内 float，24 位尾数精度。

double ccrandom128_f64next(ccrandom128_t *s);
double ccrandom256_f64next(ccrandom256_t *s);
double ccrandom512_f64next(ccrandom512_t *s);
// 返回 [0, 1) 范围内 double，53 位尾数精度。
```

### 编译期配置

| 宏 | 作用 | 默认 |
| --- | --- | --- |
| `CCRANDOM_INLINE` | 函数内联关键字，可覆盖为 `static`（C99）| `static inline` |
| `CCRANDOM_NOEXCEPT` | C++17 noexcept 标注 | 未定义 |

### 线程安全

每个实例是独立的 u64 数组，**无内部锁**。推荐每线程一个实例，从主生成器或 OS 熵源播种。

### 与非加密保证

ccrandom **不适于加密**。输出经过非线性加扰，但保留线性状态转移的全部信息——约 O(2^64) 输出后状态可被重构。

---

## ccbi — 任意精度整数

头文件: [`include/ccbi.h`](https://github.com/CandyMi/ccalg/blob/master/include/ccbi.h)

### 类型

```c
#define CCBI_SSO_LIMBS  8   /* 可预定义覆盖，2~65536（受栈空间约束） */

typedef struct ccbi {
    uint32_t  *limbs;                    /* → internal[]（SSO）或堆 */
    uint32_t   internal[CCBI_SSO_LIMBS]; /* 内联缓冲区（默认 256-bit） */
    int        sign;                     /* -1/0/1 */
    uint32_t   used;                     /* used limb count */
    uint32_t   cap;                      /* allocated limb capacity */
} ccbi_t;  /* 20 + 4*CCBI_SSO_LIMBS B (默认 52B) */
```

元数据宏（取代直接字段访问）：

| 宏 | 作用 |
| --- | --- |
| `CCBI_SIGN(z)` / `CCBI_SET_SIGN(z,v)` | 符号 (-1/0/1) |
| `CCBI_USED(z)` / `CCBI_SET_USED(z,v)` | 有效 limb 数 |
| `CCBI_CAP(z)` / `CCBI_SET_CAP(z,v)` | 分配容量 |

### 生命周期

```c
void   ccbi_init(ccbi_t *z);          // 初始化为零（指向 SSO 缓冲）
void   ccbi_destroy(ccbi_t *z);       // 仅在堆上时 free
int    ccbi_copy(ccbi_t *z, const ccbi_t *src);
void   ccbi_swap(ccbi_t *a, ccbi_t *b); // O(1) 指针/SSO 混合交换
void   ccbi_zero(ccbi_t *z);
void   ccbi_one(ccbi_t *z);
```

### 赋值

```c
int  ccbi_from_int(ccbi_t *z, int64_t v);
int  ccbi_from_uint(ccbi_t *z, uint64_t v);
int  ccbi_from_str(ccbi_t *z, const char *str, int base);
     // base 2~36；十进制快速路径 9 位/chunk
```

### 比较

```c
int    ccbi_cmp(const ccbi_t *a, const ccbi_t *b);
int    ccbi_cmp_int(const ccbi_t *a, int64_t b);
int    ccbi_cmp_abs(const ccbi_t *a, const ccbi_t *b);
int    ccbi_sign(const ccbi_t *z);
int    ccbi_is_zero(const ccbi_t *z);
int    ccbi_is_one(const ccbi_t *z);
int    ccbi_is_neg(const ccbi_t *z);
```

### 算术

```c
int  ccbi_add(ccbi_t *z, const ccbi_t *a, const ccbi_t *b);
int  ccbi_sub(ccbi_t *z, const ccbi_t *a, const ccbi_t *b);
int  ccbi_mul(ccbi_t *z, const ccbi_t *a, const ccbi_t *b);  // 自动派发: schoolbook / Karatsuba / Toom-3
int  ccbi_div_rem(ccbi_t *q, ccbi_t *r, const ccbi_t *a, const ccbi_t *b);
int  ccbi_mod(ccbi_t *r, const ccbi_t *a, const ccbi_t *m);

void ccbi_neg(ccbi_t *z, const ccbi_t *a);
void ccbi_abs(ccbi_t *z, const ccbi_t *a);
int  ccbi_inc(ccbi_t *z);
int  ccbi_dec(ccbi_t *z);
int  ccbi_add_uint(ccbi_t *z, uint32_t v);
int  ccbi_mul_uint(ccbi_t *z, uint32_t v);
int  ccbi_muladd(ccbi_t *z, const ccbi_t *x, uint32_t y);  // z += x*y
```

### 移位

```c
int    ccbi_shl(ccbi_t *z, const ccbi_t *a, size_t n);
int    ccbi_shr(ccbi_t *z, const ccbi_t *a, size_t n);
size_t ccbi_bit_length(const ccbi_t *z);
```

### 位运算

对 magnitude（绝对值）进行按位逻辑运算，结果恒为非负。

```c
int  ccbi_and(ccbi_t *z, const ccbi_t *a, const ccbi_t *b);  // z = a & b
int  ccbi_or (ccbi_t *z, const ccbi_t *a, const ccbi_t *b);  // z = a | b
int  ccbi_xor(ccbi_t *z, const ccbi_t *a, const ccbi_t *b);  // z = a ^ b
int  ccbi_not(ccbi_t *z, const ccbi_t *a);                   // z = ~a  (bit_length 范围内取反)
```

单 bit 操作：

```c
int  ccbi_test_bit(const ccbi_t *z, size_t i);    // 返回 0/1，越界返回 0
int  ccbi_set_bit(ccbi_t *z, size_t i);            // 第 i 位置 1，自动扩容
int  ccbi_clear_bit(ccbi_t *z, size_t i);          // 第 i 位清零
int  ccbi_flip_bit(ccbi_t *z, size_t i);           // 第 i 位翻转，自动扩容
```

`ccbi_not` 仅在 `bit_length(a)` 范围内取反，高位清零。例如 `~1 = 0`（1-bit 空间）、`~2 = 1`（2-bit 空间）。

### 数论

```c
int ccbi_gcd(ccbi_t *z, const ccbi_t *a, const ccbi_t *b);
int ccbi_pow_mod(ccbi_t *z, const ccbi_t *base,
                 const ccbi_t *exp, const ccbi_t *mod);
```

### 转换

```c
int64_t   ccbi_to_int(const ccbi_t *z);
int       ccbi_to_str_len(const ccbi_t *z, int base);
int       ccbi_to_str_buf(const ccbi_t *z, char *buf, size_t len, int base);
char     *ccbi_to_str(const ccbi_t *z, int base);  // 内部 malloc
void      ccbi_free_str(char *s);
int       ccbi_print(const ccbi_t *z, int base, int newline);
```

### 分配器钩子

```c
#define CCBI_MALLOC(sz)   CCBI_REALLOC(NULL, sz)
#define CCBI_REALLOC(p,sz) realloc
#define CCBI_FREE(p)      free(p)
```

SSO 小值不走分配器。

### 编译期配置

| 宏 | 默认 | 说明 |
| --- | --- | --- |
| `CCBI_SSO_LIMBS` | `8` | SSO 缓冲区 limb 数（256-bit）。设 0 禁用 SSO |
| `CCBI_MALLOC` / `CCBI_REALLOC` / `CCBI_FREE` | malloc / realloc / free | 堆分配器替换钩子 |

---

## ccbits — 位运算原语

头文件: [`include/ccbits.h`](https://github.com/CandyMi/ccalg/blob/master/include/ccbits.h)

跨平台/编译器的位运算原语库。GCC/Clang/MSVC 平台使用编译器内建（单指令），
未知编译器使用纯 C 可移植 fallback。每个函数均可通过预定义宏覆盖。

**不依赖任何运行时支持，不需要初始化。**

### 函数一览

| 分类 | 函数 | 宽度 | 默认实现（GCC/Clang → MSVC → fallback）|
| --- | --- | :-: | --- |
| **popcount** | `ccbits_popcount8` | 8 | `__builtin_popcount` → `__popcnt16` → SWAR |
| | `ccbits_popcount16` | 16 | `__builtin_popcount` → `__popcnt16` → SWAR |
| | `ccbits_popcount32` | 32 | `__builtin_popcount` → `__popcnt` → SWAR |
| | `ccbits_popcount64` | 64 | `__builtin_popcountll` → `__popcnt64` → SWAR |
| **clz** | `ccbits_clz16` | 16 | `__builtin_clz` → `_BitScanReverse` → 二分分解 |
| | `ccbits_clz32` | 32 | `__builtin_clz` → `_BitScanReverse` → 二分分解 |
| | `ccbits_clz64` | 64 | `__builtin_clzll` → `_BitScanReverse64` → 二分分解 |
| **ctz** | `ccbits_ctz16` | 16 | `__builtin_ctz` → `_BitScanForward` → 二分分解 |
| | `ccbits_ctz32` | 32 | `__builtin_ctz` → `_BitScanForward` → de Bruijn 乘法 |
| | `ccbits_ctz64` | 64 | `__builtin_ctzll` → `_BitScanForward64` → de Bruijn 乘法 |
| **rotate** | `ccbits_rotl32` / `ccbits_rotr32` | 32 | 编译器 idiom 识别 → `_rotl`/`_rotr` → 移位+或 |
| | `ccbits_rotl64` / `ccbits_rotr64` | 64 | 编译器 idiom 识别 → `_rotl64`/`_rotr64` → 移位+或 |
| **bswap** | `ccbits_bswap16` | 16 | `__builtin_bswap16` → `_byteswap_ushort` → 移位 |
| | `ccbits_bswap32` | 32 | `__builtin_bswap32` → `_byteswap_ulong` → 移位 |
| | `ccbits_bswap64` | 64 | `__builtin_bswap64` → `_byteswap_uint64` → 移位 |
| **bitrev** | `ccbits_bitrev8` | 8 | `__builtin_bitreverse8`(Clang) → nibble 表 → 交换 |
| | `ccbits_bitrev32` | 32 | `__builtin_bitreverse32`(Clang) → 二分交换 |
| | `ccbits_bitrev64` | 64 | `__builtin_bitreverse64`(Clang) → 二分交换 |
| **pow2** | `ccbits_ceilpow2_32` | 32 | 位涂抹（无分支） |
| | `ccbits_ceilpow2_64` | 64 | 位涂抹（无分支） |
| **test** | `ccbits_ispow2_32` (宏) | 32 | `(x) > 0U & ((x) & (x-1U)) == 0U` |
| | `ccbits_ispow2_64` (宏) | 64 | `(x) > 0ULL & ((x) & (x-1ULL)) == 0ULL` |
| **bit_width** | `ccbits_bit_width8` | 8 | `32 - clz32((uint8_t)x)` （先掩码再 clz） |
| | `ccbits_bit_width16` | 16 | `32 - clz32((uint16_t)x)` （先掩码再 clz） |
| | `ccbits_bit_width32` | 32 | `32 - clz(x)` （借 clz） |
| | `ccbits_bit_width64` | 64 | `64 - clz(x)` （借 clz） |
| **mask_low** | `ccbits_mask_low32` | 32 | `(1U << n) - 1U`（n≥32 时饱和为全 1） |
| | `ccbits_mask_low64` | 64 | `(1ULL << n) - 1ULL`（n≥64 时饱和） |
| **parity** | `ccbits_parity8` | 8 | `popcount8(x) & 1` |
| | `ccbits_parity16` | 16 | `popcount16(x) & 1` |
| | `ccbits_parity32` | 32 | `popcount32(x) & 1` |
| | `ccbits_parity64` | 64 | `popcount64(x) & 1` |
| **sign_ext** | `ccbits_sign_ext32` | 32 | XOR-sub 分支无跳转 |
| | `ccbits_sign_ext64` | 64 | XOR-sub 分支无跳转 |

### 安全保证

| 安全点 | 说明 |
| --- | --- |
| **clz/ctz 对零输入** | 返回类型宽度（16/32/64），无 UB |
| **rotate 超宽移位** | `k` 内部 `k &= N-1` 后再移位，无 UB |
| **ceilpow2(0)** | 返回 0（有符号溢出回绕） |
| **ispow2(0)** | 返回 0 |

### 覆盖机制

所有函数均可通过 `#define` **在 `#include` 之前**覆盖：

```c
// 强制使用特定实现
#define ccbits_popcount32(x) fast_popcount(x)
#include "ccbits.h"
```

### 示例

```c
#include "ccbits.h"
#include <stdio.h>

int main() {
    uint32_t x = 0x12345678U;
    printf("popcount(%#x) = %d\n", x, ccbits_popcount32(x));       // 13
    printf("clz(%#x)     = %d\n", x, ccbits_clz32(x));            // 3
    printf("ctz(%#x)     = %d\n", x, ccbits_ctz32(x));            // 3
    printf("ceilpow2(%u)  = %u\n", x, ccbits_ceilpow2_32(x));     // 0x20000000
    printf("ispow2(%#x)  = %d\n", x, ccbits_ispow2_32(x));        // 0
    printf("bswap(%#x)   = %#x\n", x, ccbits_bswap32(x));         // 0x78563412

    uint32_t r = ccbits_rotl32(x, 8);
    printf("rotl(%#x, 8) = %#x\n", x, r);                         // 0x34567812

    uint64_t y = 0;
    printf("clz(0)        = %d\n", ccbits_clz64(y));              // 64，安全
}
```

---

## ccshuffle — Fisher-Yates 洗牌

头文件: [`include/ccshuffle.h`](https://github.com/CandyMi/ccalg/blob/master/include/ccshuffle.h)

**无分配**的原地 Fisher-Yates (Durstenfeld) 洗牌。O(n) 时间，O(1) 空间
（超过 `CCSHUFFLE_BSIZE` 的大元素暂用 `malloc` 做 swap buffer）。

### 类型

```c
typedef uint64_t (*ccshuffle_prng_t)(void *state) CCSHUFFLE_NOEXCEPT;
// PRNG 回调：返回 [0, 2^64) 均匀分布随机值。
// 可传入 ccrandom128_next / ccrandom256_next / ccrandom512_next。
```

### 编译期配置

| 宏 | 作用 | 默认 |
| --- | --- | --- |
| `CCSHUFFLE_BSIZE` | 栈上 swap buffer 大小（元素 > 此值走 heap `malloc`）| `64` |
| `CCSHUFFLE_NOEXCEPT` | C++17 noexcept 标注 | 未定义 |

### 洗牌函数

```c
void ccshuffle_knuth(void *base, size_t len, size_t sz,
                     void *state, ccshuffle_prng_t prng_next);
```

**参数：**
- `base` — 数组起始指针
- `len` — 元素数量（≤ 1 无操作）
- `sz` — 每个元素的字节大小
- `state` — PRNG 状态（传给 `prng_next`）
- `prng_next` — PRNG 回调

**示例：**

```c
ccrandom128_t rng;
ccrandom128_init(&rng, seed);

int arr[100];
ccshuffle_knuth(arr, 100, sizeof(int), &rng,
                (ccshuffle_prng_t)ccrandom128_next);
```

**NULL 安全：** base / state / prng_next 任一为 NULL 时无操作。

### 内部：随机范围生成

```c
size_t _ccshuffle_range(uint64_t rnd, size_t range,
                        void *state, ccshuffle_prng_t next);
```

返回 `[0, range)` 内的均匀随机索引。用于构建其他需要无偏随机索引的组件。
通过 Lemire 乘法消除模偏差——三平台实现：

| 平台 | 方法 | 指令 |
| --- | --- | --- |
| GCC/Clang x64 & ARM64, ICC | `(__uint128_t) rnd * range >> 64` | `mul` + `shr` |
| MSVC x64 | `_umul128(rnd, range, &hi)` | `mul` + `shr` |
| MSVC ARM64 | `rnd * range` + `__umulh(rnd, range)` | `mul` + `umulh` |
| 其他（i686, 老旧编译器） | 拒绝采样 `UINT64_MAX - (UINT64_MAX % range)` | 两个 `div` |

前三个路径完全消除 `div` 指令，拒绝重试概率约 `range / 2^64`（对实际 `range ≤ 10⁶` 约 `3e-14`，基本永不触发）。
