# 多线程 03 · 原子操作、内存序与缓存

> 前置：[happens-before](01-threads-and-memory-model.md)、[智能指针](../cpp/07-raii-and-smart-pointers.md)
> 实验：[lab0](../../labs/lab0_race/race.cpp)、[lab2](../../labs/lab2_mining/mining.cpp)、[lab4](../../labs/lab4_smart_ptr/smart_ptr.h)
> 目标：区分原子性与发布；读懂 CAS；解释引用计数与伪共享。

## 1. 原子性不等于整个算法不可分割

`std::atomic<int> count{0}` 允许多个线程安全地操作同一个计数器。但 `count.store(count.load() + 1)` 仍是两个操作：两个线程可以读到相同旧值，再写入相同新值。这里没有普通内存的数据竞争，却会丢失更新。

应使用一次原子读—改—写 `count.fetch_add(1)` 或 `++count`。同理，“余额是 atomic”不保证“检查余额后扣款”作为整体正确，更不保证两个账户的转账具有原子性。多字段不变量通常先用锁解决。

原子类型也不承诺全部无锁。可通过 `is_lock_free()` 查询特定对象的实现，或查看 `is_always_lock_free`；不要从源代码没有 mutex 推出算法无锁，更不能从“无锁”推出更快。

## 2. 内存序决定哪些其他访问也被排序

| 内存序 | 适用操作 | 在本课程中的用途 |
|---|---|---|
| `relaxed` | load、store、RMW | 独立统计、最小值，只要求原子性 |
| `release` | store、RMW | 发布此前完成的初始化或写入 |
| `acquire` | load、RMW | 接收与之配对的发布 |
| `acq_rel` | RMW | 同时接收之前的状态并发布自己的状态 |
| `seq_cst` | load、store、RMW，默认值 | 需要顺序一致推理的起点 |

