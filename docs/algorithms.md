# 算法原理

> 每个容器的核心算法与数据结构图解

---

## ccmap — 红黑树

红黑树是自平衡二叉搜索树，每个节点有红/黑颜色属性。ccmap 是侵入式实现——节点嵌入用户结构体。

### 红黑树五条性质

1. 节点非红即黑
2. 根节点是黑色
3. 所有叶子（NIL）是黑色
4. 红色节点的两个子节点必须是黑色（无连续红）
5. 从任意节点到其所有后代叶子的每条路径上，黑色节点数量相同（黑高一致）

### 插入流程

插入后可能违反性质 2 或 4，通过**重新着色 + 旋转**修复：

```mermaid
flowchart TD
    A["新节点 = 红色"] --> B{"父节点是黑色?"}
    B -->|是| C["✅ 完成，无需修复"]
    B -->|否| D{"叔叔节点是红色?"}
    D -->|是| E["重新着色：父+叔→黑，祖父→红"]
    E --> F["问题上移至祖父"]
    F --> B
    D -->|否| G{"当前节点是\n父的右孩子?"}
    G -->|是| H["左旋父节点"]
    H --> I["右旋祖父 + 重新着色"]
    G -->|否| I
    
    style A fill:#f96,stroke:#333
    style C fill:#6f6,stroke:#333
    style E fill:#ff9,stroke:#333
    style I fill:#9cf,stroke:#333
```

### 旋转操作

> 参数 `x` 为旋转轴心节点，`y` 为其子节点。旋转保持 BST 性质，仅改变局部指针。

```mermaid
flowchart LR
    subgraph 右旋["右旋 _rb_rot_right(m, x)"]
        direction TB
        subgraph Rbefore["Before"]
            Xr("x") --> Yr("y")
            Xr --> Gr("γ")
            Yr --> Ar("α")
            Yr --> Br("β")
        end
        subgraph Rafter["After"]
            Yr2("y") --> Ar2("α")
            Yr2 --> Xr2("x")
            Xr2 --> Br2("β")
            Xr2 --> Gr2("γ")
        end
        Rbefore --> Rafter
    end
    subgraph 左旋["左旋 _rb_rot_left(m, x)"]
        direction TB
        subgraph Lbefore["Before"]
            Xl("x") --> Al("α")
            Xl --> Yl("y")
            Yl --> Bl("β")
            Yl --> Gl("γ")
        end
        subgraph Lafter["After"]
            Yl2("y") --> Xl2("x")
            Yl2 --> Gl2("γ")
            Xl2 --> Al2("α")
            Xl2 --> Bl2("β")
        end
        Lbefore --> Lafter
    end
```

### 查找最小/最大

- 最小：沿左子树走到叶子 → O(log n)
- 最大：沿右子树走到叶子 → O(log n)
- ccmap 维护 `first` 指针，O(1) 获取最小节点

---

## cchashmap — 链式哈希表

侵入式链式哈希表。节点缓存 hash 值避免重复计算。

### 结构

```mermaid
flowchart TD
    subgraph buckets["桶数组 (cchashmap_node_t **)"]
        B0["[0]"] --> N1["node {key:42, hash:42}"]
        B1["[1]"] --> NULL
        B2["[2]"] --> N2["node {key:18, hash:18}"] --> N3["node {key:34, hash:34}"]
        B3["[3]"] --> NULL
        B4["[4]"] --> N4["node {key:20, hash:20}"]
    end
    
    style B0 fill:#e8e8e8,stroke:#333
    style B1 fill:#e8e8e8,stroke:#333
    style B2 fill:#e8e8e8,stroke:#333
    style B3 fill:#e8e8e8,stroke:#333
    style B4 fill:#e8e8e8,stroke:#333
```

### 核心操作流程

```mermaid
flowchart TD
    subgraph insert["插入 set(k, v)"]
        I1["hash = hash_fn(k, seed)"] --> I2["idx = hash & (cap - 1)"]
        I2 --> I3["遍历 buckets[idx] 链表"]
        I3 --> I4{"找到重复?"}
        I4 -->|是| I5["❌ 返回 false, *out=已存在"]
        I4 -->|否| I6["头插: node→next = buckets[idx]"]
        I6 --> I7["buckets[idx] = node, size++"]
        I7 --> I8{"size/cap ≥ MAX_LOAD?"}
        I8 -->|是| I9["resize: 2× 扩容 + rehash"]
        I8 -->|否| I10["✅ 返回 true"]
    end
    
    subgraph find["查找 get(probe)"]
        F1["hash = hash_fn(probe, seed)"] --> F2["idx = hash & (cap - 1)"]
        F2 --> F3["遍历 buckets[idx] 链表"]
        F3 --> F4{"hash 匹配 AND equal?"}
        F4 -->|是| F5["✅ 返回节点指针"]
        F4 -->|否| F6["返回 NULL"]
    end
```

### 扩容 (Rehash)

```mermaid
flowchart LR
    subgraph old["旧桶 (cap=4)"]
        O0["[0]: A→D"]
        O1["[1]: 空"]
        O2["[2]: B→C"]
        O3["[3]: 空"]
    end
    subgraph new["新桶 (cap=8)"]
        N0["[0]:"]
        N1["[1]:"]
        N2["[2]: C→D"]
        N3["[3]:"]
        N4["[4]: A"]
        N5["[5]:"]
        N6["[6]: B"]
        N7["[7]:"]
    end
    old -->|"2×扩容, 重新 hash&7"| new
```

