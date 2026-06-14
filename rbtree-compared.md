# ccmap vs Linux rbtree vs FreeBSD rbtree 对比分析

> 三种 C 语言红黑树（及类红黑树）实现的全面对比。
>
> - **ccmap** — `include/ccmap.h`，ccalg 项目的一部分
> - **Linux rbtree** — `include/linux/rbtree.h` + `lib/rbtree.c`
> - **FreeBSD rbtree** — `sys/sys/tree.h`（`RB_*` 宏族）

---

## 1. 设计哲学与 API 风格

| 维度 | ccmap | Linux rbtree | FreeBSD rbtree |
|------|-------|-------------|----------------|
| **风格** | 完整封装式 — 一个 `ccmap_t` 结构体管理全部状态，用户调用 `ccmap_insert/find/erase` | 半封装式 — 核心操作是外部函数（`rb_insert_color`, `rb_erase`），用户自己负责 BST 下降和 `rb_link_node` | 基于宏的代码生成式 — `RB_GENERATE(name, ...)` 为用户自定义类型生成所有函数 |
| **使用者负担** | 极低 — 只需要 embed 节点，传 cmp 函数或宏 | 中等 — 插入需手动 BST 下降 + `rb_link_node` + `rb_insert_color` | 低 — 只需 `RB_GENERATE()` 声明，编译器为你生成完整的类型安全 API |
| **数据结构的泛型方式** | `void*` + 侵入式节点 + `CCMAP_CONTAINER` 宏 | `void*` + 侵入式节点 + `rb_entry`（container_of） | C 宏模板 — `RB_GENERATE(name, type, field, cmp)` 展开为命名空间化的函数族 |
| **核心文件** | 单头文件 `ccmap.h`（~300 行） | `rbtree.h` + `rbtree_augmented.h` + `rbtree.c`（~700 行 C） | 单头文件 `sys/tree.h`（~1400 行宏） |

**ccmap** 的设计是最贴近用户的：容器结构封装了所有状态（root / first / last / size），用户只管调用完成体接口。**Linux** 选择最低层级的暴露 — 将一个插入分拆为"BST 下降 → 链接 → 着色修复"三个阶段，给调用者最大的控制权（比如在链接前做 RCU 准备）。**FreeBSD** 采用宏代码生成，每个实例获得一组独立命名的类型安全函数，连比较函数都是编译期内联的。

---

## 2. 节点结构 & 内存布局

### 结构体定义

**ccmap:**
```c
typedef struct ccmap_node {
    struct ccmap_node *child[2];  // left=0, right=1
    uintptr_t               pc;   // parent | color (low bit)
} ccmap_node_t;
```
→ **24 字节**（64-bit 下 2×8 + 8）

**Linux:**
```c
struct rb_node {
    unsigned long  __rb_parent_color;  // parent | color (low 2 bits)
    struct rb_node *rb_right;
    struct rb_node *rb_left;
};
```
→ **24 字节** — 但 `__rb_parent_color` 占用低 **2 位**（不是 1 位）

**FreeBSD:**
```c
// 无固定 struct — 用户通过 RB_ENTRY(type) 在自己的结构体里声明节点
#define RB_ENTRY(type) struct { struct type *rbe_link[3]; }
```
→ **24 字节** — `rbe_link[0]` = parent, `rbe_link[1]` = left, `rbe_link[2]` = right

三个实现节点大小相等（24 字节），但编码方式截然不同。

---

## 3. 颜色编码 — 核心差异

| | ccmap | Linux | FreeBSD |
|---|-------|-------|---------|
| **占用位** | `pc` 低 **1 位**（bit 0） | `__rb_parent_color` 低 **1 位**（bit 0） | 借用 parent 指针的低 **2 位**（L-bit + R-bit） |
| **颜色含义** | 0=RED, 1=BLACK | 0=RED, 1=BLACK | 不直接使用"红/黑" — 改用 **rank-difference** |
| **掩码** | `~(uintptr_t)1` | `~3`（保留 2 位） | `~3`（保留 2 位） |
| **位利用率** | 只用 1 位 | 保留 2 位但只用 1 位 | **用满 2 位** — 分别编码"左子树 rank 差 = 2"和"右子树 rank 差 = 2" |

这是最深刻的架构分水岭：**FreeBSD 的 RB 实现不再是经典红黑树，而是弱 AVL 树（weak AVL / rank-balanced tree）**，基于 Haeupler-Sen-Tarjan 2015 论文。

**经典红黑树**（ccmap / Linux）的性质：
- 5 条标准性质，颜色是节点的二元属性
- 黑色高度在所有路径上相等
- 最长路径 ≤ 2 × 最短路径

