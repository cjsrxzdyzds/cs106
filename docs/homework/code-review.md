# 作业代码审查汇总（HW1–HW6）

审查对象是本仓库 submodule 指向的解答（`half-dreamer/AP1400-2-HW1..6`）。
方法：用 GCC 13 加 `-fsanitize=address,undefined` 编译每份作业，运行课程自带的 GoogleTest；再对测试没覆盖到的可疑点写最小复现程序。

标记说明：**[ASan]** Sanitizer 实测，**[测试]** 课程单元测试失败，**[链接]** 链接失败，**[复现]** 最小程序复现，**[审阅]** 读代码确认。

## 总览

| 作业 | ASan 构建下的测试结果 | 主要问题 |
|---|---|---|
| HW1 | 23/24 通过，BONUS 失败 | 消元时不选主元，遇到 0 主元就除零 |
| HW2 | 运行到 TEST9 时崩溃 | 课程提供的 `crypto.cpp` 越界读；`mine()` 没按规格实现 |
| HW3 | 链接失败 | 有 3 个函数没实现；`find_node` 有逻辑错误；多处内存泄漏 |
| HW4 | 21/21 通过，但有泄漏 | `SharedPtr::operator=` 泄漏；`UniquePtr` 实际上可以被拷贝 |
| HW5 | TEST6/TEST10 触发 use-after-free | 这两个是**测试本身**的问题；学生代码另有浅拷贝和泄漏 |
| HW6 | 链接失败 | `q4` 违反 ODR；`q1` 算法和签名都不对；`q3` 泄漏 |

---

## HW1 · 线性代数库 {#hw1}

| # | 问题 | 依据 | 修正 |
|---|---|---|---|
| 1 | `upper_triangular` 不选主元，对角元为 0 时除零，BONUS 测试 `{{0,2,3},…}` 失败 | [测试] | 部分选主元：每列先把绝对值最大的行 `ero_swap` 到对角位置 |
| 2 | `determinant` 用余子式展开，复杂度 O(n!)；`inverse` 用伴随矩阵，对每个元素都算一次 O(n!) 的行列式 | [审阅] | 用带部分主元的 LU 分解，O(n³)；逆矩阵用高斯–约当消元 |
| 3 | `determinant(m) == 0` 对浮点数判等，而且计算了两次 | [审阅] | 与相对阈值比较；结果缓存一次 |
| 4 | `ero_swap` 中 `r1 > rowNum(m) - 1`：矩阵为空时 `-1` 被转换成 `SIZE_MAX`，越界检查永远通过 | [审阅] | 改为 `r1 >= rows`，统一使用 `size_t`（见 [cpp/01 §5](../cpp/01-types-and-initialization.md)） |
| 5 | `for (vector<double> rowVec : matrix)` 每次迭代都拷贝一整行 | [审阅] | `const auto&` |
| 6 | 头文件里写 `using std::vector;`，污染所有包含它的翻译单元 | [审阅] | 移到 `.cpp` 中 |
| 7 | `vector<vector<double>>` 不连续，i-j-k 的循环顺序对 cache 不友好 | [复现] lab1：n=512 时 ijk 420 ms，ikj 48 ms | 见 [lab1](../../labs/lab1_matmul/matmul.cpp) |

## HW2 · 中心化加密货币 {#hw2}

