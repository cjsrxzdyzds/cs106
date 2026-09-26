# day-one · 系统编程、算法与形式化推理

一个用于长期自学的代码与笔记仓库：以 **C/C++ 多线程编程、算法实现与 CUDA 算子** 为主线，把知识落实为可运行、可测试、可解释的程序，同时用于面试准备；后续加入 **OCaml 函数式编程**与 **Lean 4 形式化证明**，逐步连接到量化研究与工程相关的学习兴趣。

学习的基本单位是一个具体问题：先写出正确实现，解释不变量和复杂度，再验证并发行为与性能，最后尝试用另一种语言或证明方式重新理解它。

## 跨会话学习进度

[STUDY.md](STUDY.md) 是当前停点与下一步的唯一入口；[学习日志](learning/LOG.md) 保存作答、练习与复习证据。新学习会话按 [AGENTS.md](AGENTS.md) 先恢复进度，完成小节后更新。课件已经写完不等于学习者已经掌握。

同仓库的新 chat 可以说“按 STUDY.md 继续学习”；仓库外的 chat 需要提供该文件与最近日志，跨机器需要先同步 Git 中的记录。

## 当前从哪里开始

| 内容 | 当前状态 | 入口 |
|---|---|---|
| 工具链、构建与调试 | 已有文档 | [工具链](docs/00-toolchain.md) |
| 现代 C++ 基础 | 已有 8 章 | [课程总览](docs/README.md) |
| C++ 多线程编程 | 已有 6 章，含练习与参考思路 | [多线程课程](docs/concurrency/README.md) |
| 并发实验 | 已有 lab0–lab6，共 7 个 | [实验目录](labs/README.md) |
| AP1400-2 作业复盘 | 已有代码审查文档；原作业通过子模块获取 | [作业审查](docs/homework/code-review.md) |
| C 与 POSIX 线程 | 规划中，尚无独立课程 | 见下方路线 |
| 算法实现与面试训练 | 已有 6 章、12 个代表性算法及测试 | [算法课程](docs/algorithms/README.md) |
| CUDA 算子入门 | 已有 5 章、4 类算子与 CPU 对照；GPU 待验证 | [CUDA 课程](docs/cuda/README.md) |
| OCaml | 规划中，尚无代码与课程 | 见下方路线 |
| Lean 4 | 规划中，尚无代码与课程 | 见下方路线 |

**当前以 C++ 并发、算法练习与 CUDA 入门交替推进。** OCaml 与 Lean 先保留明确的学习目标，等主线跑通后逐步加入。现有目录不代表所有长期目标已经覆盖。

## 五条相互连接的学习线

### 1. C/C++：从内存与所有权到多线程

已有 C++ 课程覆盖类型、容器、模板、类、移动语义、RAII、智能指针与多态。并发部分按以下顺序推进：

1. [线程与内存模型](docs/concurrency/01-threads-and-memory-model.md)：生命周期、数据竞争、happens-before。
2. [互斥与等待](docs/concurrency/02-mutexes-condition-variables-semaphores.md)：不变量、死锁、条件变量、信号量、关闭协议。
3. [原子与内存序](docs/concurrency/03-atomics-and-memory-order.md)：发布、CAS、引用计数、伪共享。
4. [任务与线程池](docs/concurrency/04-tasks-futures-thread-pools.md)：future、异常、任务分配、排空与并行收益。
5. [并发数据结构](docs/concurrency/05-concurrent-data-structures.md)：线性化、锁粒度、节点回收、OCC。
6. [测试与调试](docs/concurrency/06-testing-and-debugging.md)：Sanitizer、受控交错、模型检查与结课项目。

后续补充 C 的对应实现：用 pthread 创建和回收线程，用 mutex/condition variable 实现有界队列，练习手动清理与错误返回；再与 C++ RAII 版本对照。C11 原子单独建立语言层面的推理，不把 C++ 的对象生命周期规则直接套过去。

每完成一个并发组件，都要回答：谁拥有数据？哪些访问冲突？同步关系在哪里？失败时如何恢复？等待者如何退出？

### 2. 算法：独立实现、验证正确性、分析成本

算法练习贯穿并发学习，不必等并发课程全部结束才开始。先做串行版本，再判断哪些部分适合并行。

| 阶段 | 长期覆盖目标 | 必须讲清楚的内容 |
|---|---|---|
| 基础 | 二分、排序、双指针、前缀和、链表 | 边界、不变量、时间与空间复杂度 |
| 数据结构 | 堆、哈希表、BST、并查集、LRU | 接口语义、所有权、退化情况 |
| 图与搜索 | BFS、DFS、拓扑排序、最短路 | 状态表示、访问标记、算法前提 |
| 优化与决策 | 贪心、动态规划、回溯、Top-K | 正确性理由、状态转移、剪枝条件 |
| 与系统结合 | 矩阵计算、批量归约、生产者—消费者 | 局部性、分解方式、同步开销 |

