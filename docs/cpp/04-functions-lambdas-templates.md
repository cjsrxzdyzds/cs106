# C++ 04 · 函数对象、Lambda 与模板

> CS106L：Templates、Template Functions、Functions & Lambdas、（部分学期）Template Metaprogramming / Concepts
> 作业：HW1（`std::bind`/lambda）、HW3（`bfs(std::function<void(Node*&)>)`）、HW4（类模板）、HW6 q1（泛型梯度下降）、q3（比较器函数对象）

## 1. 可调用对象的四种形态

```cpp
double f(double x) { return x * x; }                  // 1. 函数（可退化为函数指针）
struct Square { double operator()(double x) const { return x * x; } };  // 2. 函数对象（functor）
auto sq = [](double x) { return x * x; };             // 3. lambda（编译器生成的 functor）
std::function<double(double)> g = sq;                 // 4. 类型擦除的包装器
```

它们都能被 `std::invoke(callable, args...)` 统一调用。区别在于**类型信息**：

| 形态 | 每个实例的类型 | 可否携带状态 | 能否内联 | 额外开销 |
|---|---|---|---|---|
| 函数指针 | 所有同签名函数共享 `double(*)(double)` | 否 | 通常不能（间接调用） | 间接跳转 |
| functor / lambda | **每个都是独立的类型** | 可以 | 可以（作为模板实参时） | 无 |
| `std::function` | 同签名共享一个类型 | 可以 | 通常不能 | 间接调用；大对象可能堆分配 |

> 🔧 编译器视角：`std::sort(v.begin(), v.end(), [](int a, int b){ return a < b; })` 往往比 C 的 `qsort` 快，
> 原因正是 lambda 有唯一类型，`sort` 针对它被**单态化**（monomorphization）实例化，比较操作被完全内联；
> 而 `qsort` 通过函数指针回调，编译器通常无法跨越这个间接调用做优化。

### HW6 q1：为什么需要模板而不是函数指针

作者的实现签名是 `double gradient_descent(double init, double step, double (*func)(double))`。课程测试里有这样的调用：

```cpp
struct Func { double operator()(double a) { return cos(a); } };
q1::gradient_descent(0.01, 0.01, Func{});                 // functor 不能转换为函数指针
q1::gradient_descent<double, Func>(0.0, 0.01);            // 显式给出模板实参、不传函数对象
```

