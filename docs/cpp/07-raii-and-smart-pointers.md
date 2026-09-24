# C++ 07 · RAII 与智能指针

> CS106L：RAII & Smart Pointers
> 作业：HW4（实现 `UniquePtr` / `SharedPtr`）
> CMU 参考：15-213 第 9.9 节 *Dynamic Memory Allocation*（Malloc Lab）、第 9.11 节 *Common Memory-Related Bugs*
> 实验：[lab4_smart_ptr](../../labs/lab4_smart_ptr/)

## 1. RAII：资源获取即初始化

**把资源的生命周期绑定到对象的生命周期**：构造函数获取资源，析构函数释放资源。
因为 C++ 保证局部对象在离开作用域时（无论是正常返回、`break`、还是异常栈展开）都会析构，资源就不会泄漏。

```cpp
void bad() {
    int* p = new int[100];
    risky();              // 若抛异常，delete[] 永远不会执行
    delete[] p;
}

void good() {
    std::vector<int> v(100);   // 或 std::unique_ptr<int[]>
    risky();                   // 抛异常时 v 的析构函数依然会运行
}
```

RAII 是 C++ 资源管理的统一答案，不局限于内存：

| 资源 | RAII 类型 |
|---|---|
| 堆内存 | `std::unique_ptr`、`std::shared_ptr`、容器 |
| 互斥锁 | `std::lock_guard`、`std::unique_lock`、`std::scoped_lock` |
| 线程 | `std::jthread`（析构时自动 join） |
| 文件 | `std::fstream` |

CS106L 的说法是：**在现代 C++ 中，你几乎不应该写出裸的 `new` 和 `delete`。**

15-213 第 9.11 节列出的 C 内存错误——忘记释放、重复释放、释放后使用、引用已释放的栈变量——在 RAII + 智能指针下基本都能在类型层面避免。

## 2. `std::unique_ptr`：独占所有权

```cpp
auto p = std::make_unique<Widget>(arg1, arg2);   // 优先使用 make_unique
p->method();
std::unique_ptr<Widget> q = std::move(p);        // 所有权转移，p 变为空
Widget* raw = q.get();                           // 观察，不拥有
q.reset();                                       // 立即释放
```

- 零开销：与裸指针大小相同（使用默认删除器时），析构就是一次 `delete`；
- 不可拷贝、可移动；
- 自定义删除器：`std::unique_ptr<FILE, decltype(&fclose)> f(fopen(...), &fclose);`
- 作为函数参数：`void take(std::unique_ptr<T> p)` 表示“我接管所有权”；只是使用对象则传 `T&` 或 `T*`。

## 3. `std::shared_ptr`：共享所有权

```cpp
auto a = std::make_shared<Widget>();
auto b = a;                      // 引用计数 +1
std::cout << a.use_count();      // 2
```

### 内部结构：控制块

```
 shared_ptr<T> a ─┬─ T* ptr ─────────────────┐
                  └─ ControlBlock* ─┐        │
 shared_ptr<T> b ─┬─ T* ptr ────────┼────────┤
                  └─ ControlBlock* ─┤        ▼
                                    ▼     ┌──────┐
                         ┌──────────────┐ │  T   │
                         │ strong = 2   │ └──────┘
                         │ weak   = 1   │
                         │ deleter,alloc│
                         └──────────────┘
```

- `shared_ptr` 大小是两个指针；
- 引用计数存放在**每个被管理对象一个**的控制块中，计数操作是**原子的**；
- `make_shared` 把对象和控制块放进同一次分配，少一次 `malloc`，局部性更好（[lab4](../../labs/lab4_smart_ptr/smart_ptr.h) 中的 `InplaceBlock`）；
- `weak_count` 用于 `std::weak_ptr`：观察但不拥有，可以打破循环引用。

### 对比：HW4 的 `SharedPtr` 为什么不对

HW4 原实现用一个 `static std::map<T*, int>` 记录所有指针的计数：

