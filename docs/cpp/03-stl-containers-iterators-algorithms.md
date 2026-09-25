# C++ 03 · STL：容器、迭代器与算法

> CS106L：Sequence Containers、Associative Containers & Iterators、Algorithms
> 作业：HW1（`vector<vector<double>>`、`std::transform`）、HW2（`std::map`）、HW3（`std::queue`）、HW6（`std::sort`、`std::priority_queue`）
> CMU 参考：15-213 第 6 章 *The Memory Hierarchy*（数据布局与局部性）
> 实验：[lab1_matmul](../../labs/lab1_matmul/matmul.cpp)

STL 的设计核心是把**容器**（存储）和**算法**（操作）通过**迭代器**（访问协议）解耦：
M 个容器 × N 个算法只需要 M + N 份实现，而不是 M × N 份。

## 1. 序列容器

| 容器 | 内存布局 | 随机访问 | 头部插入 | 尾部插入 | 中间插入 | 迭代器失效 |
|---|---|---|---|---|---|---|
| `std::vector` | 连续 | O(1) | O(n) | 均摊 O(1) | O(n) | 扩容时全部失效；插入/删除点之后失效 |
| `std::deque` | 分段连续 | O(1) | O(1) | O(1) | O(n) | 头尾插入使所有迭代器失效（引用不失效） |
| `std::list` | 链表 | O(n) | O(1) | O(1) | O(1)（已有迭代器） | 只有被删除的元素失效 |
| `std::array` | 连续、定长 | O(1) | — | — | — | — |

**默认使用 `std::vector`**。即使理论复杂度更差，连续内存带来的 cache 友好性和硬件预取在实践中往往更重要（Bjarne Stroustrup 有一个著名的演示：在中间插入的场景下 `vector` 依然快于 `list`）。

### 布局决定性能：HW1 的 `vector<vector<double>>`

HW1 用“向量的向量”表示矩阵，每一行是一次独立的堆分配，行与行之间不连续。
[lab1](../../labs/lab1_matmul/matmul.cpp) 把它改为一块连续内存 `std::vector<double>`（行主序，`a[i*n + j]`），并对比不同循环顺序：

```
n = 512, threads = 4（4 vCPU 容器中的一次测量，Release）
ijk (HW1 order) : 420 ms     最内层循环对 B 按列访问，步长 n×8 字节，几乎每次访问都 cache miss
ikj             :  48 ms     最内层对 B、C 都是步长为 1 的顺序访问，且可被编译器向量化
blocked         :  49 ms     分块；n 更大、超过 L2/L3 容量时优势才明显
parallel ikj    :  16 ms     4 线程按行划分
```

只交换两层循环就快了近 9 倍——这就是 15-213 Cache Lab 要让你体会的事情：**算法复杂度相同，访存模式不同，性能可以差一个数量级**。

## 2. 关联容器

| 容器 | 实现 | 查找 | 有序遍历 | 键的要求 |
|---|---|---|---|---|
| `std::map` / `std::set` | 红黑树 | O(log n) | ✅ | `operator<`（严格弱序） |
| `std::unordered_map` / `unordered_set` | 哈希表（链地址） | 均摊 O(1) | ❌ | `std::hash` + `operator==` |

`operator[]` 的语义是“查找，不存在就**插入**一个值初始化的元素”：

```cpp
std::map<std::string, int> m;
if (m["alice"] > 0) {}   // ⚠️ 副作用：插入了 {"alice", 0}；const map 上不能调用 operator[]
if (auto it = m.find("alice"); it != m.end() && it->second > 0) {}   // ✅
if (m.contains("alice")) {}                                            // C++20
```

HW4 的 `SharedPtr` 正是利用了 `operator[]` 的这个语义（`PointerToCountMap[ptr]++` 自动插入 0 再加 1），但这也意味着析构空指针时 `map[nullptr]` 会被插入并减为负数。

### 选对“键”

HW2 使用 `std::map<std::shared_ptr<Client>, double>`：键是**指针**，排序依据是地址。于是

- `get_client(id)` 只能线性扫描，O(n)；
- 遍历顺序取决于内存分配器，每次运行可能不同（不确定性）；
- 原实现遍历时写 `for (pair<shared_ptr<Client>, double> p : clients)`：循环变量按值声明（而且与元素类型 `pair<const shared_ptr<Client>, double>` 并不相同），**每次迭代都拷贝构造一个 pair**，其中拷贝 `shared_ptr` 意味着一次原子自增和一次原子自减（见 [concurrency/03](../concurrency/03-atomics-and-memory-order.md)）。
  写成 `const auto&` 就没有拷贝；若写成 `const pair<shared_ptr<Client>, double>&`，类型不匹配仍会**悄悄绑定到一个临时对象**上，照样拷贝——这是 `auto` 比手写类型更安全的一个具体例子。

