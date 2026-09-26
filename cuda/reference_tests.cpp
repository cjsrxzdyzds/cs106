#include "reference.h"
#include "../labs/common/check.h"
#include <iostream>
#include <limits>
#include <random>

template<class F> void rejects(F f) {
    bool caught = false;
    try { f(); } catch (const std::invalid_argument&) { caught = true; }
    CHECK(caught);
}
int main() {
    using namespace cuda_study;
    CHECK(vector_add({}, {}).empty());
    CHECK((vector_add({1, -2, 3}, {4, 2, -1}) == std::vector<float>{5, 0, 2}));
    rejects([] { vector_add({1}, {}); });
    CHECK_EQ(sum({}), 0);
    CHECK_EQ(sum({1, -2, 3, -4}), -2);
    CHECK((matmul({1,2,3,4,5,6}, {7,8,9,10,11,12}, 2,3,2) ==
           std::vector<float>{58,64,139,154}));
    CHECK((matmul({}, {}, 2,0,3) == std::vector<float>(6, 0)));
    rejects([] { matmul({1}, {}, 2,1,1); });
    CHECK(softmax({}, 0, 3).empty());
    rejects([] { softmax({}, 0, 0); });
    rejects([] { softmax({1}, 1, 2); });
    rejects([] { softmax({std::numeric_limits<float>::infinity()}, 1, 1); });
    rejects([] { softmax({std::numeric_limits<float>::quiet_NaN()}, 1, 1); });
    CHECK(near(softmax({1000}, 1, 1)[0], 1));
    auto uniform = softmax({10000,10000,10000}, 1,3);
    for (float x : uniform) CHECK(near(x, 1.0/3));
    auto probability = softmax({0, static_cast<float>(std::log(2.0))}, 1,2);
    CHECK(near(probability[0], 1.0/3)); CHECK(near(probability[1], 2.0/3));
    std::mt19937 rng(42);
    for (std::size_t cols : {1u, 7u, 255u, 257u, 1025u}) {
        std::vector<float> x(3*cols);
        for (float& v : x) v = static_cast<float>(static_cast<int>(rng()%41)-20);
        auto p = softmax(x,3,cols);
        for (std::size_t row = 0; row < 3; ++row) {
            double total = 0;
            for (std::size_t col = 0; col < cols; ++col) {
                CHECK(p[row*cols+col] >= 0); total += p[row*cols+col];
            }
            CHECK(near(total,1));
        }
        for (float& v : x) v += 1000;  // 对这些整数输入，平移在 float 中可精确表示。
        auto shifted = softmax(x,3,cols);
        for (std::size_t i=0; i<p.size(); ++i) CHECK(near(p[i],shifted[i]));
    }
    CHECK(!near(std::numeric_limits<double>::quiet_NaN(), 1));
    std::cout << "CPU operator references OK (this does not execute CUDA)\n";
}
