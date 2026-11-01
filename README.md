# C++ STL 源码解剖

> 不背私有变量名，先跑出证据，再回答「标准库到底怎么做到」。

一套面向 C++ 内功期的 STL 源码学习材料：**6 章可编译的探针程序 + 6 张分步图解 + 1 张学习路线图**。每一章都用一个能跑的程序，把 `sizeof`、元素地址、配置器计数器、`bucket_count()` 打印出来，用输出验证结论，而不是抄别人的笔记。

## 快速开始

```powershell
git clone https://github.com/kurosakimaon/cpp-stl-internals.git
cd cpp-stl-internals
start index.html          # Windows
# open index.html        # macOS
```

`index.html` 是总路线图，从这里可以进入模块目录、详细教程和六张图解。想直接看某一张图，打开 `visuals/` 下对应的页面即可。

## 目录结构

```text
.
├── index.html                     # 学习总路线图（入口）
├── stl_internals/
│   ├── index.html                 # 模块目录：六章卡片与学习建议
│   ├── tutorial.md                # 详细教程：目标、知识点、失效规则速查、源码地图
│   ├── 01_allocator_traits.cpp   # 配置器：容器的地基
│   ├── 02_vector_internals.cpp   # vector：三根指针与增长曲线
│   ├── 03_list_deque_internals.cpp
│   ├── 04_rbtree_map_internals.cpp
│   ├── 05_hashtable_internals.cpp
│   └── 06_iterators_algorithms.cpp
└── visuals/
    ├── index.html                 # 图解专区
    ├── allocator.html             # 配置器 · 6 步
    ├── dynamic_array.html         # vector · 7 步
    ├── list_deque.html            # list / deque · 7 步
    ├── rb_tree.html               # 红黑树 · 7 步
    ├── hash_table.html            # 哈希表 · 7 步
    ├── iterators.html             # 迭代器与算法 · 6 步
    ├── visual.css                 # 图解共用样式
    └── visual.js                  # 图表播放器：步进 / 自动播放 / #step-N
```

## 六章主线

| 步骤 | 主题 | 核心问题 | 图解 |
|---|---|---|---|
| 01 | 配置器 | 内存分配与对象构造为什么拆开？`rebind` 干了什么？ | [6 步](visuals/allocator.html) |
| 02 | vector | 三根指针怎么摆？扩容为什么摊还 O(1)？谁失效了？ | [7 步](visuals/dynamic_array.html) |
| 03 | list / deque | 哨兵环怎么省掉特判？分段连续怎么随机访问？ | [7 步](visuals/list_deque.html) |
| 04 | map / set | 红黑树五条规矩，插入时何时变色、单旋、双旋？ | [7 步](visuals/rb_tree.html) |
| 05 | unordered_map | 桶数组 + 全局单链表，rehash 后谁死谁活？ | [7 步](visuals/hash_table.html) |
| 06 | 迭代器与算法 | `iterator_traits`、标签分派、内省排序、二分 | [6 步](visuals/iterators.html) |

图解共 40 帧，全部为静态 SVG + CSS 动画，不依赖任何外部库；`visual.js` 负责步骤切换、自动播放，并支持用 `#step-3` 这样的锚点直达某一帧。

## 编译与运行探针程序

单个文件即可编译，无第三方依赖：

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic stl_internals/01_allocator_traits.cpp -o demo
./demo
```

Windows（MinGW / MSYS2）：

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic .\stl_internals\02_vector_internals.cpp -o demo.exe
.\demo.exe
```

全部六章一次跑完：

```powershell
Get-ChildItem .\stl_internals\*.cpp | ForEach-Object {
    $exe = Join-Path $env:TEMP ($_.BaseName + '.exe')
    g++ -std=c++17 -Wall -Wextra -Wpedantic $_.FullName -o $exe
    & $exe
}
```

## 说明

- 代码以 **libstdc++（GCC）** 为主，文中会标注 MSVC（`vector` / `xtree` / `xhash`）的差异。要学的是套路，不是背某个实现的私有变量名。
- 扩容倍数、缓冲区大小、桶数序列都是实现细节，不同标准库不一样；探针程序的意义就在于让你在自己机器上量一遍。
- 图解里的数字（桶数 1 → 13 → 29 → 59 → 127、坏哈希 `find` 58 次等）由本机 g++（libstdc++）实测得到，换编译器后可能不同。
- 教程中的 CMake 片段是接入示例，本仓库只包含独立可编译的单文件程序，不附带 `CMakeLists.txt`。

## 学习顺序

1. **先会用**：知道什么场景选什么容器（前置，本仓库不重复讲 API）。
2. **再拆开**：按 01 → 06 顺序读源码、跑探针、看图解。第 1 步是地基，第 4 步是重头戏。
3. **后验证**：改类型、改数量、换坏哈希，用输出验证结论；做完每章练习再看下一章。

> 看懂不算会，能复现才算。容器一改，先想迭代器。
