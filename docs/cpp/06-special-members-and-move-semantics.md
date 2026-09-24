# C++ 06 · 特殊成员函数与移动语义

> CS106L：Special Member Functions、Move Semantics
> 作业：HW3（`BST` 的拷贝/移动构造与赋值）、HW5（`Cappuccino`/`Mocha` 的深拷贝）
> 这一章是理解 RAII、智能指针以及 `std::thread`/`std::unique_lock`/`std::future` 所有权语义的基础。

## 1. 六个特殊成员函数

```cpp
class Widget {
public:
    Widget();                              // 默认构造
    ~Widget();                             // 析构
    Widget(const Widget&);                 // 拷贝构造
    Widget& operator=(const Widget&);      // 拷贝赋值
    Widget(Widget&&) noexcept;             // 移动构造（C++11）
    Widget& operator=(Widget&&) noexcept;  // 移动赋值（C++11）
};
```

编译器会在需要时隐式生成它们，默认行为是**逐成员**执行对应操作。对于持有裸指针的类，逐成员拷贝指针 = **浅拷贝**，两个对象指向同一块内存，析构时 double free。

隐式生成规则（简化版，完整表格见 Howard Hinnant 的著名图表）：

| 你声明了… | 默认构造 | 析构 | 拷贝构造/赋值 | 移动构造/赋值 |
|---|---|---|---|---|
| 什么都没声明 | 生成 | 生成 | 生成 | 生成 |
| 任意构造函数 | **不生成** | 生成 | 生成 | 生成 |
| 析构函数 | 生成 | — | 生成（已弃用） | **不生成**（退化为拷贝） |
| 拷贝构造 | **不生成**（它也是构造函数） | 生成 | 拷贝赋值：生成（已弃用） | **不生成** |
| 拷贝赋值 | 生成 | 生成 | 拷贝构造：生成（已弃用） | **不生成** |
| 移动构造 | **不生成** | 生成 | **删除** | 移动赋值：不生成 |
| 移动赋值 | 生成 | 生成 | **删除** | 移动构造：不生成 |

## 2. Rule of Zero / Three / Five

- **Rule of Three**（C++98）：若你需要自定义析构、拷贝构造、拷贝赋值中的任何一个，那么通常三个都需要。
- **Rule of Five**（C++11）：再加上移动构造和移动赋值。
- **Rule of Zero**：最好的做法是**一个都不写**——用管理资源的成员（`std::vector`、`std::string`、`std::unique_ptr`）来组合出类，让编译器生成的版本自动正确。

HW5 是 Rule of Three 的反面教材。`EspressoBased` 持有 `std::vector<Ingredient*>` 并在析构中 `delete` 它们，但：

```cpp
// 原实现 mocha.cpp
Mocha::Mocha(const Mocha& cap) {
    name = cap.name;
    ingredients = cap.ingredients;   // 浅拷贝：两个 Mocha 共享同一批 Ingredient*
}
```

用 AddressSanitizer 运行一个最小程序 `Mocha a; { Mocha b(a); }`，`b` 析构时 `delete` 了所有配料，`a` 析构时再次访问——ASan 报告 **heap-use-after-free**。

而 `Cappuccino::operator=` 直接 `ingredients.clear()`，没有 `delete` 旧的配料，ASan 的 LeakSanitizer 在课程的 TEST8 上报告了泄漏。

如果把 `std::vector<Ingredient*>` 换成 `std::vector<std::unique_ptr<Ingredient>>`，泄漏和 double free 在**类型层面**就不可能发生；深拷贝则通过虚函数 `clone()` 实现（见 [cpp/08](08-inheritance-and-polymorphism.md) 和 [lab5](../../labs/lab5_thread_pool/thread_pool_test.cpp)）。

## 3. 值类别与右值引用

每个表达式都有一个**类型**和一个**值类别**：

```
            expression
           /          \
      glvalue        rvalue
      /     \       /      \
  lvalue    xvalue      prvalue
```

- **lvalue**：有身份、不能被移动，例如变量名 `x`、`*p`、`v[0]`；
- **prvalue**：纯右值，例如字面量 `42`、`a + b`、返回值类型为非引用的函数调用 `f()`；
- **xvalue**：“将亡值”，有身份但可以被移动，例如 `std::move(x)`。

`T&&` 只能绑定右值（prvalue 或 xvalue）。于是重载决议可以区分“可以窃取资源的对象”与“必须保留的对象”：

```cpp
void push_back(const T& x);  // 拷贝
void push_back(T&& x);       // 移动：x 马上就要消亡，可以偷走它的资源
```

**`std::move` 什么都不移动**，它只是一个 `static_cast<T&&>`，把左值标记为可以被移动。真正的移动发生在被调用的移动构造/移动赋值中。

⚠️ 一个具名的右值引用本身是左值：

```cpp
void f(Widget&& w) {
    Widget a = w;             // 拷贝！w 有名字，是左值
    Widget b = std::move(w);  // 移动
}
```

## 4. 移动构造与移动赋值的正确写法