**Rank-balanced tree / weak AVL**（FreeBSD）的性质：
- 每个节点的"秩"定义为到 null 的高度
- 子链接的 **rank-difference** 只能是 1 或 2（不是 1 或 0）
- 叶节点的秩总是 1
- 最大高度 ≤ 2 log₂(n+1) — 和红黑树相同
- **插入/删除修复路径更短**（实验表明平均少 30% 的旋转）

FreeBSD 的 `_RB_L`（1）和 `_RB_R`（2）不是"红/黑"，而是"该方向 rank-difference = 2"的标记。

---

## 4. Parent 指针访问

**ccmap** — 宏展开方式：
```c
#define _rb_p(n)  ((ccmap_node_t *)((n)->pc & ~(uintptr_t)1))
#define _rb_c(n)  ((int)((n)->pc & 1))
#define _rb_sp(n, p)   ((n)->pc = ((n)->pc & 1) | (uintptr_t)(p))
#define _rb_sc(n, c)   ((n)->pc = ((n)->pc & ~(uintptr_t)1) | (uintptr_t)(c))
#define _rb_spc(n,p,c) ((n)->pc = (uintptr_t)(p) | (uintptr_t)(c))
```
→ 每个访问都是宏 → 编译器完全内联，**零开销**。`_rb_spc` 可在一条指令中同时设置 parent 和 color。

**Linux** — 更接近函数风格：
```c
#define rb_parent(r)  ((struct rb_node *)((r)->__rb_parent_color & ~3))
static inline void rb_set_parent_color(struct rb_node *rb,
                                       struct rb_node *p, int color) {
    rb->__rb_parent_color = (unsigned long)p + color;
}
```
→ `__rb_parent_color` 按 **3** 对齐（`& ~3`），而非按 1。这是正确的，因为低 2 位都被设为可用 — 尽管目前只用 1 位。

**FreeBSD** — 最复杂，因为用 2 位编码 rank-difference：
```c
#define _RB_PTR(elm)          _RB_PTR_OP((elm), &, ~_RB_LR)  // 清除 2 位
#define _RB_BITS(elm)         ((__uintptr_t)elm)
#define _RB_BITSUP(elm, field) _RB_BITS(_RB_UP(elm, field))

#define RB_SET_PARENT(dst, src, field) do { \
    _RB_UP(dst, field) = (__typeof(src))((__uintptr_t)src | \
        (_RB_BITSUP(dst, field) & _RB_LR)); \
} while(0)
```
→ 读取 parent 时屏蔽 2 位，但设置 parent 时必须 **保留已有的 rank-difference 位**，使用逐位 XOR/OR 操作。

---

## 5. 比较机制

| | ccmap | Linux | FreeBSD |
|---|-------|-------|---------|
| **默认** | 函数指针 `ccmap_cmp_t` 存在 `ccmap_t.cmp` 中 | **无内置比较** — 由调用者在 BST 下降阶段自己写 3 路分支 | 宏参数 `cmp` — 在插入/查找时作为表达式展开 |
| **零开销覆盖** | `#define CCMAP_COMPARE(a,b)` 抑制 `init()` 的 cmp 参数，全内联 | 辅助模板 `rb_add_cached(node, tree, less)` 接受函数指针或内联函数 | 宏比较总是内联 — 但调用者必须提供 `cmp` 表达式模板 |
| **比较签名** | `int64_t (*)(const node*, const node*)` — 完整 3 路比较 | CMP / less — `bool (*less)(node*, const node*)` 或 `int (*cmp)(key, node*)` | `__typeof(cmp(NULL, NULL))` — `cmp` 作为表达式直接展开 |

ccmap 的双模式（fn ptr + macro）设计在 `#define CCMAP_COMPARE` 时也自动抑制了 `ccmap_t` 结构体中的 `cmp` 字段，**不浪费内存**。

Linux 的 find 接口支持 **外部键查找**（不用构造完整节点）：
```c
rb_find(key, tree, cmp);  // cmp 签名：int (*)(const void *key, const struct rb_node *)
```
ccmap 和 FreeBSD 不支持这个 — 必须传与节点同类型的 probe。

---

## 6. 迭代 — first/last 缓存

