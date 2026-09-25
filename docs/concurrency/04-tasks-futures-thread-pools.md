# 多线程 04 · 任务、future、线程池与并行分解

> 前置：[lambda 与模板](../cpp/04-functions-lambdas-templates.md)、[互斥与等待](02-mutexes-condition-variables-semaphores.md)
> 实验：[lab1 矩阵乘法](../../labs/lab1_matmul/matmul.cpp)、[lab5 线程池](../../labs/lab5_thread_pool/thread_pool.h)
> 目标：将计算表达为任务；通过 future 传递结果与异常；说明线程池的结束协议和性能边界。

## 1. 从“创建线程”转向“提交工作”

一个任务描述要计算什么，一个线程提供执行任务的资源。每杯咖啡创建一个线程会把小计算变成大量线程创建和回收；固定线程池让同一批线程重复处理任务。

任务的输入所有权必须清楚。lab5 把 Drink 深拷贝后移动进 lambda，每个任务拥有自己的饮品，不必给配方加锁。若捕获外部对象引用，则该对象必须活到任务结束，且并发访问仍需满足同步协议。`future` 的存在不会延长任意引用的生命周期。

## 2. future 是结果通道

| 工具 | 谁执行计算 | 结果如何进入共享状态 |
|---|---|---|
| `promise<T>` + `future<T>` | 调用者安排 | 手动 set_value 或 set_exception |
| `packaged_task<R()>` | 调用它的线程 | 调用时自动保存返回值或异常 |
| `async` | 由启动策略决定 | 库运行可调用对象并保存结果 |

普通 future 只允许一次 `get()`，它可能阻塞，并会重新抛出任务保存的异常。仅 `wait()` 不会取出结果或重新抛异常。多个读者需要 `shared_future`，但它仍不替结果所指向的可变对象提供同步。

以下是可独立编译的示例：

```cpp
#include <cassert>
#include <future>
#include <thread>
#include <utility>

int main() {
    std::packaged_task<int()> task([] { return 6 * 7; });
    auto result = task.get_future();
    std::jthread worker(std::move(task));
    assert(result.get() == 42);
    worker.join();
}
```

`packaged_task` 本身不会新建线程。如果构造后一直不调用它，结果就不会凭空出现；若提供者在未提供结果时放弃共享状态，消费者会收到 broken_promise。

