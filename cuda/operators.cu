#include <cuda_runtime.h>
#include <math_constants.h>
#include "reference.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

// 教学入口，只对下方固定测试形状启动；不是通用 tensor 库接口。
namespace {
void check_cuda(cudaError_t status, const char* call) {
    if (status != cudaSuccess) throw std::runtime_error(std::string(call) + ": " + cudaGetErrorString(status));
}
#define CUDA_CHECK(call) check_cuda((call), #call)
class DeviceBuffer {
public:
    explicit DeviceBuffer(std::size_t count) {
        if (count) CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&data_), count * sizeof(float)));
    }
    ~DeviceBuffer() { if (data_) cudaFree(data_); } // 析构不抛异常；正常操作逐次检查。
    DeviceBuffer(const DeviceBuffer&) = delete;
    DeviceBuffer& operator=(const DeviceBuffer&) = delete;
    float* get() const { return data_; }
private:
    float* data_ = nullptr;
};
void upload(DeviceBuffer& device, const std::vector<float>& host) {
    if (!host.empty()) CUDA_CHECK(cudaMemcpy(device.get(), host.data(), host.size()*sizeof(float), cudaMemcpyHostToDevice));
}
std::vector<float> download(const DeviceBuffer& device, std::size_t count) {
    std::vector<float> host(count);
    if (count) CUDA_CHECK(cudaMemcpy(host.data(), device.get(), count*sizeof(float), cudaMemcpyDeviceToHost));
    return host;
}
void finish_launch() {
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDeviceSynchronize()); // 正确性教学：立即暴露异步执行错误。
}
constexpr unsigned Block = 256; // reduction/softmax 要求 2 的幂，且 launch 与模板一致。
constexpr unsigned Tile = 16;

__global__ void add_kernel(const float* a, const float* b, float* out, std::size_t n) {
    std::size_t i = static_cast<std::size_t>(blockIdx.x)*blockDim.x + threadIdx.x;
    const std::size_t stride = static_cast<std::size_t>(gridDim.x)*blockDim.x;
    for (; i < n; i += stride) out[i] = a[i] + b[i];
}

template<unsigned B>
__global__ void reduce_kernel(const float* input, float* partial, std::size_t n) {
    static_assert(B > 0 && (B & (B - 1)) == 0, "block must be a power of two");
    __shared__ float scratch[B];
    unsigned t = threadIdx.x;
    std::size_t i = static_cast<std::size_t>(blockIdx.x)*B + t;
    scratch[t] = i < n ? input[i] : 0.0f;
    __syncthreads();
    for (unsigned stride = B/2; stride > 0; stride /= 2) {
        if (t < stride) scratch[t] += scratch[t+stride];
        __syncthreads();
    }
    if (t == 0) partial[blockIdx.x] = scratch[0];
}

__global__ void matmul_naive(const float* a, const float* b, float* c, int m, int k, int n) {
    int row = static_cast<int>(blockIdx.y*blockDim.y+threadIdx.y);
    int col = static_cast<int>(blockIdx.x*blockDim.x+threadIdx.x);
    if (row >= m || col >= n) return;
    float acc = 0;
    for (int inner=0; inner<k; ++inner) acc += a[row*k+inner]*b[inner*n+col];
    c[row*n+col] = acc;
}

__global__ void matmul_tiled(const float* a, const float* b, float* c, int m, int k, int n) {
    __shared__ float tile_a[Tile][Tile], tile_b[Tile][Tile];
    unsigned x=threadIdx.x, y=threadIdx.y;
    int row=static_cast<int>(blockIdx.y*Tile+y), col=static_cast<int>(blockIdx.x*Tile+x);
    float acc=0;
    for (int base=0; base<k; base+=Tile) {
        int ak=base+static_cast<int>(x), bk=base+static_cast<int>(y);
        tile_a[y][x] = row<m && ak<k ? a[row*k+ak] : 0;
        tile_b[y][x] = bk<k && col<n ? b[bk*n+col] : 0;
        __syncthreads();
        for (unsigned inner=0; inner<Tile; ++inner) acc += tile_a[y][inner]*tile_b[inner][x];
        __syncthreads(); // 所有人用完这一块，才能覆盖下一块。
    }
    if (row<m && col<n) c[row*n+col]=acc;
}

