# 线程安全分析

cclag 容器**本身不是线程安全的**——与大多数 C 数据结构库一样，线程安全由调用方保证。本页分析在**外部读写锁 (rwlock)** 保护下，各容器的操作分类与可达并行度。

> **约定**：读锁 = 共享锁（多读锁可同时持有），写锁 = 独占锁（单写者，无并发读锁）。

---

## 汇总

| 容器 | 内部分配 | 读操作数 | 写操作数 | 读并行度 | 写并行度 | 关键注意 |
| --- | --- | --- | --- | --- | --- | --- |
| `ccmap` | 无 | 8 | 3 | N 路 | 1 路 | 只读安全，操作期间不改变结构 |
| `cchashmap` | 桶数组 | 2 | 4 | N 路 | 1 路 | **`set` 可能触发 resize**——必须持写锁 |
| `cclink` | 无 | 9 | 5 | N 路 | 1 路 | 只读安全；`verify` 仅调试用 |
| `cclist` | 无 | 11 | 11 | N 路 | 1 路 | 只读安全；`verify` 仅调试用 |
| `ccheap` | 指针数组 | 2 | 3 | N 路 | 1 路 | `peek` 返回内部指针，并发写使指针悬空 |
| `ccvector` | 元素数组 | 6 | 7 | N 路† | 1 路 | **† 读锁期间指针有效；写锁 resize 后指针全部失效** |
| `ccflatmap` | 元素数组 | 10 | 10 | N 路† | 1 路 | **† 同 ccvector——二分查找结果在写锁 resize 后悬空** |

---

## ccmap — 侵入式红黑树

**无内部分配**。树结构修改仅发生于 insert/erase。

| 操作 | 锁类型 | 并行度 | 备注 |
| --- | --- | --- | --- |
| `ccmap_find` | 读锁 | N 路 | 只读遍历，不修改任何字段 |
| `ccmap_begin` | 读锁 | N 路 | 返回 `first` 指针（insert/erase 维护） |
| `ccmap_first` | 读锁 | N 路 | `begin` 的同义词 |
| `ccmap_end` | 读锁 | N 路 | 总是返回 `NULL` |
| `ccmap_rbegin` | 读锁 | N 路 | O(log n) 从 root 走到最右 |
| `ccmap_next` | 读锁 | N 路 | 只读 `child[]` 和 `pc` 字段 |
| `ccmap_prev` | 读锁 | N 路 | 只读 `child[]` 和 `pc` 字段 |
| `ccmap_size` | 读锁 | N 路 | 原子读取 `size` 字段 |
| `ccmap_insert` | 写锁 | 1 路 | 修改树结构 + `size` + 可能更新 `first` |
| `ccmap_erase` | 写锁 | 1 路 | 修改树结构 + `size` + 可能更新 `first` |
| `ccmap_clear` | 写锁 | 1 路 | 重置 root/first/size |
| `ccmap_init` | — | — | 初始化调用，通常在锁创建之前 |

> **并行度总结**：所有读操作可完全并发（N 路），写操作串行化。适合读多写少场景。

---

## cchashmap — 侵入式链式哈希表

**内部管理桶数组（`cchashmap_node_t **`）**。`set` 可能在负载因子超过 `CCHASHMAP_MAX_LOAD` 时触发 2× resize（`realloc` 桶数组）。

| 操作 | 锁类型 | 并行度 | 备注 |
| --- | --- | --- | --- |
| `cchashmap_get` | 读锁 | N 路 | 只读遍历桶链表，不修改状态 |
| `cchashmap_find` | 读锁 | N 路 | `get` 的别名 |
| `cchashmap_size` | 读锁 | N 路 | 原子读取 `size` 字段 |
| `cchashmap_set` | **写锁** | 1 路 | ⚠️ 即使 key 重复（插入失败），也可能先触发 resize |
| `cchashmap_del` | 写锁 | 1 路 | 修改链表 + `size` |
| `cchashmap_clear` | 写锁 | 1 路 | 重置桶数组 |
| `cchashmap_destroy` | 写锁 | 1 路 | 释放桶数组；调用前须确保无其他线程访问 |
| `cchashmap_init` | — | — | 初始化调用，通常在锁创建之前 |

