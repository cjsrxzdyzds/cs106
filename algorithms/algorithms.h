#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

// 教学契约见 docs/algorithms/README.md。输入规模须使计数与前缀和可由 long long 表示。
namespace study {

inline std::optional<std::pair<std::size_t, std::size_t>>
two_sum(const std::vector<int>& a, int target) {
    std::unordered_map<long long, std::size_t> seen;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const long long need = static_cast<long long>(target) - a[i];
        if (auto it = seen.find(need); it != seen.end()) return {{it->second, i}};
        seen.emplace(a[i], i);  // 保留较早位置；先查再插入，不能使用自己两次。
    }
    return std::nullopt;
}

inline long long subarray_sum(const std::vector<int>& a, int target) {
    std::unordered_map<long long, long long> frequency{{0, 1}};
    long long prefix = 0, result = 0;
    for (int x : a) {
        prefix += x;
        if (auto it = frequency.find(prefix - target); it != frequency.end())
            result += it->second;
        ++frequency[prefix];
    }
    return result;
}

// 前提：a 非递减排序；返回首个 >= target 的位置，找不到返回 a.size()。
inline std::size_t lower_bound_index(const std::vector<int>& a, int target) {
    std::size_t left = 0, right = a.size();
    while (left < right) {
        const auto mid = left + (right - left) / 2;
        if (a[mid] < target) left = mid + 1;
        else right = mid;
    }
    return left;
}

// 按字节计算，不把 UTF-8 多字节序列识别为一个字符。
inline std::size_t longest_unique(std::string_view text) {
    std::array<std::size_t, 256> next{};
    std::size_t left = 0, best = 0;
    for (std::size_t right = 0; right < text.size(); ++right) {
        auto c = static_cast<unsigned char>(text[right]);
        left = std::max(left, next[c]);
        best = std::max(best, right - left + 1);
        next[c] = right + 1;
    }
    return best;
}

// 返回距离，不存在严格更大的右侧元素时为 0。
inline std::vector<std::size_t> next_greater_distance(const std::vector<int>& a) {
    std::vector<std::size_t> answer(a.size(), 0), pending;
    for (std::size_t i = 0; i < a.size(); ++i) {
        while (!pending.empty() && a[pending.back()] < a[i]) {
            auto j = pending.back(); pending.pop_back();
            answer[j] = i - j;
        }
        pending.push_back(i);
    }
    return answer;
}

inline std::vector<int> sliding_max(const std::vector<int>& a, std::size_t k) {
    if (k == 0 || k > a.size()) throw std::invalid_argument("invalid window size");
    std::deque<std::size_t> candidates;
    std::vector<int> answer;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (i >= k && !candidates.empty() && candidates.front() <= i - k)
            candidates.pop_front();
        while (!candidates.empty() && a[candidates.back()] <= a[i])
            candidates.pop_back();
        candidates.push_back(i);
        if (i >= k - 1) answer.push_back(a[candidates.front()]);
    }
    return answer;
}

struct TreeNode {
    explicit TreeNode(int x) : value(x) {}
    int value;
    std::unique_ptr<TreeNode> left, right;
};

inline std::vector<std::vector<int>> level_order(const TreeNode* root) {
    std::vector<std::vector<int>> answer;
    if (!root) return answer;
    std::queue<const TreeNode*> q;
    q.push(root);
    while (!q.empty()) {
        const auto width = q.size();
        std::vector<int> level;
        for (std::size_t i = 0; i < width; ++i) {
            auto* node = q.front(); q.pop();
            level.push_back(node->value);
            if (node->left) q.push(node->left.get());
            if (node->right) q.push(node->right.get());
        }
        answer.push_back(std::move(level));
    }
    return answer;
}

// 重复值各占一个名次，k 从 1 开始。
inline int kth_largest(const std::vector<int>& a, std::size_t k) {
    if (k == 0 || k > a.size()) throw std::invalid_argument("invalid rank");
    std::priority_queue<int, std::vector<int>, std::greater<int>> heap;
    for (int x : a) {
        heap.push(x);
        if (heap.size() > k) heap.pop();
    }
    return heap.top();
}

using Graph = std::vector<std::vector<std::size_t>>;
inline void validate_graph(const Graph& graph) {
    for (const auto& edges : graph)
        for (auto v : edges)
            if (v >= graph.size()) throw std::invalid_argument("invalid vertex");
}

// 有向无权图；不可达节点为 nullopt。
inline std::vector<std::optional<std::size_t>>
bfs_distances(const Graph& graph, std::size_t source) {
    validate_graph(graph);
    if (source >= graph.size()) throw std::invalid_argument("invalid source");
    std::vector<std::optional<std::size_t>> distance(graph.size());
    std::queue<std::size_t> q;
    distance[source] = 0; q.push(source);
    while (!q.empty()) {
        auto u = q.front(); q.pop();
        for (auto v : graph[u]) {
            if (!distance[v]) {
                distance[v] = *distance[u] + 1;
                q.push(v);  // 入队时标记，避免同一节点被重复入队。
            }
        }
    }
    return distance;
}

// Kahn：有环返回 nullopt；空图有合法的空拓扑序。
inline std::optional<std::vector<std::size_t>> topological_order(const Graph& graph) {
    validate_graph(graph);
    std::vector<std::size_t> indegree(graph.size(), 0), result;
    for (const auto& edges : graph) for (auto v : edges) ++indegree[v];
    std::queue<std::size_t> ready;
    for (std::size_t v = 0; v < graph.size(); ++v) if (indegree[v] == 0) ready.push(v);
    while (!ready.empty()) {
        auto u = ready.front(); ready.pop(); result.push_back(u);
        for (auto v : graph[u]) if (--indegree[v] == 0) ready.push(v);
    }
    if (result.size() != graph.size()) return std::nullopt;
    return result;
}

// 正整数面额可重复使用；不可达为 -1；amount >= 0。
inline int coin_change(const std::vector<int>& coins, int amount) {
    if (amount < 0) throw std::invalid_argument("negative amount");
    for (int c : coins) if (c <= 0) throw std::invalid_argument("nonpositive coin");
    std::vector<int> dp(static_cast<std::size_t>(amount) + 1, -1);
    dp[0] = 0;
    for (std::size_t sum = 1; sum < dp.size(); ++sum) {
        for (int c : coins) {
            if (static_cast<std::size_t>(c) <= sum && dp[sum - c] >= 0) {
                int candidate = dp[sum - c] + 1;
                if (dp[sum] < 0 || candidate < dp[sum]) dp[sum] = candidate;
            }
        }
    }
    return dp[amount];
}

// 从 1..n 中选 k 个，按字典序返回；0<=k<=n，组合数量由调用者控制。
inline std::vector<std::vector<int>> combinations(int n, int k) {
    if (n < 0 || k < 0 || k > n) throw std::invalid_argument("invalid combination");
    std::vector<std::vector<int>> result;
    std::vector<int> path;
    std::function<void(long long)> search = [&](long long start) {
        if (path.size() == static_cast<std::size_t>(k)) {
            result.push_back(path); return;
        }
        const auto need = k - static_cast<int>(path.size());
        for (long long value = start; value <= static_cast<long long>(n) - need + 1; ++value) {
            path.push_back(static_cast<int>(value));
            search(value + 1);
            path.pop_back();
        }
    };
    search(1);
    return result;
}
}  // namespace study
