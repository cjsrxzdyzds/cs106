# CUDA 04 · 稳定 Softmax、数值验证与结课项目

> 目标：组合最大值归约、求和归约与逐元素变换；把算子正确性与性能分开验收。
> 源码：[softmax_rows](../../cuda/operators.cu)；CPU 对照：[softmax](../../cuda/reference.h)。

## 1. 从公式中发现数值问题

每一行 x 的 Softmax 定义为 `y[i]=exp(x[i])/Σexp(x[j])`。若 x=[1000,1000]，直接求 exp 会溢出，但正确概率显然应为 [0.5,0.5]。

把所有项同时减去同一个 m 不改变比值，因为分子分母都乘了 exp(-m)。取 m=max(x)，使每个指数自变量不大于 0：

```text
m = max(x)
s = sum(exp(x-m))
y = exp(x-m)/s
```

最大项贡献 exp(0)=1，所以对有限输入、非空行，分母不会因所有项下溢而成为 0。很小的项可能下溢到 0，这是有限精度行为。本课不定义 NaN、正负无穷、mask 或全屏蔽行语义；CPU 参考会拒绝非有限输入，GPU 测试也只传有限值。

## 2. 一个 block 负责一行

每行用 256 个线程，线程 t 负责列 `t,t+256,t+512,...`，因此列数可以超过或小于线程数。按顺序执行：

1. 每线程求自己负责元素的 local_max；没有元素时取负无穷。
2. shared memory 归约得到整行最大值 m。
3. 每线程计算负责元素的 exp(x-m) 之和，再归约得到 s。
4. 每线程重新计算 exp(x-m)/s，写出独占列。

本实现重新计算 exp，而不为每个元素保存中间指数，便于展示多阶段协作。更少的计算与更少的存储之间存在取舍，优化时需要测量。

## 3. 重用 shared memory 的隐蔽同步点

max 和 sum 复用同一个 scratch 数组。完成最大值归约后，每个线程先把 scratch[0] 读入局部 maximum，然后经过屏障，才允许用 local_sum 覆盖 scratch[t]。

```cpp
const float maximum = scratch[0];
__syncthreads();
// 所有线程已保存 maximum，现在可以覆盖 scratch。
scratch[t] = local_sum;
```

真实源码在屏障后先计算 local_sum，再写 scratch。若去掉这道屏障，线程 0 可能提前覆盖 scratch[0]，而另一个线程尚未读取最大值，它将拿到部分和当作 m。前一轮末尾的屏障只保证最大值已经生成，不保证所有线程已经把它读走。这是“生产完成”与“消费完成”两个不同条件。

同一行不需要跨 block 同步，因为整行由一个 block 负责。行数很少或行非常长时，这种映射的性能可能受限；跨块 Softmax 则需要另一个完整合并协议，不能直接把 block 数加倍。

## 4. 三层数值检查

首先逐元素与 double 中间计算的 CPU 参考比较：`abs(actual-reference) <= atol + rtol*abs(reference)`。比较函数先拒绝 NaN/Inf；如果只写 `abs(error)>tol`，NaN 比较可能使错误漏过。

其次检查概率不为负、每行和约等于 1。仅检查行和不够，错误地输出均匀分布也能通过，必须保留逐元素参考。

最后使用结构性质检查：相等输入产生均匀分布、单列输出 1、一个远大于其他元素的项概率接近 1、对一行加同一常数后结果近似不变。平移不变性也要考虑输入 float 舍入；CPU 测试选用可精确表示平移的整数。

GPU 测试覆盖 1、7、255、256、257、1025 列，以及相等的大值、单个主导值和随机行。CPU 额外检查空行数及非法形状；GPU 内核仅接受 cols>0，若 rows=0，未来的通用 host 包装应直接返回而非启动零块。

浮点结果还可能受指令融合、计算顺序和编译选项影响。启用 fast math、半精度等选项后必须重新制定误差要求，不应沿用一次测试的阈值。[NVIDIA 浮点计算参考](https://docs.nvidia.com/cuda/cuda-programming-guide/05-appendices/mathematical-functions.html)

## 5. Compute Sanitizer 与测量流程

在已经成功编译并运行 CUDA 测试的 NVIDIA 环境，进一步执行：

```bash
compute-sanitizer --tool memcheck --error-exitcode 1 ./cuda/build-gpu/cuda_operator_tests matmul
compute-sanitizer --tool racecheck --error-exitcode 1 ./cuda/build-gpu/cuda_operator_tests reduce
compute-sanitizer --tool racecheck --error-exitcode 1 ./cuda/build-gpu/cuda_operator_tests softmax
compute-sanitizer --tool synccheck --error-exitcode 1 ./cuda/build-gpu/cuda_operator_tests softmax
```

对其余算子也运行相应检查。memcheck 用于越界等内存错误，racecheck 检查 shared memory 访问危险，synccheck 检查同步使用问题；这些工具不能证明数学公式正确，也不能把 racecheck 理解为检测所有 global memory 竞态。[Compute Sanitizer 官方文档](https://docs.nvidia.com/compute-sanitizer/ComputeSanitizer/index.html)

性能测试用未插桩的 Release 程序，先数值验证，再预热和计时。分别记录 kernel-only 与端到端结果，注明矩阵/张量形状、GPU、驱动、Toolkit、编译选项和误差阈值。当前仓库未取得 GPU 实测结果。

## 6. 结课项目：融合与取舍

先独立实现 `bias + ReLU` 或逐行 `scale + Softmax`，保留一个 CPU 对照，然后完成：

- 给出形状、布局、dtype、非法输入处理与是否允许原地计算的契约。
- 解释哪个线程写哪个输出，哪些中间数据需要同步，尾部如何处理。
- 比较分离 kernel 与融合 kernel：同时验证结果，测量传输次数及 kernel 时间，不把减少一次启动直接当成速度证明。
- 覆盖空输入策略、单元素、非整块尺寸、极端但合法输入，运行内存与同步检查。
- 写出一个优化可能退化的场景，例如寄存器需求增加或小问题启动成本占主导。

参考思路：bias+ReLU 可按逐元素独占输出融合；scale+Softmax 需要重新推导最大值，尤其 scale 为负时，原输入最大值不再映射为缩放后最大值。题目中的“简单融合”也可能改变归约逻辑。

完成后再进入 warp shuffle、LayerNorm、转置、卷积与 Attention；本轮不把这些高级专题标为已经实现。

[上一章](03-matmul-and-tiling.md) · [CUDA 目录](README.md) · [项目首页](../../README.md)
