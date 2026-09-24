// Lab 2（HW2 扩展）：线程安全的中心化账本 + 并行挖矿
//
// HW2 的 Server 是单线程的：clients 与全局 pending_trxs 没有任何保护。
// 本实验把它改造成可被多个线程同时调用的版本，涉及：
//   - std::shared_mutex：读多写少（查询余额）用共享锁，修改用独占锁
//   - 不在持锁状态下做长时间计算（挖矿搜索在锁外进行）
//   - 乐观并发控制（OCC，参考 CMU 15-445）：提交时检查 epoch 是否变化
//   - 并行搜索 + std::atomic 的 CAS 循环求“最小合法 nonce”，
//     让并行结果与串行结果一致（确定性）
//   - std::jthread / std::stop_token 协作式取消
//
// 为了不依赖 OpenSSL，这里用 FNV-1a + splitmix64 代替 sha256，签名校验也省略了；
// 这些与并发结构无关。
//
// 对应文档：docs/README.md

#include <atomic>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <random>
#include <regex>
#include <shared_mutex>
#include <sstream>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "../common/check.h"

namespace {

// ---------- 哈希与“工作量证明” ----------

std::uint64_t mix64(std::uint64_t x) {  // splitmix64 finalizer
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

std::string toy_hash(const std::string& s) {
    std::uint64_t h = 1469598103934665603ULL;  // FNV-1a 64
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    std::ostringstream os;
    os << std::hex << std::setw(16) << std::setfill('0') << mix64(h);
    return os.str();
}

// HW2 规则：哈希的前 10 个字符中出现连续 3 个 '0'
bool is_valid_proof(const std::string& digest) {
    return digest.substr(0, 10).find("000") != std::string::npos;
}

constexpr std::size_t kNoNonce = std::numeric_limits<std::size_t>::max();

std::size_t sequential_find_nonce(const std::string& mempool, std::size_t limit) {
    for (std::size_t nonce = 0; nonce < limit; ++nonce)
        if (is_valid_proof(toy_hash(mempool + std::to_string(nonce)))) return nonce;
    return kNoNonce;
}

// 线程 t 负责 nonce ≡ t (mod threads)。
// best 记录目前找到的最小合法 nonce；每个线程一旦越过 best 就可以停止，
// 因为它之后只会找到更大的 nonce。
std::size_t parallel_find_nonce(const std::string& mempool, unsigned threads, std::size_t limit) {
    std::atomic<std::size_t> best{kNoNonce};
    {
        std::vector<std::jthread> workers;
        for (unsigned t = 0; t < threads; ++t) {
            workers.emplace_back([&, t](std::stop_token st) {
                for (std::size_t nonce = t; nonce < limit && !st.stop_requested();
                     nonce += threads) {
                    if (nonce > best.load(std::memory_order_relaxed)) return;
                    if (!is_valid_proof(toy_hash(mempool + std::to_string(nonce)))) continue;
                    // “原子 min”：CAS 循环。compare_exchange 失败时 cur 会被更新为最新值。
                    std::size_t cur = best.load(std::memory_order_relaxed);
                    while (nonce < cur &&
                           !best.compare_exchange_weak(cur, nonce, std::memory_order_relaxed)) {
                    }
                    return;
                }
            });
        }
        // 必须显式 join！jthread 的析构函数是 “request_stop(); join();”。
        // 若依赖 vector 析构，workers[0] 析构时会让线程 0 立即停止搜索，
        // 它负责的更小的合法 nonce 就可能被漏掉 —— 编写本实验时真实踩到过这个坑。
        // stop_token 应当只用于“确实想取消”的场景（例如超时或调用方放弃）。
        for (auto& w : workers) w.join();
    }
    // relaxed 在这里足够：best 是唯一被共享的数据，join 提供了 happens-before。
    return best.load(std::memory_order_relaxed);
}

// ---------- 账本 ----------

struct Trx {
    std::string sender, receiver;
    double value = 0;
    std::string str() const {
        std::ostringstream os;
        os << sender << '-' << receiver << '-' << value;
        return os.str();
    }
};

// 与 HW2 的 Server::parse_trx 相同的格式 "ali-hamed-1.5"
std::optional<Trx> parse_trx(const std::string& s) {
    static const std::regex pattern(R"(([a-zA-Z]+)-([a-zA-Z]+)-(\d+(?:\.\d+)?))");
    std::smatch m;
    if (!std::regex_match(s, m, pattern)) return std::nullopt;
    return Trx{m[1], m[2], std::stod(m[3])};
}

class Ledger {
public:
    static constexpr double kInitialCoins = 5;
    static constexpr double kReward = 6.25;

    bool add_client(const std::string& id) {
        std::unique_lock lk(m_);
        return wallets_.emplace(id, kInitialCoins).second;
    }

    double wallet(const std::string& id) const {
        std::shared_lock lk(m_);  // 多个读者可以并发
        auto it = wallets_.find(id);
        return it == wallets_.end() ? 0 : it->second;
    }

    double total_coins() const {
        std::shared_lock lk(m_);
        double sum = 0;
        for (auto& [id, w] : wallets_) sum += w;
        return sum;
    }

    // “检查余额”与“加入 pending”必须在同一个临界区内完成，
    // 否则两个线程可能都通过检查，然后一起把余额花成负数（check-then-act 竞态）。
    bool add_pending_trx(const std::string& trx_str) {
        auto trx = parse_trx(trx_str);
        if (!trx || trx->value <= 0 || trx->sender == trx->receiver) return false;
        std::unique_lock lk(m_);
        auto s = wallets_.find(trx->sender);
        if (s == wallets_.end() || !wallets_.contains(trx->receiver)) return false;
        double outgoing = 0;
        for (const Trx& p : pending_)
            if (p.sender == trx->sender) outgoing += p.value;
        if (s->second - outgoing < trx->value) return false;
        pending_.push_back(*trx);
        return true;
    }

    // 返回获胜矿工 id；pending 为空或提交冲突时返回 nullopt。
    std::optional<std::string> mine(const std::vector<std::string>& miners) {
        // 1) 在锁内拍快照
        std::vector<Trx> snapshot;
        std::uint64_t epoch;
        {
            std::shared_lock lk(m_);
            if (pending_.empty()) return std::nullopt;
            snapshot = pending_;
            epoch = epoch_;
        }
        // 2) 在锁外做昂贵的搜索 —— 此时其他线程仍可查询余额、提交交易
        std::string mempool;
        for (const Trx& t : snapshot) mempool += t.str();
        const auto k = static_cast<unsigned>(miners.size());
        std::size_t nonce = parallel_find_nonce(mempool, k, 1'000'000);
        if (nonce == kNoNonce) return std::nullopt;
        const std::string& winner = miners[nonce % k];

        // 3) 提交：若期间有别的 mine() 已提交（epoch 变了），快照已过期，放弃。
        //    pending_ 只会在尾部追加、只会被 mine() 从头部移除，
        //    所以 epoch 不变意味着 pending_ 的前 snapshot.size() 项正是快照。
        std::unique_lock lk(m_);
        if (epoch_ != epoch) return std::nullopt;
        for (const Trx& t : snapshot) {
            wallets_[t.sender] -= t.value;
            wallets_[t.receiver] += t.value;
        }
        pending_.erase(pending_.begin(), pending_.begin() + snapshot.size());
        wallets_[winner] += kReward;
        ++epoch_;
        return winner;
    }

    std::size_t pending_size() const {
        std::shared_lock lk(m_);
        return pending_.size();
    }

private:
    mutable std::shared_mutex m_;  // mutable：const 成员函数（读操作）也需要加锁
    std::map<std::string, double> wallets_;
    std::vector<Trx> pending_;
    std::uint64_t epoch_ = 0;
};

}  // namespace

int main() {
    // (a) 并行搜索与串行搜索得到相同的 nonce（确定性）
    for (const char* pool : {"ali-hamed-1.5", "ali-hamed-1.5mhmd-maryam-2.25", "x-y-3"}) {
        for (unsigned threads : {1u, 2u, 3u, 8u})
            CHECK_EQ(parallel_find_nonce(pool, threads, 1'000'000),
                     sequential_find_nonce(pool, 1'000'000));
    }

    // (b) 并发转账 + 并发挖矿，检查守恒不变量
    Ledger ledger;
    const std::vector<std::string> ids = {"ali", "hamed", "mhmd", "maryam", "mahi", "navid"};
    for (auto& id : ids) CHECK(ledger.add_client(id));
    CHECK(!ledger.add_client("ali"));
    const double initial = ledger.total_coins();

    std::atomic<int> blocks{0};
    std::atomic<bool> done{false};
    {
        std::vector<std::jthread> ts;
        for (int t = 0; t < 6; ++t)  // 交易线程
            ts.emplace_back([&, t] {
                std::mt19937 gen(t);
                std::uniform_int_distribution<std::size_t> who(0, ids.size() - 1);
                std::uniform_int_distribution<int> cents(1, 200);
                for (int i = 0; i < 300; ++i) {
                    auto a = ids[who(gen)], b = ids[who(gen)];
                    ledger.add_pending_trx(a + "-" + b + "-" + std::to_string(cents(gen) / 100.0));
                }
            });
        for (int t = 0; t < 2; ++t)  // 两个互相竞争的“矿池”
            ts.emplace_back([&] {
                while (!done.load()) {
                    if (ledger.mine(ids)) blocks.fetch_add(1);
                    std::this_thread::yield();
                }
            });
        for (int t = 0; t < 2; ++t)  // 只读查询线程
            ts.emplace_back([&] {
                while (!done.load())
                    for (auto& id : ids) CHECK(ledger.wallet(id) >= 0);
            });
        for (int i = 0; i < 6; ++i) ts[i].join();  // 等交易线程结束
        while (ledger.pending_size() != 0) std::this_thread::yield();  // 等全部被挖完
        done.store(true);
    }

    const double expected = initial + Ledger::kReward * blocks.load();
    std::cout << "blocks mined: " << blocks.load() << ", total coins: " << ledger.total_coins()
              << " (expected " << expected << ")\n";
    CHECK(std::abs(ledger.total_coins() - expected) < 1e-6);
    for (auto& id : ids) CHECK(ledger.wallet(id) >= -1e-9);
    std::cout << "lab2 OK\n";
}
