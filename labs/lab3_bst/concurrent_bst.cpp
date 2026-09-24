// Lab 3（HW3 扩展）：并发二叉搜索树的三种加锁粒度
//
//   CoarseBST  一把 std::mutex 保护整棵树          —— 最简单，读写完全串行
//   RwBST      一把 std::shared_mutex              —— 查询可以并发，插入仍串行
//   HohBST     每个节点一把锁，hand-over-hand 加锁  —— 不同子树上的操作可以并发
//             （lock coupling / latch crabbing，CMU 15-445 B+Tree 并发控制的核心技巧）
//
// 与 HW3 的差异：节点所有权用 std::unique_ptr 表达（RAII，不再手写 delete），
// 因此不存在 HW3 里拷贝赋值、移动赋值、后置 ++ 的内存泄漏问题。
//
// 对应文档：docs/README.md, docs/README.md

#include <algorithm>
#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <random>
#include <shared_mutex>
#include <thread>
#include <vector>

#include "../common/check.h"

namespace {

// ---------- 单线程核心：被 Coarse/Rw 两个版本复用 ----------
class SeqBST {
public:
    bool insert(int key) {
        std::unique_ptr<Node>* slot = &root_;  // 指向“应当挂新节点的那个指针”
        while (*slot) {
            if (key == (*slot)->key) return false;
            slot = key < (*slot)->key ? &(*slot)->left : &(*slot)->right;
        }
        *slot = std::make_unique<Node>(key);
        ++size_;
        return true;
    }
    bool contains(int key) const {
        const Node* cur = root_.get();
        while (cur) {
            if (key == cur->key) return true;
            cur = key < cur->key ? cur->left.get() : cur->right.get();
        }
        return false;
    }
    std::size_t size() const { return size_; }

private:
    struct Node {
        explicit Node(int k) : key(k) {}
        int key;
        std::unique_ptr<Node> left, right;
    };
    std::unique_ptr<Node> root_;
    std::size_t size_ = 0;
};

class CoarseBST {
public:
    bool insert(int k) {
        std::lock_guard lk(m_);
        return t_.insert(k);
    }
    bool contains(int k) const {
        std::lock_guard lk(m_);
        return t_.contains(k);
    }
    std::size_t size() const {
        std::lock_guard lk(m_);
        return t_.size();
    }

private:
    mutable std::mutex m_;
    SeqBST t_;
};

class RwBST {
public:
    bool insert(int k) {
        std::unique_lock lk(m_);
        return t_.insert(k);
    }
    bool contains(int k) const {
        std::shared_lock lk(m_);
        return t_.contains(k);
    }
    std::size_t size() const {
        std::shared_lock lk(m_);
        return t_.size();
    }

private:
    mutable std::shared_mutex m_;
    SeqBST t_;
};

// ---------- Hand-over-hand（lock coupling） ----------
//
// 规则：先锁住孩子，再释放父亲。任何时刻一个操作至多持有两把锁，
// 且所有线程都按“自顶向下”的同一顺序加锁 —— 这就排除了死锁（没有环形等待）。
//
// 不变量：slot（某个父节点里的 left/right，或 root_）只在持有其所属锁时读写。
class HohBST {
public:
    bool insert(int key) {
        std::unique_lock<std::mutex> parent(root_m_);  // root_ 由 root_m_ 保护
        std::unique_ptr<Node>* slot = &root_;
        while (true) {
            if (!*slot) {
                *slot = std::make_unique<Node>(key);  // 持有 parent 锁，写入安全
                size_.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
            Node* cur = slot->get();
            std::unique_lock<std::mutex> child(cur->m);  // 1. 锁孩子
            parent.unlock();                              // 2. 放父亲
            if (key == cur->key) return false;
            slot = key < cur->key ? &cur->left : &cur->right;
            parent = std::move(child);  // 3. 孩子变成新的父亲
        }
    }

    bool contains(int key) const {
        std::unique_lock<std::mutex> parent(root_m_);
        const Node* cur = root_.get();
        while (cur) {
            std::unique_lock<std::mutex> child(cur->m);
            parent.unlock();
            if (key == cur->key) return true;
            cur = key < cur->key ? cur->left.get() : cur->right.get();
            parent = std::move(child);
        }
        return false;
    }

    std::size_t size() const { return size_.load(std::memory_order_relaxed); }

    // 删除（尤其是两个孩子的情况需要找后继并改写两处指针）在 lock coupling 下
    // 要同时持有 “被删节点的父亲 + 被删节点 + 后继路径”，留作练习，见文档。

private:
    struct Node {
        explicit Node(int k) : key(k) {}
        int key;
        std::unique_ptr<Node> left, right;
        mutable std::mutex m;
    };
    mutable std::mutex root_m_;
    std::unique_ptr<Node> root_;
    std::atomic<std::size_t> size_{0};
};

// ---------- 测试与简单基准 ----------

template <class Tree>
void stress(const char* name, unsigned threads) {
    constexpr int kKeysPerThread = 20'000;
    std::vector<int> keys(kKeysPerThread * threads);
    std::iota(keys.begin(), keys.end(), 0);
    std::shuffle(keys.begin(), keys.end(), std::mt19937(42));  // 随机顺序 → 期望树高 O(log n)

    Tree tree;
    std::atomic<int> inserted{0};
    lab::Timer timer;
    {
        std::vector<std::jthread> ts;
        for (unsigned t = 0; t < threads; ++t)
            ts.emplace_back([&, t] {
                // 每个线程插入自己的一段，同时穿插大量查询（读多写少）
                for (int i = 0; i < kKeysPerThread; ++i) {
                    int k = keys[t * kKeysPerThread + i];
                    if (tree.insert(k)) inserted.fetch_add(1, std::memory_order_relaxed);
                    CHECK(!tree.insert(k));  // 重复插入必须失败
                    for (int r = 0; r < 4; ++r) CHECK(tree.contains(k));
                }
            });
    }
    CHECK_EQ(inserted.load(), static_cast<int>(keys.size()));
    CHECK_EQ(tree.size(), keys.size());
    for (int k : keys) CHECK(tree.contains(k));
    CHECK(!tree.contains(-1));
    std::cout << name << timer.ms() << " ms\n";
}

}  // namespace

int main() {
    const unsigned threads = std::max(2u, std::thread::hardware_concurrency());
    std::cout << "threads = " << threads << "\n";
    stress<CoarseBST>("coarse mutex     : ", threads);
    stress<RwBST>("shared_mutex     : ", threads);
    stress<HohBST>("hand-over-hand   : ", threads);
    std::cout << "lab3 OK\n";
}
