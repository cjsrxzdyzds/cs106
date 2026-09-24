// Lab 0: 数据竞争、原子操作与伪共享（false sharing）
//
// 用法：
//   ./lab0_race            运行所有“正确”的版本并做校验
//   ./lab0_race racy       额外运行一个故意有数据竞争的版本
//                          （用 -DLAB_SANITIZER=thread 构建时，TSan 会报告 data race）
//
// 对应文档：docs/concurrency/README.md

#include <array>
#include <atomic>
#include <cstring>
#include <iostream>
#include <mutex>
#include <new>
#include <thread>
#include <vector>

#include "../common/check.h"

namespace {

constexpr int kThreads = 4;
constexpr long kIters = 200'000;

// 1) 未同步的 ++：这是未定义行为（UB），不只是“结果可能偏小”。
long racy_counter() {
    long counter = 0;
    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t)
        ts.emplace_back([&] {
            for (long i = 0; i < kIters; ++i) ++counter;  // data race!
        });
    for (auto& th : ts) th.join();
    return counter;
}

// 2) 互斥锁：正确，但每次 ++ 都要经过锁的获取/释放。
long mutex_counter() {
    long counter = 0;
    std::mutex m;
    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t)
        ts.emplace_back([&] {
            for (long i = 0; i < kIters; ++i) {
                std::lock_guard<std::mutex> lk(m);
                ++counter;
            }
        });
    for (auto& th : ts) th.join();
    return counter;
}

// 3) 原子变量：计数器只需要原子性，不需要与其他内存操作建立顺序，relaxed 足够。
//    join() 本身建立了 happens-before，所以主线程读取时能看到最终值。
long atomic_counter() {
    std::atomic<long> counter{0};
    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t)
        ts.emplace_back([&] {
            for (long i = 0; i < kIters; ++i) counter.fetch_add(1, std::memory_order_relaxed);
        });
    for (auto& th : ts) th.join();
    return counter.load(std::memory_order_relaxed);
}

// 4) 每线程局部计数，最后归约：通常最快，这是 15-418 反复强调的思路——
//    先消除共享，再谈同步。
//    这里对比两种布局：计数器紧挨着（同一 cache line，伪共享）与按 cache line 对齐。
struct Packed {
    std::atomic<long> v{0};
};
struct alignas(64) Padded {  // 64 = 常见 x86/ARM cache line 大小
    std::atomic<long> v{0};
};

template <class Slot>
long per_thread_counter() {
    std::array<Slot, kThreads> slots{};
    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t)
        ts.emplace_back([&slots, t] {
            for (long i = 0; i < kIters; ++i) slots[t].v.fetch_add(1, std::memory_order_relaxed);
        });
    for (auto& th : ts) th.join();
    long sum = 0;
    for (auto& s : slots) sum += s.v.load(std::memory_order_relaxed);
    return sum;
}

template <class F>
void run(const char* name, F f) {
    lab::Timer timer;
    long got = f();
    double ms = timer.ms();
    std::cout << name << ": " << got << " (expected " << kThreads * kIters << "), " << ms
              << " ms\n";
    CHECK_EQ(got, kThreads * kIters);
}

}  // namespace

int main(int argc, char** argv) {
    static_assert(sizeof(Padded) == 64 && sizeof(Packed) == sizeof(long));

    if (argc > 1 && std::strcmp(argv[1], "racy") == 0) {
        long got = racy_counter();
        std::cout << "racy: " << got << " (expected " << kThreads * kIters
                  << ") -- 结果不可信，程序含 UB\n";
    }
    run("mutex        ", mutex_counter);
    run("atomic       ", atomic_counter);
    run("packed slots ", per_thread_counter<Packed>);
    run("padded slots ", per_thread_counter<Padded>);
    std::cout << "lab0 OK\n";
}
