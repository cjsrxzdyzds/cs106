# 算法参考实现与测试

学习入口：[6 章算法课](../docs/algorithms/README.md)。

- `algorithms.h`：12 个函数的完整参考实现及接口契约。
- `tests.cpp`：边界测试、固定种子的随机小输入、独立参考算法交叉验证。
- `CMakeLists.txt`：独立构建，不依赖原作业子模块或 CUDA。

从仓库根目录执行：

```bash
cmake -S algorithms -B algorithms/build -DCMAKE_BUILD_TYPE=Release
cmake --build algorithms/build -j 4
ctest --test-dir algorithms/build --output-on-failure
./algorithms/build/algorithm_tests 1
```

测试参数 1–6 对应课件章节。CHECK 在 Release 下也有效。学习时先尝试独立实现，再打开参考代码；不要删除测试来让错误实现“通过”。

验证记录：2026-09-26，macOS arm64 / Apple Clang 21，Release 与 ASan/UBSan 均为 6/6 测试通过。