| # | 问题 | 依据 | 修正 |
|---|---|---|---|
| 1 | 课程提供的 `crypto::Base64Encode` 直接返回 `BUF_MEM::data`，这个缓冲区不以 `'\0'` 结尾，随后 `std::string{base64Text}` 调 `strlen` 越界读。`RSA*` 对象也从不释放 | [ASan] heap-buffer-overflow，`crypto.cpp:136` | 用 `std::string(ptr->data, ptr->length)` 构造；用 RAII 包装 `RSA_free` / `BIO_free_all`。**这是课程代码的问题** |
| 2 | `add_client` 遇到重复 id 会追加 4 位数字，而 `parse_trx` 的正则 `[a-zA-Z]+` 不接受数字，所以这些客户端永远无法转账 | [审阅] | 放宽正则，见 [cpp/02 §5](../cpp/02-streams-and-strings.md) |
| 3 | `transfer_money` 签名的是 `id`，而不是交易字符串；服务器也按 id 验签，所以签名防不了篡改和重放 | [审阅] | 签名并验证完整的交易字符串 |
| 4 | `mine()` 只是把各个客户端的随机 nonce 加起来；没有构造 mempool，没有做 sha256 校验，没有奖励矿工，也没有应用交易 | [审阅] | 按规格实现；并行版本见 [lab2](../../labs/lab2_mining/mining.cpp) |
| 5 | 余额检查没有算上尚未打包的 pending 交易，同一个发送者可以连续提交多笔交易，超额花费 | [审阅] | 检查时减去该发送者已有的 pending 支出（lab2 的 `add_pending_trx`） |
| 6 | 遍历 `map` 时按值拷贝 `pair<shared_ptr<Client>, double>`，每次都有原子引用计数操作；以指针为键，按 id 查找只能 O(n) 扫描 | [审阅] | `const auto&`；改用以 id 为键的 map |
| 7 | 把 `Server const* const` 改成了 `Server* const`，绕开了作业想练的 `const` 正确性 | [审阅] | 见 [cpp/05 §3](../cpp/05-classes-const-operators.md) |

## HW3 · 二叉搜索树 {#hw3}

| # | 问题 | 依据 | 修正 |
|---|---|---|---|
| 1 | `find_parrent`、`find_successor`、`delete_node` 没有实现，整个项目链接不过 | [链接] `undefined reference to BST::find_successor(int)` | 补全实现 |
| 2 | `find_node` 往左走之前检查的是 `right == nullptr`：在树 `{5, 3}` 中找 3 返回“找不到” | [复现] | 删掉这两个多余的检查，循环条件 `*cur != nullptr` 已经足够 |
| 3 | 后置 `operator++` 用 `new BST(*this)` 创建旧值并返回它的引用，每调用一次泄漏一整棵树 | [审阅] | 按值返回：`BST old(*this); ++*this; return old;` |
| 4 | 拷贝赋值和移动赋值都没有释放旧树 | [审阅] | copy-and-swap，见 [cpp/06 §5](../cpp/06-special-members-and-move-semantics.md) |
| 5 | 拷贝构造的参数是 `BST&` 而不是 `const BST&`，`const` 对象和临时对象都无法拷贝 | [审阅] | 改为 `const BST&` |
| 6 | 5 个友元比较运算符按值接收 `Node`，每比较一次拷贝一个节点 | [审阅] | 用 `const Node&`，或者用 C++20 的 `<=>` |
| 7 | `nodesOfBST` 递归时反复拼接 vector，最坏 O(n²)；`length()` 每次都要 O(n) 遍历 | [审阅] | 复用 `bfs`；把大小缓存为成员变量 |

## HW4 · 智能指针 {#hw4}