- 容量始终为 2 的幂 → `hash & (cap - 1)` 替代取模
- 负载因子默认 1.25 → 触发 2× 扩容
- 懒分配：首次 insert 才分配桶数组

---

## ccheap — D-ary 堆

D-ary 堆是二叉堆的泛化，每个节点有 D 个子节点（ccheap 支持 2/4/8）。

### 堆结构（以二叉堆为例）

```mermaid
flowchart TD
    R["[0] priority=1 (min)"] --> L["[1] priority=5"]
    R --> RI["[2] priority=3"]
    L --> LL["[3] priority=9"]
    L --> LR["[4] priority=7"]
    RI --> RL["[5] priority=4"]
    RI --> RR["[6] priority=8"]
    
    style R fill:#6f6,stroke:#333
```

### 插入 (上滤 / Sift-up)

```mermaid
flowchart TD
    A["新元素追加到数组尾部"] --> B["i = len - 1"]
    B --> C{"i > 0 且 parent < node?"}
    C -->|是| D["swap(i, parent)"]
    D --> E["i = parent"]
    E --> C
    C -->|否| F["✅ 完成"]
```

### 弹出 (下滤 / Sift-down)

```mermaid
flowchart TD
    A["取出 data[0] (堆顶)"] --> B{"len == 1?"}
    B -->|是| C["len--，返回堆顶"]
    B -->|否| D["data[0] = data[len-1], len--"]
    D --> E["i = 0"]
    E --> F{"子节点中有更大的?"}
    F -->|是| G["swap(i, 最大子节点)"]
    G --> H["i = 最大子节点索引"]
    H --> F
    F -->|否| I["✅ 完成，返回堆顶"]
```

### D-ary 子节点

| Arity | 子节点公式 | 编译期展开 |
| --- | --- | --- |
| 2 | `parent*2+1, parent*2+2` | 2 路 if |
| 4 | `parent*4+k+1` (k=0..3) | 4 路 if |
| 8 | `parent*8+k+1` (k=0..7) | 8 路 if |

> 子节点比较通过 `#if CCHEAP_ARITY_N > N` 编译期展开，无循环开销。

---

## cclink — 侵入式单向链表

```mermaid
flowchart LR
    H["head"] --> N1["node A"] --> N2["node B"] --> N3["node C"] --> NULL
```

- 每个节点只存 `next` 指针
- 头插 O(1)，尾插 O(n)
- 无内部哨兵节点

---

## cclist — 侵入式双向链表

```mermaid
flowchart LR
    H["head sentinel\n(dummy)"] <--> N1["node A"] <--> N2["node B"] <--> T["tail sentinel\n(dummy)"]
    H -..-> N1
    T -..-> N2
```

- 使用 head/tail 哨兵节点简化边界条件
- `push_front` / `push_back` 均为 O(1)
- `insert_before` / `insert_after` 给定节点 O(1)
- `splice_back`: 将整个 src 链表移至 dst 尾部，O(1)

---

## ccvector — 动态数组

值存储的连续内存数组，自动扩容。

### 均摊扩容

```mermaid
flowchart TD
    A["push_back 调用"] --> B{"len == cap?"}
    B -->|否| C["data[len++] = elem\n✅ O(1)"]
    B -->|是| D["realloc: new_cap = cap * 2"]
    D --> E["复制旧元素到新内存"]
    E --> F["data[len++] = elem"]
    F --> G["✅ 均摊 O(1)"]
    
    style C fill:#6f6,stroke:#333
    style G fill:#ff9,stroke:#333
```

扩容策略：初始 cap=8，每次翻倍，均摊 O(1)。

---

## ccflatmap — 排序数组映射

基于排序数组的 key-value 映射，二分查找 O(log n)，插入 O(n)。

### 插入流程

```mermaid
flowchart TD
    A["二分查找定位插入点"] --> B{"key 已存在?"}
    B -->|是| C["替换 value，❌ 不增加 size"]
    B -->|否| D["插入点右侧元素整体右移"]
    D --> E["写入新元素: key + value"]
    E --> F{"扩容?"}
    F -->|是| G["2× realloc"]
    F -->|否| H["✅ 完成"]
    
    style C fill:#f96,stroke:#333
    style H fill:#6f6,stroke:#333
```

### 二分查找

```mermaid
flowchart TD
    A["lo=0, hi=len-1"] --> B{"lo <= hi?"}
    B -->|否| C["❌ 未找到，lo 即插入点"]
    B -->|是| D["mid = lo + (hi-lo)/2"]
    D --> E{"data[mid].key == key?"}
    E -->|是| F["✅ 返回索引"]
    E -->|小于| G["lo = mid + 1"]
    E -->|大于| H["hi = mid - 1"]
    G --> B
    H --> B
```

---

## 零开销回调

所有支持比较/哈希的容器均提供两种分发模式：

```mermaid
flowchart LR
    subgraph fn_ptr["函数指针模式 (默认)"]
        A["cmp_fn(a, b)"] --> B["间接调用\n(indirect call)"]
    end
    subgraph macro["宏模式 (零开销)"]
        C["CCXXX_COMPARE(a, b)"] --> D["直接内联\n(zero overhead)"]
    end
    fn_ptr -->|"#define CCXXX_COMPARE"| macro
```

- 宏模式下比较/哈希逻辑被编译器直接内联
- 无函数指针间接调用、无寄存器溢出
- 适合热路径极致性能场景
