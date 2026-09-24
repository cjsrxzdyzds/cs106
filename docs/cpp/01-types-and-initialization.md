# C++ 01 · 类型、初始化、引用与 `const`

> CS106L：Types & Structs、Initialization & References
> 作业：HW1（`Matrix` 类型别名、`size_t` 与 `int`、按引用传参）

## 1. 静态类型与类型推导

C++ 是静态类型语言：每个表达式的类型在编译期确定。`auto` 不是动态类型，而是让编译器用**模板实参推导的规则**替你写出类型。

```cpp
auto a = 1;              // int
auto b = 1.0;            // double
auto c = "hi";           // const char*   ⚠️ 不是 std::string
auto d = std::string{"hi"};
const auto& e = d;       // const std::string&
auto f = e;              // std::string —— auto 会丢掉引用与顶层 const
decltype(auto) g = e;    // const std::string& —— 保留表达式的精确类型
```

规则要点：

| 写法 | 推导结果 | 常见用途 |
|---|---|---|
| `auto x = expr;` | 去掉引用和顶层 `const`，数组/函数退化为指针 | 局部变量 |
| `auto& x = expr;` | 保留 `const`，绑定左值 | 遍历并修改元素 |
| `const auto& x = expr;` | 可以绑定任何东西（包括临时量） | 只读遍历 |
| `auto&& x = expr;` | 转发引用：左值→`T&`，右值→`T&&` | 泛型代码、range-for 的内部实现 |

### 类型别名

HW1 用 `using Matrix = std::vector<std::vector<double>>;` 定义别名。`using` 比 `typedef` 更易读，而且可以模板化：

```cpp
template <typename T>
using Grid = std::vector<std::vector<T>>;
Grid<double> m;
```

⚠️ HW1 的头文件里还写了 `using std::vector;`。**头文件中的 `using` 声明/指令会污染所有包含它的翻译单元**，应只在 `.cpp` 或函数作用域里使用。

### 结构化绑定（C++17）

```cpp
std::map<std::string, double> wallets{{"ali", 5}, {"hamed", 5}};
for (const auto& [id, coins] : wallets)   // HW2 的 show_wallets 可以这样写
    std::cout << id << " : " << coins << '\n';

auto [q, r] = std::div(17, 5);            // 解包 struct
```

## 2. 初始化：统一初始化与它的坑

```cpp
int a = 1;          // 拷贝初始化
int b(1);           // 直接初始化
int c{1};           // 列表初始化（brace init），禁止窄化转换
int d{};            // 值初始化为 0
int e;              // ⚠️ 局部变量：未初始化，读取是 UB

double x = 3.7;
int f{x};           // 编译错误：narrowing
int g = x;          // 编译通过，静默截断为 3
```

列表初始化的一个经典陷阱——`std::initializer_list` 构造函数优先：

```cpp
std::vector<int> v1(3, 0);   // {0, 0, 0}
std::vector<int> v2{3, 0};   // {3, 0}   ← 选择了 initializer_list<int> 构造函数
```

HW1 里 `vector<double>(m, 0)` 用圆括号是正确的；若改成花括号就成了“两个元素的向量”。

HW3 的 `BST(std::initializer_list<int>)` 正是利用这一机制，让 `BST bst{5, 3, 8};` 成为可能。

> 🔧 编译器视角：“最令人烦恼的解析”（most vexing parse）——`Timer t();` 声明的是一个函数而不是对象。
> 花括号初始化 `Timer t{};` 不存在这种歧义，这也是 CS106L 推荐“默认使用花括号”的原因之一。

## 3. 引用与指针

| | 引用 `T&` | 指针 `T*` |
|---|---|---|
| 可否为空 | 不可（语义上） | 可以（`nullptr`） |
| 可否重新绑定 | 不可 | 可以 |
| 用途 | 参数传递、别名、返回内部元素 | 可选对象、所有权转移（用智能指针）、数据结构链接 |

HW1 的 `double& element(Matrix&, int, int)` 返回引用，使调用者可以写 `element(m, 0, 0) = 3;`。

