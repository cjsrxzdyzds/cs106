// Lab 4（HW4 扩展）：正确且线程安全的 UniquePtr / SharedPtr
//
// HW4 原实现的主要问题（详见 docs/README.md）：
//   - UniquePtr 的拷贝构造里 static_assert(true, ...) 什么也不阻止；
//     拷贝赋值缺少 return（UB），而且会导致两个 UniquePtr 管同一个指针 → double free
//   - SharedPtr 用 static std::map<T*, int> 计数：非线程安全、每次操作 O(log n)、
//     reset() 直接把计数清零会让其他持有者悬空
//   - make_unique / make_shared 返回裸指针，失去了“异常安全的单点构造”的意义
//
// 这里的实现：
//   - UniquePtr：删除拷贝，提供移动（Rule of Five 中的“move-only”形态）
//   - SharedPtr：每个被管理对象一个控制块（control block），
//     引用计数为 std::atomic，make_shared 把控制块和对象放在同一次分配里
//
// 线程安全语义与 std::shared_ptr 相同：
//   * 不同的 SharedPtr 对象（即使指向同一对象）可以被不同线程同时拷贝/销毁；
//   * 同一个 SharedPtr 对象被多个线程同时读写（如一个线程 reset、另一个拷贝）仍是数据竞争，
//     需要外部加锁或 C++20 的 std::atomic<std::shared_ptr<T>>。
#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace lab {

// ======================= UniquePtr =======================

template <typename T>
class UniquePtr {
public:
    UniquePtr() noexcept = default;
    explicit UniquePtr(T* p) noexcept : p_(p) {}

    UniquePtr(const UniquePtr&) = delete;             // 独占所有权：禁止拷贝
    UniquePtr& operator=(const UniquePtr&) = delete;

    UniquePtr(UniquePtr&& other) noexcept : p_(other.release()) {}
    UniquePtr& operator=(UniquePtr&& other) noexcept {
        reset(other.release());  // 自移动赋值也安全：release 先把指针取走
        return *this;
    }

    ~UniquePtr() { delete p_; }

    T* get() const noexcept { return p_; }
    T& operator*() const noexcept { return *p_; }  // 返回引用，而不是像 HW4 那样返回拷贝
    T* operator->() const noexcept { return p_; }
    explicit operator bool() const noexcept { return p_ != nullptr; }

    T* release() noexcept { return std::exchange(p_, nullptr); }
    void reset(T* p = nullptr) noexcept {
        T* old = std::exchange(p_, p);  // 先换再删：即使 ~T 间接访问 *this 也看到一致状态
        delete old;
    }

private:
    T* p_ = nullptr;
};

template <typename T, typename... Args>
UniquePtr<T> make_unique(Args&&... args) {
    return UniquePtr<T>(new T(std::forward<Args>(args)...));  // 完美转发构造参数
}

// ======================= SharedPtr =======================

namespace detail {

class ControlBlock {
public:
    void add_ref() noexcept {
        // 增加计数不需要与任何其他内存操作排序：能执行拷贝的线程
        // 手里已经有一个有效引用，对象不可能在此期间被销毁。
        strong_.fetch_add(1, std::memory_order_relaxed);
    }

    void release() noexcept {
        // 减计数需要 acq_rel：
        //   release —— 本线程之前对对象的所有写入，要在“计数减到 0 的那个线程”
        //              销毁对象之前可见；
        //   acquire —— 最后一个线程要“看到”其他线程的这些写入后再调用析构。
        if (strong_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            dispose();
            delete this;
        }
    }

    long use_count() const noexcept { return strong_.load(std::memory_order_relaxed); }

protected:
    virtual ~ControlBlock() = default;
    virtual void dispose() noexcept = 0;  // 销毁被管理的对象

private:
    std::atomic<long> strong_{1};
};

// SharedPtr<T>(new T) 的情形：对象与控制块分两次分配
template <typename T>
class PointerBlock final : public ControlBlock {
public:
    explicit PointerBlock(T* p) : p_(p) {}

private:
    void dispose() noexcept override { delete p_; }
    T* p_;
};

// make_shared 的情形：对象直接嵌在控制块里，一次分配、局部性更好
template <typename T>
class InplaceBlock final : public ControlBlock {
public:
    template <typename... Args>
    explicit InplaceBlock(Args&&... args) {
        ::new (static_cast<void*>(&storage_)) T(std::forward<Args>(args)...);
    }
    T* get() noexcept { return std::launder(reinterpret_cast<T*>(&storage_)); }

private:
    void dispose() noexcept override { get()->~T(); }
    alignas(T) std::byte storage_[sizeof(T)];
};

}  // namespace detail

template <typename T>
class SharedPtr {
public:
    SharedPtr() noexcept = default;

    explicit SharedPtr(T* p) : p_(p) {
        if (!p) return;
        try {
            cb_ = new detail::PointerBlock<T>(p);
        } catch (...) {
            delete p;  // 分配控制块失败时不能泄漏 p（std::shared_ptr 同样如此）
            throw;
        }
    }

    SharedPtr(const SharedPtr& o) noexcept : p_(o.p_), cb_(o.cb_) {
        if (cb_) cb_->add_ref();
    }
    SharedPtr(SharedPtr&& o) noexcept
        : p_(std::exchange(o.p_, nullptr)), cb_(std::exchange(o.cb_, nullptr)) {}

    // copy-and-swap：按值接收参数，同时处理拷贝赋值、移动赋值与自赋值，
    // 且天然异常安全（强保证）
    SharedPtr& operator=(SharedPtr o) noexcept {
        swap(o);
        return *this;
    }

    ~SharedPtr() {
        if (cb_) cb_->release();
    }

    void swap(SharedPtr& o) noexcept {
        std::swap(p_, o.p_);
        std::swap(cb_, o.cb_);
    }

    void reset() noexcept { SharedPtr().swap(*this); }
    void reset(T* p) { SharedPtr(p).swap(*this); }  // 只释放“自己那一份”引用

    T* get() const noexcept { return p_; }
    T& operator*() const noexcept { return *p_; }
    T* operator->() const noexcept { return p_; }
    explicit operator bool() const noexcept { return p_ != nullptr; }
    long use_count() const noexcept { return cb_ ? cb_->use_count() : 0; }

private:
    template <typename U, typename... Args>
    friend SharedPtr<U> make_shared(Args&&... args);

    T* p_ = nullptr;
    detail::ControlBlock* cb_ = nullptr;
};

template <typename T, typename... Args>
SharedPtr<T> make_shared(Args&&... args) {
    auto* cb = new detail::InplaceBlock<T>(std::forward<Args>(args)...);
    SharedPtr<T> sp;
    sp.p_ = cb->get();
    sp.cb_ = cb;
    return sp;
}

}  // namespace lab