> **关键警告**：`set` 看似是"写操作"，但即使只是检查重复也需要写锁——因为 resize 可能在任何 `set` 调用中发生。**不能**先持读锁检查再升级为写锁（`pthread_rwlock` 不支持锁升级，会导致死锁或竞态）。
>
> **并行度总结**：读 2 路（`get` + `size`），写 1 路。适合极高读频率的缓存场景。

---

## cclink — 侵入式单向链表

**无内部分配**。链表仅通过节点指针操作。

| 操作 | 锁类型 | 并行度 | 备注 |
| --- | --- | --- | --- |
| `cclink_begin` | 读锁 | N 路 | 返回 `head` |
| `cclink_end` | 读锁 | N 路 | 总是 `NULL` |
| `cclink_next` | 读锁 | N 路 | 跟随 `node->next` |
| `cclink_front` | 读锁 | N 路 | 返回 `head` |
| `cclink_back` | 读锁 | N 路 | 返回 `tail` |
| `cclink_size` | 读锁 | N 路 | 读取 `size` |
| `cclink_empty` | 读锁 | N 路 | `size == 0` |
| `cclink_has_cycle` | 读锁 | N 路 | 调试用；遍历整个链表 |
| `cclink_verify` | 读锁 | N 路 | 调试用；并发写时结果无意义 |
| `cclink_push` | 写锁 | 1 路 | 头部插入，修改 head/size |
| `cclink_push_back` | 写锁 | 1 路 | 尾部追加，修改 tail/size |
| `cclink_remove` | 写锁 | 1 路 | O(n) 删除，修改链表 + size |
| `cclink_pop_front` | 写锁 | 1 路 | 弹出头部 |
| `cclink_clear` | 写锁 | 1 路 | 重置 head/tail/size |
| `cclink_init` | — | — | 初始化调用 |

> **并行度总结**：只读安全，可 N 路读并发。写操作串行化。

---

## cclist — 侵入式双向链表

**无内部分配**。含 head/tail 哨兵和 size 计数器。

| 操作 | 锁类型 | 并行度 | 备注 |
| --- | --- | --- | --- |
| `cclist_begin` | 读锁 | N 路 | 返回 `head` |
| `cclist_end` | 读锁 | N 路 | 总是 `NULL` |
| `cclist_rbegin` | 读锁 | N 路 | 返回 `tail` |
| `cclist_next` | 读锁 | N 路 | 跟随 `node->next` |
| `cclist_prev` | 读锁 | N 路 | 跟随 `node->prev` |
| `cclist_front` | 读锁 | N 路 | 返回 `head` |
| `cclist_back` | 读锁 | N 路 | 返回 `tail` |
| `cclist_size` | 读锁 | N 路 | 读取 `size` |
| `cclist_empty` | 读锁 | N 路 | `size == 0` |
| `cclist_has_cycle` | 读锁 | N 路 | 调试用 |
| `cclist_verify` | 读锁 | N 路 | 调试用；并发写时结果无意义 |
| `cclist_push_front` | 写锁 | 1 路 | 修改 head/size |
| `cclist_push_back` | 写锁 | 1 路 | 修改 tail/size |
| `cclist_pop_front` | 写锁 | 1 路 | 修改 head/size |
| `cclist_pop_back` | 写锁 | 1 路 | 修改 tail/size |
| `cclist_insert_before` | 写锁 | 1 路 | 修改节点链接 |
| `cclist_insert_after` | 写锁 | 1 路 | 修改节点链接 |
| `cclist_remove` | 写锁 | 1 路 | 修改节点链接 + size |
| `cclist_splice_back` | 写锁 | 1 路 | 批量移动节点 |
| `cclist_move` | 写锁 | 1 路 | 内部调用 `splice_back` |
| `cclist_clear` | 写锁 | 1 路 | 重置 head/tail/size |
| `cclist_init` | — | — | 初始化调用 |

