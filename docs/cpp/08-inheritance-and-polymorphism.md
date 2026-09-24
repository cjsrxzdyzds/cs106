# C++ 08 · 继承与多态

> CS106L：Inheritance（以及 Classes 一讲中关于接口的部分）
> 作业：HW5（`Ingredient` → `Cinnamon`/`Milk`/…；`EspressoBased` → `Cappuccino`/`Mocha`）
> 实验：[lab5_thread_pool](../../labs/lab5_thread_pool/thread_pool_test.cpp)（用 `unique_ptr` + `clone()` 重写 HW5 的类层次）

## 1. 继承的三种用途

1. **接口继承**（最常用）：基类只声明纯虚函数，派生类实现。`Ingredient::get_name() = 0` 即是；
2. **实现继承**：复用基类的数据与代码。`Ingredient` 的 `price_unit`、`units`、`price()`；
3. **访问控制**：`protected` 构造函数让 `Ingredient` / `EspressoBased` 只能作为基类使用。

C++ Core Guidelines 建议优先使用组合；只有在需要“通过基类指针/引用使用派生类对象”（即运行时多态）时才使用 public 继承。

## 2. 虚函数与动态分派

```cpp
class Ingredient {
public:
    virtual ~Ingredient() = default;
    virtual std::string get_name() const = 0;   // 纯虚函数：Ingredient 是抽象类
    double price() const { return price_unit * units; }  // 非虚：所有派生类相同
};
class Milk final : public Ingredient {
public:
    std::string get_name() const override { return "Milk"; }   // override：让编译器检查签名
};
```

- `override`：若签名与基类虚函数不匹配（例如漏了 `const`），编译报错，而不是悄悄定义一个新函数；
- `final`：禁止进一步派生/重写，也为编译器去虚化提供依据。

> 🔧 编译器视角：典型实现（Itanium C++ ABI）中，每个含虚函数的类有一张**虚函数表（vtable）**，每个对象开头存一个指向它的 **vptr**。
> `p->get_name()` 被编译为“读 vptr → 取表项 → 间接调用”。编译器能证明动态类型时（对象是局部值、类是 `final`、或 LTO/全程序分析下只有一个实现）
> 会做**去虚化（devirtualization）**，把间接调用变成直接调用甚至内联。这是类层次分析（CHA）、快速类型分析（RTA）等程序分析技术在编译器中的直接应用。

## 3. 多态基类必须有虚析构函数

```cpp
Ingredient* i = new Milk(2);
delete i;   // 若 ~Ingredient 不是 virtual：未定义行为
```

HW5 的 `Ingredient` **没有**声明虚析构函数，而 `EspressoBased` 的析构函数正是通过 `Ingredient*` 删除各种配料——这是 UB。
在当前的派生类都没有额外成员的情况下，它“碰巧”表现正常（`name` 在基类中，会被基类析构正确释放），但只要某个派生类加了一个 `std::string` 成员，就会泄漏或崩溃。`-Wdelete-non-virtual-dtor`（包含在 `-Wall` 中）会对此发出警告。

规则：**要么 public 且 virtual，要么 protected 且非 virtual**（后者表示“不允许通过基类指针删除”）。

## 4. 对象切片（Object Slicing）

```cpp
Cappuccino c;
EspressoBased e = c;          // 若允许，只拷贝基类部分，派生部分被“切掉”
void f(EspressoBased e);      // 按值传参同样切片
```

HW5 把 `EspressoBased` 的拷贝构造和赋值声明为 `protected`，阻止了外部的切片拷贝——这是作业设计中很好的一点。

## 5. 多态对象的拷贝：虚 `clone()`

C++ 没有虚构造函数。要深拷贝一个 `vector<Ingredient*>`，而不知道每个元素的动态类型，就需要一个虚函数：

```cpp
class Ingredient {
public:
    virtual std::unique_ptr<Ingredient> clone() const = 0;
};
```

HW5 要求实现 `virtual Ingredient* copy()`，思路正确；问题在于返回裸指针，以及 `Mocha` 的拷贝构造根本没有调用它（直接复制了指针，ASan 报告 use-after-free，见 [HW5 精讲](../homework/code-review.md#hw5)）。

### 用 CRTP 消除重复

HW5 为 8 种配料写了几乎一样的 8 个类，作者用宏 `DEFCLASS` 生成了其中一个。另一种不依赖预处理器的方法是**奇异递归模板模式（CRTP）**：

```cpp
template <typename Derived>
class IngredientBase : public Ingredient {
public:
    using Ingredient::Ingredient;
    std::unique_ptr<Ingredient> clone() const override {
        return std::make_unique<Derived>(static_cast<const Derived&>(*this));
    }
};

class Milk final : public IngredientBase<Milk> {
public:
    explicit Milk(std::size_t units) : IngredientBase<Milk>(10, units) {}
    std::string get_name() const override { return "Milk"; }
};
```

`clone()` 只写一次，每个派生类自动获得返回正确类型的实现。[lab5](../../labs/lab5_thread_pool/thread_pool_test.cpp) 采用了这一写法。

## 6. `dynamic_cast` 与 `reinterpret_cast`

| 转换 | 检查 | 用途 |
|---|---|---|
| `static_cast<Derived*>(base)` | 编译期；运行时不检查 | 确定知道动态类型时 |
| `dynamic_cast<Derived*>(base)` | 运行时（RTTI），失败返回 `nullptr` | 不确定动态类型时 |
| `reinterpret_cast<Derived*>(base)` | 无；把位模式重新解释 | 与底层字节打交道；**几乎从不用于类层次** |

课程的 HW5 测试 TEST10 写了：

```cpp
EspressoBased* esp{new Mocha{}};
reinterpret_cast<Cappuccino*>(esp)->add_side_item(new Cookie{1});   // 把 Mocha 当 Cappuccino 用
```

`Mocha` 与 `Cappuccino` 是兄弟类，这里的 `reinterpret_cast` 是 UB，只是因为两者布局相同才“能跑”。同一个测试和 TEST6 还在 `delete esp` 之后读取 `sides.size()`，引用的正是已被释放对象的成员——ASan 会在这两个测试上报告 heap-use-after-free，**这是课程测试代码本身的问题，不是学生实现的问题**。

## 7. 运行时多态 vs. 编译期多态

| | 虚函数 | 模板 / concepts | `std::variant` + `std::visit` |
|---|---|---|---|
| 类型集合 | 开放（随时可加派生类） | 开放 | 封闭（在定义处列出） |
| 分派时机 | 运行时 | 编译期 | 运行时（通常是跳转表） |
| 对象存储 | 通常在堆上（指针） | 值 | 值（大小 = 最大成员） |
| 可否放进同一个容器 | 可以（基类指针） | 不可以 | 可以 |

HW5 这样“饮品种类固定、操作不断增加”的场景，也可以用 `std::variant<Cappuccino, Mocha>` 建模。

## 练习

1. 给 HW5 的 `Ingredient` 加上虚析构函数，打开 `-Wall` 重新编译，观察警告的变化。
2. 用 `std::vector<std::unique_ptr<Ingredient>>` 重写 `EspressoBased`，使 `Cappuccino` 和 `Mocha` 不需要手写任何特殊成员函数（除了需要深拷贝的拷贝构造）。
3. 在 Compiler Explorer 中对比：`Milk` 标记与不标记 `final` 时，`void f(const Milk& m) { m.get_name(); }` 生成的代码有何不同。