template<unsigned B>
__global__ void softmax_rows(const float* input, float* output, int cols) {
    static_assert(B > 0 && (B & (B - 1)) == 0, "block must be a power of two");
    __shared__ float scratch[B];
    unsigned t=threadIdx.x;
    std::size_t base=static_cast<std::size_t>(blockIdx.x)*cols;
    float local_max=-CUDART_INF_F;
    for (int col=static_cast<int>(t); col<cols; col+=B) local_max=fmaxf(local_max,input[base+col]);
    scratch[t]=local_max;
    __syncthreads();
    for (unsigned stride=B/2; stride>0; stride/=2) {
        if (t<stride) scratch[t]=fmaxf(scratch[t],scratch[t+stride]);
        __syncthreads();
    }
    const float maximum=scratch[0];
    __syncthreads(); // 所有线程先读 maximum，再重用 scratch 存 sum。
    float local_sum=0;
    for (int col=static_cast<int>(t); col<cols; col+=B) local_sum+=expf(input[base+col]-maximum);
    scratch[t]=local_sum;
    __syncthreads();
    for (unsigned stride=B/2; stride>0; stride/=2) {
        if (t<stride) scratch[t]+=scratch[t+stride];
        __syncthreads();
    }
    const float denominator=scratch[0];
    for (int col=static_cast<int>(t); col<cols; col+=B)
        output[base+col]=expf(input[base+col]-maximum)/denominator;
}

std::vector<float> data(std::size_t n, unsigned seed) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> dist(-1,1);
    std::vector<float> result(n);
    for (float& x : result) x=dist(gen);
    return result;
}
void compare(const std::vector<float>& actual, const std::vector<float>& expected,
             double atol=1e-5, double rtol=1e-4) {
    if (actual.size()!=expected.size()) throw std::runtime_error("output shape mismatch");
    for (std::size_t i=0; i<actual.size(); ++i)
        if (!cuda_study::near(actual[i],expected[i],atol,rtol))
            throw std::runtime_error("numerical mismatch at index " + std::to_string(i));
}
void test_add() {
    for (std::size_t n : {0u,1u,255u,256u,257u,100003u}) {
        auto a=data(n,1), b=data(n,2);
        DeviceBuffer da(n),db(n),dc(n); upload(da,a); upload(db,b);
        if (n) {
            unsigned grid=static_cast<unsigned>(std::min<std::size_t>((n+Block-1)/Block,32));
            add_kernel<<<grid,Block>>>(da.get(),db.get(),dc.get(),n); finish_launch();
        }
        compare(download(dc,n),cuda_study::vector_add(a,b));
    }
}
float gpu_sum(const std::vector<float>& a) {
    if (a.empty()) return 0;
    DeviceBuffer input(a.size()), scratch1(a.size()), scratch2(a.size());
    upload(input,a);
    const float* source=input.get(); float* destination=scratch1.get();
    std::size_t count=a.size();
    // 至少执行一次：n=1 时也把结果写入 scratch。
    do {
        unsigned blocks=static_cast<unsigned>((count+Block-1)/Block);
        reduce_kernel<Block><<<blocks,Block>>>(source,destination,count); finish_launch();
        count=blocks; source=destination;
        destination=destination==scratch1.get() ? scratch2.get() : scratch1.get();
    } while (count>1);
    float result=0;
    CUDA_CHECK(cudaMemcpy(&result,source,sizeof(float),cudaMemcpyDeviceToHost));
    return result;
}
class Event {
public:
    Event() { CUDA_CHECK(cudaEventCreate(&event_)); }
    ~Event() { cudaEventDestroy(event_); }
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
    cudaEvent_t get() const { return event_; }
private:
    cudaEvent_t event_{};
};
void benchmark_add() {
    constexpr std::size_t n=1u<<22;
    constexpr int repeats=100;
    auto a=data(n,11), b=data(n,12);
    DeviceBuffer da(n),db(n),dc(n); upload(da,a); upload(db,b);
    unsigned grid=static_cast<unsigned>((n+Block-1)/Block);
    add_kernel<<<grid,Block>>>(da.get(),db.get(),dc.get(),n); finish_launch(); // warmup
    Event start,stop;
    CUDA_CHECK(cudaEventRecord(start.get()));
    for (int i=0;i<repeats;++i) {
        add_kernel<<<grid,Block>>>(da.get(),db.get(),dc.get(),n);
        CUDA_CHECK(cudaGetLastError());
    }
    CUDA_CHECK(cudaEventRecord(stop.get()));
    CUDA_CHECK(cudaEventSynchronize(stop.get()));
    float elapsed=0;
    CUDA_CHECK(cudaEventElapsedTime(&elapsed,start.get(),stop.get()));
    compare(download(dc,n),cuda_study::vector_add(a,b));
    double ms=elapsed/repeats;
    if (!(ms>0)) throw std::runtime_error("timer resolution insufficient");
    std::cout << "vector_add kernel-only mean_ms=" << ms
              << " effective_GB_per_s=" << (3.0*n*sizeof(float))/(ms*1e6)
              << " (logical bytes, not measured DRAM traffic)\n";
}

