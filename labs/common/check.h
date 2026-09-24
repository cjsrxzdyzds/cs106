// 轻量断言与计时工具：labs 不依赖 GoogleTest，任何 C++20 编译器都能直接编译运行。
#pragma once

#include <chrono>
#include <cstdio>
#include <cstdlib>

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, \
                         #cond);                                                 \
            std::exit(1);                                                        \
        }                                                                        \
    } while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))

namespace lab {

class Timer {
public:
    Timer() : start_(std::chrono::steady_clock::now()) {}
    double ms() const {
        using namespace std::chrono;
        return duration<double, std::milli>(steady_clock::now() - start_).count();
    }

private:
    std::chrono::steady_clock::time_point start_;
};

}  // namespace lab
