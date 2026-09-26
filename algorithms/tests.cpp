#include "algorithms.h"
#include "../labs/common/check.h"
#include <climits>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <random>
#include <string>

namespace {
std::mt19937 rng(20260925);
std::vector<int> random_array(std::size_t n) {
    std::vector<int> a(n);
    for (int& x : a) x = static_cast<int>(rng() % 21) - 10;
    return a;
}
template<class F> void rejects(F&& f) {
    bool caught = false;
    try { f(); } catch (const std::invalid_argument&) { caught = true; }
    CHECK(caught);
}
void hash_prefix() {
    CHECK(!study::two_sum({}, 0));
    CHECK(!study::two_sum({3}, 6));
    CHECK(study::two_sum({3, 3}, 6));
    CHECK(study::two_sum({INT_MIN, INT_MAX}, -1));
    CHECK_EQ(study::subarray_sum({0, 0, 0}, 0), 6);
    for (int trial = 0; trial < 300; ++trial) {
        auto a = random_array(rng() % 30);
        int target = static_cast<int>(rng() % 21) - 10;
        bool exists = false;
        long long count = 0;
        for (std::size_t i = 0; i < a.size(); ++i) {
            long long sum = 0;
            for (std::size_t j = i; j < a.size(); ++j) {
                sum += a[j]; if (sum == target) ++count;
                if (j > i && static_cast<long long>(a[i]) + a[j] == target) exists = true;
            }
        }
        auto pair = study::two_sum(a, target);
        CHECK_EQ(pair.has_value(), exists);
        if (pair) {
            CHECK(pair->first < pair->second && pair->second < a.size());
            CHECK_EQ(static_cast<long long>(a[pair->first]) + a[pair->second], target);
        }
        CHECK_EQ(study::subarray_sum(a, target), count);
    }
}
void search_window() {
    CHECK_EQ(study::longest_unique("abba"), 2u);
    CHECK_EQ(study::longest_unique(std::string("\xff\0\xff", 3)), 2u);
    for (int trial = 0; trial < 300; ++trial) {
        auto a = random_array(rng() % 40); std::sort(a.begin(), a.end());
        for (int target = -12; target <= 12; ++target)
            CHECK_EQ(study::lower_bound_index(a, target),
                     static_cast<std::size_t>(std::lower_bound(a.begin(), a.end(), target) - a.begin()));
        std::string text;
        for (std::size_t i = 0; i < a.size(); ++i) text += static_cast<char>('a' + rng() % 6);
        std::size_t best = 0;
        for (std::size_t i = 0; i < text.size(); ++i) {
            std::array<bool, 256> used{};
            for (std::size_t j = i; j < text.size(); ++j) {
                auto c = static_cast<unsigned char>(text[j]);
                if (used[c]) break;
                used[c] = true; best = std::max(best, j - i + 1);
            }
        }
        CHECK_EQ(study::longest_unique(text), best);
    }
}
void monotonic() {
    CHECK(study::next_greater_distance({}).empty());
    rejects([] { study::sliding_max({}, 1); });
    rejects([] { study::sliding_max({1}, 0); });
    for (int trial = 0; trial < 200; ++trial) {
        auto a = random_array(1 + rng() % 35);
        std::vector<std::size_t> expected(a.size(), 0);
        for (std::size_t i = 0; i < a.size(); ++i)
            for (std::size_t j = i + 1; j < a.size(); ++j)
                if (a[j] > a[i]) { expected[i] = j - i; break; }
        CHECK(study::next_greater_distance(a) == expected);
        for (std::size_t k = 1; k <= a.size(); ++k) {
            std::vector<int> maxima;
            for (std::size_t i = 0; i + k <= a.size(); ++i)
                maxima.push_back(*std::max_element(a.begin() + i, a.begin() + i + k));
            CHECK(study::sliding_max(a, k) == maxima);
        }
    }
}
void tree_heap() {
    CHECK(study::level_order(nullptr).empty());
    auto root = std::make_unique<study::TreeNode>(3);
    root->left = std::make_unique<study::TreeNode>(9);
    root->right = std::make_unique<study::TreeNode>(20);
    root->right->left = std::make_unique<study::TreeNode>(15);
    root->right->right = std::make_unique<study::TreeNode>(7);
    CHECK((study::level_order(root.get()) == std::vector<std::vector<int>>{{3},{9,20},{15,7}}));
    rejects([] { study::kth_largest({}, 1); });
    rejects([] { study::kth_largest({1}, 0); });
    rejects([] { study::kth_largest({1}, 2); });
    for (int trial = 0; trial < 200; ++trial) {
        auto a = random_array(1 + rng() % 40); auto sorted = a;
        std::sort(sorted.begin(), sorted.end(), std::greater<int>());
        for (std::size_t k = 1; k <= a.size(); ++k) CHECK_EQ(study::kth_largest(a, k), sorted[k - 1]);
    }
}
void graphs() {
    CHECK(study::topological_order({})->empty());
    CHECK(!study::topological_order({{0}}));
    CHECK(!study::topological_order({{1},{0}}));
    rejects([] { study::bfs_distances({}, 0); });
    rejects([] { study::topological_order({{1}}); });
    for (int trial = 0; trial < 200; ++trial) {
        const std::size_t n = 1 + rng() % 8;
        study::Graph g(n);
        const int inf = 1000;
        std::vector<std::vector<int>> d(n, std::vector<int>(n, inf));
        for (std::size_t u = 0; u < n; ++u) {
            d[u][u] = 0;
            for (std::size_t v = 0; v < n; ++v)
                if (rng() % 4 == 0) { g[u].push_back(v); d[u][v] = std::min(d[u][v], 1); }
        }
        // 独立 Floyd-Warshall 参考，不复制被测 BFS 的控制流程。
        for (std::size_t k = 0; k < n; ++k)
            for (std::size_t u = 0; u < n; ++u)
                for (std::size_t v = 0; v < n; ++v) d[u][v] = std::min(d[u][v], d[u][k] + d[k][v]);
        for (std::size_t source = 0; source < n; ++source) {
            auto actual = study::bfs_distances(g, source);
            for (std::size_t v = 0; v < n; ++v) {
                CHECK_EQ(actual[v].has_value(), d[source][v] != inf);
                if (actual[v]) CHECK_EQ(*actual[v], static_cast<std::size_t>(d[source][v]));
            }
        }
        // 有环当且仅当某条边 v->u 可沿路径回到 v（含自环）。
        bool cycle = false;
        for (std::size_t u = 0; u < n; ++u) for (auto v : g[u])
            if (d[v][u] != inf) cycle = true;
        auto order = study::topological_order(g);
        CHECK_EQ(order.has_value(), !cycle);
        if (order) {
            CHECK_EQ(order->size(), n);
            auto sorted = *order; std::sort(sorted.begin(), sorted.end());
            std::vector<std::size_t> position(n);
            for (std::size_t i = 0; i < n; ++i) { CHECK_EQ(sorted[i], i); position[(*order)[i]] = i; }
            for (std::size_t u = 0; u < n; ++u) for (auto v : g[u]) CHECK(position[u] < position[v]);
        }
    }
}
void dp_backtracking() {
    CHECK_EQ(study::coin_change({1, 2, 5}, 11), 3);
    CHECK_EQ(study::coin_change({2}, 3), -1);
    CHECK_EQ(study::coin_change({}, 0), 0);
    rejects([] { study::coin_change({0}, 2); });
    rejects([] { study::coin_change({-1}, 2); });
    rejects([] { study::coin_change({1}, -1); });
    rejects([] { study::combinations(2, 3); });
    for (int trial = 0; trial < 200; ++trial) {
        std::vector<int> coins;
        for (int c = 1; c <= 7; ++c) if (rng() % 2) coins.push_back(c);
        int amount = static_cast<int>(rng() % 30);
        // 金额状态图上的 BFS 是独立参考：每条边表示使用一枚硬币。
        study::Graph graph(static_cast<std::size_t>(amount) + 1);
        for (int sum = 0; sum <= amount; ++sum)
            for (int c : coins) if (sum + c <= amount) graph[sum].push_back(sum + c);
        auto distance = study::bfs_distances(graph, 0);
        CHECK_EQ(study::coin_change(coins, amount), distance[amount] ? static_cast<int>(*distance[amount]) : -1);
    }
    for (int n = 0; n <= 9; ++n) for (int k = 0; k <= n; ++k) {
        std::vector<std::vector<int>> expected;
        for (unsigned mask = 0; mask < (1u << n); ++mask) {
            std::vector<int> subset;
            for (int i = 0; i < n; ++i) if (mask & (1u << i)) subset.push_back(i + 1);
            if (subset.size() == static_cast<std::size_t>(k)) expected.push_back(subset);
        }
        std::sort(expected.begin(), expected.end());
        CHECK(study::combinations(n, k) == expected);
    }
}
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    switch (std::atoi(argv[1])) {
        case 1: hash_prefix(); break;
        case 2: search_window(); break;
        case 3: monotonic(); break;
        case 4: tree_heap(); break;
        case 5: graphs(); break;
        case 6: dp_backtracking(); break;
        default: return 2;
    }
    std::cout << "algorithm chapter " << argv[1] << " OK\n";
}
