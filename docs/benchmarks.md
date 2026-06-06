# 性能基准

所有基准在以下环境运行：**GCC · -O2 · Release**。
数据为单次运行取最优值。

## ccmap vs `std::map` — 100K 顺序操作

| 操作 | ccmap | `std::map` | 倍率 |
| --- | --- | --- | --- |
| insert | 20.48 ms | 13.61 ms | 1.50× |
| find | 9.12 ms | 6.55 ms | 1.39× |
| erase | **1.56 ms** | 13.24 ms | **0.12×** |

> ccmap 擦除比 `std::map` 快 8.5×——侵入式设计无需释放节点内存。

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

## 构建与运行

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target bench
```
