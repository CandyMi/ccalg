# ccbi — 大数库

> **任意精度整数运算库**，提供完整的有符号整数运算，支持 2~36 进制字符串转换，
> 包含 GCD、模幂等经典算法。

`ccbi` 是 `ccalg` 项目的一部分，遵循 **header-only · C99 兼容 · BSD 协议** 的传统。

---

## 1. 设计

### 1.1 内部表示

采用 **小端序符号-绝对值**（sign-magnitude）表示法，limb 基数为 2³²。

```
ccbi_t (20 + 4·CCBI_SSO_LIMBS B)
  limbs   : uint32_t*      → 指向 internal[] 或堆
  internal: uint32_t[CCBI_SSO_LIMBS] → 内联缓冲区
  sign    : int            → -1/0/1
  used    : uint32_t       → used limb count
  cap     : uint32_t       → allocated limb capacity
```

- `limbs[0]` = 最低 32-bit，`limbs[used-1]` = 最高
- 两 limb 乘积产生 64-bit 中间结果
- **SSO (Small-String Optimization)**: 当 `used ≤ CCBI_SSO_LIMBS` 时完全零分配

**SSO 配置表**（`#define CCBI_SSO_LIMBS <N>` 覆盖默认值）：

| CCBI_SSO_LIMBS | 结构体大小 | 零分配阈值 | 备注 |
|:---:|:---:|:---:|:---|
| 2 | 28 B | ≤ 64-bit | 最少，省栈 |
| 4 | 36 B | ≤ 128-bit | |
| **8** (默认) | **52 B** | **≤ 256-bit** | |
| 16 | 84 B | ≤ 512-bit | |
| 32 | 148 B | ≤ 1024-bit | |
| 64 | 276 B | ≤ 2048-bit | |
| 128 | 532 B | ≤ 4096-bit | |

`cap` 为 `uint32_t` 无上限（最大可表示约 16TB 的大数），`CCBI_SSO_LIMBS` 有效范围主要受栈空间约束：`sizeof(ccbi_t) = 20 + 4·CCBI_SSO_LIMBS`。

### 1.2 核心算法

`n` 表示操作数的 limb 数（32-bit 为单位），`N` 表示十进制字符串长度，`e` 表示指数位长度，`k` 表示模数 limb 数。

| 运算 | 算法 | 时间复杂度 | 数值举例<br>(limb 操作数) | CPU 耗时估算<br>(x86-64 @ ~3 GHz, 单核) | 说明 |
|------|------|:---:|:---|:---|:---|
| 加法/减法 | 教科书逐字进位/借位 | **O(n)** | 512-bit: ~17 次<br>2048-bit: ~65 次 | 512-bit: &lt; 0.1 μs<br>2048-bit: &lt; 0.1 μs | 单次线性扫描 |
| 乘法 | Karatsuba + 教科书回退<br>**+ 平方自动检测** | **O(n^1.585)** | 512-bit: ~560 次¹<br>1024-bit: ~2800 次<br>2048-bit: ~14000 次 | 512-bit: ~0.8 μs<br>1024-bit: ~4 μs<br>2048-bit: ~20 μs | 普通教科书 n² 次乘；<br>平方路径 n(n+1)/2 次 (−47%)；<br>Montgomery 幂运算中 ~80% 的<br>乘法为平方，整体模幂加速 ~35% |
| 除法 | 多 limb 试商除法<br>**+ 预计算倒数** | **O(n²)** | 1024÷512-bit: ~4000 次<br>2048÷1024-bit: ~16000 次 | ~8 μs<br>~32 μs | 每轮试商用预计算倒数 (ceil(2^64/v_top))<br>代替 divq：一次预计算 divq + 每轮<br>1× mulq (3-5 cyc) vs 每轮 1× divq (25-50 cyc)；<br>单 limb 除数为 O(n) 快速路径 |
| GCD | 二进制 Stein 算法 | **O(n²)** | 1024-bit: ~15000 次<br>2048-bit: ~60000 次 | ~30 μs<br>~120 μs | 最坏 O(n) 轮移位+减法 |
| 模幂 | Montgomery CIOS + Sliding Window (w=4) | **O(log e · k²)** | 2048-bit 模, 指数 2048-bit:<br>~2048 × 8192 × 0.8<br>≈ 1340 万次 | ~20 ms¹ | CIOS 内层 ~2k² 次运算；<br>Sliding Window 降倍数约 20%；<br>受益于平方路径，模幂加快 ~35% |
| from_str (base=10) | 9 位分块转换 | **O(N²)** | 100 位: ~1000 次<br>1000 位: ~14 万次 | 100 位: &lt; 0.1 μs<br>1000 位: ~0.3 ms | N/9 轮迭代, 每轮 O(limb_cnt)<br>随数字增长线性增加 |
| to_str / to_str_buf | 逐 base 除法取余 | **O(N²)** | 100 位: ~1000 次<br>1000 位: ~14 万次 | 100 位: &lt; 0.1 μs<br>1000 位: ~0.5 ms | 每次 udiv_limb 为 O(n)；<br>n 随输出长度线性增长 |

> ¹ 平方路径和除法倒数来自运行时自动检测，无需用户干预。CPU 耗时基于 limb 操作数按平均指令吞吐换算，实际受缓存、分支预测和编译器优化影响。

### 1.3 运行时常数优化

以下优化在 `ccbi_mul` 和 `ccbi_div_rem` 内部自动触发，对外 API 完全透明：

**平方自动检测**

当 `ccbi_mul(z, a, b)` 的 `a` 与 `b` 指向同一对象（`a == b`）时，`_ccbi_mul_schoolbook` 可通过三级派发链（schoolbook → Karatsuba → Toom-3）的基例自动利用对称性，将乘法次数从 n² 降至 n(n+1)/2。