| | ccmap | Linux | FreeBSD |
|---|-------|-------|---------|
| **O(1) begin** | ✅ — `ccmap_t` 有 `first`/`last` 缓存 | 基础树 ❌（需要 `rb_first` O(log n) 从 root 左倾）；`rb_root_cached` ✅ 有 `rb_leftmost` | ❌ — `RB_MIN`/`RB_MAX` 通过遍历实现 |
| **后继/前驱** | `ccmap_next` / `ccmap_prev` — 标准中序遍历 | `rb_next` / `rb_prev` — 标准中序遍历（在 `lib/rbtree.c`） | `name##_RB_NEXT` / `name##_RB_PREV` |
| **end 哨兵** | 始终返回 `NULL` | 始终返回 `NULL` | 始终返回 `NULL` |
| **postorder** | ❌ | ✅ `rb_first_postorder` / `rb_next_postorder` | ❌ |
| **fast-path** | 插入时如果 key < first → 直接挂左；如果 key > last → 直接挂右 | ❌ | ❌ |

ccmap 的 `first`/`last` 缓存是其性能亮点 — **插入 fast-path 跳过整个 BST 下降**（减到 O(1) 而不是 O(log n)），并且在 erase 后从子节点位置重算指针以维持缓存。

Linux 的 `rb_root_cached` 提供类似功能（只缓存 `leftmost`），但不缓存 `last`，也没有 erase 后的缓存重算逻辑。

---

## 7. 插入算法

### 流程

| 阶段 | ccmap | Linux | FreeBSD |
|------|-------|-------|---------|
| **BST 下降** | 内置在 `ccmap_insert` 中 | 调用者负责（`rb_add` / `rb_find_add` 辅助函数） | 内置在 `name##_RB_INSERT` 中 |
| **节点初始化** | 清空 child，设 `pc = (NULL \| RED)` | `rb_link_node(node, parent, rb_link)` — 只设 parent+color+child | `RB_SET(elm, parent, field)` |
| **插入后修复** | `_rb_ins_fix`（循环 + 旋转） | `rb_insert_color(node, root)` → `__rb_insert(node, root, dummy_rotate)` | `name##_RB_INSERT_COLOR`（rank-based 修复） |
| **修复逻辑** | 标准红黑树 Case 1-3（叔红/叔黑左旋/右旋） | 标准红黑树 Case 1-3（和 ccmap 相同） | **rank-balanced** 修复：用 `_RB_MOD_XOR` 翻转 rank-difference 位 |
| **duplicate 处理** | 比较 == 0 → 设 `*out` 返回 -1 | 调用者在 `rb_find_add` 中检测；`rb_add` 忽略 | 比较 == 0 → 返回旧节点 |

**关键差异**：ccmap 和 Linux 的插入修复算法在**拓扑上等价**（都是标准 CLRS 红黑树），但实现方式不同：

- ccmap 使用 `while (_rb_red(p))` + 显式叔节点检查，修复在 `_rb_ins_fix` 中
- Linux 的 `__rb_insert` 使用同一个 `while (true)` 循环 + `continue`，代码更紧凑

FreeBSD 的插入修复完全不同 — 它不是在着色/旋转，而是 **翻转 rank 位**（`_RB_MOD_XOR`），通过调整 link 的 rank-difference 来恢复 weak AVL 性质。

---

## 8. 删除算法

| | ccmap | Linux | FreeBSD |
|---|-------|-------|---------|
| **后继选择** | 右子树 non-NULL 时选 `_rb_min(right)` | 同左 | 同左（`while (RB_LEFT(in)) in = RB_LEFT(in)`） |
| **transplant** | `_rb_transplant` — setne 消除分支 | `__rb_change_child` — 直接 if/else | `RB_SWAP_CHILD` — if/else 写 |
| **删除修复** | `_rb_del_fix` — 从 x/xp 开始沿树上升 | `____rb_erase_color` — 循环 4 个 case | `name##_RB_REMOVE_COLOR` — rank-based 修复 |
| **fix 启动条件** | 如果 `yc == BLACK` 才调 fix | 同 — `rebalance` 在成功替换后决定 | 总是调用 `REMOVE_COLOR`，内部判断 |
| **first/last 重算** | ✅ 从子节点或 parent 重算 | ❌ 调用者自行处理（`rb_erase_cached` 返回旧的 leftmost） | ❌ 无缓存 |
| **size** | ✅ `m->size--` | ❌ | ❌ |

ccmap 在 `ccmap_erase` 中有独特的 first/last 重算策略：从 `z` 的子节点或 parent **向上/向子树**重算，利用了 erase 前缓存的 `is_first`/`is_last` 标记。

Linux 的 `__rb_erase_augmented` 是所有删除操作的统一入口 — 非增强版本只是把 dummy callbacks 传进去让编译器优化掉。

FreeBSD 的删除是三者中最复杂的（约 100 行宏展开），因为它需要在该修复 weak-AVL rank 的同时处理更精细的 rank-difference 状态机。

---

## 9. 增强树 / Augmented 支持