> **并行度总结**：只读安全，N 路读并发。写操作串行化。

---

## ccheap — D-ary 堆

**内部管理指针数组（`ccheap_node_t **`）**。`peek` 返回数组内部指针。

| 操作 | 锁类型 | 并行度 | 备注 |
| --- | --- | --- | --- |
| `heap_peek` | 读锁 | N 路 | 返回 `data.buckets[0]` 指针——读锁期间有效 |
| `heap_size` | 读锁 | N 路 | 读取 `data.len` |
| `heap_insert` | 写锁 | 1 路 | 可能触发 resize；上浮调整 |
| `heap_pop` | 写锁 | 1 路 | 下沉调整；可能触发缩容（当前实现不缩容） |
| `heap_destroy` | 写锁 | 1 路 | 释放内部数组 |
| `heap_init` | — | — | 初始化调用 |

> **并行度总结**：读操作极少（仅 `peek` + `size`），N 路并发。写操作串行化。
>
> **`peek` 指针有效性**：读锁释放后，`peek` 返回的指针**不应再使用**——后续的写操作可能触发 resize 使指针失效。如需长期持有，调用方应在读锁内拷贝数据。

---

## ccvector — 动态数组

**值存储**，内部管理连续内存。读操作返回内部数组指针。

| 操作 | 锁类型 | 并行度 | 备注 |
| --- | --- | --- | --- |
| `ccvector_at` | 读锁 | N 路† | 返回 `&buckets[i]`——读锁期间有效 |
| `ccvector_front` | 读锁 | N 路† | 返回 `&buckets[0]` |
| `ccvector_back` | 读锁 | N 路† | 返回 `&buckets[len-1]` |
| `ccvector_size` | 读锁 | N 路 | 读取 `len` |
| `ccvector_capacity` | 读锁 | N 路 | 读取 `cap` |
| `ccvector_empty` | 读锁 | N 路 | `len == 0` |
| `ccvector_push_back` | 写锁 | 1 路 | 可能触发 2× resize——**所有现存指针失效** |
| `ccvector_pop_back` | 写锁 | 1 路 | `len--`，不触发 resize |
| `ccvector_reserve` | 写锁 | 1 路 | 预分配，可能触发 resize |
| `ccvector_clear` | 写锁 | 1 路 | `len = 0`，不释放内存 |
| `ccvector_destroy` | 写锁 | 1 路 | 释放内部数组 |
| `ccvector_init` | — | — | 初始化调用 |

> **† 关键警告**：`at`/`front`/`back` 返回内部数组指针。读锁释放后或任何 `push_back`/`reserve` 写操作后，这些指针**立即失效**。如需跨锁使用数据，应在读锁内拷贝值。
>
> **并行度总结**：读 N 路（指针在读锁内有效），写 1 路。适合只读快照或读锁内即用即弃的模式。

---

## ccflatmap — 排序数组映射

**值存储**，内部管理连续内存，二分查找。与 `ccvector` 有相同的指针失效风险。