**除法预计算倒数** (`ccbi_div_rem`)

标准化后在循环外计算 `v_recip = ceil(2^64 / v_top_limb)`（一次 64/32 除法），每轮试商用：
```c
qd = (uint32_t)((u128)u_top * v_recip >> 64);   // 1× mulq (3-5 cyc)
```
代替：
```c
qd = (uint32_t)(u_top / v_top_limb);             // 1× divq (25-50 cyc)
```
不支持 `__uint128_t` 的编译器自动回退原始 `divq` 路径。

### 1.4 分配器钩子

```c
#define CCBI_MALLOC(sz)   CCBI_REALLOC(NULL, sz)
#define CCBI_REALLOC(p,sz) realloc
#define CCBI_FREE(p)      free(p)
```

预定义后 `#include "ccbi.h"` 即可替换。SSO 小值不走分配器。

---

## 2. API 参考

### 类型

```c
#define CCBI_SSO_LIMBS  8   /* 可用户重定义 */

typedef struct ccbi {
    uint32_t  *limbs;
    uint32_t   internal[CCBI_SSO_LIMBS];
    uint32_t   meta;  /* sign(2) | used(15) | cap(15) */
} ccbi_t;
```

元数据通过宏访问：`CCBI_SIGN(z)`, `CCBI_USED(z)`, `CCBI_CAP(z)`, `CCBI_SET_SIGN(z,v)`, 等。

### 生命周期

```c
void   ccbi_init(ccbi_t *z);          // 初始化为零（指向 SSO 缓冲）
void   ccbi_destroy(ccbi_t *z);       // 仅在堆上时 free
int    ccbi_copy(ccbi_t *z, const ccbi_t *src); // 深拷贝
void   ccbi_swap(ccbi_t *a, ccbi_t *b); // O(1) 指针/SSO 全交换
void   ccbi_zero(ccbi_t *z);           // 清零（归还堆到 SSO）
void   ccbi_one(ccbi_t *z);            // 设为 1（SSO，零分配）
```

### 赋值

```c
int  ccbi_from_int(ccbi_t *z, int64_t v);
int  ccbi_from_uint(ccbi_t *z, uint64_t v);
int  ccbi_from_str(ccbi_t *z, const char *str, int base);
// base 范围 2~36。十进制有 9 位分块快速路径。支持 + / - 前缀。
```

### 比较

```c
int    ccbi_cmp(const ccbi_t *a, const ccbi_t *b);    // <0 / 0 / >0
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
int  ccbi_mul(ccbi_t *z, const ccbi_t *a, const ccbi_t *b);
int  ccbi_div_rem(ccbi_t *q, ccbi_t *r, const ccbi_t *a, const ccbi_t *b);
int  ccbi_mod(ccbi_t *r, const ccbi_t *a, const ccbi_t *m);

void ccbi_neg(ccbi_t *z, const ccbi_t *a);
void ccbi_abs(ccbi_t *z, const ccbi_t *a);
int  ccbi_inc(ccbi_t *z);
int  ccbi_dec(ccbi_t *z);
int  ccbi_add_uint(ccbi_t *z, uint32_t v);
int  ccbi_mul_uint(ccbi_t *z, uint32_t v);
int  ccbi_muladd(ccbi_t *z, const ccbi_t *x, uint32_t y);  // z += x·y
```

### 移位

```c
int    ccbi_shl(ccbi_t *z, const ccbi_t *a, size_t n);
int    ccbi_shr(ccbi_t *z, const ccbi_t *a, size_t n);
size_t ccbi_bit_length(const ccbi_t *z);
```

### 数论

```c
int ccbi_gcd(ccbi_t *z, const ccbi_t *a, const ccbi_t *b);
int ccbi_pow_mod(ccbi_t *z, const ccbi_t *base,
                 const ccbi_t *exp, const ccbi_t *mod);
```

### 转换

```c
int64_t   ccbi_to_int(const ccbi_t *z);

// 用户提供缓冲区版本（推荐）：
int       ccbi_to_str_len(const ccbi_t *z, int base);
int       ccbi_to_str_buf(const ccbi_t *z, char *buf, size_t len, int base);

// 自动分配版本（向后兼容）：
char     *ccbi_to_str(const ccbi_t *z, int base);  // caller 负责 free
void      ccbi_free_str(char *s);

int       ccbi_print(const ccbi_t *z, int base, int newline);
```

用法示例：

```c
// 缓冲版（零分配）：
char buf[64];
int n = ccbi_to_str_buf(&a, buf, sizeof(buf), 10);
printf("result = %.*s\n", n, buf);

// 自动分配版：
char *s = ccbi_to_str(&a, 10);
printf("result = %s\n", s);
ccbi_free_str(s);
```

---

## 3. 快速上手

```c
#include "ccbi.h"
#include <stdio.h>

int main() {
    ccbi_t a, b, c;
    ccbi_init(&a); ccbi_init(&b); ccbi_init(&c);

    ccbi_from_int(&a, 12345);
    ccbi_from_str(&b, "987654321098765432109876543210", 10);
    ccbi_mul(&c, &a, &b);
    char *s = ccbi_to_str(&c, 10);
    printf("result = %s\n", s);
    ccbi_free_str(s);

    ccbi_destroy(&a); ccbi_destroy(&b); ccbi_destroy(&c);
    return 0;
}
```

编译：

```bash
cc -I include -o example example.c && ./example
```

SSO 大小可调：

```bash
cc -DCCBI_SSO_LIMBS=16 -I include -o example example.c && ./example
```
