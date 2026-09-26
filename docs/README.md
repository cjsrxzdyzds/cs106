# 现代 C++ 与多线程编程：以 AP1400-2 为项目主线

> 以 **Stanford CS106L（Standard C++ Programming）** 的主题顺序为主线讲解现代 C++，
> 以 **AUT AP1400-2** 的 6 个作业作为练手项目，
> 并参考 **CMU 15-213 / 15-418 / 15-445** 的相关内容，把每个作业扩展成多线程实验。

本文档面向已经会 C（或其他类 C 语言）、想系统掌握现代 C++（C++17/20）并入门并发编程的读者。
仓库的整体自学目标与后续路线见 [项目首页](../README.md)；本目录是已经落地的 C++ 与并发课程。

---

本页保留 C++ 与并发课程的完整路线。新增学习线：[算法课程（6 章）](algorithms/README.md) · [CUDA 算子课程（5 章）](cuda/README.md)。二者可独立构建实验，当前验证状态见各自目录。

## 1. 文档结构

```
docs/
├── README.md                     ← 你在这里：总览、路线图、课程映射
├── 00-toolchain.md               构建、测试、调试与 Sanitizer
├── cpp/                          第一部分：现代 C++（CS106L 主线）✅
│   ├── 01-types-and-initialization.md
│   ├── 02-streams-and-strings.md
│   ├── 03-stl-containers-iterators-algorithms.md
│   ├── 04-functions-lambdas-templates.md
│   ├── 05-classes-const-operators.md
│   ├── 06-special-members-and-move-semantics.md
│   ├── 07-raii-and-smart-pointers.md
│   └── 08-inheritance-and-polymorphism.md
├── concurrency/
│   ├── README.md                 第二部分：多线程目录与实验路线
│   └── 01–06                     线程、同步、原子、线程池、数据结构、测试
├── algorithms/                  类 LeetCode 算法课（6 章）
├── cuda/                        CUDA 算子课（5 章，GPU 待验证）
└── homework/
    └── code-review.md            第三部分：HW1–HW6 代码审查汇总（ASan / 单元测试 / 最小复现实证）

labs/                             可运行的并发实验（C++20，无第三方依赖；验证环境见下文）
```

完成状态：C++ 部分 8 章、多线程部分 6 章正文与 7 个实验均已提供，包含练习、参考思路与结课验收，
见 [concurrency/README.md](concurrency/README.md)；逐个作业的精讲目前合并为一份
[代码审查汇总](homework/code-review.md)，每一条结论都标注了依据（Sanitizer 实测、测试失败、链接错误、最小复现或代码审阅）。

---

## 2. 课程映射总表

| 主题 | CS106L（主线） | AP1400-2 作业 | CMU 参考 | 本文档章节 | 实验 |
|---|---|---|---|---|---|
| 类型、初始化、引用、`const` | Types & Structs, Initialization | HW1 | — | [cpp/01](cpp/01-types-and-initialization.md) | — |
| 流与字符串 | Streams | HW1 `show`、HW2 `parse_trx`、HW6 q2/q3 | 15-213 系统级 I/O（第 10 章） | [cpp/02](cpp/02-streams-and-strings.md) | — |
| 容器、迭代器、算法 | Containers, Iterators, Algorithms | HW1、HW6 | 15-213 存储器层次（第 6 章） | [cpp/03](cpp/03-stl-containers-iterators-algorithms.md) | [lab1](../labs/lab1_matmul/matmul.cpp) |
| 函数对象、lambda、模板 | Templates, Functions & Lambdas | HW1 `transform`、HW3 `bfs`、HW4、HW6 q1 | — | [cpp/04](cpp/04-functions-lambdas-templates.md) | — |
| 类、`const` 正确性、运算符重载 | Classes, Const Correctness, Operators | HW2、HW3 | — | [cpp/05](cpp/05-classes-const-operators.md) | — |
| 特殊成员函数、移动语义 | Special Member Functions, Move Semantics | HW3、HW5 | — | [cpp/06](cpp/06-special-members-and-move-semantics.md) | — |
| RAII 与智能指针 | RAII & Smart Pointers | HW4 | 15-213 动态内存分配（第 9 章） | [cpp/07](cpp/07-raii-and-smart-pointers.md) | [lab4](../labs/lab4_smart_ptr/) |
| 继承与多态 | Inheritance | HW5 | — | [cpp/08](cpp/08-inheritance-and-polymorphism.md) | [lab5](../labs/lab5_thread_pool/) |
| 线程与内存模型 | Multithreading（部分学期开设） | — | 15-213 第 12 章；15-418 Memory Consistency | [concurrency/01](concurrency/01-threads-and-memory-model.md) | [lab0](../labs/lab0_race/race.cpp) |
| 互斥量、条件变量、信号量 | 同上 | HW2 扩展 | 15-213 第 12 章（sbuf、读者-写者） | [concurrency/02](concurrency/02-mutexes-condition-variables-semaphores.md) | [lab6](../labs/lab6_queue/) |
| 原子操作与内存序 | — | HW4 扩展 | 15-418 Cache Coherence, Synchronization, Lock-free | [concurrency/03](concurrency/03-atomics-and-memory-order.md) | [lab0](../labs/lab0_race/race.cpp)、[lab4](../labs/lab4_smart_ptr/) |
| 任务、future、线程池、并行分解 | — | HW1、HW5 扩展 | 15-418 Parallel Programming Basics, Work Distribution | [concurrency/04](concurrency/04-tasks-futures-thread-pools.md) | [lab1](../labs/lab1_matmul/matmul.cpp)、[lab5](../labs/lab5_thread_pool/) |
| 并发数据结构 | — | HW3、HW2 扩展 | 15-445 Index Concurrency Control, OCC；15-418 Fine-grained Synchronization | [concurrency/05](concurrency/05-concurrent-data-structures.md) | [lab2](../labs/lab2_mining/mining.cpp)、[lab3](../labs/lab3_bst/concurrent_bst.cpp) |
| 并发程序的测试与调试 | — | 全部 | 15-418；程序分析相关论文 | [concurrency/06](concurrency/06-testing-and-debugging.md) | 全部 |

