# 性能基准

所有基准在以下环境运行：**Apple M 系列 · Apple Clang 14 · -O2 · Release**。

## ccmap vs `std::map` — 100K 顺序操作

| 操作 | ccmap | `std::map` | 倍率 |
| --- | --- | --- | --- |
| insert | 21.75 ms | 14.89 ms | 1.46× |
| find | 9.94 ms | 7.24 ms | 1.37× |
| erase | **1.58 ms** | 15.70 ms | **0.10×** |

> ccmap 擦除比 `std::map` 快 10×——侵入式设计无需释放节点内存。

## cchashmap vs `std::unordered_map` — 100K 随机操作

| 操作 | cchashmap | `std::unordered_map` | 倍率 |
| --- | --- | --- | --- |
| insert | **5.17 ms** | 9.25 ms | **0.56×** |
| find | **2.69 ms** | 6.63 ms | **0.41×** |
| erase | **3.29 ms** | 24.26 ms | **0.14×** |

> 全链路优于 STL——链式哈希 + 位掩码索引 + hash 预检查三重优化生效。

## cclist vs `std::list` — 200K 操作

| 操作 | cclist | `std::list` | 倍率 |
| --- | --- | --- | --- |
| push_back | **0.83 ms** | 10.58 ms | **0.08×** |
| iterate | 0.58 ms | 0.90 ms | 0.64× |
| pop_front | **0.83 ms** | 8.22 ms | **0.10×** |
| splice | <0.01 ms | <0.01 ms | — |

> push/pop 比 `std::list` 快 10-12×。侵入式链表无单独节点分配开销。

## ccheap vs `std::priority_queue` — 200K 操作

| 操作 | ccheap | `std::priority_queue` | 倍率 |
| --- | --- | --- | --- |
| push | 3.79 ms | 2.81 ms | 1.35× |
| pop | 45.31 ms | 16.50 ms | 2.75× |

> 默认指针模式使用函数指针回调，pop 落后于 STL。启用 `CCHEAP_COMPARE`（宏内联）和 `CCHEAP_VALUE`（连续存储）可显著缩小差距。

## 构建与运行

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target bench
```
