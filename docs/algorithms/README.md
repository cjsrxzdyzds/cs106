# 算法课程 · 从题意到可验证实现

这是一组类 LeetCode 的 C++ 面试算法课，先覆盖 6 类常见思路、12 个代表性问题。题意与讲解在仓库内完整给出，不要求访问题库，也不复制平台题面。目标是能从输入约束推导算法，写出正确实现，再解释复杂度与边界。

前置：vector、string、引用、基础 STL、递归。源码用 C++20；尚未覆盖排序全家桶、链表、并查集、带权最短路、线段树等专题，这些留待后续扩展。

| 章 | 代表性问题 | 核心思路 | 实现函数 |
|---|---|---|---|
| [01 哈希与前缀和](01-hash-and-prefix-sums.md) | 两数之和、和为目标的连续子数组 | 保存“此前见过什么” | two_sum / subarray_sum |
| [02 二分与滑动窗口](02-search-and-sliding-window.md) | 首个不小于目标的位置、最长无重复片段 | 单调边界与窗口不变量 | lower_bound_index / longest_unique |
| [03 单调栈与单调队列](03-monotonic-stack-and-deque.md) | 右侧更大元素、窗口最大值 | 淘汰不可能再最优的候选 | next_greater_distance / sliding_max |
| [04 树与堆](04-trees-and-heaps.md) | 层序遍历、第 k 大元素 | 分层搜索、保留有限候选 | level_order / kth_largest |
| [05 图与拓扑排序](05-graphs-and-topological-sort.md) | 无权最短路、依赖排序 | BFS 层次、入度消除 | bfs_distances / topological_order |
| [06 动态规划与回溯](06-dp-and-backtracking.md) | 最少硬币、枚举组合 | 重用子问题、搜索并撤销 | coin_change / combinations |

## 怎么学

每章按“题意 → 手算样例 → 基线 → 不变量 → 实现 → 测试 → 面试追问”推进。先自己写，再看 [参考实现](../../algorithms/algorithms.h)。正文展示关键循环，完整头文件与接口以参考实现为准。

实现对非法窗口、名次、图节点、硬币与组合参数抛 `std::invalid_argument`；空输入并不一律非法，以各章契约为准。前缀和及计数假设输入规模使结果可由 `long long` 表示，容器分配也可能失败；这不是无限规模输入的算法库。

## 构建与验证

从仓库根目录运行：

```bash
cmake -S algorithms -B algorithms/build -DCMAKE_BUILD_TYPE=Release
cmake --build algorithms/build -j 4
ctest --test-dir algorithms/build --output-on-failure
# 只练第一章
./algorithms/build/algorithm_tests 1
```

[测试代码](../../algorithms/tests.cpp) 包含固定边界与固定随机种子的交叉验证：暴力枚举、标准库排序/二分、Floyd-Warshall、金额状态图 BFS、位掩码枚举等。测试程序对随机小输入查错，不构成所有输入正确的形式化证明。

GCC/Clang 可另外启用 ASan/UBSan：

```bash
cmake -S algorithms -B algorithms/build-asan -DALGORITHMS_SANITIZE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build algorithms/build-asan -j 4
ctest --test-dir algorithms/build-asan --output-on-failure
```

## 结课练习

从“连续子数组计数、窗口最大值、图最短路、最少硬币”中任选两题，关闭参考实现独立重写；分别提供三个边界输入、一个能让错误解法失败的反例，以及正确性说明。最后用五分钟解释其中一题，并回答输入条件变化后原方法是否仍适用。

连接并发/CUDA：BFS 的层次、动态规划的依赖和归约的结合方式决定了哪些工作能并行，不能把顺序算法的每个循环简单改成多个线程。继续阅读 [CUDA 算子课程](../cuda/README.md)。

[项目首页](../../README.md) · [C++ 基础课程](../README.md)
