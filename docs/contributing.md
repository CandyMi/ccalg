# 参与贡献

感谢对 cclag 的关注！

## 提交 vcpkg 官方注册表

### 前置条件

- release tag 已创建并推送到 GitHub（当前: `v0.1.0`）
- `ports/cclag/portfile.cmake` 中 SHA512 已填入（已填充）

### 操作步骤

```bash
# 1. Fork 并克隆 vcpkg
git clone https://github.com/<你的用户名>/vcpkg
cd vcpkg

# 2. 复制覆盖端口文件
curl -o ports/cclag/vcpkg.json \
  https://raw.githubusercontent.com/CandyMi/ccalg/master/ports/cclag/vcpkg.json
curl -o ports/cclag/portfile.cmake \
  https://raw.githubusercontent.com/CandyMi/ccalg/master/ports/cclag/portfile.cmake

# 3. 本地验证
./bootstrap-vcpkg.sh
./vcpkg install cclag
ls -la installed/x64-linux/include/cclag/   # 应看到 8 个 .h 文件

# 4. 提交 PR
git checkout -b add-cclag
git add ports/cclag/
git commit -m "[cclag] add new port (version 0.1.0)"
git push origin add-cclag
```

### PR 模板

**标题：**

```
[cclag] Add new port (0.1.0)
```

**描述：**

```markdown
Add cclag — header-only high-performance data-structure library for C/C++.

- 8 intrusive/container types: RB-tree (ccmap), hashmap (cchashmap),
  treap with order statistics (cctreap), d-ary heap (ccheap),
  dynamic array (ccvector), sorted-array map (ccflatmap),
  doubly/singly linked lists (cclist/cclink)
- Compatible with C89 / C99 / C++ / MSVC
- Zero internal node allocation (heap/vector/flatmap allocate)
- BSD 3-Clause license

Source: https://github.com/CandyMi/ccalg
Homepage: https://ccalg.dev
```

### CI 期望

| 检查项 | 预期 |
|--------|------|
| 构建 | 所有 triplet 通过 |
| 安装检查 | header-only，无链接依赖 |
| 许可证 | BSD-3-Clause 声明 |
| 耗时 | 约 20-40 分钟 |

---

## 提交 ConanCenter

### 前置条件

- release tag 已存在（`v0.1.0`）
- 仓库根目录已有 `conanfile.py`

### 操作步骤

```bash
# 1. Fork 并克隆 ConanCenter
git clone https://github.com/conan-io/conan-center-index
cd conan-center-index

# 2. 创建 recipe 目录
mkdir -p recipes/cclag/0.1.0
```

把仓库根目录的 `conanfile.py` 复制过去，并按 ConanCenter 规范调整（需添加 `test_package/` 目录和 `conandata.yml`）：

```bash
cp <cclag-path>/conanfile.py recipes/cclag/0.1.0/
```

> 注意：ConanCenter 对 recipe 有额外规范要求（`layout`、`validate`、`test_package`、`conandata.yml`）。
> 具体请参考 [ConanCenter contributing guide](https://github.com/conan-io/conan-center-index/blob/master/README.md)。

### CI 期望

| 检查项 | 预期 |
|--------|------|
| 构建 | 全平台矩阵通过 |
| Hook 检查 | `conan-center hooks` 无告警 |
| 耗时 | 约 30-60 分钟 |

---

## 代码规范

- C89/C99 兼容，避免 C99-only 语法（如 `//` 注释用 `/* */`）
- MSVC 兼容，避免 GNU C 扩展
- 每个 `.h` 自包含，无跨头文件依赖
- 使用 `CCXXX_` 前缀空间，避免宏污染
- 提交前运行 `cmake --build build --target check` 确保测试通过
