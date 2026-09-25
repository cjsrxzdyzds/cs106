# C++ 05 · 类、`const` 正确性与运算符重载

> CS106L：Classes、Const Correctness、Operator Overloading
> 作业：HW2（`Server`/`Client`、`static`、`friend`、`const` 成员函数）、HW3（嵌套类 `Node`、比较运算符、`++`、`<<`、`=`）

## 1. 类的职责：维护不变量

类的意义不是“把数据和函数放在一起”，而是**维护不变量（invariant）**：私有数据只能通过成员函数修改，而每个成员函数都保证调用前后不变量成立。

- HW3 `BST` 的不变量：对任意节点，左子树所有值 < 节点值 < 右子树所有值；
- HW2 `Server` 的不变量：钱包余额非负；所有 pending 交易的发送者余额足以支付。

这也是并发编程的出发点：**锁保护的不是变量，而是不变量**。在一个成员函数执行期间，不变量可能被暂时破坏，如果另一个线程恰好在此时观察对象，就会看到不一致的状态（见 [concurrency/02](../concurrency/02-mutexes-condition-variables-semaphores.md)）。

## 2. 构造函数与成员初始化列表

```cpp
Client::Client(std::string id, Server& server)
    : server(&server), id(std::move(id)) {        // 初始化列表：直接构造成员
    crypto::generate_key(public_key, private_key);
}
```

- 引用成员、`const` 成员、没有默认构造函数的成员**只能**在初始化列表中初始化；
- 在构造函数体内赋值 = 先默认构造、再赋值，多做一次工作；
- ⚠️ 成员按**类中声明的顺序**初始化，与初始化列表的书写顺序无关。HW2 原实现写的是 `: id(id), server(&server)`，而 `server` 声明在 `id` 之前，GCC 的 `-Wreorder` 会给出警告。若某个成员的初始化依赖另一个成员，顺序错了就会读到未初始化的值。

单参数构造函数默认可用于隐式转换，一般应标记 `explicit`：

```cpp
class UniquePtr { public: explicit UniquePtr(T* p); };
void f(UniquePtr<int>);
f(new int(1));     // 若没有 explicit，会隐式构造一个临时 UniquePtr，在 f 返回时 delete 掉这个指针
```

## 3. `const` 正确性

`const` 成员函数承诺不修改对象的可观察状态：

```cpp
class Client {
public:
    std::string get_id() const;          // 可在 const Client& 上调用
    double get_wallet() const;
};
```

> 🔧 编译器视角：成员函数有一个隐式参数 `this`。普通成员函数中 `this` 的类型是 `Client*`，`const` 成员函数中是 `const Client*`。
> 所以 `const` 修饰的其实是 `this` 指向的对象，重载决议也据此区分 `get_root()` 与 `get_root() const`。

`const` 具有传染性，这正是 HW2 想训练的内容：作业要求 `Client` 持有 `Server const* const server`，那么 `Client::get_wallet()` 通过它调用的 `Server::get_wallet()` 也必须是 `const` 的。原实现把成员改成了 `Server* const`，绕开了这个约束。

HW3 同时提供了两个重载：

```cpp
Node*& get_root();          // 非 const：返回“根指针本身”的引用，调用者可以改写根
Node*  get_root() const;    // const：只能拿到一个副本
```

这是标准库 `operator[]`、`begin()` 等接口的惯用模式。

### `mutable` 与线程安全

`const` 成员函数中要修改的“非可观察状态”（缓存、互斥量）用 `mutable` 标记：

```cpp
class Ledger {
public:
    double wallet(const std::string& id) const {
        std::shared_lock lk(m_);   // 读操作也要加锁，所以 m_ 必须是 mutable
        ...
    }
private:
    mutable std::shared_mutex m_;
};
```

C++11 起标准库对 `const` 有一个约定：**`const` 成员函数可以被多个线程同时调用而不产生数据竞争**。因此，自己的类型若在 `const` 成员函数中修改 `mutable` 成员，就必须自己加同步——[lab2](../../labs/lab2_mining/mining.cpp) 的 `Ledger` 与 [lab3](../../labs/lab3_bst/concurrent_bst.cpp) 的各个树都遵循这一点。

## 4. `static` 成员与全局状态

HW2 的 `parse_trx` 被声明为 `static` 成员函数：它不访问任何对象状态，本质上是放在类作用域里的普通函数。

