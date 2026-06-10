# 构建指南

cclag 是 header-only 库，核心代码无需构建。构建系统用于**单元测试**和**基准测试**。

## CMake（推荐）

### 配置

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

所有产物隔离在 `build/` 目录（已在 `.gitignore` 中）。

### 目标一览

| 目标 | 命令 | 说明 |
| --- | --- | --- |
| 全量构建 | `cmake --build build` | 8 个测试 + 10 个 benchmark |
| 测试 | `cmake --build build --target check` | 构建 + CTest 运行全部单元测试 |
| 基准 | `cmake --build build --target bench` | 构建 + 依次运行全部 benchmark |
| 文档 | `cmake --build build --target docs-html` | docs/*.md → HTML（语法高亮） |
| 单项构建 | `cmake --build build --target test_ccmap` | 只构建指定目标 |
| CTest 过滤 | `ctest --test-dir build -L unit` | 按标签过滤测试 |

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
├── include/cclag/         # 所有 .h 头文件
└── share/doc/cclag/       # LICENSE, README.md
```

自定义前缀：`cmake --install build --prefix ~/.local`

安装后在代码中：

```c
#include "cclag/ccmap.h"       // 而非 "ccmap.h"
#include "cclag/cchashmap.h"
#include "cclag/cclink.h"
#include "cclag/cclist.h"
#include "cclag/ccheap.h"
```

## Premake5（备选）

### 生成

```bash
premake5 gmake                      # 生成 Makefile（旧版本用 gmake2）
make -C build                       # 默认 debug 构建
make -C build config=release -j4    # release 构建（8 测试 + 10 基准）
```

产物位置：

| 类型 | 路径 |
| --- | --- |
| 测试 | `build/bin/Release/test_*` |
| 基准 | `build/bin/Release/bench_*` |

### 目标一览

| 目标 | 命令 | 说明 |
| --- | --- | --- |
| 全量构建 | `make -C build config=release -j4` | 8 个测试 + 10 个 benchmark |
| 测试 | `make -C build check` | 构建 + 依次运行全部测试 |
| 基准 | `make -C build bench` | 构建 + 依次运行全部 benchmark（带容器名分隔） |
| 文档 | `make -C build docs-html` | docs/*.md → HTML（需 python3） |
| 单项构建 | `make -C build config=release test_ccmap` | 只构建指定目标 |

### 构建单个目标

```bash
make -C build config=release test_ccmap       # 只构建红黑树测试
make -C build config=release bench_cclink     # 只构建单向链表基准
```

### 运行测试

```bash
# 方式一：check 目标（推荐）
make -C build check

# 方式二：直接运行
./build/bin/Release/test_ccmap
./build/bin/Release/test_cchashmap
./build/bin/Release/test_cclink
./build/bin/Release/test_cclist
./build/bin/Release/test_ccheap
./build/bin/Release/test_ccvector
./build/bin/Release/test_ccflatmap
./build/bin/Release/test_cctreap
```

### 运行基准

```bash
# 方式一：bench 目标（推荐，含分隔横幅）
make -C build bench

# 方式二：直接运行
./build/bin/Release/bench_ccmap
./build/bin/Release/bench_cchashmap
./build/bin/Release/bench_cclink
./build/bin/Release/bench_cclist
./build/bin/Release/bench_ccheap
./build/bin/Release/bench_ccvector
./build/bin/Release/bench_ccflatmap
./build/bin/Release/bench_cctreap
./build/bin/Release/bench_ccmap_prefetch
./build/bin/Release/bench_ccmap_prefetch_on
```

### 安装

```bash
premake5 install                    # 默认安装到 /usr/local
premake5 install --prefix=~/.local  # 自定义路径
```

安装后：

```
~/.local/
├── include/cclag/         # 所有 .h 头文件
└── share/doc/cclag/       # LICENSE, README.md
```

安装后在代码中：

```c
#include "cclag/ccmap.h"       // 而非 "ccmap.h"
#include "cclag/cchashmap.h"
#include "cclag/cclink.h"
#include "cclag/cclist.h"
#include "cclag/ccheap.h"
```

## vcpkg

```bash
# 方式一：叠加端口（本地仓库路径）
vcpkg install cclag --overlay-ports=<path-to-cclag>/ports

# 方式二：清单模式（在项目的 vcpkg.json 中添加依赖）
#   "dependencies": [ "cclag" ]
#   vcpkg install --overlay-ports=<path-to-cclag>/ports
```

安装后在代码中：

```c
#include <cclag/ccmap.h>
```

> **提交官方注册表**：标记 release tag（如 `v0.1.0`）后将 `ports/cclag/` 目录提交至 [vcpkg 仓库](https://github.com/Microsoft/vcpkg)。

## Conan（2.x）

```bash
# 从本地源码创建包
conan create . -tf ""

# 在项目的 conanfile.txt 中添加：
# [requires]
# cclag/0.1.0
```

安装后在代码中：

```c
#include <cclag/ccmap.h>
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


