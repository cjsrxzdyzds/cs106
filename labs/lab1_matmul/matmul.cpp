// Lab 1（HW1 扩展）：矩阵乘法的局部性与并行化
//
// HW1 用 std::vector<std::vector<double>> 表示矩阵并按 i-j-k 顺序做乘法。
// 本实验依次展示（参考 CMU 15-213 “Cache Lab / 存储器层次” 与 15-418 “并行分解”）：
//   1. 扁平化存储：一块连续内存，行主序（row-major）
//   2. 循环交换 i-k-j：最内层循环对 B 与 C 都是步长为 1 的访问
//   3. 分块（tiling）：让工作集装进 cache
//   4. 按行划分给多个线程：各线程写互不相交的行，无需任何锁
//
// 用法：./lab1_matmul [n] [threads]   默认 n=512，threads=hardware_concurrency

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include "../common/check.h"

namespace {

struct Matrix {
    std::size_t n = 0;
    std::vector<double> a;  // row-major: a[i * n + j]

    explicit Matrix(std::size_t n_) : n(n_), a(n_ * n_, 0.0) {}
    double& operator()(std::size_t i, std::size_t j) { return a[i * n + j]; }
    double operator()(std::size_t i, std::size_t j) const { return a[i * n + j]; }
};

Matrix random_matrix(std::size_t n, unsigned seed) {
    Matrix m(n);
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> dis(-1.0, 1.0);
    for (double& x : m.a) x = dis(gen);
    return m;
}

// 与 HW1 相同的 i-j-k 顺序：最内层对 B 按列访问，步长为 n，cache 不友好。
Matrix mul_ijk(const Matrix& A, const Matrix& B) {
    const std::size_t n = A.n;
    Matrix C(n);
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j) {
            double sum = 0;
            for (std::size_t k = 0; k < n; ++k) sum += A(i, k) * B(k, j);
            C(i, j) = sum;
        }
    return C;
}

// 计算 C 的第 [row_begin, row_end) 行，i-k-j 顺序。并行版本复用这个函数。
void mul_ikj_rows(const Matrix& A, const Matrix& B, Matrix& C, std::size_t row_begin,
                  std::size_t row_end) {
    const std::size_t n = A.n;
    for (std::size_t i = row_begin; i < row_end; ++i)
        for (std::size_t k = 0; k < n; ++k) {
            const double aik = A(i, k);
            for (std::size_t j = 0; j < n; ++j) C(i, j) += aik * B(k, j);
        }
}

Matrix mul_ikj(const Matrix& A, const Matrix& B) {
    Matrix C(A.n);
    mul_ikj_rows(A, B, C, 0, A.n);
    return C;
}

Matrix mul_blocked(const Matrix& A, const Matrix& B, std::size_t bs = 64) {
    const std::size_t n = A.n;
    Matrix C(n);
    for (std::size_t ii = 0; ii < n; ii += bs)
        for (std::size_t kk = 0; kk < n; kk += bs)
            for (std::size_t jj = 0; jj < n; jj += bs)
                for (std::size_t i = ii; i < std::min(ii + bs, n); ++i)
                    for (std::size_t k = kk; k < std::min(kk + bs, n); ++k) {
                        const double aik = A(i, k);
                        for (std::size_t j = jj; j < std::min(jj + bs, n); ++j)
                            C(i, j) += aik * B(k, j);
                    }
    return C;
}

// 数据并行：静态地把行切成 threads 份。
// 各线程只写 C 中属于自己的行、只读 A 和 B，因此没有数据竞争；
// thread::join() 保证主线程之后能看到所有写入（happens-before）。
Matrix mul_parallel(const Matrix& A, const Matrix& B, unsigned threads) {
    const std::size_t n = A.n;
    Matrix C(n);
    {
        std::vector<std::jthread> workers;
        const std::size_t chunk = (n + threads - 1) / threads;
        for (unsigned t = 0; t < threads; ++t) {
            const std::size_t begin = t * chunk;
            const std::size_t end = std::min(n, begin + chunk);
            if (begin >= end) break;
            workers.emplace_back([&, begin, end] { mul_ikj_rows(A, B, C, begin, end); });
        }
    }  // jthread 在此析构并 join。
    // 为什么要多一层作用域？return 语句先初始化返回值、再析构局部变量。
    // NRVO 不是强制的：若编译器选择把 C “移动”到返回值，而 workers 还没 join，
    // 那么工作线程仍在写一个已被移走的 vector —— 数据竞争 + use-after-move。
    return C;
}

bool almost_equal(const Matrix& X, const Matrix& Y) {
    for (std::size_t i = 0; i < X.a.size(); ++i)
        if (std::abs(X.a[i] - Y.a[i]) > 1e-9 * X.n) return false;
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const std::size_t n = argc > 1 ? std::strtoul(argv[1], nullptr, 10) : 512;
    unsigned threads = argc > 2 ? std::strtoul(argv[2], nullptr, 10)
                                : std::max(1u, std::thread::hardware_concurrency());

    const Matrix A = random_matrix(n, 1), B = random_matrix(n, 2);

    auto bench = [](const char* name, auto&& f) {
        lab::Timer t;
        Matrix C = f();
        std::cout << name << t.ms() << " ms\n";
        return C;
    };

    std::cout << "n = " << n << ", threads = " << threads << "\n";
    Matrix ref = bench("ijk (HW1 order) : ", [&] { return mul_ijk(A, B); });
    Matrix c1 = bench("ikj             : ", [&] { return mul_ikj(A, B); });
    Matrix c2 = bench("blocked         : ", [&] { return mul_blocked(A, B); });
    Matrix c3 = bench("parallel ikj    : ", [&] { return mul_parallel(A, B, threads); });

    CHECK(almost_equal(ref, c1));
    CHECK(almost_equal(ref, c2));
    CHECK(almost_equal(ref, c3));
    std::cout << "lab1 OK\n";
}
