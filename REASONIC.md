# REASONIC — 项目设计规范与接口约定

> 自动提取自 `include/*.h`，用于后续开发与评审参考。

---

## 项目概览

- **项目名**: `alg`
- **类型**: header-only C 数据结构库 (兼容 C89 / C99 / C++ / MSVC)
- **许可**: BSD
- **作者**: CandyMi
- **核心特征**: 侵入式 (intrusive)、零内部节点分配 (部分除外)、编译期零开销回调

---

## 1. 文件组织

| 文件 | 结构 | 说明 |
| --- | --- | --- |
| [ccmap.h](include/ccmap.h) | `ccmap_t` / `ccmap_node_t` | 侵入式红黑树 (有序映射) |
| [cchashmap.h](include/cchashmap.h) | `cchashmap_t` / `cchashmap_node_t` | 侵入式链式哈希表 |
| [cclink.h](include/cclink.h) | `cclink_t` / `cclink_node_t` | 侵入式单向链表 |
| [cclist.h](include/cclist.h) | `cclist_t` / `cclist_node_t` | 侵入式双向链表 |
| [ccheap.h](include/ccheap.h) | `ccheap_t` / `ccheap_node_t` | D-ary 堆 (优先队列，支持值/指针双模式) |

---

## 2. 侵入式设计 (Intrusive Containers)

### 2.1 节点嵌入

用户将 `xxx_node_t` 嵌入自己的结构体，容器只管理节点指针，**不分配/释放用户结构体内存**。

```c
struct my_entry {
    int key;
    ccmap_node_t node;    // 嵌入节点
};
```

**例外**: `cchashmap_t` 内部管理桶数组 (`buckets`)，`ccheap_t` 在 `CCHEAP_VALUE` 模式下拥有元素内存。

### 2.2 container_of 宏

所有容器提供 `CCXXX_CONTAINER(ptr, type, member)` 从节点指针还原出父结构体指针。实现基于标准 `offsetof` + 指针运算。

```
CCMAP_CONTAINER(ptr, type, member)
CCHASHMAP_CONTAINER(ptr, type, member)
CCLIST_CONTAINER(ptr, type, member)
(ccheap 无此宏，但可通过 ccheap_node_t 的直接字段访问)
```

### 2.3 节点类型保护

每个容器的节点 `typedef` 外有 `#ifndef CCXXX_NODE_T` 守卫，允许用户在 `#include` 之前预先定义自己的节点结构，避免冲突。

---

## 3. 可移植 inline 宏

所有公共 API 函数声明为 `static inline`，通过以下模式实现跨编译器兼容：

```c
#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
  #define CCXXX_INLINE static inline     // C99 / C++
#elif defined(_MSC_VER)
  #define CCXXX_INLINE static __inline    // MSVC
#elif defined(__GNUC__)
  #define CCXXX_INLINE static __inline__  // GNU89
#else
  #define CCXXX_INLINE static             // 退化
#endif
```

每个容器有自己的 `CCXXX_INLINE` 前缀 (`CCMAP_INLINE`、`CCHASHMAP_INLINE`、`CCLIST_INLINE`、`CCHEAP_INLINE`)。

---

## 4. 比较/哈希分发机制 (零开销回调)

### 4.1 两种模式

| 模式 | 机制 | 开销 |
| --- | --- | --- |
| **函数指针** (默认) | `init()` 时传入回调函数 | 间接调用 |
| **宏定义** (可选) | `#include` 前 `#define` 比较/哈希宏 | 编译期内联，零开销 |

### 4.2 具体分发宏

| 容器 | 函数指针类型 | 宏覆盖 |
| --- | --- | --- |
| `ccmap` | `ccmap_cmp_t`: `int64_t (*)(const ccmap_node_t*, const ccmap_node_t*)` | `#define CCMAP_COMPARE(a, b)` |
| `cchashmap` | `cchashmap_hash_t`: `uint64_t (*)(const cchashmap_node_t*, size_t seed)` <br> `cchashmap_equal_t`: `bool (*)(const cchashmap_node_t*, const cchashmap_node_t*)` | `#define CCHASHMAP_HASH(n, seed)` <br> `#define CCHASHMAP_EQUAL(a, b)` |
| `ccheap` | `ccheap_compare_t`: `int64_t (*)(const ccheap_node_t*, const ccheap_node_t*)` | `#define CCHEAP_COMPARE(a, b)` |
| `cclist` | 无需比较 | 无 |

