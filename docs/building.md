# 构建指南

alg 是 header-only 库，核心代码无需构建。构建系统用于**单元测试**和**基准测试**。

## CMake（推荐）

### 配置

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

所有产物隔离在 `build/` 目录（已在 `.gitignore` 中）。

### 目标一览

| 目标 | 命令 | 说明 |
| --- | --- | --- |
| 全量构建 | `cmake --build build` | 4 个测试 + 4 个 benchmark |
| 测试 | `cmake --build build --target check` | 构建 + CTest 运行全部单元测试 |
| 基准 | `cmake --build build --target bench` | 构建 + 依次运行全部 benchmark |
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

### 手动运行单个测试

```bash
./build/test_ccmap      # 红黑树
./build/test_cchashmap  # 哈希表
./build/test_cclist     # 双向链表
./build/test_ccheap     # 优先队列
```

### 手动运行单个 benchmark

```bash
./build/bench_ccmap      # ccmap vs std::map
./build/bench_cchashmap  # cchashmap vs std::unordered_map
./build/bench_cclist     # cclist vs std::list
./build/bench_ccheap     # ccheap vs std::priority_queue
```

## Premake5（备选）

```bash
premake5 gmake2               # 生成 Makefile
make -C build config=release  # 构建
./build/bin/Release/test_ccmap
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
cmake --build build --target clean  # 或只清理编译产物
```
