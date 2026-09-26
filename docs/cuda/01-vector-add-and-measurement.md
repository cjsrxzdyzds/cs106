# CUDA 01 · 向量加法、访存与计时

> 目标：写出无数据竞争的逐元素 kernel，处理任意尾部，并测量实际 GPU 时间。
> 源码：[add_kernel / benchmark_add](../../cuda/operators.cu)；CPU 对照：[vector_add](../../cuda/reference.h)。

## 1. 从数学规格到线程分工

输入同长度连续 float 数组 a、b，输出 out[i]=a[i]+b[i]。空数组返回空结果，不启动零大小 grid。测试使用有限值且结果可表示的输入；本算子没有广播、任意 stride 或不同类型转换。

每个输出只依赖同位置的两个输入。因此将每个 i 分给唯一线程，就不需要原子操作或 block barrier。先写出 CPU for 循环，再把循环下标拆成线程起点和全 grid 步长：

```cpp
std::size_t i = std::size_t(blockIdx.x) * blockDim.x + threadIdx.x;
const std::size_t stride = std::size_t(gridDim.x) * blockDim.x;
for (; i < n; i += stride) out[i] = a[i] + b[i];
```

这叫 grid-stride loop。若总线程数为 T，线程 t 负责 `t,t+T,t+2T,...`。不同线程的下标对 T 的余数不同，因此不会重复写；所有合法非负下标都能表示为这类形式，因此不漏元素。测试故意把 grid 限到最多 32 块，让长数组真正经过多次循环。

尾部检查 `i<n` 保证不越界。n=255、256、257 分别验证块内尾部、整块、跨块边界，n=100003 验证非整齐大输入。只测试 1024 这种整块尺寸容易掩盖错误。

## 2. 连续访问为何重要

相邻线程在同一轮访问相邻 float，通常有利于将访问合并成较少的内存事务。若让相邻线程跨越很大的步长，可能读入很多用不上的数据。具体事务数依赖对齐、硬件与访问模式，不能仅按源代码 load 次数得出实际显存流量。[NVIDIA 性能指南：内存访问](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/index.html)

本算子每个元素按逻辑计算需读两个 float、写一个 float，即 12 字节，只做一次加法。算术强度约 1/12 FLOP/byte，通常适合研究带宽而非浮点算力；小输入也可能主要受启动成本限制。

不应为了“优化”把数据先放进 shared memory：没有跨线程复用，额外搬运与同步可能只是增加工作。

## 3. 测量 kernel 与测量整个程序是两件事

`bench_add` 提供可运行的计时入口：先分配、上传并预热一次，然后在同一默认 stream 用两个 CUDA Event 包围 100 次 kernel 启动，等待结束事件完成，再取平均毫秒数。上传、下载、CPU 参考与显存分配不在该 Event 区间里。[NVIDIA：Event 计时](https://docs.nvidia.com/cuda/cuda-programming-guide/02-basics/asynchronous-execution.html#cuda-events)

```bash
./cuda/build-gpu/cuda_operator_tests vector_add
./cuda/build-gpu/cuda_operator_tests bench_add
```

报告输出的 `effective_GB_per_s = 12*n/(ms*1e6)` 是按逻辑字节数计算的有效带宽，**不是硬件测量的 DRAM 流量**。重复处理同一输入可能命中缓存；不能拿它直接声称超过或达到某个显存带宽极限。

每次 kernel 后加 `cudaDeviceSynchronize()` 适合定位错误，但会影响流水线与测量。因此计时循环仅检查启动错误，结束事件同步后再校验完整输出。不要删掉最终同步或数值比较。

如果要比较 CPU 与 GPU 对一个应用的收益，另用 host 时钟包含上传、kernel、等待与下载，注明是否包含分配。对小数组，GPU kernel 快也不代表端到端更快。

## 4. 测试、练习与面试追问

1. 将 grid 固定为 1，仍能处理超过 256 个元素吗？**答案**：可以，grid-stride 继续迭代，但并行资源利用可能不足。
2. 把 out 改成每个线程写 `out[0]` 会怎样？**答案**：产生多个写者，破坏原本的独占输出分工；不能靠 block barrier 修复跨块冲突。
3. 扩展成 `out=alpha*a+b`，再融合一个 ReLU。**验收**：CPU 对照覆盖负数、零、正数，比较融合与两次 kernel 的数值及计时范围。
4. 在真实 GPU 上重复计时至少五批，记录 GPU、Toolkit、输入长度与每批均值，再报告中位数。当前仓库没有填写任何实测 GPU 性能数字。

[上一章](00-execution-model-and-toolchain.md) · [目录](README.md) · [下一章](02-reduction.md)