> 关于 CS106L：该课程每学期的讲次安排不同（例如 Fall 2019 由 Avery Wang 主讲、有完整录像，近年的版本调整了顺序并加入了更多 C++20 内容），
> 所以表中按**主题**而不是讲次编号对应。并发部分 CS106L 只在部分学期用一讲简单介绍，本文档的第二部分主要参考 CMU 课程和
> *C++ Concurrency in Action* 展开。

---

## 3. 建议学习路线（约 12 周）

| 周 | 阅读 | 动手 |
|---|---|---|
| 0 | [00-toolchain](00-toolchain.md)；搭好 Docker/CMake/GTest 环境（见根目录 PDF） | 跑通 HW1 的空实现与单元测试 |
| 1 | cpp/01、cpp/02 | HW1 前半（`zeros`…`transpose`） |
| 2 | cpp/03、cpp/04 | HW1 后半 + 阅读 [HW1 精讲](homework/code-review.md#hw1) |
| 3 | cpp/05 | HW2 |
| 4 | cpp/06 | HW3（最值得做的一个） |
| 5 | cpp/07 | HW4 |
| 6 | cpp/08 | HW5 |
| 7 | 回顾 cpp/03、cpp/04 | HW6 |
| 8 | concurrency/01 + CS:APP 第 12.1–12.4 节 | lab0、lab1 |
| 9 | concurrency/02 + CS:APP 第 12.5 节 | lab6、lab2 |
| 10 | concurrency/03 + 15-418 同步相关讲义 | lab4 |
| 11 | concurrency/04 | lab5 |
| 12 | concurrency/05、06 + 15-445 Index Concurrency 讲义 | lab3，并完成各章“练习” |

如果已熟悉 C++，可以直接从第 8 周开始；
如果主要兴趣是并发，建议至少先读 cpp/06 和 cpp/07：移动语义与 RAII 是理解 `std::thread`、`std::unique_lock`、`std::future` 所有权语义的前提。

---

## 4. 运行实验

```bash
cd labs
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
ctest --test-dir build --output-on-failure

# 用 ThreadSanitizer 检查数据竞争
cmake -S . -B build-tsan -DLAB_SANITIZER=thread && cmake --build build-tsan -j
ctest --test-dir build-tsan --output-on-failure
./build-tsan/lab0_race racy        # 故意制造一个数据竞争，观察 TSan 报告

# 用 AddressSanitizer + UBSan 检查内存错误与未定义行为
cmake -S . -B build-asan -DLAB_SANITIZER=address && cmake --build build-asan -j
```

需要支持 C++20 的编译器及标准库（用到了 `std::jthread`、`std::counting_semaphore`、`std::stop_token`）。
仓库历史记录：GCC 13 的 Release / TSan / ASan+UBSan 与 Clang 18 Release 测试通过。
2026-09-25 本机复核：macOS arm64、Apple Clang 21，Release 下 7/7 测试通过；
lab5 异常测试有一处忽略 `future::get()` 返回值的编译警告。历史 Sanitizer 结论不等同于本次复测，
完整操作与验证边界见 [测试与调试](concurrency/06-testing-and-debugging.md)。

---

## 5. 约定

- 代码默认 C++20；与作业（C++14/17）不同的写法会注明。
- “⚠️” 表示常见错误或未定义行为（UB）；“🔧 编译器视角” 小节从编译器/程序分析的角度解释语言规则背后的原因。
- 文中给出的性能数据来自一次在 4 vCPU 容器中的测量，仅用于说明量级与趋势，请在自己的机器上复现。
