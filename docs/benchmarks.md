# 性能基准

> GCC · -O2 · Release。数据为单次运行取最优值。

## ccmap vs `std::map` — 100K 顺序操作

### 函数指针模式（默认）

| 操作 | ccmap | `std::map` | 倍率 |
| --- | --- | --- | --- |
| insert | 22.55 ms | 14.38 ms | 1.57× |
| find | 9.11 ms | 8.61 ms | 1.06× |
| erase | **2.15 ms** | 24.46 ms | **0.09×** |

### `CCMAP_COMPARE` 宏模式（零开销内联比较）

| 操作 | ccmap | `std::map` | 倍率 |
| --- | --- | --- | --- |
| insert | 19.60 ms | 13.54 ms | 1.45× |
| find | **6.84 ms** | 8.27 ms | **0.83×** |
| erase | **1.94 ms** | 15.96 ms | **0.12×** |

> **erase 始终快 8-11×**：侵入式设计不释放节点内存，只修改指针。`std::map` 需释放内部树节点 + 析构 `pair`。
>
> **宏模式**：insert 接近持平（1.45×），find 反超（0.83×）——比较完全内联，与 `std::map` 模板 functor 相同零开销。
> 定义 `CCMAP_COMPARE(a,b)` 宏即可切换，无需修改业务代码。

## cchashmap vs `std::unordered_map` vs uthash — 200K 随机操作

| 操作 | cchashmap | uthash | `std::unordered_map` |
| --- | --- | --- | --- |
| insert | **12.22 ms** | 32.14 ms | 33.45 ms |
| find | **5.35 ms** | 38.03 ms | 9.44 ms |
| erase | **10.23 ms** | 23.80 ms | 73.68 ms |

| 操作 | cchashmap/stl | uthash/stl | cchashmap/uthash |
| --- | --- | --- | --- |
| insert | **0.37×** | 0.96× | **0.38×** |
| find | **0.57×** | 4.03× | **0.14×** |
| erase | **0.14×** | 0.32× | **0.43×** |

> 全链路优于 STL 和 uthash。cchashmap 与 uthash 同为 header-only 侵入式 C 哈希表，但 cchashmap 缓存 hash 值（避免重复计算）+ 位掩码索引 + `CCHASHMAP_DEFAULT_SLOT=64` 减少 resize 次数，find 比 uthash 快 7×。

## cclist vs `std::list` — 200K 操作

| 操作 | cclist | `std::list` | 倍率 |
| --- | --- | --- | --- |
| push_back | **0.76 ms** | 9.82 ms | **0.08×** |
| iterate | 0.53 ms | 0.69 ms | 0.77× |
| pop_front | **0.69 ms** | 8.27 ms | **0.08×** |
| splice | <0.01 ms | <0.01 ms | — |

> push/pop 比 `std::list` 快 12×。侵入式链表无单独节点分配开销。

## cclink vs `std::forward_list` — 200K 操作

| 操作 | cclink | `std::forward_list` | 倍率 |
| --- | --- | --- | --- |
| push | **0.26 ms** | 8.69 ms | **0.03×** |
| iterate | 0.57 ms | 0.62 ms | 0.93× |
| pop | **0.47 ms** | 9.02 ms | **0.05×** |

> push/pop 比 `std::forward_list` 快 19-33×。侵入式单链表 head+tail 指针，push_front / pop_front 均为 O(1)。

## ccheap vs `std::priority_queue` — 200K 操作

### 函数指针模式（默认，`container_of` 恢复父结构体）

| 操作 | ccheap | `std::priority_queue` | 倍率 |
| --- | --- | --- | --- |
| push | 4.61 ms | 3.84 ms | 1.20× |
| pop | 55.01 ms | 31.37 ms | 1.75× |

### `CCHEAP_COMPARE` 宏模式（零开销内联比较）

| 操作 | ccheap | `std::priority_queue` | 倍率 |
| --- | --- | --- | --- |
| push | 6.22 ms | 3.95 ms | 1.57× |
| pop | 57.50 ms | 22.11 ms | 2.60× |

> 侵入式指针数组。函数指针模式与 STL 接近；宏模式直接比较 `node.priority` 字段（无需 `container_of`），消除间接调用。
> 通过 `-DCCHEAP_ARITY=4` / `8` 可切换 D-ary 分叉数。

## ccvector vs `std::vector` — 500K 操作

| 操作 | ccvector | `std::vector` | 倍率 |
| --- | --- | --- | --- |
| push_back | **1.22 ms** | 2.30 ms | **0.53×** |
| random access | 4.67 ms | 6.80 ms | **0.83×** |
| pop_back | ~0 ms | ~0 ms | ~1× |