| # | 问题 | 依据 | 修正 |
|---|---|---|---|
| 1 | `SharedPtr::operator=` 把旧计数减 1，但减到 0 时不释放 | [ASan] TEST21 泄漏 8 字节 | 见 [lab4](../../labs/lab4_smart_ptr/smart_ptr.h) |
| 2 | `operator=` 返回的是参数 `ptr`，不是 `*this` | [审阅] | `return *this;` |
| 3 | `reset()` 把计数直接清零并 `delete`，其他仍持有该对象的 `SharedPtr` 全部悬空 | [审阅] | 只释放自己那一份引用：`SharedPtr().swap(*this)` |
| 4 | 计数放在全局的 `static std::map<T*, int>` 里：每次操作 O(log n)，条目从不删除，多线程下存在数据竞争 | [审阅] | 为每个对象分配控制块，计数用 `std::atomic` |
| 5 | `UniquePtr` 的拷贝构造只写了 `static_assert(true, ...)`，这个断言恒为真，所以拷贝照样能编译，而且 `_p` 没有初始化 | [审阅] | `UniquePtr(const UniquePtr&) = delete;` 并提供移动操作 |
| 6 | `UniquePtr::operator=` 缺少 `return` 语句（UB），并且会让两个对象拥有同一个指针 | [审阅] | 删除拷贝赋值，提供移动赋值 |
| 7 | `operator*` 按值返回 `T`，通过 `*p = x` 修改不会作用到原对象 | [审阅] | 返回 `T&` |
| 8 | `make_unique` / `make_shared` 返回裸指针，而且不能转发构造参数 | [审阅] | 可变参数模板加完美转发，见 [cpp/04 §6](../cpp/04-functions-lambdas-templates.md) |

## HW5 · 继承与多态 {#hw5}

| # | 问题 | 依据 | 修正 |
|---|---|---|---|
| 1 | `Mocha` 的拷贝构造写了 `ingredients = cap.ingredients`，是浅拷贝 | [复现] 运行 `Mocha a; { Mocha b(a); }` 时 ASan 报 heap-use-after-free | 用 `clone()` 深拷贝 |
| 2 | `Cappuccino::operator=` 调用 `clear()` 时没有 `delete` 旧的配料和旧的附加项 | [ASan] TEST8 泄漏 4 处，共 224 字节 | copy-and-swap |
| 3 | `Ingredient` 没有虚析构函数，却通过 `Ingredient*` 执行 `delete` | [审阅] UB | `virtual ~Ingredient() = default;` |
| 4 | `EspressoBased` 的拷贝构造和拷贝赋值同样是浅拷贝指针 | [审阅] | 改用 `vector<unique_ptr<Ingredient>>`，见 [cpp/08](../cpp/08-inheritance-and-polymorphism.md) |
| 5 | 赋值运算符返回 `void` | [审阅] | 返回 `T&` |
| 6 | **课程测试本身的问题**：TEST6/TEST10 在 `delete esp` 之后还读取 `sides.size()`；TEST10 用 `reinterpret_cast` 把 `Mocha*` 当作 `Cappuccino*` 使用 | [ASan] heap-use-after-free | 不属于学生代码的问题 |

## HW6 · STL 综合 {#hw6}

| # | 问题 | 依据 | 修正 |
|---|---|---|---|
| 1 | `q4::kalman_filter` 是定义在头文件里的非 `inline` 函数，被两个 `.cpp` 包含后违反 ODR | [链接] `multiple definition of q4::kalman_filter` | 加 `inline` |
| 2 | `kalman_filter` 没有 `return` 语句 | [审阅] UB | 实现它，或者返回加权平均 |
| 3 | `q1::gradient_descent` 只会以固定步长向正方向移动，不会沿梯度方向下降，可能永远不收敛；签名用函数指针，无法接收测试传入的函数对象（作者因此注释掉了 q1 的测试） | [审阅] | 改为模板 `template <class T, class F> T gradient_descent(T init, T step, F f = F{})`，并实现 `x -= step * f'(x)` |
| 4 | `q3` 用 `new Flight{...}` 创建对象，压入的是它的拷贝，原对象泄漏 | [ASan] 泄漏 6 处，`q3.h:84` | `pq.push(Flight{...})` |
| 5 | `q3` 中 `for (int i = 0; i < connections - 1; i++)`：`connections` 是 `size_t`，为 0 时减 1 下溢 | [审阅] | 先判断是否为 0 |
| 6 | 头文件里写 `using namespace std;` | [审阅] | 删除 |
| 7 | `q2::read_file` 打不开文件时只打印一行错误，然后继续执行 | [审阅] | 抛出异常，见 [cpp/02 §1](../cpp/02-streams-and-strings.md) |
