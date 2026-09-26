# CUDA 03 · 矩阵乘法与 Shared Memory 分块

> 目标：从一线程一输出推导分块复用；解释两个屏障各自保护什么。
> 源码：[matmul_naive / matmul_tiled](../../cuda/operators.cu)；CPU 对照：[matmul](../../cuda/reference.h)。

## 1. 先明确形状与布局

A 为 M×K，B 为 K×N，C 为 M×N，均为连续行主序 float。计算 `C[row,col]=Σ A[row,inner]*B[inner,col]`，不含转置、batch、alpha/beta 或任意 stride。

A 的地址为 `row*K+inner`，B 为 `inner*N+col`，C 为 `row*N+col`。把 K 和 N 混写，在方阵上可能碰巧正确，所以必须测试非方阵。本课 host 测试只启动受控的小维度；kernel 用 int 索引，不是任意超大尺寸的接口。

CPU 参考使用 double 累加后转回 float，便于检测明显数值偏差；GPU 累加采用 float，允许预定的误差。K=0 定义为零矩阵；M=0 或 N=0 返回空输出，不启动 kernel。

## 2. 朴素版本：每个线程计算一个 C 元素

二维 block 的 x 对应列，y 对应行。每个线程通过 blockIdx 与 threadIdx 得到 row、col，越界则返回；其余线程沿 K 累加并写回自己唯一负责的元素。

```cpp
int row = blockIdx.y * blockDim.y + threadIdx.y;
int col = blockIdx.x * blockDim.x + threadIdx.x;
if (row >= M || col >= N) return;
float acc = 0;
for (int inner = 0; inner < K; ++inner)
    acc += A[row*K + inner] * B[inner*N + col];
C[row*N + col] = acc;
```

独占输出不需要 atomic。问题是邻近输出重复读取 A 的同一行片段与 B 的同一列片段。缓存可能有所帮助，但可以进一步在算法中显式复用。

与 [CPU 矩阵实验](../../labs/lab1_matmul/matmul.cpp) 对照：CPU 改变循环顺序让连续访问更友好；GPU 还需要考虑相邻线程的访问关系，不能只逐字翻译 CPU 的循环顺序。

## 3. T×T 输出块如何复用输入

固定 Tile=16，每个 block 有 16×16=256 个线程，负责 C 的一个 16×16 子块。沿 K 方向逐块推进：

1. 每线程搬一个 A 元素和一个 B 元素到 `tile_a[y][x]`、`tile_b[y][x]`。
2. 块内屏障，等待所有输入槽准备好。
3. 每线程循环 inner=0..15，更新自己的 acc。
4. 再次屏障，保证所有线程已经用完这一块，才能覆盖成下一块。

在一个完整 tile 中，加载 2T² 个 float 后供 T² 个输出各做 T 次乘加，约有 2T³ FLOP。相对每线程重复读取输入，这提供了显式复用机会。shared memory 本身也有访问与资源成本，不代表 Tile 越大越快。

两个屏障作用不同：第一个防止读到尚未搬入的数据，第二个防止快线程覆盖慢线程仍在读取的数据。删除第二个屏障后，错误可能只在某些调度下出现，数值偶尔正确不构成证明。

## 4. 尾部统一填零

M、K、N 不必是 16 的倍数。超出 A、B 有效范围的加载写 0，所有线程仍参加每轮协作，最后仅有效 row、col 写 C。零是乘加归约的中性填充值。

不能照搬朴素版本的早返回到分块版本：一个输出列越界的线程，仍可能负责搬入同块其他有效线程需要的 A 元素；一个输出行越界的线程，也可能搬入有效 B 元素。除了屏障协议，还要保证协作加载完整。

测试包含 3×5 乘 5×7、17×19 乘 19×23、完整 16×16 tile、K=0 与空输出。只有正方形且整块大小的测试不足以验证地址和边界。

## 5. 性能分析从证据开始

先通过 CPU 对照和内存检查，再比较 naive 与 tiled。若计时，复用第 01 章的 Event 方法，分配与传输保持相同范围，多次重复。计数约 `2*M*N*K` FLOP；计时为 ms 时，GFLOP/s 约为 `2*M*N*K/(ms*1e6)`。

更大 Tile 会占更多 shared memory 和线程资源，可能降低同时驻留的块数；更多寄存器也可能限制占用率。占用率是资源利用指标，不是性能目标本身。应通过实际测量找限制，而不是追求某个比率最大。[NVIDIA 性能指南](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/index.html)

本课未实现 Tensor Core、寄存器分块或流水线。工程中的通用 GEMM 可进一步与 cuBLAS 对照，但必须先统一布局、精度、转置和计时口径，不能把教学 kernel 与库默认设置的数字直接比较。

## 练习与参考思路

1. 画出 block(0,0)、thread(2,3) 在第一轮搬入的 A、B 坐标。**参考**：threadIdx.x=2、y=3 时，A[3,2] 与 B[3,2]；随后该线程计算 C[3,2]，使用整条 tile 行列。
2. 为 17×19 乘 19×23 算出输出 grid。**答案**：x=2、y=2；K 方向两轮，每轮都需尾部判断。
3. 删除第二个屏障，设计怎样验证这个修改错误。**参考**：使用多个 K tile、多次运行并执行 racecheck；同时解释读写覆盖的具体窗口，不能只靠偶发数值失败。
4. 将 Tile 参数化后比较 8、16、32。**验收**：启动尺寸与模板一致，检查设备限制，每个版本先验证再计时，不预设 32 最快。

[上一章](02-reduction.md) · [目录](README.md) · [下一章](04-softmax-and-validation.md)