> push_back 比 STL 快 1.9×——ccvector 更轻量。`ccvector_at` 用 `__builtin_expect` 标注边界检查为 unlikely 分支，编译器将热路径优化为直接数组索引，access 反超 STL。

## ccflatmap vs ccmap vs `std::map` — 50K 随机操作

| 操作 | ccflatmap | flat+sort | ccmap | `std::map` |
| --- | --- | --- | --- | --- |
| insert | 460.72 ms | **3.48 ms** | 19.26 ms | 11.54 ms |
| find | **5.70 ms** | — | 7.46 ms | 9.01 ms |
| iterate | **0.09 ms** | — | 0.69 ms | 0.76 ms |
| erase | 485.02 ms | — | **6.55 ms** | 23.90 ms |
| ers+sort | **5.84 ms** | — | — | — |

> **逐个插入/删除**（`ccflatmap_insert`/`ccflatmap_erase`）：因 O(n) memmove 较慢，适合低频修改场景。
>
> **批量插入**（`push_back` + `sort`）：3.48 ms 比 ccmap 快 5.5×——追加 O(1) + 快速排序 O(n log n)。
>
> **批量删除**（`erase_unordered` + `sort`）：5.84 ms 比 ccmap 快，比逐个删除快 83×——swap-with-last O(1) + 最后一次性重排。
>
> **find**：branchless 二分搜索（cmov）5.70 ms，三家最快。**iterate**：连续内存指针递增 0.09 ms，比 ccmap 快 8×。

### 删除策略对比：`erase` vs `erase_unordered` + `sort`

| 删除数 | sorted erase | unordered+sort | 倍率 | 推荐 |
| ---: | ---: | ---: | ---: | --- |
| 1 | 0.035 ms | 0.779 ms | 22× | `erase` |
| 5 | 0.239 ms | 0.838 ms | 3.5× | `erase` |
| 10 | 0.312 ms | 0.646 ms | 2.1× | `erase` |
| 20 | 0.311 ms | 0.975 ms | 3.1× | `erase` |
| 50 | 0.924 ms | 1.192 ms | 1.3× | 交叉点 |
| 100 | 1.943 ms | 0.964 ms | 0.5× | `erase_unordered` |
| 200 | 3.391 ms | 0.930 ms | 0.3× | `erase_unordered` |
| 500 | 8.718 ms | 1.096 ms | 0.1× | `erase_unordered` |
| 1,000 | 27.91 ms | 1.080 ms | 0.04× | `erase_unordered` |
| 5,000 | 89.65 ms | 2.619 ms | 0.03× | `erase_unordered` |
| 50,000 | 420.1 ms | 5.756 ms | 0.01× | `erase_unordered` |

> **规则**：删除 ≤50 个元素用 `ccflatmap_erase`，超过 50 个用 `ccflatmap_erase_unordered` + `ccflatmap_sort`。
> `erase_unordered` 的 sort 是固定入场费（≈0.8ms），与删除数量无关。`erase` 成本随删除数 K 线性增长（O(Kn) memmove）。
> 交叉点约在 K = 2·log₂(n) ≈ 32（理论值），实测 ≈50（含常数因子）。

## cctreap vs `std::map` — 100K 顺序操作

### insert / find / erase / iterate

| 操作 | cctreap | `std::map` | 倍率 |
| --- | --- | --- | --- |
| insert | **6.06 ms** | 14.54 ms | **0.42×** |
| find | 15.13 ms | 12.09 ms | 1.25× |
| erase | **6.28 ms** | 18.61 ms | **0.34×** |
| iterate | 2.46 ms | 1.92 ms | 1.28× |

> insert 和 erase 比 STL 快 2-3×——侵入式无节点分配开销，treap 旋转比红黑树着色更简洁。

### 顺序统计（kth / rank）

| 操作 | cctreap | `std::map` | 倍率 |
| --- | --- | --- | --- |
| kth (N=100K, 随机访问) | 7.53 ms | 1.63 ms (顺序遍历) | — |
| **rank (N=5K)** | **0.40 ms** | **56.43 ms** | **0.007× (141× 更快)** |

> **kth**：STL map 不支持随机访问，只能从头迭代 O(k)。treap 利用 `size` 字段二分，每次 O(log n)。
>
> **rank**：STL `std::distance(begin, it)` 每次 O(n)，N=5000 时已慢 141×。treap 下降过程中累加左子树大小，每次 O(log n)。大数据集下差距进一步拉大。

## 构建与运行

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target bench
```