作业还要求把 `std::vector<std::string> pending_trxs;` 定义为**全局变量**：头文件里 `extern` 声明，某个 `.cpp` 中定义。这是一个值得反思的设计：

- 所有 `Server` 对象共享同一个 pending 列表；
- 测试之间通过全局状态互相影响（测试执行顺序改变，结果就可能改变）；
- 在多线程下，它是一个没有任何保护的共享可变状态。

[lab2](../../labs/lab2_mining/mining.cpp) 把它改为 `Ledger` 的私有成员，由同一把锁保护。

## 5. `friend`

`friend` 授予一个非成员函数（或另一个类）访问私有成员的权限。HW2 用它实现 `show_wallets(const Server&)`；HW3 用它实现 `operator<<` 与 `int < Node` 这样的比较。

`friend` 不破坏封装——它是类接口的一部分，由类自己声明。但要避免滥用：HW3 把 `nodesOfBST`、`buildBST_Recur` 这类内部辅助函数也声明为 `friend`，更好的做法是把它们作为 `private static` 成员函数。

## 6. 运算符重载

### 成员还是非成员？

| 运算符 | 推荐形式 | 原因 |
|---|---|---|
| `=`, `[]`, `()`, `->`, 类型转换 | 必须是成员 | 语言规定 |
| `+=`, `-=`, `++`, `--` 等修改自身的 | 成员 | 需要修改 `*this` |
| `+`, `-`, `==`, `<` 等对称二元运算 | 非成员（常为 `friend`） | 允许左操作数发生隐式转换，如 `1 + x` |
| `<<`, `>>`（流） | 非成员 | 左操作数是流 |

### 比较运算符：C++20 `<=>`

HW3 为 `Node` 与 `int` 的比较手写了 10 个运算符（5 个成员、5 个友元，友元还**按值**接收 `Node`，每次比较都拷贝一个节点）。C++20 中：

```cpp
struct Node {
    int value;
    friend auto operator<=>(const Node& n, int v) { return n.value <=> v; }
    friend bool operator==(const Node& n, int v) { return n.value == v; }
};
// 编译器自动改写：n < 5、5 > n、5 == n、n != 5 … 全部可用
```

对于成员逐个比较的类型，直接 `auto operator<=>(const T&) const = default;` 即可。

### 自增运算符的规范形式

```cpp
class BST {
public:
    BST& operator++() {          // 前置：修改自身，返回引用
        for_each_node([](Node& n) { ++n.value; });
        return *this;
    }
    BST operator++(int) {        // 后置：int 只是用于区分的哑参数
        BST old(*this);          // 拷贝旧值
        ++*this;                 // 复用前置版本
        return old;              // 按值返回（移动或 NRVO，无额外深拷贝）
    }
};
```

对比 HW3 原实现：后置 `++` 用 `new BST(*this)` 在堆上创建旧值并返回其引用，**每次调用泄漏一整棵树**。作者正确地意识到了不能返回局部变量的引用，但解决方向错了——应该把返回类型改为值类型。

### 赋值运算符

- 返回 `T&`（即 `*this`），以支持 `a = b = c`；HW5 的 `void operator=(...)` 不满足这一约定；
- 处理自赋值；
- 释放旧资源（HW3、HW5 的原实现都忘了这一点，导致泄漏）；
- 最稳妥的写法是 **copy-and-swap**，见 [cpp/06](06-special-members-and-move-semantics.md)。

### 流输出

```cpp
friend std::ostream& operator<<(std::ostream& out, const BST& bst);   // 注意 const&
```

HW3 的签名是 `operator<<(ostream&, BST&)`，因为 `BST::length()` 不是 `const` 成员函数——一个缺少 `const` 的函数迫使整条调用链都不能用 `const`。

### `explicit operator bool`

HW4 的智能指针正确地使用了 `explicit operator bool() const`：允许 `if (p)`，但禁止 `int x = p;` 或 `p1 + p2` 这类荒谬的隐式转换（“safe bool” 问题）。

## 练习

1. 为 HW3 的 `Node` 用 `<=>` 重写全部比较运算符，并确认原有测试仍然通过。
2. 把 HW2 的 `Client::server` 改回作业要求的 `Server const* const`，并让代码重新编译通过：需要把哪些函数改成 `const`？
3. 为什么 `operator<<` 不能是 `BST` 的成员函数？