| 操作 | 锁类型 | 并行度 | 备注 |
| --- | --- | --- | --- |
| `ccflatmap_find` | 读锁 | N 路† | 二分查找，返回内部数组指针 |
| `ccflatmap_begin` | 读锁 | N 路† | 返回 `&buckets[0]` |
| `ccflatmap_end` | 读锁 | N 路 | 总是 `NULL` |
| `ccflatmap_rbegin` | 读锁 | N 路† | 返回 `&buckets[len-1]` |
| `ccflatmap_next` | 读锁 | N 路† | `p + 1`（指针算术） |
| `ccflatmap_prev` | 读锁 | N 路† | `p - 1` |
| `ccflatmap_at` | 读锁 | N 路† | 返回 `&buckets[i]` |
| `ccflatmap_size` | 读锁 | N 路 | 读取 `len` |
| `ccflatmap_empty` | 读锁 | N 路 | `len == 0` |
| `ccflatmap_capacity` | 读锁 | N 路 | 读取 `cap` |
| `ccflatmap_insert` | 写锁 | 1 路 | 二分查找 + memmove；可能 resize |
| `ccflatmap_erase` | 写锁 | 1 路 | 二分查找 + memmove |
| `ccflatmap_erase_at` | 写锁 | 1 路 | 跳过查找，直接 memmove |
| `ccflatmap_erase_unordered` | 写锁 | 1 路 | swap-with-last，破坏排序 |
| `ccflatmap_push_back` | 写锁 | 1 路 | 无序追加，可能 resize |
| `ccflatmap_sort` | 写锁 | 1 路 | 原地快速排序 |
| `ccflatmap_reserve` | 写锁 | 1 路 | 可能触发 resize |
| `ccflatmap_clear` | 写锁 | 1 路 | `len = 0` |
| `ccflatmap_destroy` | 写锁 | 1 路 | 释放内部数组 |
| `ccflatmap_init` | — | — | 初始化调用 |

> **† 指针失效警告**：同 `ccvector`——所有读操作返回的内部指针在读锁释放后或写锁 resize 后失效。
>
> **并行度总结**：读 N 路（指针在读锁内有效），写 1 路。批量操作（`push_back` 循环 + `sort`）应在单个写锁临界区内完成。

---

## 使用模式建议

### 模式 1：标准 rwlock 包装

```c
pthread_rwlock_t lock = PTHREAD_RWLOCK_INITIALIZER;
ccmap_t map;
ccmap_init(&map, my_cmp);

// 读操作
pthread_rwlock_rdlock(&lock);
ccmap_node_t *n = ccmap_find(&map, &probe);
// ... 使用 n ...
pthread_rwlock_unlock(&lock);

// 写操作
pthread_rwlock_wrlock(&lock);
ccmap_insert(&map, &node, NULL);
pthread_rwlock_unlock(&lock);
```

### 模式 2：值拷贝避免指针失效（ccvector / ccflatmap）

```c
pthread_rwlock_rdlock(&lock);
CCFLATMAP_NODE_T *p = ccflatmap_find(&m, &probe);
CCFLATMAP_NODE_T copy = *p;  // 读锁内拷贝
pthread_rwlock_unlock(&lock);
// 此后使用 copy——不受后续写操作影响
```

### 模式 3：批量写入

```c
pthread_rwlock_wrlock(&lock);
ccflatmap_reserve(&m, N);
for (int i = 0; i < N; i++)
    ccflatmap_push_back(&m, items[i]);
ccflatmap_sort(&m);
pthread_rwlock_unlock(&lock);
```

### 不推荐的模式

```c
// ❌ 锁升级——pthread_rwlock 不支持
pthread_rwlock_rdlock(&lock);
if (!cchashmap_get(&m, &probe)) {
    pthread_rwlock_unlock(&lock);      // 窗口期！
    pthread_rwlock_wrlock(&lock);      // 其他线程可能已插入
    cchashmap_set(&m, &node, NULL);
}
pthread_rwlock_unlock(&lock);

// ✅ 正确：直接持写锁
pthread_rwlock_wrlock(&lock);
if (!cchashmap_get(&m, &probe))
    cchashmap_set(&m, &node, NULL);
pthread_rwlock_unlock(&lock);
```

---

## 内部依赖的线程安全性

| 内部依赖 | 线程安全性 |
| --- | --- |
| 桶/指针数组（内联于 cchashmap / ccheap） | 无锁——由外层容器的写锁保护 |
| 用户提供的比较/哈希回调 | 调用方保证——回调不应有副作用或访问共享状态 |
| 用户提供的分配器（`CCXXX_REALLOC` 等） | 调用方保证——自定义分配器需自行保证线程安全 |

cchashmap 和 ccheap 内部自管桶/指针数组，线程安全完全由外层容器的锁保证。
