# 构建指南

ccalg 是 header-only 库，核心代码无需构建。构建系统用于**单元测试**和**基准测试**。

## CMake（推荐）

### 配置

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

所有产物隔离在 `build/` 目录（已在 `.gitignore` 中）。

### 目标一览

| 目标 | 命令 | 说明 |
| --- | --- | --- |
| 全量构建 | `cmake --build build` | 16 个测试 + 10 个 benchmark |
| 测试 | `cmake --build build --target check` | 构建 + CTest 运行全部单元测试 |
| 基准 | `cmake --build build --target bench` | 构建 + 依次运行全部 benchmark |
| 文档 | `cmake --build build --target docs-html` | docs/*.md → HTML（语法高亮） |
| 单项构建 | `cmake --build build --target test_ccmap` | 只构建指定目标 |
| CTest 过滤 | `ctest --test-dir build -L unit` | 按标签过滤测试 |

### CI 构建矩阵

GitHub Actions 在每次推送和 PR 时运行 **14 个构建任务**，覆盖：

| 架构 | 编译器 | 特殊标志 |
|------|--------|----------|
| Linux x86_64 | GCC, Clang | — |
| Linux x86 | GCC (multilib), Clang (multilib) | `-m32` |
| Linux x86_64 | GCC (ASan), Clang (ASan) | `-fsanitize=address` |
| macOS arm64 | Apple Clang | — |
| Windows x64 | MSVC, ClangCL, MinGW GCC | — |
| Windows x86 | MSVC, ClangCL, MinGW GCC | `-A Win32` 或 `-m32` |

ASan（Address Sanitizer）构建在每次提交时自动发现内存错误。

### CTest 集成

测试用例已注册到 CTest：

- **标签**: `unit` — 可用 `-L unit` 过滤
- **通过判定**: 退出码 0 + 输出匹配 `0 failed`
- **超时**: 30 秒
- **失败输出**: `--output-on-failure` 打印失败详情

```bash
# 只跑测试
ctest --test-dir build -L unit --output-on-failure

# 跑全部 (含 benchmark)
ctest --test-dir build --output-on-failure
```

### 安装

```bash
cmake --install build --prefix /usr/local
```

安装后：

```
/usr/local/
├── include/ccalg/         # 所有 .h 头文件
└── share/doc/ccalg/       # LICENSE, README.md
```

自定义前缀：`cmake --install build --prefix ~/.local`

安装后在代码中：

```c
#include "ccalg/ccmap.h"       // 而非 "ccmap.h"
#include "ccalg/cchashmap.h"
#include "ccalg/cclink.h"
#include "ccalg/cclist.h"
#include "ccalg/ccheap.h"
```

## 手动编译

无需构建系统时可直接编译单个文件：

```bash
# C 测试
gcc -std=c99 -Wall -Wextra -Wno-missing-field-initializers \
    -I include -o test_ccmap tests/test_ccmap.c

# C++ 基准
g++ -std=c++11 -O2 -Wall \
    -I include -o bench_ccmap bench/bench_ccmap.cpp
```

## 清理

```bash
rm -rf build/          # 删除所有构建产物
```


