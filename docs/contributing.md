# 参与贡献

感谢对 ccalg 的关注！

## 提交 vcpkg 官方注册表

### 前置条件

- release tag 已创建并推送到 GitHub（当前: `v0.1.0`）
- `ports/ccalg/portfile.cmake` 中 SHA512 已填入（已填充）

### 操作步骤

```bash
# 1. Fork 并克隆 vcpkg
git clone https://github.com/<你的用户名>/vcpkg
cd vcpkg

# 2. 复制覆盖端口文件
curl -o ports/ccalg/vcpkg.json \
  https://raw.githubusercontent.com/CandyMi/ccalg/master/ports/ccalg/vcpkg.json
curl -o ports/ccalg/portfile.cmake \
  https://raw.githubusercontent.com/CandyMi/ccalg/master/ports/ccalg/portfile.cmake

# 3. 本地验证
./bootstrap-vcpkg.sh
./vcpkg install ccalg
ls -la installed/x64-linux/include/ccalg/   # 应看到 8 个 .h 文件

# 4. 提交 PR
git checkout -b add-ccalg
git add ports/ccalg/
git commit -m "[ccalg] add new port (version 0.1.0)"
git push origin add-ccalg
```

### PR 模板

**标题：**

```
[ccalg] Add new port (0.1.0)
```

**描述：**

```markdown
Add ccalg — header-only high-performance data-structure library for C/C++.

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

> ConanCenter 使用 **Conan 2.x**，且 recipe 结构有严格规范。
> ConanCenter 对 header-only 库的 recipe 相对简单。

### 前置条件

- release tag 已推送到 GitHub（当前: `v0.1.0`）
- 本地安装 Conan ≥ 2.x

### recipe 文件结构

ConanCenter 中每个库的 recipe 按以下目录结构组织：

```
conan-center-index/recipes/ccalg/
├── config.yml              # 版本 → 文件夹映射
└── all/
    ├── conanfile.py         # 主 recipe
    ├── conandata.yml        # 源码下载地址 + checksum
    ├── patches/             # （可选）补丁
    └── test_package/
        ├── conanfile.py     # 测试 recipe
        ├── CMakeLists.txt
        └── test_package.cpp
```

### 各文件说明

#### config.yml

```yaml
versions:
  "0.1.0":
    folder: all
```

#### conandata.yml

```yaml
sources:
  "0.1.0":
    url: "https://github.com/CandyMi/ccalg/archive/refs/tags/v0.1.0.tar.gz"
    sha256: "646fd3e0202948b696e7be33dc14c8c1543afe88f9bd0a9966c0e5a704df89b6"
```

#### conanfile.py

参照 [parallel-hashmap](https://github.com/conan-io/conan-center-index/tree/master/recipes/parallel-hashmap)（也是 header-only C/C++ 库）编写：

```python
from conan import ConanFile
from conan.tools.files import copy, get
from conan.tools.layout import basic_layout
import os

required_conan_version = ">=1.50.0"

class CcalgConan(ConanFile):
    name = "ccalg"
    description = "Header-only high-performance data-structure library for C/C++"
    license = "BSD-3-Clause"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://ccalg.dev"
    topics = ("data-structures", "header-only", "intrusive", "c", "c-plus-plus")
    package_type = "header-library"
    settings = "os", "arch", "compiler", "build_type"
    no_copy_source = True

    def layout(self):
        basic_layout(self, src_folder="src")

    def package_id(self):
        self.info.clear()

    def source(self):
        get(self, **self.conan_data["sources"][self.version],
            strip_root=True)

    def package(self):
        copy(self, "LICENSE", src=self.source_folder,
             dst=os.path.join(self.package_folder, "licenses"))
        # 安装 8 个头文件到 include/ccalg/
        for h in ["ccmap.h", "cchashmap.h", "cclink.h", "cclist.h",
                   "ccheap.h", "ccvector.h", "ccflatmap.h", "cctreap.h"]:
            copy(self, h, src=os.path.join(self.source_folder, "include"),
                 dst=os.path.join(self.package_folder, "include", "ccalg"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "ccalg")
        self.cpp_info.set_property("cmake_target_name", "ccalg::ccalg")
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = ["include"]
```

#### test_package/conanfile.py

```python
from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, cmake_layout
import os

class TestPackageConan(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    generators = "CMakeToolchain", "CMakeDeps", "VirtualRunEnv"
    test_type = "explicit"

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires(self.tested_reference_str)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if can_run(self):
            bin_path = os.path.join(self.cpp.build.bindirs[0], "test_package")
            self.run(bin_path, env="conanrun")
```

#### test_package/CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.15)
project(test_package LANGUAGES C)

find_package(ccalg REQUIRED CONFIG)

add_executable(${PROJECT_NAME} test_package.c)
target_link_libraries(${PROJECT_NAME} PRIVATE ccalg::ccalg)
target_compile_features(${PROJECT_NAME} PRIVATE c_std_99)
```

#### test_package/test_package.c

```c
#include <ccalg/ccmap.h>

int main() {
    ccmap_t m;
    ccmap_init(&m, NULL);  // no comparison function needed for empty test
    ccmap_destroy(&m);
    return 0;
}
```

### 操作步骤

```bash
# 1. Fork 并克隆 ConanCenter
git clone https://github.com/<你的用户名>/conan-center-index
cd conan-center-index

# 2. 创建 recipe（从 ccalg 仓库的模板复制）
mkdir -p recipes/ccalg/all/test_package

# conandata.yml
cat > recipes/ccalg/all/conandata.yml << 'EOF'
sources:
  "0.1.0":
    url: "https://github.com/CandyMi/ccalg/archive/refs/tags/v0.1.0.tar.gz"
    sha256: "646fd3e0202948b696e7be33dc14c8c1543afe88f9bd0a9966c0e5a704df89b6"
EOF

# config.yml
cat > recipes/ccalg/config.yml << 'EOF'
versions:
  "0.1.0":
    folder: all
EOF

# 3. 编写 conanfile.py、test_package/*（参考上方模板）
#    … 粘贴 conanfile.py 内容 …
#    … 粘贴 test_package/* 内容 …

# 4. 本地验证
conan create recipes/ccalg/all/conanfile.py --version=0.1.0

# 5. 提交 PR
git checkout -b add-ccalg
git add recipes/ccalg/
git commit -m "ccalg: add new recipe (version 0.1.0)"
git push origin add-ccalg
```

### PR 模板

**标题：**

```
ccalg: add new recipe (0.1.0)
```

**描述：**

```markdown
Add ccalg — header-only high-performance data-structure library for C/C++.

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
| 构建 | 全平台矩阵通过（~30+ configurations） |
| Hook 检查 | `conan-center hooks` 零告警 |
| CLA | 需签署 CLA Assistant |
| 耗时 | 约 30-60 分钟（取决于队列） |

ConanCenter 的 CI 会为每个支持的平台/编译器组合构建二进制包。
header-only 库的构建耗时较短，但队列等待时间不定。

---

## 代码规范

- C89/C99 兼容，避免 C99-only 语法（如 `//` 注释用 `/* */`）
- MSVC 兼容，避免 GNU C 扩展
- 每个 `.h` 自包含，无跨头文件依赖
- 使用 `CCXXX_` 前缀空间，避免宏污染
- 提交前运行 `cmake --build build --target check` 确保测试通过
