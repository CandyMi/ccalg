# alg — C 数据结构库文档

**header-only · 侵入式 · 零开销回调 · C89 兼容**

## 文档导航

| 文档 | 说明 |
| --- | --- |
| [快速开始](getting-started.md) | 5 分钟上手，含完整示例 |
| [API 参考](api-reference.md) | 四个容器的完整 API 手册 |
| [构建指南](building.md) | CMake / Premake5 / 手动编译 |

## 容器一览

| 容器 | 头文件 | 数据结构 | 内部分配 |
| --- | --- | --- | --- |
| `ccmap` | [`include/ccmap.h`](../include/ccmap.h) | 侵入式红黑树（有序映射） | 无 |
| `cchashmap` | [`include/cchashmap.h`](../include/cchashmap.h) | 侵入式链式哈希表 | 桶数组 |
| `cclist` | [`include/cclist.h`](../include/cclist.h) | 侵入式双向链表 | 无 |
| `ccheap` | [`include/ccheap.h`](../include/ccheap.h) | D-ary 堆（优先队列） | 数组（值模式） |

## 设计文档

| 文档 | 说明 |
| --- | --- |
| [`REASONIC.md`](../REASONIC.md) | 设计规范与接口约定（权威参考） |
| [`AGENTS.md`](../AGENTS.md) | AI Agent 操作手册 |
| [`README.md`](../README.md) | 项目简介 |

## 快速链接

- **构建**: `cmake -S . -B build && cmake --build build`
- **测试**: `ctest --test-dir build -L unit --output-on-failure`
- **基准**: `cmake --build build --target bench`
- **协议**: BSD 3-Clause