所以这一题的本意是写一个**以可调用对象类型为模板参数**的函数（作者因此注释掉了 q1 的测试）。正确写法见 [HW6 精讲](../homework/code-review.md#hw6)。

## 2. Lambda 详解

```cpp
int base = 10;
auto a = [base](int x) { return base + x; };          // 按值捕获（拷贝进闭包）
auto b = [&base](int x) { return base + x; };         // 按引用捕获
auto c = [=, &base](int x) { ... };                   // 默认按值，base 按引用
auto d = [p = std::make_unique<int>(1)] { return *p; };  // 初始化捕获（C++14）：可以“移动”进闭包
auto e = [n = 0]() mutable { return ++n; };           // mutable：允许修改按值捕获的副本
auto f = [](auto x, auto y) { return x + y; };        // 泛型 lambda（C++14）：operator() 是模板
auto g = []<typename T>(const std::vector<T>& v) { return v.size(); };  // C++20 显式模板参数
```

lambda 等价于一个编译器生成的类：

```cpp
// [base](int x) { return base + x; } 大致等价于：
class __lambda_1 {
    int base;                                        // 捕获的变量成为成员
public:
    explicit __lambda_1(int b) : base(b) {}
    int operator()(int x) const { return base + x; }   // 默认是 const 成员函数
};
```

这解释了为什么要修改按值捕获的变量需要 `mutable`：`operator()` 默认是 `const` 的。

### ⚠️ 捕获与生命周期（并发编程的头号陷阱之一）

```cpp
std::thread make_worker() {
    int local = 42;
    return std::thread([&local] { use(local); });   // ❌ 线程运行时 local 可能已经销毁
}
```

规则：**当 lambda 的生命周期可能超过当前作用域时（线程、异步任务、回调、存入容器），不要按引用捕获局部变量**。
`labs/` 中的线程使用 `[&]` 的地方，线程都在同一作用域内被 join（例如通过 `std::jthread` 的析构），这是按引用捕获安全的前提。
[lab5](../../labs/lab5_thread_pool/thread_pool_test.cpp) 把每杯饮品的副本以 `[d = std::move(d)]` 移动进任务，正是为了避免与其他线程共享。

## 3. `std::function` 与类型擦除

HW3 的 `bfs` 接受 `std::function<void(Node*&)>`，好处是可以放进 `.cpp` 文件中实现（非模板），调用者可以传任何可调用对象：

```cpp
std::vector<Node*> nodes;
bst.bfs([&nodes](BST::Node*& node) { nodes.push_back(node); });
```

代价是每次回调都是一次间接调用。若 `bfs` 是性能热点，可以改为模板：

```cpp
template <typename F>
void bfs(F&& func);                  // 需要把实现放在头文件中
```

`std::function` 要求可调用对象**可拷贝**，因此不能直接存放 `std::packaged_task` 或捕获了 `unique_ptr` 的 lambda。
C++23 的 `std::move_only_function` 解决了这个问题；[lab5](../../labs/lab5_thread_pool/thread_pool.h) 在 C++20 下用 `shared_ptr` 包装 `packaged_task` 绕过这一限制。

## 4. 函数模板

```cpp
template <typename T>
T max_of(const std::vector<T>& v) {
    T best = v.front();
    for (const T& x : v) if (best < x) best = x;
    return best;
}
max_of(std::vector<int>{1, 3, 2});   // 推导 T = int
```

模板本身不是代码，**实例化**后才是。编译器在每个用到的翻译单元中实例化，链接器合并重复实例（它们是 inline/COMDAT 的）。这带来两个实际后果：

1. **模板的定义必须对使用点可见**，因此通常整个写在头文件里。HW4 把实现放在 `.hpp` 中并在 `.h` 末尾 `#include`，就是为此；
2. 类型错误要到实例化时才暴露，错误信息往往很长——C++20 **concepts** 让约束成为接口的一部分：

```cpp
template <typename F>
    requires std::invocable<F, double> &&
             std::convertible_to<std::invoke_result_t<F, double>, double>
double gradient_descent(double init, double step, F f);

// 或者简写：
double derivative(std::invocable<double> auto f, double x);
```

## 5. 类模板

HW4 的 `UniquePtr<T>` / `SharedPtr<T>` 是类模板。几个要点：

```cpp
template <typename T>
class SharedPtr {
    static std::map<T*, int> counts;       // 每个 T 都有一份独立的静态成员
};
template <typename T>
std::map<T*, int> SharedPtr<T>::counts{};  // 静态成员的类外定义（C++17 起可用 inline static 在类内定义）
```

- `SharedPtr<int>` 和 `SharedPtr<double>` 是**完全无关**的两个类型；
- 类模板实参推导（CTAD，C++17）：`std::vector v{1, 2, 3};` 推导出 `vector<int>`。HW2 中 `shared_ptr addedClient = make_shared<Client>(...)` 也依赖 CTAD。

## 6. 可变参数模板与完美转发

`std::make_unique<T>(args...)` 需要把任意参数原样传给 `T` 的构造函数：

```cpp
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {       // Args&& 在推导语境中是“转发引用”
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
```

- `Args&&` 绑定左值时推导为 `U&`，绑定右值时为 `U&&`（引用折叠规则）；
- `std::forward<Args>(args)` 在实参原本是右值时把它转回右值，从而触发移动而非拷贝。

对比 HW4 的 `U* make_unique(U value)`：按值接收一个已构造好的 `U`、再拷贝一次到堆上，并且返回裸指针——既不能转发构造参数，也失去了异常安全。完整实现见 [lab4 `smart_ptr.h`](../../labs/lab4_smart_ptr/smart_ptr.h)。

## 7. 编译期计算

```cpp
template <std::size_t N>
constexpr std::size_t factorial() { return N <= 1 ? 1 : N * factorial<N - 1>(); }

template <typename T>
void print(const T& x) {
    if constexpr (std::is_arithmetic_v<T>) std::cout << x;   // 不满足的分支不会被实例化
    else std::cout << x.to_string();
}
```

`if constexpr`、`constexpr` 函数和 concepts 已经取代了大部分传统的 SFINAE 模板元编程技巧。

## 练习

1. 把 HW3 的 `bfs` 改为函数模板版本，用 Compiler Explorer 对比两种版本在 `-O2` 下是否内联了回调。
2. 实现 `template <typename F> double derivative(F f, double x)`（中心差分），并用 concept 约束 `F`。
3. 解释：为什么 `std::function<void()> f = [p = std::make_unique<int>(1)] {};` 无法编译？
