# CUDA 00 · 执行模型、内存与工具链

> 目标：能解释一个 GPU kernel 如何启动、访问哪块内存、何时允许读取结果。
> 完整程序：[operators.cu](../../cuda/operators.cu)。

## 1. host 发起工作，device 执行 kernel

本课程的 host 是执行普通 C++ 的 CPU，device 是运行 CUDA kernel 的 NVIDIA GPU。`__global__` 标记可从 host 启动的 GPU 函数，启动语法 `kernel<<<grid,block>>>(...)` 指定执行配置。一次启动会产生许多逻辑线程，而不是让 CPU 创建同样数量的 std::thread。

```text
CPU：准备输入 → 分配显存 → 拷入 → 启动 kernel → 等待 → 拷回并校验
GPU：                                  执行多个 block
```

kernel 启动通常相对 host 异步；函数调用返回不表示 GPU 已计算完成。本实验通过 `cudaGetLastError()` 检查启动问题，再用 `cudaDeviceSynchronize()` 等待并检查执行错误，随后读回结果。生产流水线可以使用更细的 stream/event 同步，不必处处等整个设备。[NVIDIA 异步执行说明](https://docs.nvidia.com/cuda/cuda-programming-guide/02-basics/asynchronous-execution.html)

## 2. grid、block、thread 与 warp

grid 由 block 组成，每个 block 内有线程。`blockIdx` 表示块的位置，`threadIdx` 表示块内线程位置，`blockDim` 是块尺寸。对一维布局：

```cpp
std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
```

本课采用 256 线程一块。假设 n=600，向上取整得到 3 块、768 个线程，最后 168 个线程没有对应元素，因此必须检查边界。block 大小不是输入长度，输入也无需整除 block 大小。

硬件按 warp 组织执行线程，当前 CUDA 常用 warp 为 32 个线程；同一 warp 内走不同分支可能降低执行效率。warp 不是 C++ 内存同步的替代保证，不应凭“同一 warp”就省略共享数据所需的同步。线程层次与执行模型见 [NVIDIA 编程模型](https://docs.nvidia.com/cuda/cuda-programming-guide/01-introduction/programming-model.html)。

普通 block 必须可以独立调度，不保证按编号启动或结束。不要让 block 0 忙等“所有别的 block 完成”，因为某些尚未执行的块可能等不到资源。跨 block 的合并在本课程中通过多次 kernel 启动完成。

## 3. 先识别存储位置，再安排同步

| 存储 | 本课程用途 | 容易混淆的地方 |
|---|---|---|
| host vector | CPU 输入与参考结果 | data() 不是本课普通显存指针 |
| global memory | kernel 输入、输出、部分归约结果 | 大家都能访问不代表写入无冲突 |
| shared memory | 同一 block 的协作暂存 | 不在不同 block 之间共享 |
| 线程局部变量 | 索引、累加器 | 不应假定每个变量永远驻留寄存器 |

`cudaMalloc` 分配后通过 `cudaMemcpy` 传输，最后 `cudaFree` 释放。DeviceBuffer 用 RAII 管理显存，禁用复制，避免双重释放；在正常路径，每次计算完成后才释放相关 buffer。析构不抛异常，但普通操作会报告 API 错误。

`__syncthreads()` 用于块内协作，使参与线程完成相应屏障之前的访问后再继续。教学代码要求块内所有线程都经过同一组屏障；尾部线程用中性值参加归约，不用不同循环次数或分支绕过协作。它不是整个 grid 的屏障。[NVIDIA SIMT kernel 文档](https://docs.nvidia.com/cuda/cuda-programming-guide/02-basics/writing-cuda-kernels.html)

## 4. 三个常见错误

- 直接把 CPU vector.data() 传给需要 device 数据的教学 kernel：普通 host 内存不满足该接口约定，本课先显式分配与拷贝。
- 只记录 host 在启动前后的时间：测到的可能主要是提交耗时，而非计算完成耗时。
- 给输出元素安排多个无同步写者：与 CPU 并发一样，需要重新分工或使用合适同步；全局变量不会自动原子化。

## 练习与验收

1. n=1000、block=256，算出 block 数和无效线程数。**答案**：4 块、24 个尾部线程。
2. 描述数组从 CPU 到 GPU 再回 CPU 的生命周期。**验收**：明确两份存储、拷贝方向、同步位置和释放时刻。
3. 为什么 shared memory 不能用来存所有 block 的最终和？**答案**：每块各有自己的实例；跨块结果需写到 global memory，再进行后续合并。
4. 按 [目录构建步骤](README.md#本机与-gpu-环境分别运行) 运行，记录实际发现了几个测试。没有 GPU 时应明确留下未验证状态。

[目录](README.md) · [下一章](01-vector-add-and-measurement.md)