void test_reduce() {
    for (std::size_t n : {0u,1u,255u,256u,257u,65537u,100003u}) {
        auto a=data(n,7);
        // 对抵消敏感的和使用绝对误差，不仅检查相对误差。
        if (!cuda_study::near(gpu_sum(a),cuda_study::sum(a),2e-3,2e-4))
            throw std::runtime_error("sum mismatch");
        std::fill(a.begin(),a.end(),1);
        if (gpu_sum(a)!=static_cast<float>(n)) throw std::runtime_error("ones sum mismatch");
    }
}
void test_matmul() {
    for (auto shape : {std::array<int,3>{1,1,1}, {3,5,7}, {16,16,16}, {17,19,23}, {31,0,7}, {0,5,7}, {3,5,0}}) {
        int m=shape[0],k=shape[1],n=shape[2];
        auto a=data(static_cast<std::size_t>(m)*k,3),b=data(static_cast<std::size_t>(k)*n,4);
        auto expected=cuda_study::matmul(a,b,m,k,n);
        DeviceBuffer da(a.size()),db(b.size()),dc(expected.size()); upload(da,a); upload(db,b);
        if (m && n) {
            dim3 block(Tile,Tile), grid((n+Tile-1)/Tile,(m+Tile-1)/Tile);
            matmul_naive<<<grid,block>>>(da.get(),db.get(),dc.get(),m,k,n); finish_launch();
            compare(download(dc,expected.size()),expected);
            matmul_tiled<<<grid,block>>>(da.get(),db.get(),dc.get(),m,k,n); finish_launch();
        }
        compare(download(dc,expected.size()),expected);
    }
}
void test_softmax() {
    for (int cols : {1,7,255,256,257,1025}) {
        constexpr int rows=3;
        auto a=data(rows*cols,5);
        for (int c=0;c<cols;++c) {
            a[c]=10000;                        // 相等的大值，不应 exp 溢出。
            a[cols+c]=c==0 ? 1000 : -1000;   // 单个主导项。
        }
        DeviceBuffer da(a.size()),db(a.size()); upload(da,a);
        softmax_rows<Block><<<rows,Block>>>(da.get(),db.get(),cols); finish_launch();
        auto out=download(db,a.size()); compare(out,cuda_study::softmax(a,rows,cols));
        for (int row=0;row<rows;++row) {
            double total=0;
            for (int c=0;c<cols;++c) {
                if (out[row*cols+c]<0) throw std::runtime_error("negative probability");
                total+=out[row*cols+c];
            }
            if (!cuda_study::near(total,1)) throw std::runtime_error("row sum mismatch");
        }
    }
}
}
int main(int argc,char** argv) {
    try {
        if (argc!=2) throw std::invalid_argument("choose vector_add|reduce|matmul|softmax|bench_add");
        const std::string mode=argv[1];
        if (mode!="vector_add" && mode!="reduce" && mode!="matmul" && mode!="softmax" && mode!="bench_add")
            throw std::invalid_argument("unknown operator");
        int devices=0;
        cudaError_t status=cudaGetDeviceCount(&devices);
        if (status==cudaErrorNoDevice || (status==cudaSuccess && devices==0)) {
            std::cout << "SKIP: no CUDA device\n"; return 77;
        }
        CUDA_CHECK(status); // 驱动初始化等错误作为失败报告，不伪装为测试成功。
        CUDA_CHECK(cudaSetDevice(0));
        if (mode=="vector_add") test_add();
        else if (mode=="reduce") test_reduce();
        else if (mode=="matmul") test_matmul();
        else if (mode=="softmax") test_softmax();
        else benchmark_add();
        std::cout << "CUDA " << mode << " OK\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; return EXIT_FAILURE;
    }
}
