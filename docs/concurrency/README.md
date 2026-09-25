# 第二部分：C++ 多线程编程

本部分包含 6 章正文、配套实验、练习与参考思路。学习目标是能实现并解释一个并发组件：共享什么、如何同步、何时结束，以及如何检验正确性和性能。

默认使用 C++20。先修内容为引用与 lambda、移动语义、RAII、容器基础；不熟悉时先阅读 [C++ 课程](../README.md)。参考脉络包括 CS:APP 第 12 章、CMU 并行与数据库课程，以及 *C++ Concurrency in Action*。正文结合本仓库代码讲解，不以外部课程讲次编号作为先修要求。

## 章节与学习产出

| 章 | 正文 | 学完后应能做到 | 配套实验 |
|---|---|---|---|
| 01 | [线程与内存模型](01-threads-and-memory-model.md) | 解释生命周期、数据竞争与 happens-before | [lab0](../../labs/lab0_race/race.cpp)、[lab1](../../labs/lab1_matmul/matmul.cpp) |
| 02 | [互斥量、条件变量、信号量](02-mutexes-condition-variables-semaphores.md) | 写出锁不变量、等待谓词和关闭协议 | [lab6](../../labs/lab6_queue/)、[lab2](../../labs/lab2_mining/mining.cpp) |
| 03 | [原子操作与内存序](03-atomics-and-memory-order.md) | 解释发布、CAS、引用计数与伪共享 | [lab0](../../labs/lab0_race/race.cpp)、[lab4](../../labs/lab4_smart_ptr/) |
| 04 | [任务、future 与线程池](04-tasks-futures-thread-pools.md) | 传递结果与异常，设计 drain，测量并行收益 | [lab5](../../labs/lab5_thread_pool/)、[lab1](../../labs/lab1_matmul/matmul.cpp) |
| 05 | [并发数据结构](05-concurrent-data-structures.md) | 讨论线性化、锁耦合、回收与 OCC | [lab3](../../labs/lab3_bst/concurrent_bst.cpp)、[lab2](../../labs/lab2_mining/mining.cpp) |
| 06 | [测试、调试与结课验收](06-testing-and-debugging.md) | 用工具、受控交错与不变量建立证据 | [全部实验](../../labs/README.md) |

## 建议的学习方式

每章先阅读，再运行已有测试；随后关掉参考实现，独立写一个最小版本，最后完成练习。章内短片段若标为“节选”，需要放回相应函数或类；带完整头文件与 main 的示例可以独立编译。

完整运行命令与诊断流程见 [第 06 章](06-testing-and-debugging.md)。实验是教学实现：队列的容量与异常约束、线程池的构造失败路径、BST 未实现并发删除等边界在对应章节明确说明。

完成六章后，用 [航班处理流水线结课项目](06-testing-and-debugging.md#7-性能报告与结课项目) 检验是否能把所有权、同步、算法、测试串起来。结课项目及扩展练习由学习者实现，现有实验不代表这些扩展已经完成。

## 历史实验观察

以下沿用仓库此前在 4 vCPU 容器、Release 构建下记录的数据，并非本次本机测量。仅用于说明现象，不作为性能承诺。

- **伪共享**（lab0）：4 个线程各自累加计数器。计数器挤在同一条 cache line 里时耗时 10.2 ms，按 64 字节对齐分开后 1.45 ms。
- **访存模式与并行**（lab1）：512×512 矩阵乘法，ijk 顺序 420 ms，ikj 顺序 48 ms，ikj 再用 4 线程并行 16 ms。
- **锁粒度**（lab3）：BST 插入加查询，一把全局 mutex 176 ms，`shared_mutex` 843 ms，hand-over-hand 2100 ms。临界区很短时，细粒度锁每一层都要多做一次加解锁，而且所有操作都必须先经过根节点的锁，所以反而最慢。这正是 15-418 强调的：**要先测量，再决定锁粒度**。
- **`std::jthread` 的析构陷阱**（lab2）：jthread 析构时会先调用 `request_stop()` 再 `join()`。并行搜索时如果依赖 vector 析构来 join，线程 0 会被提前叫停，漏掉最小的合法 nonce。编写实验时真实遇到过这个 bug。
- **NRVO 与 join 的顺序**（lab1）：必须确保工作线程先 join，再 `return` 结果，否则程序是否正确取决于编译器有没有做 NRVO 这个可选优化。


[返回课程总览](../README.md) · [返回项目首页](../../README.md)