更合理的数据结构是 `std::map<std::string, Account>` 或 `std::unordered_map`，以 id 为键。

## 3. 容器适配器

`std::stack`、`std::queue`（默认基于 `deque`）、`std::priority_queue`（默认基于 `vector` 的二叉堆）。

`priority_queue` 的比较器语义容易弄反：**比较器 `comp(a, b)` 返回 true 表示 a 的优先级更低**，默认的 `std::less` 得到大顶堆。HW6 q3 需要“权重最小的航班优先”，所以比较器写成 `a.weight > b.weight`：

```cpp
struct Compare {
    bool operator()(const Flight& a, const Flight& b) const {   // 注意：const 引用 + const 成员函数
        return a.weight() > b.weight();
    }
};
std::priority_queue<Flight, std::vector<Flight>, Compare> pq;
```

## 4. 迭代器

迭代器是“泛化的指针”。按能力分为（C++20 用 concepts 描述）：

```
input → forward → bidirectional → random_access → contiguous
         (forward_list)  (list, map)    (deque)      (vector, array, string)
```

算法按所需的最弱迭代器类别声明要求，例如 `std::sort` 需要随机访问迭代器，所以 `std::list` 要用成员函数 `list::sort`。

### 迭代器失效

```cpp
for (auto it = v.begin(); it != v.end(); ++it)
    if (*it == 0) v.push_back(1);   // ⚠️ push_back 可能扩容，it 失效 → UB
```

在单线程中这是“修改正在遍历的容器”；在多线程中，这正是“一个线程遍历、另一个线程插入”的数据竞争——STL 容器**不提供任何内部同步**（见 [concurrency/05](../concurrency/05-concurrent-data-structures.md)）。

## 5. 算法

`<algorithm>` 和 `<numeric>` 里有上百个算法。常用的：

| 类别 | 算法 |
|---|---|
| 查找 | `find`, `find_if`, `binary_search`, `lower_bound`, `any_of`/`all_of` |
| 变换 | `transform`, `for_each`, `copy`, `copy_if`, `fill`, `generate` |
| 归约 | `accumulate`, `reduce`（可并行）, `inner_product`, `transform_reduce` |
| 排序 | `sort`, `stable_sort`, `partial_sort`, `nth_element` |
| 删除 | `remove`/`remove_if` + `erase`（C++20：`std::erase_if`） |

HW1 中的几个例子：

```cpp
// 两个行向量逐元素相加
std::transform(r1.begin(), r1.end(), r2.begin(), out.begin(), std::plus<double>());

// 一行乘以常数：std::bind 写法（原实现）与 lambda 写法
std::transform(row.begin(), row.end(), row.begin(),
               std::bind(std::multiplies<double>(), std::placeholders::_1, c));
std::transform(row.begin(), row.end(), row.begin(), [c](double x) { return x * c; });   // 更清晰
```

CS106L 和 *Effective Modern C++*（Item 34）都建议**优先使用 lambda 而不是 `std::bind`**：可读性更好，也更容易被内联。

HW1 的 `dotProduct` 手写循环可以直接用 `std::inner_product`；
C++20 Ranges 让很多组合更自然：

```cpp
#include <ranges>
auto evens = v | std::views::filter([](int x) { return x % 2 == 0; })
               | std::views::transform([](int x) { return x * x; });
```

### erase–remove 惯用法

`std::remove_if` 并不真正删除元素（它不知道容器的存在），只是把保留的元素前移并返回新的逻辑结尾：

```cpp
v.erase(std::remove_if(v.begin(), v.end(), pred), v.end());   // C++17 及以前
std::erase_if(v, pred);                                         // C++20
```

### 并行算法（C++17）

```cpp
#include <execution>
std::sort(std::execution::par, v.begin(), v.end());
double s = std::reduce(std::execution::par_unseq, v.begin(), v.end());
```

GCC 的 libstdc++ 通过 Intel TBB 实现并行策略，链接时需要 `-ltbb`；没有 TBB 时会退化为串行。
使用 `par` 时，传入的函数对象**不得有数据竞争**；使用 `par_unseq` 时还不得加锁（可能在同一线程内被向量化交错执行）。

## 练习

1. 用 `perf stat -e cache-misses` 运行 `lab1_matmul 1024 1`，对比 ijk 与 ikj 的 cache miss 数量。
2. 把 HW1 的 `Matrix` 改为连续存储的类（参考 lab1 的 `struct Matrix`），并保持原有接口测试通过。
3. HW2 的 `clients` 改用 `std::unordered_map<std::string, ...>` 后，`show_wallets` 的输出顺序会发生什么变化？这对测试意味着什么？