当用户定义了宏时，`init()` 对应的函数指针参数用 `(void)arg` 静默忽略，消除 "unused parameter" 警告。

### 4.3 比较语义约定

- 返回值 `int64_t`
- `> 0` → `a` 优先级更高（更靠近根 / 更小序）
- `< 0` → `b` 优先级更高
- `== 0` → 相等
- `ccmap` 相等表示重复 key，插入失败返回 -1

---

## 5. 内存分配器挂钩

每个需要动态内存的容器提供可替换的分配宏：

| 宏 | 作用 | 默认值 |
| --- | --- | --- |
| `CCXXX_MALLOC(sz)` | 分配 | `CCXXX_REALLOC(NULL, sz)` |
| `CCXXX_REALLOC` | 重分配 | `realloc` |
| `CCXXX_FREE(ptr)` | 释放 | `free(ptr)` |

- `cchashmap` 和 `ccheap` 使用此机制
- `ccmap` 和 `cclist` 完全无内部分配，不需要这些宏

---

## 6. API 命名惯例

### 6.1 生命周期

| 操作 | 命名 | 说明 |
| --- | --- | --- |
| 初始化 | `xxx_init(m, ...)` | 参数依次为: 容器指针, 回调函数... |
| 销毁 | `xxx_destroy(m)` | 释放内部资源，不释放用户节点 |
| 清空 | `xxx_clear(m)` | 重置为空，不释放内存 |

### 6.2 增删查

| 操作 | 通用名 | 别名 | 返回值 |
| --- | --- | --- | --- |
| 插入 | `xxx_insert(m, node, out)` | hashmap: `xxx_set` | `0` 成功 / `-1` 重复或失败 |
| 查找 | `xxx_find(m, probe)` | hashmap: `xxx_get` | 找到的节点指针 / `NULL` |
| 删除 | `xxx_erase(m, node)` | hashmap: `xxx_del` | `void` |

**`out` 参数模式**: `ccmap_insert` 和 `cchashmap_set` 有 `xxx_node_t **out` 参数——若 key 已存在，写入已存在的节点指针；若插入成功，写入新节点指针。

### 6.3 迭代器

| 操作 | 命名 | 说明 |
| --- | --- | --- |
| 前向首 | `xxx_begin(m)` / `xxx_first(m)` | 按序第一个元素 |
| 后向首 | `xxx_rbegin(m)` | reverse begin |
| 末尾哨兵 | `xxx_end(m)` | 总是返回 `NULL` |
| 后继 | `xxx_next(n)` | 节点 → 下一节点 |
| 前驱 | `xxx_prev(n)` | 节点 → 前驱节点 |

**约定**: `xxx_end()` 总是返回 `NULL`（作为迭代终点哨兵）。标准迭代模式：

```c
for (ccmap_node_t *n = ccmap_begin(&m); n != ccmap_end(&m); n = ccmap_next(n)) {
    struct my_entry *e = CCMAP_CONTAINER(n, struct my_entry, node);
}
```

`ccmap` 额外维护 `first` 指针，因此提供 `ccmap_first()` 作为 `ccmap_begin()` 的同义词，且在有 `first` 时插入有 fast-path。

### 6.4 链表特有操作

| 操作 | 命名 |
| --- | --- |
| 头插 / 尾插 | `push_front` / `push_back` |
| 头弹 / 尾弹 | `pop_front` / `pop_back` |
| 前插 / 后插 | `insert_before` / `insert_after` |
| 移除 | `remove` |
| 拼接 | `splice_back` (移动 src → dst 尾部) |
| 转移 | `move(to, from)` — 带错误码的 splice |

### 6.5 大小 / 空判断

| 操作 | 命名 |
| --- | --- |
| 大小 | `xxx_size(m)` |
| 判空 | `xxx_empty(m)` (仅 cclist) |

---

## 7. 错误处理惯例

### 7.1 返回值模式

| 返回值 | 含义 |
| --- | --- |
| `NULL` | 查无此值 / 容器空 / 参数无效 |
| `0` | 成功 |
| `-1` | 失败 (参数无效 / 重复 / 内存不足) |
| `false` / `true` | bool 操作 (hashmap insert 等) |

