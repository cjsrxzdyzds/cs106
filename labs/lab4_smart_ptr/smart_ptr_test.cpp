// Lab 4 测试：单线程语义 + 多线程并发拷贝/销毁
#include <atomic>
#include <iostream>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "../common/check.h"
#include "smart_ptr.h"

namespace {

std::atomic<int> g_alive{0};
std::atomic<int> g_destroyed{0};

struct Tracked {
    explicit Tracked(int v, std::string s = "") : value(v), name(std::move(s)) { ++g_alive; }
    ~Tracked() {
        --g_alive;
        ++g_destroyed;
    }
    int value;
    std::string name;
};

void test_unique() {
    static_assert(!std::is_copy_constructible_v<lab::UniquePtr<int>>);
    static_assert(!std::is_copy_assignable_v<lab::UniquePtr<int>>);
    static_assert(std::is_nothrow_move_constructible_v<lab::UniquePtr<int>>);

    {
        auto a = lab::make_unique<Tracked>(1, "a");
        CHECK_EQ(a->value, 1);
        CHECK_EQ((*a).name, std::string("a"));
        (*a).value = 10;  // operator* 返回引用：修改作用于原对象
        CHECK_EQ(a->value, 10);

        lab::UniquePtr<Tracked> b = std::move(a);
        CHECK(!a);
        CHECK(b);
        lab::UniquePtr<Tracked>& alias = b;
        b = std::move(alias);  // 自移动赋值不应释放对象（用别名绕开 -Wself-move）
        CHECK(b && b->value == 10);

        Tracked* raw = b.release();
        CHECK(!b);
        CHECK_EQ(g_alive.load(), 1);
        b.reset(raw);
        b.reset(new Tracked(2));
        CHECK_EQ(g_alive.load(), 1);
    }
    CHECK_EQ(g_alive.load(), 0);
}

void test_shared_single_thread() {
    {
        auto a = lab::make_shared<Tracked>(7);
        CHECK_EQ(a.use_count(), 1);
        {
            lab::SharedPtr<Tracked> b = a;
            lab::SharedPtr<Tracked> c;
            c = b;
            CHECK_EQ(a.use_count(), 3);
            lab::SharedPtr<Tracked>& c_alias = c;
            c = c_alias;  // 自赋值
            CHECK_EQ(a.use_count(), 3);
            c.reset();  // 与 HW4 不同：reset 只释放自己那一份
            CHECK_EQ(a.use_count(), 2);
            CHECK_EQ(b->value, 7);
        }
        CHECK_EQ(a.use_count(), 1);

        lab::SharedPtr<Tracked> d(new Tracked(8));
        a = std::move(d);  // 旧对象（7）引用归零被销毁
        CHECK(!d);
        CHECK_EQ(a->value, 8);
        CHECK_EQ(g_alive.load(), 1);
    }
    CHECK_EQ(g_alive.load(), 0);
}

// 多个线程从同一个“只读”的 SharedPtr 拷贝出自己的副本并反复销毁。
// 若计数不是原子的，要么对象被提前析构（use-after-free），要么泄漏/重复析构。
void test_shared_concurrent() {
    const int before = g_destroyed.load();
    constexpr int kThreads = 8, kIters = 20'000;
    {
        const auto root = lab::make_shared<Tracked>(42);
        std::vector<std::jthread> ts;
        for (int t = 0; t < kThreads; ++t)
            ts.emplace_back([&root] {
                std::vector<lab::SharedPtr<Tracked>> mine;
                for (int i = 0; i < kIters; ++i) {
                    mine.push_back(root);  // 并发拷贝同一个 root：只读 root，安全
                    CHECK_EQ(mine.back()->value, 42);
                    if (mine.size() > 16) mine.clear();
                }
            });
    }
    CHECK_EQ(g_destroyed.load() - before, 1);  // 恰好析构一次
    CHECK_EQ(g_alive.load(), 0);
}

// release/acquire 的作用：线程在放弃引用之前写入的数据，
// 必须对最后执行析构的线程可见。
struct Payload {
    std::vector<int> data;
    ~Payload() {
        long sum = 0;
        for (int x : data) sum += x;
        CHECK_EQ(sum, 8L * 1000);  // 所有线程的写入都可见
    }
};

void test_release_acquire() {
    auto root = lab::make_shared<Payload>();
    root->data.assign(8 * 1000, 0);
    std::vector<std::jthread> ts;
    for (int t = 0; t < 8; ++t)
        ts.emplace_back([p = root, t]() mutable {  // 每个线程持有自己的副本
            for (int i = 0; i < 1000; ++i) p->data[t * 1000 + i] = 1;  // 写不相交区间
            p.reset();  // 释放；可能是最后一个
        });
    root.reset();
}

}  // namespace

int main() {
    test_unique();
    test_shared_single_thread();
    test_shared_concurrent();
    test_release_acquire();
    std::cout << "lab4 OK\n";
}