⚠️ **悬空引用**：绝不能返回局部变量的引用。HW3 作者在注释里意识到了这一点（后置 `operator++` 不能返回局部 `BST` 的引用），但改用 `new BST(*this)` 并返回 `*temp` 的引用，又引入了内存泄漏。正确答案是**按值返回**，见 [cpp/05](05-classes-const-operators.md)。

### 参数传递的默认规则

| 参数类型 | 只读 | 需要修改 | 需要“拿走”（sink） |
|---|---|---|---|
| 便宜的类型（`int`, `double`, 指针, `std::string_view`） | `T` | `T&` | `T` |
| 其他类型 | `const T&` | `T&` | `T`（然后 `std::move`）或 `T&&` |

HW1 大多数函数签名是 `const Matrix&`，符合规则；但 HW2 中 `Server::get_client(const std::string id)` 按值传入 `const std::string`，既拷贝又无法移动，应当是 `const std::string&` 或 `std::string_view`。

## 4. `const`

```cpp
const int* p1;        // 指向 const int 的指针：*p1 不可改，p1 可改
int* const p2 = &x;   // const 指针：p2 不可改，*p2 可改
const int* const p3;  // 都不可改
```

从右往左读即可。HW2 中 `Client` 的成员 `Server* const server;` 是“不可重新指向的指针”，但通过它仍可以调用 `Server` 的非 `const` 成员函数。
作业原始要求是 `Server const* const server;`（指向常量 Server 的常量指针），这会迫使 `Server::get_wallet` 等函数被声明为 `const` 成员函数——这正是作业想训练的 `const` 正确性，见 [cpp/05](05-classes-const-operators.md)。

`constexpr` 与 `consteval`：

```cpp
constexpr double kReward = 6.25;           // 编译期常量
constexpr std::size_t square(std::size_t n) { return n * n; }
static_assert(square(4) == 16);            // 编译期求值
```

## 5. 整数类型：`size_t` 与 `int`

HW1 中出现了大量这样的代码：

```cpp
int rowNum(const Matrix& m) { return m.size(); }         // size_t → int，可能截断
for (int i = 0; i < n; i++)                               // n 是 size_t：有符号/无符号比较
if (r1 > rowNum(matrix) - 1) throw ...;                  // r1 是 size_t
```

最后一行的问题：当 `matrix` 为空时 `rowNum(matrix) - 1 == -1`，比较时 `-1` 被转换为 `size_t` 的最大值，于是越界检查**永远通过**。这是 C/C++ 的“usual arithmetic conversions”规则导致的。

建议：

- 容器下标、大小统一使用 `std::size_t`（或 C++20 的 `std::ssize()` 获得有符号大小）；
- 打开 `-Wsign-compare -Wconversion`；
- 对“减 1”要格外警惕，改写为 `r1 >= rows` 形式。

## 6. `std::optional`：表达“可能没有值”

CS106L 的 Type Safety 一讲介绍了 `std::optional`。对比 HW2 的写法：

```cpp
// 作业签名：用输出参数 + bool / 异常表达失败
static bool parse_trx(std::string trx, std::string& sender, std::string& receiver, double& value);

// 更类型安全的写法（labs/lab2_mining/mining.cpp）
std::optional<Trx> parse_trx(const std::string& s);
if (auto trx = parse_trx(s)) use(trx->sender);
```

同理，HW3 的 `find_node` 返回 `Node**`，用 `nullptr` 表达“没找到”；对于只读查询，返回 `Node*` 或 `std::optional<std::reference_wrapper<Node>>` 更清楚。

## 练习

1. 写出下列变量的类型：`auto a = {1, 2};`、`auto b{1};`、`const auto c = &x;`（`x` 为 `int`）。
2. 修改 HW1 的 `ero_swap`，使其在空矩阵上也能正确抛出异常，且没有有符号/无符号比较警告。
3. 把 HW2 的 `parse_trx` 改写为返回 `std::optional<Trx>` 的版本，并思考：什么时候异常比 `optional` 更合适？
