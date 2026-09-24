// Lab 6 测试（HW6 扩展）：多生产者 / 多消费者
//
// 场景借用 HW6 q3：生产者解析航班记录，消费者把它们放进各自的 priority_queue，
// 最后合并得到全局最优航班。这里用合成数据代替 flights.txt。
#include <algorithm>
#include <atomic>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "../common/check.h"
#include "bounded_queue.h"

namespace {

struct Flight {
    std::string number;
    std::size_t duration, connection_times, price;
    std::size_t weight() const { return duration + connection_times + 3 * price; }
};

struct ByWeight {  // HW6 q3 的 Compare：权重小的优先
    bool operator()(const Flight& a, const Flight& b) const { return a.weight() > b.weight(); }
};

void test_bounded_queue() {
    constexpr int kProducers = 3, kConsumers = 4, kPerProducer = 5000;
    lab::BoundedQueue<Flight> q(64);

    std::vector<std::priority_queue<Flight, std::vector<Flight>, ByWeight>> local(kConsumers);
    std::atomic<long> consumed{0};
    {
        std::vector<std::jthread> consumers;
        for (int c = 0; c < kConsumers; ++c)
            consumers.emplace_back([&, c] {
                while (auto f = q.pop()) {  // nullopt → 队列已关闭且为空
                    local[c].push(std::move(*f));
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
            });

        {
            std::vector<std::jthread> producers;
            for (int p = 0; p < kProducers; ++p)
                producers.emplace_back([&, p] {
                    for (int i = 0; i < kPerProducer; ++i) {
                        std::size_t id = p * kPerProducer + i;
                        // 构造出唯一的最优航班：id == 4242 时权重为 0
                        std::size_t w = id == 4242 ? 0 : 1 + id % 997;
                        CHECK(q.push(Flight{"QR" + std::to_string(id), w, 0, 0}));
                    }
                });
        }  // 所有生产者 join
        q.close();
    }  // 所有消费者 join

    CHECK_EQ(consumed.load(), long{kProducers} * kPerProducer);
    // 合并各消费者的局部堆顶（归约）
    std::vector<Flight> tops;
    for (auto& pq : local)
        if (!pq.empty()) tops.push_back(pq.top());
    auto best = std::min_element(tops.begin(), tops.end(), [](auto& a, auto& b) {
        return a.weight() < b.weight();
    });
    CHECK(best != tops.end());
    CHECK_EQ(best->number, std::string("QR4242"));
    CHECK(!q.push(Flight{"late", 1, 1, 1}));  // 关闭后 push 失败
}

void test_semaphore_queue() {
    constexpr int kProducers = 4, kConsumers = 4, kPerProducer = 5000;
    lab::SemaphoreQueue<long, 16> q;
    std::atomic<long> sum{0};
    {
        std::vector<std::jthread> ts;
        for (int c = 0; c < kConsumers; ++c)
            ts.emplace_back([&] {
                // sbuf 没有 close()：每个消费者恰好消费 总数/消费者数 个
                for (int i = 0; i < kProducers * kPerProducer / kConsumers; ++i)
                    sum.fetch_add(q.pop(), std::memory_order_relaxed);
            });
        for (int p = 0; p < kProducers; ++p)
            ts.emplace_back([&, p] {
                for (int i = 1; i <= kPerProducer; ++i) q.push(long{p} * kPerProducer + i);
            });
    }
    const long n = long{kProducers} * kPerProducer;
    CHECK_EQ(sum.load(), n * (n + 1) / 2);
}

}  // namespace

int main() {
    test_bounded_queue();
    test_semaphore_queue();
    std::cout << "lab6 OK\n";
}
