# 性能基准

所有基准在以下环境运行：**Apple M 系列 · Apple Clang 14 · -O2 · Release**。

## ccmap vs `std::map` — 100K 顺序操作

| 操作 | ccmap | `std::map` | 倍率 |
| --- | --- | --- | --- |
| insert | 22.26 ms | 13.59 ms | 1.64× |
| find | 9.28 ms | 6.60 ms | 1.41× |
| erase | **1.63 ms** | 16.41 ms | **0.10×** |

> ccmap 擦除比 `std::map` 快 10×——侵入式设计无需释放节点内存。

## cchashmap vs `std::unordered_map` — 100K 随机操作

| 操作 | cchashmap | `std::unordered_map` | 倍率 |
| --- | --- | --- | --- |
| insert | **10.78 ms** | 14.49 ms | **0.74×** |
| find | **<0.01 ms** | 3.61 ms | **~0×** |
| erase | **2.15 ms** | 23.64 ms | **0.09×** |

> 查找几乎瞬时完成——链式哈希 + 位掩码索引的开销极小。

## cclist vs `std::list` — 200K 操作

| 操作 | cclist | `std::list` | 倍率 |
| --- | --- | --- | --- |
| push_back | **0.81 ms** | 9.92 ms | **0.08×** |
| iterate | 0.54 ms | 0.74 ms | 0.72× |
| pop_front | **0.70 ms** | 9.78 ms | **0.07×** |
| splice | <0.01 ms | <0.01 ms | — |

> push/pop 比 `std::list` 快 12× 以上。侵入式链表无单独节点分配开销。

## ccheap vs `std::priority_queue` — 200K 操作

| 操作 | ccheap | `std::priority_queue` | 倍率 |
| --- | --- | --- | --- |
| push | 3.97 ms | 2.85 ms | 1.39× |
| pop | 38.48 ms | 16.11 ms | 2.39× |

> 默认指针模式使用函数指针回调。启用 `CCHEAP_COMPARE`（宏内联）和 `CCHEAP_VALUE`（连续存储）可显著缩小差距。

## 构建与运行

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target bench
```