### 7.2 空指针安全

所有公共函数在第一行进行 `if (!m)` 或 `if (!m || !node)` 守卫，传入 `NULL` 容器不会崩溃。

### 7.3 链表专用错误码

`cclist` 提供详细错误码枚举 `cclist_ecode_t` (`UNKNOWN`、`NOERROR`、`ISNULL`、`NOHEAD`、`NONEXT`、`HASCYCLE`、`MISSPREV`、`SIZEERROR`) 和 `cclist_verify()` 用于调试时检查双向链表不变量。

---

## 8. 配置宏汇总

### 编译前可定义的宏

| 宏 | 所属 | 作用 | 默认 |
| --- | --- | --- | --- |
| `CCMAP_COMPARE(a,b)` | ccmap | 内联比较函数 | 无 (用函数指针) |
| `CCMAP_NODE_T` | ccmap | 替代默认节点定义 | 无 |
| `CCHASHMAP_HASH(n,s)` | cchashmap | 内联哈希函数 | 无 |
| `CCHASHMAP_EQUAL(a,b)` | cchashmap | 内联判等函数 | 无 |
| `CCHASHMAP_NODE_T` | cchashmap | 替代默认节点定义 | 无 |
| `CCHASHMAP_MAX_LOAD` | cchashmap | 最大负载因子 | `1.25` |
| `CCHASHMAP_REALLOC` | cchashmap | 重分配函数 | `realloc` |
| `CCHASHMAP_MALLOC` | cchashmap | 分配函数 | `realloc(NULL, sz)` |
| `CCHASHMAP_FREE` | cchashmap | 释放函数 | `free(ptr)` |
| `CCHEAP_COMPARE(a,b)` | ccheap | 内联比较函数 | 无 |
| `CCHEAP_VALUE` | ccheap | 值模式 (直接存元素) | 无 (指针模式) |
| `CCHEAP_ARITY` | ccheap | D-ary 分叉数 | `2` (可选 `4`/`8`) |
| `CCHEAP_REALLOC` | ccheap | 重分配 | `realloc` |
| `CCHEAP_MALLOC` | ccheap | 分配 | `realloc(NULL, sz)` |
| `CCHEAP_FREE` | ccheap | 释放 | `free` |
| `CCHEAP_DEFAULT_CAP` | ccheap | 初始容量 | `32` |
| `CCNODE_T` | ccheap | 替代默认节点类型 | `struct ccheap_node` |

### 内部常量

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `CCMAP_RED` / `CCMAP_BLACK` | `0` / `1` | 红黑树颜色 |
| `CCMAP_LEFT` / `CCMAP_RIGHT` | `0` / `1` | 树方向 |
| `CCHASHMAP_DEFAULT_SLOT` | `8` | hashmap 初始桶数 |
| `CCHEAP_DEFAULT_CAP` | `32` | heap 初始容量 |

---

## 9. 红黑树实现细节 (ccmap)

- **颜色编码**: 存储在父指针 `pc` 的最低有效位 (parent pointer | color)。节省 `sizeof(uint32_t)` 的开销。
- **节点大小**: `ccmap_node_t` 为 16 字节 (64-bit): `child[2]` (16B) + `pc` (8B)。
- **维护 `first` 指针**: 额外维护最小节点指针，使得 `ccmap_first()` / `ccmap_begin()` 为 O(1)，且插入时若比 `first` 更小则有 fast-path。
- **内部辅助函数**: 以 `_rb_` 前缀命名 (`_rb_p`, `_rb_c`, `_rb_red`, `_rb_sp`, `_rb_sc`, `_rb_spc`, `_rb_min`, `_rb_rot_left`, `_rb_rot_right`, `_rb_ins_fix`, `_rb_del_fix`)。
- **删除后 `first` 更新**: `ccmap_erase` 需要 O(log n) 时间找到新的最小节点。

---

## 10. 哈希表实现细节 (cchashmap)