| 问题 | 后果 |
|---|---|
| 全局 map，所有 `SharedPtr<T>` 共享 | 每次拷贝/析构都要 O(log n) 查找；多线程下 map 本身就是数据竞争 |
| `operator=` 把旧指针计数减 1 但从不检查是否为 0 | 被覆盖的对象永远不会被释放。课程 TEST21（`ptr2 = ptr1`）在 ASan 下报告泄漏 |
| `operator=` 返回参数 `ptr` 而不是 `*this` | `(a = b).reset()` 会重置 `b` 而不是 `a` |
| `reset()` 把计数直接置 0 并 `delete` | 其他仍持有该指针的 `SharedPtr` 全部悬空 |
| 计数为 0 的条目从不删除 | map 只增不减；地址被复用时依赖“残留 0”恰好正确 |

这些问题的根源是同一个：**计数应该跟随被管理的对象（控制块），而不是放在一个全局表里**。

## 4. 智能指针与线程安全

这是面试和实际工程中都极易混淆的点：

1. **控制块是线程安全的**：不同线程可以同时拷贝、销毁**不同的** `shared_ptr` 对象，即使它们指向同一个控制块；
2. **`shared_ptr` 对象本身不是**：同一个 `shared_ptr` 变量被一个线程赋值、另一个线程读取，是数据竞争。需要加锁，或使用 C++20 的 `std::atomic<std::shared_ptr<T>>`；
3. **被指向的对象更不是**：`shared_ptr` 只管理生命周期，`T` 的成员访问需要 `T` 自己同步。

[lab4](../../labs/lab4_smart_ptr/smart_ptr.h) 实现了一个带原子计数的 `SharedPtr`，并用 ThreadSanitizer 验证了多线程并发拷贝/销毁时对象恰好析构一次。引用计数为什么用 `relaxed` 自增、`acq_rel` 自减，是理解内存序的经典案例，详见 [concurrency/03](../concurrency/README.md)。

⚠️ 性能提示：`shared_ptr` 的每次拷贝都是一次原子读-改-写，多核高频拷贝同一个控制块会导致缓存行在核间来回迁移。函数参数只是“使用”对象时，传 `const shared_ptr<T>&` 或直接传 `T&`。

## 5. 选择指南

| 场景 | 选择 |
|---|---|
| 唯一拥有者 | `std::unique_ptr<T>`（默认选择） |
| 多个拥有者、生命周期无法静态确定（例如多线程共享的只读配置） | `std::shared_ptr<T>` |
| 需要观察一个共享对象但不延长其生命 / 打破环 | `std::weak_ptr<T>` |
| 只是使用，不涉及所有权 | `T&` 或 `T*` |
| 多态对象的容器 | `std::vector<std::unique_ptr<Base>>` |

HW2 用 `std::shared_ptr<Client>` 存储客户端，但所有客户端实际上都由 `Server` 唯一拥有；若对外只返回观察指针，`std::unique_ptr` 或直接存值（`std::map<std::string, Client>`）会更清晰。

## 6. 异常安全的三个级别

| 保证 | 含义 | 例子 |
|---|---|---|
| 不抛出（nothrow） | 操作不会失败 | 析构函数、`swap`、移动构造（应标记 `noexcept`） |
| 强保证 | 失败时状态回滚到调用前 | copy-and-swap 赋值；`vector::push_back` |
| 基本保证 | 失败时不泄漏、不变量成立，但状态可能改变 | 大多数操作的最低要求 |

为什么推荐 `make_unique(args)` 而不是 `unique_ptr<T>(new T(args))`？在 C++17 之前，`f(std::unique_ptr<T>(new T), g())` 的求值顺序允许“先 `new T`，再调用 `g()`，最后构造 `unique_ptr`”；若 `g()` 抛异常，`new` 出来的对象就泄漏了。C++17 收紧了求值顺序规则，但 `make_*` 仍然更简洁，且 `make_shared` 只分配一次。

## 练习

1. 阅读 [lab4 `smart_ptr.h`](../../labs/lab4_smart_ptr/smart_ptr.h)，为 `SharedPtr` 增加 `weak_count` 和一个最小的 `WeakPtr`（提示：控制块的生命周期要由 `strong + weak` 共同决定，`dispose()` 与 `delete this` 要分开；`WeakPtr::lock()` 需要一个 CAS 循环，只在 `strong > 0` 时自增）。
2. 解释：为什么 `SharedPtr(T* p)` 在分配控制块失败时必须 `delete p`？
3. 用 `std::weak_ptr` 修复一个双向链表节点互相持有 `shared_ptr` 造成的内存泄漏。
