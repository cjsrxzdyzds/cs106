# 第二部分：多线程（大纲）

> 状态：本部分的 6 章理论正文**尚未完成**。可以运行的实验代码已经在 [`labs/`](../../labs/) 中写好，并通过了 ThreadSanitizer 验证，代码注释里包含主要知识点。
> 参考课程：CMU 15-213（CS:APP 第 12 章）、15-418/618、15-445/645；参考书：Anthony Williams, *C++ Concurrency in Action*（第 2 版）。

| 章 | 主题 | 要点 | CMU 参考 | 实验 |
|---|---|---|---|---|
| 01 | 线程与内存模型 | `std::thread`/`jthread`；数据竞争的定义与 UB；happens-before；SC-DRF | 15-213 第 12 章；15-418 Memory Consistency | [lab0](../../labs/lab0_race/race.cpp) |
| 02 | 互斥量、条件变量、信号量 | 锁保护的是不变量；check-then-act 竞态；死锁与加锁顺序；带谓词的 `wait`；sbuf | 15-213 第 12.5 节 | [lab6](../../labs/lab6_queue/)、[lab2](../../labs/lab2_mining/mining.cpp) |
| 03 | 原子操作与内存序 | relaxed / acquire-release / seq_cst；CAS 循环；引用计数的内存序；MESI 缓存一致性与伪共享 | 15-418 Cache Coherence、Synchronization | [lab0](../../labs/lab0_race/race.cpp)、[lab4](../../labs/lab4_smart_ptr/) |
| 04 | 任务、future 与线程池 | 任务分解与分配；`packaged_task`；异常通过 future 传播；关闭时的语义 | 15-418 Parallel Programming Basics | [lab1](../../labs/lab1_matmul/matmul.cpp)、[lab5](../../labs/lab5_thread_pool/) |
| 05 | 并发数据结构 | 粗粒度锁、读写锁、hand-over-hand 锁、乐观并发控制 | 15-445 Index Concurrency Control、OCC | [lab3](../../labs/lab3_bst/concurrent_bst.cpp)、[lab2](../../labs/lab2_mining/mining.cpp) |
| 06 | 并发程序的测试与调试 | TSan（向量时钟）、Eraser lockset 算法、Clang Thread Safety Analysis、模型检查 | — | 全部 |

## 实验中已经测到的现象（4 vCPU 容器，Release 构建，只看量级）

- **伪共享**（lab0）：4 个线程各自累加计数器。计数器挤在同一条 cache line 里时耗时 10.2 ms，按 64 字节对齐分开后 1.45 ms。
- **访存模式与并行**（lab1）：512×512 矩阵乘法，ijk 顺序 420 ms，ikj 顺序 48 ms，ikj 再用 4 线程并行 16 ms。
- **锁粒度**（lab3）：BST 插入加查询，一把全局 mutex 176 ms，`shared_mutex` 843 ms，hand-over-hand 2100 ms。临界区很短时，细粒度锁每一层都要多做一次加解锁，而且所有操作都必须先经过根节点的锁，所以反而最慢。这正是 15-418 强调的：**要先测量，再决定锁粒度**。
- **`std::jthread` 的析构陷阱**（lab2）：jthread 析构时会先调用 `request_stop()` 再 `join()`。并行搜索时如果依赖 vector 析构来 join，线程 0 会被提前叫停，漏掉最小的合法 nonce。编写实验时真实遇到过这个 bug。
- **NRVO 与 join 的顺序**（lab1）：必须确保工作线程先 join，再 `return` 结果，否则程序是否正确取决于编译器有没有做 NRVO 这个可选优化。
