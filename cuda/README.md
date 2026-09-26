# CUDA 算子实验

课件入口：[CUDA 5 章课程](../docs/cuda/README.md)。

| 文件 | 内容 |
|---|---|
| [reference.h](reference.h) | 4 类算子的 CPU 数学参考，矩阵与归约使用 double 中间计算 |
| [reference_tests.cpp](reference_tests.cpp) | CPU 边界、已知答案及 Softmax 性质测试 |
| [operators.cu](operators.cu) | 向量加法、归约、朴素/分块 GEMM、行 Softmax，GPU 对照与向量计时 |
| [CMakeLists.txt](CMakeLists.txt) | 自动检测 CUDA，CPU 与 GPU 验证状态分开 |

从根目录运行：

```bash
cmake -S cuda -B cuda/build-cpu -DBUILD_CUDA_LESSONS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build cuda/build-cpu -j 4
ctest --test-dir cuda/build-cpu --output-on-failure
```

NVIDIA 环境的编译、4 个 GPU 测试、bench_add 与 Compute Sanitizer 命令见课件。无 nvcc 时只构建 CPU；无设备时 GPU 测试为 Skipped，驱动错误为失败。

验证记录：2026-09-26，macOS arm64 / Apple Clang 21，CPU 测试通过。CUDA 源码尚未用 nvcc 编译、未在 GPU 运行；没有提供实测 GPU 性能。GPU 程序只启动受控测试形状，不是面向任意张量布局和规模的库接口。