每份实现至少包含：问题规格、算法说明、独立实现、边界测试、复杂度分析。适合时与标准库或简单串行实现交叉验证。性能优化必须保留正确性校验，不能只展示一个更短的运行时间。

已提供 [6 章算法课](docs/algorithms/README.md) 与 [12 个 C++ 参考实现](algorithms/algorithms.h)，覆盖哈希与前缀和、二分与滑窗、单调栈与队列、树与堆、图与拓扑排序、动态规划与回溯。每章有题意、推导、不变量、复杂度、测试与面试追问；排序、链表、并查集等长期目标仍待扩展。

### 3. CUDA：从数据并行到算子优化

[CUDA 课程](docs/cuda/README.md) 从执行模型开始，依次实现向量加法、求和归约、朴素与分块矩阵乘法、稳定 Softmax。学习重点是线程索引、连续访存、shared memory、同步、尾部处理和数值误差，再用 Event 区分 kernel 与端到端耗时。

[CUDA 实验](cuda/README.md) 提供独立构建和 CPU 参考。当前 Mac 已验证 CPU 部分；GPU 代码仍需 NVIDIA GPU、驱动与 nvcc 环境编译运行。没有 CUDA 的机器也能先学推导、运行 CPU 对照，不将它们算作 GPU 验证。

### 4. OCaml：用类型与函数式方式重新实现问题

计划顺序：表达式与递归 → 代数数据类型与模式匹配 → 高阶函数与不可变数据 → 模块与接口 → 测试与工程组织，再进入并发主题。

第一批练习复用已经熟悉的问题：列表与树遍历、持久化集合、表达式求值器、事件状态机。对同一道题比较 C++ 的可变状态和所有权设计，与 OCaml 的不可变数据和类型建模，不要求机械地逐行翻译。