以 HW3 的 `BST` 为例：

```cpp
BST::BST(BST&& other) noexcept
    : root(std::exchange(other.root, nullptr)) {}   // 偷走指针，并把源对象置为有效的空状态

BST& BST::operator=(BST&& other) noexcept {
    if (this != &other) {
        destroy(root);                               // ⚠️ 先释放自己的旧树
        root = std::exchange(other.root, nullptr);
    }
    return *this;
}
```

HW3 原实现的移动赋值漏掉了释放旧树，拷贝赋值 `root = buildBST_Recur(bst.root)` 也一样——两处都泄漏。
另外，作者在移动构造的注释中写道“需要把 bst.root 置空，因为测试里传进来的可能是 `std::move` 出来的”，这个理解是正确的：**被移动后的对象仍会被析构**，必须处于“有效但未指定”的状态。

### 为什么要 `noexcept`？

`std::vector` 扩容时需要把旧元素搬到新内存。如果元素的移动构造可能抛异常，搬到一半失败就无法恢复原状，因此 `vector` 只在移动构造为 `noexcept` 时才使用移动，否则退回拷贝（`std::move_if_noexcept`）。**忘记 `noexcept` 会让 `vector<BST>` 扩容时对每棵树做深拷贝。**

## 5. Copy-and-swap：一次写对拷贝赋值和移动赋值

```cpp
class BST {
public:
    BST(const BST& other) : root(clone(other.root)) {}
    BST(BST&& other) noexcept : root(std::exchange(other.root, nullptr)) {}

    BST& operator=(BST other) noexcept {   // 按值传参：左值实参→拷贝构造，右值实参→移动构造
        swap(*this, other);
        return *this;
    }                                        // other 带着旧树析构

    friend void swap(BST& a, BST& b) noexcept { std::swap(a.root, b.root); }
    ~BST() { destroy(root); }
};
```

优点：

- 自赋值自动正确（先完整拷贝，再交换）；
- **强异常安全保证**：若拷贝抛异常，`*this` 完全没有被修改；
- 释放旧资源的逻辑只写在析构函数里一处。

代价是自赋值时多一次拷贝，以及对于可以复用已有缓冲区的类型（如 `vector` 赋值给容量足够的 `vector`）会损失一些性能。[lab4 的 `SharedPtr`](../../labs/lab4_smart_ptr/smart_ptr.h) 与 [lab5 的 `Drink`](../../labs/lab5_thread_pool/thread_pool_test.cpp) 都采用了这种写法。

## 6. 拷贝省略（Copy Elision）与 NRVO

```cpp
BST make_tree() { return BST{5, 3, 8}; }   // C++17 起保证省略：直接在调用者的存储上构造
BST make_tree2() {
    BST t{5, 3, 8};
    return t;                               // NRVO：允许但不保证省略；不省略时会隐式移动
}
```

- 返回 prvalue 时，C++17 **保证**不调用拷贝/移动构造；
- 返回局部变量时（NRVO），编译器**可以**省略；即使不省略，也会先尝试把 `t` 当作右值移动；
- ⚠️ 不要写 `return std::move(t);`，这反而阻止了 NRVO。

> 🔧 编译器视角：NRVO 不保证发生，这一点在并发代码里会造成真实的 bug。[lab1](../../labs/lab1_matmul/matmul.cpp) 的
> `mul_parallel` 中，如果在 `return C;` 时工作线程还没 join，那么当编译器选择“移动”而不是 NRVO 时，
> `C` 的缓冲区在工作线程仍在写入时就被移走了。修复方法是用内层作用域保证先 join 再 return——
> 不要让程序的正确性依赖于一个可选的优化。

## 7. 只能移动的类型

有些资源天然不可复制：独占的内存（`std::unique_ptr`）、线程（`std::thread`）、锁的所有权（`std::unique_lock`）、文件句柄、一次性的结果通道（`std::promise`/`std::future`）。它们都**删除拷贝、提供移动**：

```cpp
std::thread t1([] { work(); });
std::thread t2 = t1;              // 编译错误
std::thread t2 = std::move(t1);   // ✅ 所有权转移，t1 不再代表任何线程
std::vector<std::thread> pool;
pool.push_back(std::move(t2));
```

HW4 的 `UniquePtr` 本应是这样一个类型，但原实现声明了拷贝构造函数，并在其中写了 `static_assert(true, "...")`——条件恒为真，所以断言永远不会触发，拷贝构造“成功”编译，而且连成员 `_p` 都没有初始化。正确做法是 `= delete`，见 [HW4 精讲](../homework/code-review.md#hw4)。

## 练习

1. 用 copy-and-swap 重写 HW3 的两个赋值运算符，并用 ASan 验证不再泄漏。
2. 构造一个 `std::vector<BST>`，分别在移动构造有无 `noexcept` 的情况下 `push_back` 1000 次，统计拷贝构造被调用的次数。
3. 为 HW5 的 `EspressoBased` 写出 Rule of Zero 版本：成员应该是什么类型？还需要手写哪些特殊成员函数？
