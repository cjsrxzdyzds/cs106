#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace cuda_study {
inline std::size_t elements(std::size_t rows, std::size_t cols) {
    if (cols && rows > std::numeric_limits<std::size_t>::max() / cols)
        throw std::invalid_argument("shape overflow");
    return rows * cols;
}
inline std::vector<float> vector_add(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) throw std::invalid_argument("shape mismatch");
    std::vector<float> out(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) out[i] = a[i] + b[i];
    return out;
}
inline double sum(const std::vector<float>& a) {
    double result = 0;
    for (float x : a) result += x;
    return result;
}
inline std::vector<float> matmul(const std::vector<float>& a, const std::vector<float>& b,
                                std::size_t m, std::size_t k, std::size_t n) {
    if (a.size() != elements(m, k) || b.size() != elements(k, n))
        throw std::invalid_argument("shape mismatch");
    std::vector<float> out(elements(m, n), 0);
    for (std::size_t row = 0; row < m; ++row)
        for (std::size_t col = 0; col < n; ++col) {
            double value = 0;
            for (std::size_t inner = 0; inner < k; ++inner)
                value += static_cast<double>(a[row * k + inner]) * b[inner * n + col];
            out[row * n + col] = static_cast<float>(value);
        }
    return out;
}
// 教学算子：连续行主序、有限 float、cols>0；不定义 NaN/Inf/mask 的语义。
inline std::vector<float> softmax(const std::vector<float>& input, std::size_t rows, std::size_t cols) {
    if (cols == 0 || input.size() != elements(rows, cols))
        throw std::invalid_argument("invalid softmax shape");
    for (float x : input) if (!std::isfinite(x)) throw std::invalid_argument("nonfinite input");
    std::vector<float> output(input.size());
    for (std::size_t row = 0; row < rows; ++row) {
        auto begin = input.begin() + row * cols;
        double maximum = *std::max_element(begin, begin + cols);
        double denominator = 0;
        for (std::size_t col = 0; col < cols; ++col)
            denominator += std::exp(static_cast<double>(input[row * cols + col]) - maximum);
        for (std::size_t col = 0; col < cols; ++col)
            output[row * cols + col] = static_cast<float>(
                std::exp(static_cast<double>(input[row * cols + col]) - maximum) / denominator);
    }
    return output;
}
inline bool near(double actual, double expected, double atol = 1e-5, double rtol = 1e-4) {
    return std::isfinite(actual) && std::isfinite(expected) &&
           std::abs(actual - expected) <= atol + rtol * std::abs(expected);
}
}  // namespace cuda_study
