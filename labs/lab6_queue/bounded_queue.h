// Lab 6：有界阻塞队列（生产者-消费者）的两种实现
//
//   BoundedQueue    mutex + 两个 condition_variable（not_full / not_empty）+ close()
//   SemaphoreQueue  C++20 std::counting_semaphore，逐行对应 CS:APP（CMU 15-213）
//                   第 12 章的 sbuf：slots / items 两个计数信号量 + 一个互斥量
//
// 对应文档：docs/README.md
#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <semaphore>
#include <vector>

namespace lab {

template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : cap_(capacity) {}

    // 队列满时阻塞；若队列已关闭返回 false
    bool push(T value) {
        std::unique_lock lk(m_);
        not_full_.wait(lk, [&] { return closed_ || q_.size() < cap_; });
        if (closed_) return false;
        q_.push_back(std::move(value));
        lk.unlock();
        not_empty_.notify_one();
        return true;
    }

    // 队列空时阻塞；队列关闭且为空时返回 nullopt（消费者据此退出）
    std::optional<T> pop() {
        std::unique_lock lk(m_);
        not_empty_.wait(lk, [&] { return closed_ || !q_.empty(); });
        if (q_.empty()) return std::nullopt;
        T v = std::move(q_.front());
        q_.pop_front();
        lk.unlock();
        not_full_.notify_one();
        return v;
    }

    // 关闭后：push 失败；pop 继续取完剩余元素后返回 nullopt
    void close() {
        {
            std::lock_guard lk(m_);
            closed_ = true;
        }
        not_full_.notify_all();
        not_empty_.notify_all();
    }

private:
    std::mutex m_;
    std::condition_variable not_full_, not_empty_;
    std::deque<T> q_;
    std::size_t cap_;
    bool closed_ = false;
};

// CS:APP sbuf 的 C++20 版本（环形缓冲区）。
//   sbuf_insert: P(slots); P(mutex); buf[rear++] = item; V(mutex); V(items);
//   sbuf_remove: P(items); P(mutex); item = buf[front++]; V(mutex); V(slots);
template <typename T, std::ptrdiff_t Capacity>
class SemaphoreQueue {
public:
    SemaphoreQueue() : buf_(Capacity) {}

    void push(T value) {
        slots_.acquire();  // P(slots)：等待空位
        {
            std::lock_guard lk(m_);
            buf_[rear_++ % Capacity] = std::move(value);
        }
        items_.release();  // V(items)：宣告多了一个元素
    }

    T pop() {
        items_.acquire();  // P(items)：等待元素
        T v;
        {
            std::lock_guard lk(m_);
            v = std::move(buf_[front_++ % Capacity]);
        }
        slots_.release();  // V(slots)
        return v;
    }

private:
    std::vector<T> buf_;
    std::size_t front_ = 0, rear_ = 0;
    std::mutex m_;
    std::counting_semaphore<Capacity> slots_{Capacity};
    std::counting_semaphore<Capacity> items_{0};
};

}  // namespace lab