| | ccmap | Linux | FreeBSD |
|---|-------|-------|---------|
| **机制** | ❌ 原生不支持 | ✅ `rb_augment_callbacks` — propagate/copy/rotate 三个回调 | ✅ `RB_AUGMENT` 和 `RB_AUGMENT_CHECK` 宏 |
| **声明方式** | — | `RB_DECLARE_CALLBACKS` / `RB_DECLARE_CALLBACKS_MAX` 模板 | 用户定义 `RB_AUGMENT_CHECK(x)` 宏 |
| **传播路径** | — | 在插入/删除路径上显式调 `augment->propagate` | 在 `_RB_AUGMENT_WALK` 中从节点向上循环 |
| **常见用例** | — | interval tree（红黑树 + 区间 max） | 特定内核用途 |
| **复杂度** | — | **高** — 需要理解 3 个回调的生命周期 | **中等** — 只需 `RB_AUGMENT_CHECK` 返回 true 表示需要继续传播 |
| **rotate 回调** | — | 旋转时必须调用 `augment_rotate(old, new)` 保持子树数据 | 旋转由 `RB_ROTATE` 宏本身完成，不额外回调 |

**ccmap 不支持增强**。Linux 的增强机制最强大但也最复杂，FreeBSD 的 `RB_AUGMENT` 接口更简单（只需要 propagate 一个布尔返回）。

---

## 10. 并发安全 & RCU

| | ccmap | Linux | FreeBSD |
|---|-------|-------|---------|
| **RCU 查找** | ❌ | ✅ `rb_find_rcu` / `rb_find_add_rcu` — 用 `rcu_dereference_raw` | ❌ |
| **RCU 写** | ❌ | ✅ `rb_link_node_rcu` — `rcu_assign_pointer` | ❌ |
| **WRITE_ONCE** | ❌ | ✅ 所有 child 指针写入使用 `WRITE_ONCE` 防止编译器撕裂 | ❌ |
| **lockless 设计** | ❌ — 假设单线程或外部锁 | ✅ 专门为 lockless lookup 设计 — 通过 `WRITE_ONCE` 确保不会出现临时循环 | ❌ |

Linux 在这方面的差异是压倒性的 — **它专门设计为允许无锁并发读取**：所有对 `rb_left`/`rb_right` 的写入都通过 `WRITE_ONCE`，确保观察者不会看到循环或无效指针。

---

## 11. 性能优化对比

| 优化 | ccmap | Linux | FreeBSD |
|------|-------|-------|---------|
| **parent/color 同字段** | ✅ 1 位 | ✅ 1 位（保留 2 位） | ✅ 2 位 |
| **prefetch** | ✅ `CCMAP_PREFETCH` — 支持 GCC/Clang/MSVC/ARM/x86 全部平台 | ❌ | ❌ |
| **分支消除** | ✅ `setne` 索引取代 if 分支（`zp->child[zp->child[RIGHT]==z]`） | ❌ `__rb_change_child` 用 if/else | ❌ `RB_SWAP_CHILD` 用 if/else |
| **first/last 缓存** | ✅ 双向缓存 | ✅ `rb_root_cached` 只有 `leftmost` | ❌ |
| **insert fast-path** | ✅ key < first → 直接左挂；key > last → 直接右挂 | ❌ | ❌ |
| **`unlikely` 标注** | ✅ `ccmap_unlikely` | ✅ `unlikely()` | ❌ |
| **`__always_inline`** | `CCMAP_INLINE`（static inline） | ✅ 关键函数加 `__always_inline` | 宏展开天然内联 |
| **type safety** | ❌ 泛型 `void*` | ❌ 泛型 `void*` | ✅ 宏生成类型安全函数 |
| **`restrict` / const** | ✅ `const ccmap_node_t *probe` — find 用 const | ✅ `const struct rb_node *` | 有限 |
| **height O(1)** | ✅ `ccmap_height` — 从 size 计算上限 2·⌊log₂(n+1)⌋ | ❌ | ❌ |

**ccmap 在微观优化上的密度明显更高** — 很多来自竞赛编程风格的技术：setne 索引、prefetch 跨平台封装、fast-path 缓存、`unlikely` 标注。

---

## 12. 代码结构 & 维护维度

