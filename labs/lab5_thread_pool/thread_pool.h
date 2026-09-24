// Lab 5：固定大小线程池
//
// 关键点：
//   - 任务队列 = std::deque<std::function<void()>> + mutex + condition_variable
//   - submit() 用 std::packaged_task 把任意可调用对象包装成 std::future，
//     返回值与异常都通过 future 传回调用者
//   - std::packaged_task 只能移动，而 std::function 要求可拷贝，
//     所以用 shared_ptr 包一层（C++23 可改用 std::move_only_function）
//   - 析构：置 stopping_ 标志 → notify_all → jthread 自动 join；
//     已入队的任务会被执行完（drain），不会丢弃
//
// 对应文档：docs/README.md
#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace lab {

class ThreadPool {
public:
    explicit ThreadPool(std::size_t n = std::thread::hardware_concurrency()) {
        if (n == 0) n = 1;
        workers_.reserve(n);
        for (std::size_t i = 0; i < n; ++i) workers_.emplace_back([this] { worker_loop(); });
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ~ThreadPool() {
        {
            std::lock_guard lk(m_);
            stopping_ = true;
        }
        cv_.notify_all();
        // workers_ 中的 jthread 在成员析构时 join
    }

    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using R = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<R()>>(
            [f = std::forward<F>(f), ... args = std::forward<Args>(args)]() mutable {
                return std::invoke(std::move(f), std::move(args)...);
            });
        std::future<R> fut = task->get_future();
        {
            std::lock_guard lk(m_);
            if (stopping_) throw std::runtime_error("submit on stopped ThreadPool");
            tasks_.emplace_back([task] { (*task)(); });
        }
        cv_.notify_one();  // 在锁外通知：被唤醒的线程不必立刻再阻塞在 m_ 上
        return fut;
    }

    std::size_t size() const { return workers_.size(); }

private:
    void worker_loop() {
        while (true) {
            std::function<void()> job;
            {
                std::unique_lock lk(m_);
                // 带谓词的 wait 自动处理虚假唤醒（spurious wakeup）
                cv_.wait(lk, [this] { return stopping_ || !tasks_.empty(); });
                if (tasks_.empty()) return;  // stopping_ 且队列已空
                job = std::move(tasks_.front());
                tasks_.pop_front();
            }
            job();  // 在锁外执行任务；packaged_task 会捕获任务抛出的异常
        }
    }

    std::mutex m_;
    std::condition_variable cv_;
    std::deque<std::function<void()>> tasks_;
    bool stopping_ = false;
    // 最后声明：最先析构（join），保证 join 时上面的成员仍然有效
    std::vector<std::jthread> workers_;
};

}  // namespace lab
