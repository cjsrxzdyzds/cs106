# 并发实验（labs）

这些实验把 AP1400-2 的 6 个作业扩展到多线程场景。代码使用 C++20，没有第三方依赖，只用到 [`common/check.h`](common/check.h) 里的 `CHECK` 宏。对应的文档见 [docs/concurrency/README.md](../docs/concurrency/README.md)。

| 实验 | 扩展自 | 内容 |
|---|---|---|
| [lab0_race](lab0_race/race.cpp) | — | 数据竞争（`racy` 模式）、mutex 与 atomic 的对比、伪共享 |
| [lab1_matmul](lab1_matmul/matmul.cpp) | HW1 | 连续存储、循环交换、分块、按行并行 |
| [lab2_mining](lab2_mining/mining.cpp) | HW2 | `shared_mutex` 账本、锁外计算、乐观并发控制（OCC）、用 CAS 求最小值、保证结果确定的并行搜索 |
| [lab3_bst](lab3_bst/concurrent_bst.cpp) | HW3 | 三种并发 BST：全局锁、读写锁、hand-over-hand 锁 |
| [lab4_smart_ptr](lab4_smart_ptr/) | HW4 | 只能移动的 `UniquePtr`；用控制块和原子计数实现的 `SharedPtr` |
| [lab5_thread_pool](lab5_thread_pool/) | HW5 | 线程池、`packaged_task`/`future`、异常传播、多态任务 |
| [lab6_queue](lab6_queue/) | HW6 | 有界阻塞队列：条件变量版本，以及对应 CS:APP sbuf 的信号量版本 |

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j && ctest --test-dir build
cmake -S . -B build-tsan -DLAB_SANITIZER=thread  && cmake --build build-tsan -j && ctest --test-dir build-tsan
cmake -S . -B build-asan -DLAB_SANITIZER=address && cmake --build build-asan -j && ctest --test-dir build-asan
```

已验证的环境：GCC 13 下 Release、TSan、ASan+UBSan 三种构建全部通过；Clang 18 下 Release 构建通过。