`std::async(f)` 的默认策略可以选择延迟执行，直到等待时才在等待者线程中调用 f。需要明确异步执行时使用 `std::launch::async`。来自 async 异步策略的 future 在释放最后一个相关共享状态引用时可能等待任务；连续创建后立刻丢弃临时 future，可能让本想并行的调用串行化。不要把该析构等待规则推广到所有 promise/packaged_task future。[标准草案：async](https://eel.is/c++draft/futures.async)

## 3. 沿 lab5 走一遍任务的路径

`submit()` 把可调用对象与实参按值捕获进一个可移动闭包，再构造 packaged_task，先取得 future，最后把执行入口放进队列。调用者拿到 future 时，任务可能尚未运行，也可能已完成。

为什么再套一层 shared_ptr？C++20 的 `std::function<void()>` 要求存储的目标可复制，而 packaged_task 只能移动。捕获 shared_ptr 的小闭包可以复制，队列仍只取出并执行每个任务一次。这里 shared_ptr 用于可调用对象的包装，不意味着让多个线程同时调用同一个 packaged_task。

worker 的循环分成三段：

1. 持锁等待 `stopping_ || !tasks_.empty()`。
2. 若有任务，把队首移动到局部变量，删除队首，释放锁。
3. 在锁外执行局部任务，再回到等待状态。

队列的出队与空判断在同一临界区内，两个 worker 不能取走同一项。任务必须在锁外运行，否则所有任务串行，且任务若再次 submit 可能死锁。任务抛出的异常由 packaged_task 保存，调用者通过 `get()` 接收；线程入口直接抛异常则没有这种保护。

这个 submit 按值保存并在执行时移动实参，适合一次性任务。需要传引用时可显式使用 `std::ref`；不应把它当作能保持所有可调用对象引用限定行为的通用执行框架。

## 4. 关闭语义必须可写成状态机

lab5 采用 drain，即处理完所有已入队任务：

```text
运行中 ── 析构置 stopping_，notify_all ── 排空队列 ── worker 退出 ── join 完成
```

当 stopping 为 true 但队列不空，worker 继续执行；只有停止且空才退出。`workers_` 最后声明，成员逆序析构时它最先 join，此时 mutex、条件变量、队列仍然存在。这里 worker 不使用 stop_token，退出由 stopping 和队列状态决定。

“取消待执行任务”是另一种语义，不能偷偷替换 drain。若丢弃 packaged_task，应让相应 future 得到明确失败；正在执行的任务仍须协作停止。jthread 无法强行中断任意业务代码。

### 当前教学实现的适用边界

正常构造成功后，测试覆盖了结果、异常和 drain，但以下场景没有完整支持：

- **部分构造失败**：若创建第 k 个线程抛异常，之前的 worker 可能还在条件变量上等待；此时 ThreadPool 析构函数体不会执行，成员 jthread 的停止请求也不会唤醒普通 wait。生产级版本应在构造循环的 catch 中置停止、通知、等待已创建线程，再重新抛异常。
- **池内等待池内任务**：单 worker 正执行 A，A 又提交 B 并 `get()`，B 没有可用 worker，就会死锁。多个 worker 全被同类等待占住也一样。
- **从 worker 内销毁自己的池**：可能尝试 join 当前线程，不能这样管理池的所有权。
- **析构期间外部继续 submit**：内部停止标志不是对象生命周期管理方案。调用方应先停止并等待提交者，再销毁池。
- **无限排队与长任务**：队列无容量上限；任务永不结束，析构也无法结束。需要背压、超时或取消时须扩展接口。

这些限制应成为接口契约与后续练习，不能用一次成功测试替代论证。

## 5. lab1：先优化串行，再并行分解

矩阵乘法 C=A×B 中，ijk 最内层沿 B 的列访问；行主序下步长大。ikj 内层连续访问 B 与 C 的行，改善局部性。分块让小块数据更有机会留在缓存中，但块大小需测量。

按行分解时，线程 t 获得一个不重叠区间，只写 C 的这些行；A、B 只读。没有共享累加器，也无需在最内层加锁。调用实验程序时线程数必须大于 0；当前命令行解析没有拒绝 0，不能传入该值。

静态切分适合各行工作量接近的矩阵计算。任务耗时差异大时，队列分配能缓解负载不均，但任务太小会让排队成本超过计算成本。`hardware_concurrency()` 只是提示且可能为 0，也不等于运行时实际可用 CPU 配额。

设串行比例为 s，理想 p 核加速上限为 `1 / (s + (1-s)/p)`。s=0.1、p=4 时约 3.08，核数无限时也至多 10。实际还受带宽、缓存、调度和锁竞争影响。

比较线程扩展性时，应拿并行 ikj 对比单线程 ikj；拿较慢的 ijk 作基线会混合“算法局部性收益”和“并行收益”。同时保留数值校验，浮点归约顺序变化时应使用合理误差界，而不是盲目要求逐位相等。

## 实验与练习

```bash
./labs/build/lab5_thread_pool
./labs/build/lab1_matmul 192 1
./labs/build/lab1_matmul 192 2
./labs/build/lab1_matmul 192 4
```

1. 将 lab5 的异常测试改为只 wait，不 get，能捕获任务异常吗？**思路**：不能；必须读取结果才重新抛出保存的异常。
2. 解释为何删除线程池 worker 中的锁外执行区会削弱并行度。**思路**：所有任务都争用同一队列锁，任意时刻只有一个执行。
3. 设计创建线程失败的可注入测试。**验收**：失败后已启动 worker 都退出，构造抛异常，测试在超时前结束；不要靠耗尽系统线程资源触发失败。
4. 记录相同矩阵的 1/2/4 线程时间，计算 `S(p)=T(1)/T(p)` 和 `E(p)=S(p)/p`。**验收**：每组至少重复 5 次并报告中位数，说明测量是否包含线程启动。

[上一章](03-atomics-and-memory-order.md) · [目录](README.md) · [下一章：并发数据结构](05-concurrent-data-structures.md)
