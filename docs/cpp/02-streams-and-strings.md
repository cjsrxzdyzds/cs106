# C++ 02 · 流与字符串

> CS106L：Streams（通常占两讲，是该课程的特色内容）
> 作业：HW1 `show`（`<iomanip>` 格式化输出）、HW2 `parse_trx`（`<regex>`）、HW6 q2/q3（文件与字符串解析）
> CMU 参考：15-213 第 10 章 *System-Level I/O*（缓冲、`read`/`write` 与标准 I/O 的关系）

## 1. 流的抽象

流是“字符序列的来源或去处”的统一抽象：

```
                   std::istream ─┬─ std::ifstream      (文件)
std::ios_base ─ std::ios         ├─ std::istringstream (字符串)
                   std::ostream ─┼─ std::ofstream
                                 └─ std::ostringstream
```

因为它们共享基类，一个接受 `std::istream&` 的函数可以读文件、读字符串或读标准输入——这让解析代码很容易测试：

```cpp
std::vector<Patient> read_patients(std::istream& in);   // 可测试
std::vector<Patient> read_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open " + path);
    return read_patients(in);
}
// 测试中：std::istringstream in("name,surname,age...\n..."); read_patients(in);
```

HW6 q2 的 `read_file` 在文件打不开时只 `cerr` 一行然后继续执行，返回空向量——调用者无法区分“文件为空”和“文件不存在”。

## 2. 流状态

每个流维护 4 个状态位：`goodbit`、`eofbit`、`failbit`、`badbit`。**正确的读循环是把读操作本身作为条件**：

```cpp
std::string line;
while (std::getline(in, line)) { ... }     // ✅ 读成功才进入循环体

while (!in.eof()) {                        // ❌ 经典错误：最后一次读失败后仍会处理一次“脏”数据
    std::getline(in, line); ...
}
```

`>>` 与 `getline` 混用时的坑：`>>` 不会吞掉行尾的 `'\n'`，紧接着的 `getline` 会读到一个空行。
解决办法是 `in >> std::ws` 或统一用 `getline` 读整行再用 `istringstream` 解析——HW6 q2 采用的正是后者：

```cpp
while (std::getline(input, line)) {
    std::stringstream ss(line);
    std::string name, surname, age;
    std::getline(ss, name, ',');       // 第三个参数：分隔符
    std::getline(ss, surname, ',');
    std::getline(ss, age, ',');
    ...
}
```

## 3. 格式化输出

HW1 `show` 使用 `<iomanip>`：

```cpp
std::cout << std::setw(7) << std::fixed << std::setprecision(3) << x;
```

注意 `setw` 只作用于**下一次**输出，而 `fixed` / `setprecision` 是“粘性”的，会一直影响之后的输出——在库函数中修改 `std::cout` 的格式状态是一种副作用。C++20 的 `std::format`（GCC 13 起可用）更安全：

```cpp
std::cout << std::format("{:7.3f}", x);
```

## 4. 字符串

| 类型 | 所有权 | 用途 |
|---|---|---|
| `std::string` | 拥有 | 存储、修改 |
| `std::string_view`（C++17） | 不拥有 | 只读参数、切片（零拷贝） |
| `const char*` | 不拥有 | 与 C API 交互 |

HW6 q3 的解析代码反复执行 `fileLine = fileLine.substr(startPos + 1);`，每次都分配并拷贝剩余字符串，对 n 个字段是 O(n·len)。用 `string_view` 切片可以做到零拷贝：

```cpp
std::string_view rest = line;
auto take_after = [&](char c) {
    rest.remove_prefix(rest.find(c) + 1);
};
```

⚠️ `string_view` 不延长被引用字符串的生命周期：`std::string_view v = std::string("tmp");` 立即悬空。

数值转换：`std::stoi` / `std::stod` 失败时抛异常；C++17 的 `std::from_chars` 不抛异常、不分配内存、不受 locale 影响，适合高性能解析。

## 5. 正则表达式

HW2 的 `parse_trx` 使用 `<regex>`：

```cpp
std::regex pattern("([a-zA-Z]+)-([a-zA-Z]+)-([\\d.]+)");
std::smatch m;
if (!std::regex_match(trx, m, pattern)) throw std::runtime_error("invalid trx");
```

几点改进：

- 用原始字符串字面量 `R"(...)"` 避免双重转义：`R"(([a-zA-Z]+)-([a-zA-Z]+)-(\d+(?:\.\d+)?))"`；
- `[\d.]+` 会接受 `"1.2.3"`，随后 `std::stod` 只解析出 `1.2`，应收紧为 `\d+(\.\d+)?`；
- 构造 `std::regex` 需要编译自动机，代价不小，应声明为 `static const`（函数内的 `static` 局部变量初始化在 C++11 起是线程安全的）；
- ⚠️ **与作业其他部分的交互**：`Server::add_client` 在 id 重复时会追加 4 位随机数字（如 `ali1234`），但 `[a-zA-Z]+` 不接受数字，于是这些客户端**永远无法转账**。这是一个跨函数的规格不一致问题，单元测试恰好没有覆盖。

## 6. 流与并发

- 标准保证对同一个同步的标准流对象（如 `std::cout`）的并发输出**不构成数据竞争**，但不同线程的输出字符可能交错；
- C++20 的 `std::osyncstream`（`<syncstream>`）把一次完整输出作为原子单元提交：

```cpp
std::osyncstream(std::cout) << "worker " << id << " done\n";
```

- 本仓库 `labs/` 中只在主线程打印，避免了这个问题。

## 练习

1. 把 HW6 q2 的 `read_file` 拆成 `read_patients(std::istream&)` 与打开文件的包装函数，并为前者写一个基于 `std::istringstream` 的测试。
2. 用 `std::string_view` 和 `std::from_chars` 重写 HW6 q3 的航班解析，比较与原实现的性能差异。
3. 修改 HW2 的 `parse_trx` 正则，使其既接受带数字后缀的 id，又拒绝 `"1.2.3"` 这样的金额。