入门从 [OCaml 官方学习文档](https://ocaml.org/docs) 开始；之后结合 [Real World OCaml](https://dev.realworldocaml.org/) 学习模块、错误处理与实际程序组织。当前仓库尚未配置 OCaml 工具链。

### 5. Lean 4：把算法性质写成可检查的证明

这里的 Lean 指 **Lean 4 编程语言与定理证明器**。计划从函数、归纳类型、命题与证明开始，再练习归纳法、递归函数性质与小型算法正确性。

先证明范围明确的性质，例如列表反转两次得到原列表、插入保持集合成员关系、排序保持长度与元素排列。随后再讨论有序性和更完整的算法规格。

学习入口使用 [Lean 官方学习路线](https://lean-lang.org/learn/) 中的 *Functional Programming in Lean* 与 *Theorem Proving in Lean*。完成的证明练习不保留 `sorry`，并明确前提。证明 Lean 中的数学模型，不自动等于证明对应 C++ 实现的整数溢出、内存管理或并发行为正确；二者的对应关系需要另外建立。

## 面试准备如何融入日常学习

每个主题同时保留实现记录与口头解释。一个可复用的练习流程是：

1. **明确题意**：输入、输出、规模、边界和错误处理约定。
2. **独立实现**：先不看已有答案，写出一个正确的基线。
3. **解释正确性**：说明循环不变量、递归假设或并发操作的生效点。
4. **测试与分析**：覆盖边界，计算复杂度；有性能主张时给出测量。
5. **复盘与复述**：记录错误原因，再用几分钟讲清实现和取舍。

| 面试主题 | 可以从仓库中练习的问题 |
|---|---|
| C++ 所有权 | unique_ptr 与 shared_ptr 的差别；移动后对象；异常路径清理 |
| 多线程基础 | 为什么 join 不能修复两个 worker 之间的数据竞争？ |
| 同步设计 | 为什么条件变量需要谓词？如何关闭满队列？ |
| 原子操作 | relaxed 何时足够？CAS 失败后 expected 为什么变化？ |
| 数据结构 | BST 如何维护不变量？并发删除为什么涉及回收？ |
| 系统设计 | 线程池如何排空、传递异常、避免池内等待死锁？ |
| 性能分析 | 为什么先改善缓存局部性，再增加线程？ |

后续若按具体岗位准备，再补充相应的操作系统、网络、概率统计或研究类题目。量化方向作为应用兴趣，可以用于选择事件处理、数值计算等项目题材；当前学习路线不以某种语言的行业热度作为完成标准。

## 建议推进顺序

| 阶段 | 主线 | 可检查的产出 |
|---|---|---|
| A · 跑通与补基础 | 工具链、C++ 所有权与容器 | 独立构建，解释一处内存或生命周期错误 |
| B · 并发入门 | 第 01–02 章，lab0、lab6 | 独立实现可关闭队列，并解释等待与退出 |
| C · 内存模型与任务 | 第 03–04 章，lab4、lab5、lab1 | 解释引用计数，完成线程池与串并行对照 |
| D · 数据结构与验证 | 第 05–06 章，lab3、lab2 | 实验复盘与航班流水线结课项目 |
| E · 算法与 C 对照 | 6 章算法、后续 pthread 队列 | 独立实现、边界测试、不变量与面试复述 |
| F · GPU 算子 | 5 章 CUDA、4 类算子 | CPU 对照、GPU 正确性、同步检查与性能记录 |
| G · 函数式与证明 | OCaml、Lean 4 | 同一算法的另一种实现，以及一个明确性质的证明 |

算法小题与面试复述可以从 A 阶段开始穿插。按阶段验收推进，比给所有语言同时开一套课程更容易形成完整成果。

## 运行现有实验

需要 CMake、支持 C++20 的编译器及标准库；标准库必须提供 `std::jthread`、`std::stop_token` 和 `std::counting_semaphore`。这些实验不依赖 GoogleTest，也不要求先下载原作业子模块。

从仓库根目录执行：

```bash
cmake -S labs -B labs/build -DCMAKE_BUILD_TYPE=Release
cmake --build labs/build -j 4
ctest --test-dir labs/build --output-on-failure --timeout 120
```

数据竞争检查使用独立构建：

```bash
cmake -S labs -B labs/build-tsan -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLAB_SANITIZER=thread
cmake --build labs/build-tsan -j 4
ctest --test-dir labs/build-tsan --output-on-failure --timeout 180
```

故意含数据竞争的例子需要单独运行，预期收到诊断：

```bash
./labs/build-tsan/lab0_race racy
```

ASan/UBSan、挂起排查与工具限制见 [测试与调试](docs/concurrency/06-testing-and-debugging.md)。教学实验的输入、异常与生命周期边界见对应课件，不把测试通过理解为所有场景均已覆盖。

本次验证记录（2026-09-25）：macOS arm64、Apple Clang 21，Release 构建下 **7/7 测试通过**。lab5 异常测试存在一处忽略 `future::get()` 返回值的编译警告；本次记录不宣称重新完成了 Sanitizer 验证。

算法与 CUDA 的构建入口分别见 [算法实验](algorithms/README.md) 和 [CUDA 实验](cuda/README.md)，各自独立于并发 labs。2026-09-26 本机新增验证：算法 Release 与 ASan/UBSan 均为 6/6 通过；CUDA CPU 参考为 1/1 通过，GPU 尚未编译运行。

## 仓库结构

```text
docs/
  00-toolchain.md       构建、调试、Sanitizer
  cpp/                 8 章现代 C++ 基础
  concurrency/         6 章多线程课程、练习与结课规格
  homework/            原作业代码审查
  algorithms/          6 章类 LeetCode 算法课
  cuda/                5 章 CUDA 算子课
labs/
  lab0_race/           数据竞争、原子计数与伪共享
  lab1_matmul/         矩阵算法、局部性与并行分解
  lab2_mining/         账本、确定性搜索与 OCC
  lab3_bst/            粗锁、读写锁与锁耦合 BST
  lab4_smart_ptr/      所有权与原子引用计数
  lab5_thread_pool/    任务、future、线程池
  lab6_queue/          有界队列与生产者—消费者
algorithms/            12 个 C++ 算法与 6 组测试
cuda/                  CPU 参考、CUDA kernel 与 GPU 验证入口
HW1/ … HW6/            原 AP1400-2 作业子模块
```

算法与 CUDA 模块已有正文和实现。OCaml、Lean 仍为规划，待首个有说明、实现与验证的学习单元准备好后再加入；面试追问目前直接放在算法课中。

需要阅读原作业源代码时执行：

```bash
git submodule update --init --recursive
```

作业依赖和构建方式与独立 labs 不同，参见 [工具链文档](docs/00-toolchain.md) 和 [原环境配置 PDF](AP1400-2作业环境配置.pdf)。本地未初始化子模块时，相应目录可能为空。

## 学习记录的完成标准

一个单元完成后，应留下别人可以复现、自己过一段时间仍看得懂的材料：

- 问题与前提：解决什么，支持哪些输入和操作。
- 实现与理由：关键不变量、所有权、同步方式或证明目标。
- 验证与结果：怎么运行，哪些测试通过，哪些工具未能执行。
- 取舍与复盘：复杂度、已知限制、踩过的坑，以及下一步值得改进的地方。

仓库起源于 Duke C、Stanford CS106L、NTU Programming Notes 与 AUT AP1400-2 的学习实践，保留作业与实验作为已有基础；后续围绕上述学习目标持续整理。