| 维度 | ccmap | Linux | FreeBSD |
|------|-------|-------|---------|
| **发布形式** | 单头文件 ~300 行，`#include` 即用 | 3 文件分离（.h + .c + augmented.h） | 单头文件 ~1400 行巨型宏集合 |
| **语言标准** | C89 / C99 / C++ 兼容 | C11+（依赖 `stdbool.h`、`WRITE_ONCE` 等） | C99+（用了 `__typeof` 等 GNU C 扩展） |
| **依赖** | 仅 `<stdint.h>` / `<stddef.h>` | Linux 内核基础设施（RCU、container_of、export） | `<sys/cdefs.h>` |
| **错误处理** | ✅ 所有公共函数 NULL 安全，无 assert / abort | ❌ NULL 检查由调用者负责 | ❌ 无 NULL 检查 |
| **诊断** | `ccmap_height` 快速估算 | ❌ | `RB_RANK`（`_RB_DIAGNOSTIC` 下）校验树结构 |
| **授权** | BSD 3-Clause | GPL 2.0+ | BSD 2-Clause |

---

## 13. 汇总对比表

| 维度 | ccmap | Linux rbtree | FreeBSD rbtree |
|------|-------|-------------|----------------|
| **实质数据结构** | 经典红黑树 | 经典红黑树 | **weak AVL（Rank-Balanced Tree）** |
| **节点大小** | 24 字节 | 24 字节 | 24 字节 |
| **颜色存储** | parent 低 1 位 | parent 低 2 位（只用 1） | parent 低 2 位（用满，存 rank-diff） |
| **API 风格** | 封装式容器 | 半封装式（调用者控制 BST） | 宏代码生成（类型安全） |
| **O(1) min/max** | ✅ first + last 缓存 | ⚠️ only leftmost（cached） | ❌ 需要遍历 |
| **插入 fast-path** | ✅ 边界 key 跳过 BST | ❌ | ❌ |
| **Prefetch** | ✅ 跨平台全覆盖 | ❌ | ❌ |
| **setne 分支消除** | ✅ | ❌ | ❌ |
| **Augmented 树** | ❌ | ✅ 复杂但完备 | ✅ 简单接口 |
| **RCU / lockless** | ❌ | ✅ 原生支持 | ❌ |
| **外部键查找** | ❌ | ✅ `rb_find(key, tree, cmp)` | ❌ |
| **NULL 安全** | ✅ 全部公共函数 | ❌ | ❌ |
| **单头文件** | ✅ | ❌（3 文件） | ✅ |
| **C89 兼容** | ✅ | ❌ | ❌ |
| **GPL 兼容** | ✅ BSD | ❌ GPL | ✅ BSD |
| **代码行数** | ~300 | ~700（C） | ~1400（宏） |
| **调试辅助** | `ccmap_height` 快速估算 | ❌ | `RB_RANK` 校验树平衡 |

---

## 14. 小结

这三个实现代表了 C 语言中红黑树（及类红黑树）数据结构的**三种不同设计哲学**：

- **ccmap** — 用户友好的单体实现：封装 + 极致微观优化 + 零外部依赖 + NULL 安全。适合独立项目、库发布、竞赛编程。fast-path 缓存和 prefetch 在此类场景中最有价值。

- **Linux rbtree** — 内核级基础设施：为**无锁并发读取**设计，通过增强回调支持 interval tree 等高级用例。API 故意低层级，让内核子系统的调用者拥有完全控制权。与 RCU、`WRITE_ONCE` 等内核基础设施深度绑定。

- **FreeBSD rbtree** — 基于宏生成 + **weak AVL 算法**的实现。核心差异不是接口风格而是 **数据结构本身** — weak AVL 在旋转次数上比经典红黑树更少，但在父指针的位操作上更复杂。`RB_GENERATE` 的宏模板为每个实例生成类型安全的函数族，这是纯 C 中模拟 C++ 模板的经典手法。

---

### 脚注：FreeBSD 的"红黑树"到底是什么？

FreeBSD 的 RB 实现**不是经典红黑树**。其注释明确说：

> "rank-difference 只能是 1 或 2"

这是 **weak AVL 树（rank-balanced tree）** 的性质，由 Haeupler、Sen 和 Tarjan 在 2015 年提出。与经典红黑树"节点要么红要么黑"的二元属性完全不同，weak AVL 使用 rank-difference 来度量平衡。

从 NetBSD/OpenBSD 继承来的 `sys/tree.h` 在 2015 年后采用了 HST 论文的算法，但为保持向后兼容保留了 `RB_` 前缀和"红色/黑色"的命名 — 注释中解释：偶数的 rank-difference 被称作 "red edge"，子节点被称作 "red child"。

---

*本文基于以下源码版本分析：*
- ccmap — `include/ccmap.h`（ccalg 项目，commit 时最新）
- Linux — `include/linux/rbtree.h` + `lib/rbtree.c`（torvalds/linux master，2025）
- FreeBSD — `sys/sys/tree.h`（freebsd-src main，2025）
