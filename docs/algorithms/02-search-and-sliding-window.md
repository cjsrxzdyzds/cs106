# 算法 02 · 二分与滑动窗口

> 目标：区分“答案具有单调边界”与“窗口可以单调收缩”；统一边界定义。
> 实现：[lower_bound_index / longest_unique](../../algorithms/algorithms.h)；测试：`algorithm_tests 2`。

## 1. 找到首个不小于目标的位置

输入非递减数组 a，返回最小下标 i，使 a[i]>=target；不存在时返回 a.size()。例如 `[1,2,2,5]`、target=2 返回 1；target=6 返回 4。数组有序是前提，函数不通过 O(n) 扫描验证它。

从左扫描可用 O(n) 时间解决。二分利用谓词 `a[i]>=target` 从 false 到 true 的单调变化。采用 `[left,right)`，初始为 `[0,n)`：左边已排除的位置都小于 target，right 及右边的位置若存在都满足不小于 target。

```cpp
while (left < right) {
    auto mid = left + (right - left) / 2;
    if (a[mid] < target) left = mid + 1;
    else right = mid;
}
return left;
```

若 a[mid]<target，mid 及其左侧不可能是答案；否则 mid 可能正是第一个，必须保留这个边界，不能简单 right=mid-1。每次区间严格缩短，结束时左右边界相遇。

手算 `[1,2,2,5]`、target=2：`[0,4)` 的 mid=2，right=2；`[0,2)` 的 mid=1，right=1；`[0,1)` 的 mid=0，left=1，返回 1。空数组直接返回 0，不需要特判 n-1，也避免无符号下溢。

时间 O(log n)、空间 O(1)。不要把闭区间模板的终止条件与半开区间更新规则拼在一起。`left+(right-left)/2` 也避免直接 left+right 的潜在溢出。

## 2. 最长无重复片段

输入一个字节串，求不含重复字节的最长连续片段长度。`"abcabcbb"` 为 3，`"abba"` 为 2，空串为 0。本实现按字节处理，包括值为 0 的字节；不把 UTF-8 多字节字符当成一个单位。

暴力枚举起点，并逐步扩展直到重复，最坏 O(n²)。滑动窗口维护当前无重复区间 `[left,right]`。next[c] 记录字节 c 上次位置加 1，没有出现则为 0。

```cpp
left = std::max(left, next[c]);
best = std::max(best, right - left + 1);
next[c] = right + 1;
```

为什么要 max？在 `abba` 的最后一个 a，之前 a 对应 next=1，但当前 left 已因第二个 b 移到 2。直接设 left=1 会让窗口倒退并重新包含重复 b。

处理新字节前窗口无重复；若旧 c 在窗口中，把左端移到旧 c 后面，恰好去掉唯一新增冲突；若旧 c 已在窗口外，左端不动。left 只前进，每个 right 只处理一次，所以总时间 O(n)，固定 256 大小表的空间 O(1)。

访问数组前要把 char 转成 unsigned char。某些实现的 char 是有符号类型，高位字节可能成为负下标。

## 3. 哪些问题可以这样收缩

无重复窗口允许安全移除左边元素：移除不会引入新的重复。但“目标和”若允许负数，就没有“和过大应一直缩左端”的可靠单调性。算法 01 的前缀和解决这类计数更自然。

二分也需要证明谓词单调，不是看到数组就使用。将本章迁移到“二分答案”时，应先写出可行性判定，例如容量够大时可行，更大的容量是否必然可行。

## 练习与参考思路

1. 改为返回首个严格大于 target 的下标。**参考**：把排除左半区的条件换成 `a[mid] <= target`，其余半开区间规则不变。
2. 为二分写重复元素、全小于目标、全大于目标、空数组测试。**验收**：与 std::lower_bound 一致。
3. 最长无重复片段改为返回区间。**提示**：更新 best 时同时存起点，并规定同长时返回最早还是最晚。
4. 如果“最多允许两种不同字节”，还能只记 last position 吗？**参考**：更自然地维护窗口频次及不同字节数，超限时移动左端并减少频次。

[上一章](01-hash-and-prefix-sums.md) · [目录](README.md) · [下一章](03-monotonic-stack-and-deque.md)