每个原子对象的修改有各自的 modification order。`relaxed` 不取消这个顺序，但不会自动把另一个变量的写入发布给读取者。`seq_cst` 还对这些顺序一致操作建立统一的全序；混用弱内存序时，不能把它误解成所有内存访问都被全局串行化。[标准草案：内存序](https://eel.is/c++draft/atomics.order)

先写出需要的同步关系，再选择内存序。不要仅凭某台 x86 或 ARM 机器的运行结果推断语言保证。本课程不使用 `consume`，避免依赖依赖序与实现差异。

## 3. 一次发布的完整示例

```cpp
#include <atomic>
#include <cassert>
#include <thread>

int main() {
    int payload = 0;
    std::atomic<bool> ready{false};
    std::jthread producer([&] {
        payload = 42;
        ready.store(true, std::memory_order_release);
    });
    std::jthread consumer([&] {
        while (!ready.load(std::memory_order_acquire))
            std::this_thread::yield();
        assert(payload == 42);
    });
    producer.join();
    consumer.join();
}
```

消费者读到该 release 写出的 true，连接起发布前的 `payload = 42` 与消费后的读取。payload 只写一次，且发布后不再修改，因此不需要是 atomic。若把 ready 的两个操作都换成 relaxed，普通 payload 的读写就缺少同步，程序有数据竞争。

这是一次性协议。把 ready 清回 false 并重复写 payload，并不会自动形成安全的可复用信道；生产者可能在消费者读取时开始下一轮写入，需要确认消费完成的第二个协议。本例的自旋只是展示发布，长时间等待应使用阻塞机制，且不应声称 `yield()` 保证调度公平或有界完成。

## 4. CAS：只有观察值仍成立才更新

CAS（compare-and-exchange）比较原子对象与 expected：相等则写入 desired 并返回 true；失败则把观察到的值写回 expected。`compare_exchange_weak` 允许伪失败，所以通常放在循环里。

lab2 求原子最小值的核心（函数内节选）：

```cpp
std::size_t current = best.load(std::memory_order_relaxed);
while (candidate < current &&
       !best.compare_exchange_weak(current, candidate,
                                   std::memory_order_relaxed)) {
    // 失败后 current 已更新，重新检查 candidate 是否仍值得提交。
}
```

假设 best=100，A 想写 40、B 想写 30。A 成功后，B 用 expected=100 比较失败，expected 更新为 40；下一轮 B 仍可尝试写 30。反过来 B 先成功，A 失败后看到 30 就退出，不能把更好的结果覆盖成 40。

这里 best 就是完整共享结果，不承担发布其他数据的任务，最终读取又发生在 join 之后，所以 relaxed 足够。若另有“最佳结果对应的字符串”，不能先 CAS 更新 best 再无保护写字符串；数字与附属数据需要统一发布或一起加锁。

CAS 的双内存序重载分别指定成功与失败的顺序。失败只是读取，不能指定 release 或 acq_rel；入门时使用明确合法的组合，如成功 acq_rel、失败 acquire，或像本例两者均 relaxed。

CAS 不解决指针的 ABA 与回收问题：地址从 A 变成 B 又被复用为 A，比较相等也不代表对象还是原来的那个。第 05 章说明为什么不能直接把 BST 的指针换成 atomic 就实现安全删除。

## 5. lab4：引用计数的内存序推理

阅读 `ControlBlock::add_ref()` 和 `release()`：

- 拷贝已有的有效拥有者时执行 relaxed 自增。已有引用保证对象还活着；新指针传给另一个线程仍须通过安全的发布方式。
- 每次释放使用 acq_rel 自减。返回旧值为 1 的线程负责析构；在正确的所有权协议下，最终释放通过原子 RMW 链接收先前释放所发布的操作。
- `use_count()` 用 relaxed 读取，仅是可能立刻过期的观察值。看到 1 不能据此取得修改对象的独占权。

这一协议管理的是生命周期，不能修复两个拥有者对 T 的无同步并发写入。还有三层不同对象：共享控制块、各个 SharedPtr 句柄、被管理的 T。控制块计数安全，不代表同一个句柄可并发 reset 与复制，更不代表 T 自动安全。

教学 `lab::SharedPtr` 不支持 weak_ptr、别名构造、完整删除器与分配器协议。C++20 的 `std::atomic<std::shared_ptr<T>>` 针对标准 shared_ptr，不能直接套在自定义 `lab::SharedPtr` 上。要扩展弱引用，必须把对象销毁与控制块回收分开，且只允许在强计数仍非零时通过 CAS 增加强引用。

## 6. MESI、内存序与伪共享

MESI 用 Modified、Exclusive、Shared、Invalid 描述一种缓存一致性协议的缓存行状态。它帮助理解多核如何维护同一位置的缓存副本，但具体处理器可能使用变体；缓存一致性也不能代替 C++ 跨对象的同步规则。

lab0 的 Packed 计数器相邻存放。即使每个线程只改自己的计数器，若它们落在同一缓存行上，写入仍会触发行所有权转移。不同变量没有逻辑共享，却争用同一行，这就是伪共享。

`alignas(64)` 是实验参数，不是所有机器通用的缓存行大小保证。在本机应确认硬件特征，必要时参考实现提供的 `std::hardware_destructive_interference_size`；该常量也不应未经考虑就进入跨平台稳定 ABI。

该实验故意保留每槽位原子操作以观察布局影响。若目标仅是求和，更好的算法往往是线程内普通局部变量累计，退出时写一次独占结果槽，join 后归约。这样连高频原子写都消除了。

## 实验与练习

```bash
./labs/build/lab0_race
./labs/build/lab4_smart_ptr
./labs/build/lab2_mining
```

1. 把原子自增改成 load + store，解释即使 TSan 不报告也可能少计数。**答案要点**：单步原子，组合操作非原子。
2. 为什么一次发布示例中 payload 可以是普通 int？**答案要点**：release/acquire 连接了冲突访问，且发布后不再写。
3. 将 lab0 增加“局部累计后写一次”的版本，与 Packed/Padded 比较。**验收**：总数相同；记录构建模式和硬件；不要求某种布局必然胜出。
4. 为什么 `use_count() == 1` 不能取代 mutex？**答案要点**：计数不是独占访问许可，其他共享状态及并发取得引用的协议未受该检查保护。

[上一章](02-mutexes-condition-variables-semaphores.md) · [目录](README.md) · [下一章：任务与线程池](04-tasks-futures-thread-pools.md)
