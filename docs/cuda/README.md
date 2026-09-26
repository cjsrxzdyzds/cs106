# CUDA 算子课程 · 从索引到数值验证

先学会把一个算子正确映射到 GPU，再讨论优化。本课程使用 CUDA C++，提供向量加法、求和归约、朴素/分块矩阵乘法、行 Softmax 的教学实现。每个算子都有 CPU 参考与 GPU 对照测试，向量加法另有 Event 计时入口。

**验证状态：当前开发环境为 macOS arm64，未发现 nvcc 或 NVIDIA 设备工具。CPU 参考测试已在本机通过；`.cu` 文件尚未通过 nvcc 编译或 GPU 运行，性能与 Compute Sanitizer 检查也尚未执行。** 下面提供的是待 NVIDIA 环境验证的教学代码，不能把仅 CPU 的 CTest 成功当作 CUDA 通过。

## 学习顺序

| 章 | 内容 | 对应代码 |
|---|---|---|
| [00 执行模型与工具链](00-execution-model-and-toolchain.md) | host/device、grid/block、内存、同步与错误检查 | DeviceBuffer / finish_launch |
| [01 向量加法与测量](01-vector-add-and-measurement.md) | 索引、grid-stride、连续访问、Event 计时 | add_kernel / benchmark_add |
| [02 求和归约](02-reduction.md) | shared memory、树形合并、多轮 kernel | reduce_kernel / gpu_sum |
| [03 矩阵乘法](03-matmul-and-tiling.md) | 二维索引、复用、分块、边界填零 | matmul_naive / matmul_tiled |
| [04 Softmax 与验证](04-softmax-and-validation.md) | 稳定公式、max/sum 归约、数值误差、结课项目 | softmax_rows |

前置：C++ 指针、数组、RAII、行主序矩阵；先读 [线程与内存模型](../concurrency/01-threads-and-memory-model.md) 有助于理解生命周期，但 CPU mutex 与 GPU block barrier 不是同一个接口。算法基础可配合 [算法课程](../algorithms/README.md) 学习。

[GPU 完整源码](../../cuda/operators.cu) 使用固定教学参数：Block=256、Tile=16；测试输入使用连续 float、有限值和受控形状，不是支持任意布局、任意设备限制的通用张量库。Tensor Core、混合精度、warp shuffle、异步搬运、卷积与 Attention 尚未实现。

## 本机与 GPU 环境分别运行

从仓库根目录执行，仅需要 C++ 编译器的 CPU 参考检查：

```bash
cmake -S cuda -B cuda/build-cpu -DBUILD_CUDA_LESSONS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build cuda/build-cpu -j 4
ctest --test-dir cuda/build-cpu --output-on-failure
```

在具备兼容 NVIDIA GPU、驱动、CUDA Toolkit 与主机编译器的环境中：

```bash
nvidia-smi
nvcc --version
cmake -S cuda -B cuda/build-gpu -DBUILD_CUDA_LESSONS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build cuda/build-gpu -j 4
ctest --test-dir cuda/build-gpu -N
ctest --test-dir cuda/build-gpu --output-on-failure
./cuda/build-gpu/cuda_operator_tests vector_add
./cuda/build-gpu/cuda_operator_tests bench_add
```

检测到 nvcc 时应有 **1 个 CPU 测试和 4 个 GPU 测试**。未检测到编译器时 CMake 明确报告只构建 CPU；找到编译器但运行时没有设备时，GPU 测试以 77 返回并标记 Skipped。驱动错误作为失败报告，不冒充跳过或成功。

部署到另一 GPU 时，按其计算能力设置 `CMAKE_CUDA_ARCHITECTURES`，并使用 Toolkit 支持的目标架构；本课程不写死一个架构号，也不自动安装驱动或购买远程 GPU。

## 完成标准

每个算子应提交四项证据：线程到数据的映射图或表、同步与边界解释、CPU/GPU 误差检查、清楚注明测量范围的性能记录。没有 GPU 时可以先完成 CPU 推导与测试，GPU 项保持未验证。

官方参考：[编程模型](https://docs.nvidia.com/cuda/cuda-programming-guide/01-introduction/programming-model.html)、[SIMT kernel](https://docs.nvidia.com/cuda/cuda-programming-guide/02-basics/writing-cuda-kernels.html)、[性能指南](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/index.html)、[Compute Sanitizer](https://docs.nvidia.com/compute-sanitizer/ComputeSanitizer/index.html)。具体安装兼容性按实际 NVIDIA 环境核对。

[项目首页](../../README.md) · [实验说明](../../cuda/README.md)
