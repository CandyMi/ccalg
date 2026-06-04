# 性能基准

所有基准在以下环境运行：**Apple M 系列 · Apple Clang 14 · -O2 · Release**。
数据为 20 次运行取最优值。

## ccmap vs `std::map` — 100K 顺序操作

| 操作 | ccmap | `std::map` | 倍率 |
| --- | --- | --- | --- |
| insert | 20.48 ms | 13.61 ms | 1.50× |
| find | 9.12 ms | 6.55 ms | 1.39× |
| erase | **1.56 ms** | 13.24 ms | **0.12×** |

> ccmap 擦除比 `std::map` 快 8.5×——侵入式设计无需释放节点内存。

## cchashmap vs `std::unordered_map` — 100K 随机操作

| 操作 | cchashmap | `std::unordered_map` | 倍率 |
| --- | --- | --- | --- |
| insert | **3.01 ms** | 9.36 ms | **0.32×** |
| find | **1.40 ms** | 2.17 ms | **0.65×** |
| erase | **1.44 ms** | 12.24 ms | **0.12×** |

> 全链路优于 STL——链式哈希 + 位掩码索引 + hash 预检查三重优化生效。

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
| push | **0.20 ms** | 8.10 ms | **0.02×** |
| iterate | 0.43 ms | 0.49 ms | 0.88× |
| pop | **0.37 ms** | 7.55 ms | **0.05×** |

> push/pop 比 `std::forward_list` 快 20-40×。侵入式单链表无节点分配。

## ccheap vs `std::priority_queue` — 200K 操作

| 操作 | ccheap | `std::priority_queue` | 倍率 |
| --- | --- | --- | --- |
| push | 4.04 ms | 2.97 ms | 1.36× |
| pop | 39.86 ms | 16.91 ms | 2.36× |

> 默认指针模式使用函数指针回调。启用 `CCHEAP_COMPARE`（宏内联）和 `CCHEAP_VALUE`（连续存储）可显著缩小差距。

## 构建与运行

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target bench
```
