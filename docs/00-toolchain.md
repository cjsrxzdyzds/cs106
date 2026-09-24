# 00 · 工具链：构建、测试、调试与 Sanitizer

> 对应：CS106L 课程初期的环境配置；Duke *Introductory C Programming* 中的 gcc/Makefile/gdb/valgrind；
> CMU 15-213 各 Lab 的调试方法。
> 本仓库根目录的 [`AP1400-2作业环境配置.pdf`](../AP1400-2作业环境配置.pdf) 给出了最小可用的环境搭建步骤，本章在此基础上补充“专业工程实践”部分。

## 1. 作业项目结构

AP1400-2 每个作业的结构都相同：

```
AP1400-2-HWn/
├── CMakeLists.txt     # C++14/17，链接 GTest::GTest 和 GTest::Main
├── Dockerfile         # gcc:11.2.0 镜像 + 从源码编译安装 googletest
├── include/           # 你要实现的头文件
└── src/
    ├── main.cpp       # if (true) 调试区 / else 运行全部单元测试
    ├── hwN.cpp        # 你要实现的源文件
    └── unit_test.cpp  # 课程提供的 GoogleTest 测试
```

两种搭建方式：

```bash
# 方式 A：本机（Ubuntu/Debian；其他发行版见 PDF）
sudo apt install cmake make g++ libgtest-dev libgmock-dev libssl-dev   # HW2 需要 OpenSSL
cmake -S . -B build && cmake --build build -j && ./build/main

# 方式 B：Docker（与课程一致，最省心）
docker build -t ap-hw1 . && docker run --rm ap-hw1
```

> ⚠️ `unit_test.cpp` 包含 `<gmock/gmock.h>`，所以只装 `libgtest-dev` 会报找不到头文件，还需要 `libgmock-dev`（从源码安装 googletest 时二者会一起装上）。

## 2. 推荐的编译选项

作业的 `CMakeLists.txt` 只设置了语言标准。建议在自己的环境中加入：

```cmake
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)       # 生成 compile_commands.json，供 clangd / clang-tidy 使用
add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-compare)
```

`-Wsign-compare` / `-Wconversion` 会立刻指出 HW1 中大量 `int` 与 `size_t` 混用的比较（见 [HW1 精讲](homework/code-review.md#hw1)）。

| 构建类型 | 选项 | 用途 |
|---|---|---|
| Debug | `-O0 -g` | 单步调试 |
| RelWithDebInfo | `-O2 -g` | 性能分析（perf 需要符号） |
| Release | `-O3 -DNDEBUG` | 基准测试 |
| Sanitizer | `-O1 -g -fsanitize=...` | 找 bug（见下文） |

## 3. Sanitizer：找 bug 的第一工具

Sanitizer 是编译器插桩 + 运行时库，代价远小于 valgrind（ASan 大约 2× 减速，valgrind 通常 10–50×），并且能发现 valgrind 发现不了的问题（例如栈上越界、数据竞争）。

| Sanitizer | 选项 | 能发现 | 本文档中的实例 |
|---|---|---|---|
| AddressSanitizer | `-fsanitize=address` | 越界、use-after-free、double free、内存泄漏（LeakSanitizer） | HW4 `SharedPtr::operator=` 泄漏；HW5 `Mocha` 浅拷贝导致 use-after-free |
| UndefinedBehaviorSanitizer | `-fsanitize=undefined` | 有符号溢出、非法移位、空指针解引用、未对齐访问等 | — |
| ThreadSanitizer | `-fsanitize=thread` | 数据竞争、锁顺序反转（潜在死锁） | [lab0 `racy` 模式](../labs/lab0_race/race.cpp) |
| MemorySanitizer（仅 Clang） | `-fsanitize=memory` | 读取未初始化内存 | （适用场景）HW4 `UniquePtr` 拷贝构造没有初始化 `_p`，一旦调用就会读到未初始化值 |

在作业中启用 ASan + UBSan：

```bash
cmake -S . -B build-asan \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan -j && ./build-asan/main
```

本文档在编写时就是用这条命令对仓库里的 6 份作业解答做了体检，结果汇总在各 `homework/` 章节中。

> 🔧 编译器视角：ASan 用 **shadow memory** 把每 8 字节应用内存映射为 1 字节元数据，在每次 load/store 前插入检查；
> TSan 为每个内存位置维护少量 shadow cell，记录最近访问的线程与 **向量时钟（vector clock）**，
> 用 happens-before 关系判断两次访问是否并发——这就是动态数据竞争检测中的 FastTrack 类算法（详见
> [concurrency/06](concurrency/README.md)）。
> ASan 与 TSan 使用不同的 shadow 布局，不能同时启用。

## 4. 调试器与其他工具

- **gdb / lldb**：`break`、`watch`（数据断点，调试“谁改了这个值”极其有效）、`thread apply all bt`（死锁时查看所有线程的调用栈）。
- **clang-tidy**：静态检查，推荐开启 `bugprone-*`、`cppcoreguidelines-*`、`modernize-*`、`performance-*`。
  例如 `performance-for-range-copy` 会直接指出 HW1 中 `for (vector<double> rowVec : matrix)` 的逐行拷贝。
- **perf / Linux perf_events**：`perf stat -e cache-misses,cache-references ./lab1_matmul` 可以直接观察 lab1 中不同循环顺序的 cache 行为（对应 15-213 Cache Lab 的内容）。
- **Compiler Explorer（godbolt.org）**：观察模板实例化、内联、虚函数去虚化（devirtualization）、原子操作生成的指令（如 x86 上 `seq_cst` store 生成 `xchg` 或 `mov + mfence`）。

## 5. 单元测试的使用方式

- 作业的 `main.cpp` 通过 `if (true/false)` 在“调试区”和“运行测试”之间切换；更好的做法是保留两个可执行目标，或直接用 `--gtest_filter` 选择测试：
  ```bash
  ./build/main --gtest_filter='HW3Test.TEST1*'    # 只跑一部分
  ./build/main --gtest_filter='-HW5Test.TEST6'    # 排除某个测试
  ./build/main --gtest_repeat=100 --gtest_shuffle # 重复、打乱顺序：暴露测试间的隐式依赖
  ```
- 本仓库的 `labs/` 不依赖 GoogleTest，只用 [`labs/common/check.h`](../labs/common/check.h) 中的 `CHECK` 宏，并通过 `ctest` 统一运行。