- **链式哈希**: 数组 + 单向链表，`cchashmap_node_t` 自带 `hash` 字段缓存哈希值。
- **负载因子**: 默认 `CCHASHMAP_MAX_LOAD = 1.25`，超过时自动 resize (2x 扩容)。
- **种子**: 初始化时用容器地址 `(size_t)(uintptr_t)m` 作为 hash seed。
- **幂次容量**: bucket 索引用 `hash & (cap - 1)` 位掩码，依赖容量为 2 的幂。
- **别名**: `insert`=`set`、`find`=`get`、`erase`=`del`，通过 `#define` 提供两种命名风格。
- **destroy**: 调用方必须先删除所有节点再调用 `cchashmap_destroy` — 该函数只释放桶数组。
- **`out` 参数**: `cchashmap_set` 的 `old` 参数——key 存在时设为已存在节点，插入成功时设为 `NULL`。

---

## 11. 链表实现细节 (cclist)

- **双向链表**: `head` / `tail` 哨兵 + `size` 计数。
- **内部函数**: `_cclist_link` 和 `_cclist_unlink` 不暴露，所有公有 API 调用它们。
- **移动 / 拼接**: `cclist_move` 返回详细错误码 (`-1` NULL、`-2` 空、`-3` 自移动)，内部调用 `cclist_splice_back`。
- **不变量验证**: `cclist_verify()` 执行 6 项检查，返回 `cclist_ecode_t`。
- **环路检测**: Floyd 龟兔算法 `cclist_has_cycle()`。
- **bool 兼容**: C89 不提供 `<stdbool.h>`，手动 `typedef int bool` + `#define true/false`。

---

## 12. 堆实现细节 (ccheap)

### 12.1 双存储模式

| 模式 | 定义 | 数据布局 | 所有权 |
| --- | --- | --- | --- |
| **指针模式** (默认) | 无 | `CCNODE_T **data` (指针数组) | 调用方拥有节点 |
| **值模式** | `#define CCHEAP_VALUE` | `CCNODE_T *data` (连续值数组) | 堆拥有元素 |

值模式注意事项 (文档原文):
> IMPORTANT: value mode uses struct assignment (shallow copy). If your node_t contains pointers (e.g. `char *name`), use pointer mode instead.

### 12.2 value 模式的 pop 陷阱

`heap_pop` 在值模式下返回指向内部缓冲 `heap->popped` 的指针，**下一次 `heap_pop` 调用会覆盖该缓冲区**。调用方必须在下次 pop 前消费或拷贝返回值。

### 12.3 D-ary 展开

子节点比较循环在编译时根据 `CCHEAP_ARITY` 展开 (2/4/8)，通过 `#if CCHEAP_ARITY_N > N` 逐层条件编译。索引使用宏: `CCHEAP_PARENT(i)`、`CCHEAP_CHILD(i, k)`。

### 12.4 默认节点结构

`ccheap_node_t` 预设了 `conv` 和 `timestamp` 字段——用户可通过 `#define CCNODE_T` 覆盖。

---

## 13. 跨容器共享宏

| 宏 | 来源 | 复用者 |
| --- | --- | --- |
| `CCMAP_CONTAINER` (= `container_of`) | ccmap | ccmap 专属 |
| `CCHASHMAP_CONTAINER` (= `container_of`) | cchashmap | cchashmap 专属 |
| `CCMAP_INLINE` | ccmap | ccmap 内联宏 |
| `CCHASHMAP_INLINE` | cchashmap | cchashmap 内联宏 |
| `CCHASHMAP_REALLOC` / `CCHASHMAP_MALLOC` / `CCHASHMAP_FREE` | cchashmap | hashmap 分配器挂钩 |
| `CCHASHMAP_MAX_LOAD` / `CCHASHMAP_DEFAULT_SLOT` | cchashmap | hashmap 专属配置 |

**注意**: 各容器宏前缀已统一为各自命名空间（`CCMAP_` → ccmap，`CCHASHMAP_` → cchashmap，`CCLIST_` → cclist，`CCHEAP_` → ccheap），不再有跨容器复用。

---

## 14. 新增容器清单

向此库添加新数据结构时，应遵循:

1. [ ] 单个 header 文件放入 `include/`，以 `#ifndef CCXXX_H` / `#define CCXXX_H` / `#endif` 守卫
2. [ ] 提供 `CCXXX_INLINE` 可移植 inline 宏
3. [ ] 侵入式节点 `ccxxx_node_t`，带 `CCXXX_NODE_T` 守卫
4. [ ] `container_of` → `CCXXX_CONTAINER` 宏
5. [ ] 比较/哈希双模式分发 (函数指针 + 宏覆盖)，宏定义时 `(void)arg` 抑制警告
6. [ ] `xxx_init` / `xxx_clear` / (可选 `xxx_destroy`)
7. [ ] `xxx_insert` / `xxx_find` / `xxx_erase`（可加 `xxx_set` / `xxx_get` / `xxx_del` 别名）
8. [ ] `xxx_begin` / `xxx_end` / `xxx_next` 迭代器三件套；有序容器加 `xxx_prev`
9. [ ] `xxx_size`
10. [ ] 所有公有函数 `NULL` 参数安全
11. [ ] 动态内存管理的容器提供 `CCXXX_REALLOC` / `CCXXX_MALLOC` / `CCXXX_FREE` 分配器挂钩
12. [ ] 内部辅助函数以 `_xxx_` 前缀 (或 `_ccxxx_`) 标记
13. [ ] 保留 `(void)arg` 模式处理宏/函数指针二选一
14. [ ] BSD 许可证头部注释 + 简短设计说明

---

## 15. 构建、测试与基准测试

### 15.1 CMake 构建 (推荐)

```bash
# 配置 — 所有产物隔离在 build/ 目录
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# 全量构建 (4 个测试 + 4 个 benchmark)
cmake --build build

# 运行单元测试 (CTest, label 过滤)
ctest --test-dir build -L unit --output-on-failure

# 一键构建 + 测试
cmake --build build --target check

# 一键构建 + 基准测试
cmake --build build --target bench
```

### 15.2 Premake5 构建 (备选)

```bash
premake5 gmake2                   # 生成 Makefile
make -C build config=release -j4  # 构建（5 测试 + 5 基准）
```

产物：`build/test_*`（测试）、`build/bench_*`（基准）。单个目标：

```bash
make -C build config=release test_cclink   # 只构建单向链表测试
./build/test_cclink                        # 运行
```

### 15.3 手动编译

无构建系统时也可直接编译：

```bash
# 测试
gcc -std=c99 -Wall -Wextra -Wno-missing-field-initializers \
    -o test_ccmap tests/test_ccmap.c && ./test_ccmap

# 基准
g++ -std=c++11 -O2 -Wall -o bench_ccmap bench/bench_ccmap.cpp && ./bench_ccmap
```

### 15.4 测试覆盖总览

| 测试文件 | 覆盖容器 | 断言数 |
| --- | --- | --- |
| `tests/test_ccmap.c` | ccmap 红黑树 | 2058 |
| `tests/test_cchashmap.c` | cchashmap 哈希表 | 2043 |
| `tests/test_cclink.c` | cclink 单向链表 | 54 |
| `tests/test_cclist.c` | cclist 双向链表 | 78 |
| `tests/test_ccheap.c` | ccheap D-ary 堆 | 1255 |

### 15.5 基准测试对照

| 基准文件 | 对比 | 规模 |
| --- | --- | --- |
| `bench/bench_ccmap.cpp` | ccmap vs `std::map` | 100K insert/find/erase |
| `bench/bench_cchashmap.cpp` | cchashmap vs `std::unordered_map` | 100K random |
| `bench/bench_cclist.cpp` | cclist vs `std::list` | 200K push/iterate/pop/splice |
| `bench/bench_ccheap.cpp` | ccheap vs `std::priority_queue` | 200K push/pop |

---

## 16. 变更传播约定

### 16.1 级联更新

核心头文件 (`include/*.h`) 的任何修改，必须同步更新下游产物：

```
include/*.h 变更
  → tests/test_*.c           (测试用例覆盖新行为)
  → bench/bench_*.cpp        (基准数据重新采集)
  → docs/*.md                (API 文档 / 基准数据更新)
  → build/docs-html/*.html   (重新生成 HTML 页面)
```

一个头文件的修改在 HTML 页面反映之前不算完成。

### 16.2 提交前拉取

推送远程前必须先拉取并处理冲突：

```bash
git pull --rebase origin master   # 拉取 + 变基
# 如有冲突 → 解决 → git add → git rebase --continue
# 重新构建 + 运行测试确认无回归
# 询问用户确认后再推送
```

---

*此文档由 Reasonix Code 从 `include/*.h` 自动分析生成。*
