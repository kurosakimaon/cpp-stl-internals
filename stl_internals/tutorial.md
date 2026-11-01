# 内功期 · STL 源码解剖

> 不背 API，把标准库拆开看看：三根指针、一个哨兵、一棵红黑树、一排桶。

## 一、模块简介

徒弟，到了这一关，你不能再满足于"会调用 `push_back`、`find`、`insert`"了。

前面的 `intermediate/stl_containers.cpp` 教的是**怎么用**：什么场景选什么容器、复杂度大概是多少。这个模块要往下再钻一层：**标准库到底怎么把这件事做出来的**。

同样叫 `vector`，为什么它能在尾部摊还 O(1) 地长胖？同样叫 `map`，为什么它插入时要旋转、要变色？同样叫 `unordered_map`，为什么扩容之后迭代器全废了，但指针和引用还活着？

这些问题，光看 API 文档答不上来。得看源码，或者至少看懂源码里的**数据结构布局**和**关键算法**。

> 先打个预防针：不同标准库实现（GCC 的 libstdc++、Clang 的 libc++、MSVC 的 STL）细节不一样。本模块的代码以 **libstdc++（GCC）** 为主，同时标注 MSVC 的差异。你要学的是**套路**，不是背某个实现的私有变量名。

## 二、学习目标

学完本模块，你要能做到：

1. 说清 STL 六大组件的关系，理解"配置器把内存分配和对象构造拆开"这件事
2. 画出 `vector` 的三根指针，解释 `size/capacity`、扩容倍数、`reserve` 和迭代器失效
3. 说清 `list` 的哨兵节点、`deque` 的中控器与缓冲区，知道两者的迭代器和引用什么时候失效
4. 说清红黑树的五条规矩，能手推插入时的"变色 / 单旋 / 双旋"三种情况
5. 说清 `unordered_map` 的桶数组 + 单链表结构，理解 `bucket_count`、装载因子和 rehash
6. 理解迭代器分类与 `iterator_traits`，知道 `std::sort` 为什么是内省排序、`lower_bound` 的比较次数为什么是 O(log n)
7. 能自己写探针程序，用 `sizeof`、地址、计数器去验证源码里的结论

## 三、文件列表与学习重点

```text
  01_allocator_traits.cpp      ← 配置器：容器的地基
    重点：allocate/deallocate 与 construct/destroy 分离、rebind、传播 traits

  02_vector_internals.cpp      ← vector：三根指针和一条增长曲线
    重点：_M_start/_M_finish/_M_end_of_storage、扩容、move_if_noexcept、迭代器失效

  03_list_deque_internals.cpp  ← list 与 deque：节点和分段
    重点：哨兵节点、splice、deque 中控器/缓冲区、两端 O(1)、中间插入要定位或搬移

  04_rbtree_map_internals.cpp  ← map/set：红黑树的平衡术
    重点：五条规矩、插入三种情况、header 节点、operator[] 的插入语义

  05_hashtable_internals.cpp   ← unordered_map：桶、链表与 rehash
    重点：桶数组、全局单链表、装载因子、rehash 后谁失效谁不失效

  06_iterators_algorithms.cpp  ← 迭代器与算法：STL 的胶水
    重点：iterator_traits、标签分派、内省排序、二分查找、迭代器适配器
```

## 四、学习顺序建议

### 第一步：配置器（01_allocator_traits.cpp）

容器不直接 `new`，而是通过配置器（allocator）要内存。先把"分配生内存"和"在内存上构造对象"这两步分开，你才能理解后面所有容器的节点从哪来。

重点看 `allocate(n)` 与 `construct(p, args...)` 的区别，以及 `allocator_traits` 的 `rebind`：`list<int>` 拿到的配置器会被重绑定成"节点配置器"。

### 第二步：vector（02_vector_internals.cpp）

这是最该先啃的容器。记住三根指针：

```cpp
// libstdc++ 的 _Vector_impl 只存三个指针
T* _M_start;             // 首元素
T* _M_finish;            // 尾后（size = finish - start）
T* _M_end_of_storage;    // 容量尾（capacity = end_of_storage - start）
```

把"扩容时旧元素怎么搬"和"`move_if_noexcept` 为什么存在"这两件事搞明白，你就能解释大部分 vector 的坑。

> 🎬 扩容、size/capacity、迭代器失效的动态过程，配合[动态数组图解](../visuals/dynamic_array.html)一起看。

### 第三步：list 与 deque（03_list_deque_internals.cpp）

`list` 的重点不是 API，而是那条**首尾相接的环**：一个不存数据的 header/哨兵节点，让空链表、头插、尾插、`splice` 都不用特判。

`deque` 的重点是**分段连续**：一个中控器数组（map）存着一堆缓冲区指针，迭代器里四个指针 `cur/first/last/node`。两端插入只需要新开缓冲区，不搬旧元素；中间插入才要挑一边搬。

### 第四步：map/set（04_rbtree_map_internals.cpp）

这一章最硬，也最值钱。先记住五条规矩，再手推插入的三种情况：

1. 每个节点不是红就是黑
2. 根是黑
3. 每个 NIL（空叶子）是黑
4. 红节点的两个孩子必须是黑（不能红红相连）
5. 从任一节点到其所有 NIL 的路径上，黑节点数量相同

插入的新节点先染红，违反规矩就往上修：叔叔红→只变色；叔叔黑且"一字型"→单旋；叔叔黑且"之字型"→双旋。

> 🎬 红黑树插入的变色、单旋、双旋，看[红黑树图解](../visuals/rb_tree.html)，7 步把指针怎么改画出来。

### 第五步：unordered_map（05_hashtable_internals.cpp）

别把它想成"数组 + vector 链表"。libstdc++ 的 `_Hashtable` 是**一个全局单链表 + 一个桶数组**：桶数组里存的是"该桶第一个节点前驱"的指针，方便 O(1) 摘链；所有节点本身串在一条全局单链上，迭代器顺着这条链走。

重点观察 `bucket_count` 怎么长、`load_factor` 怎么算，以及 rehash 之后"迭代器失效、引用和指针不失效"这条规则。

### 第六步：迭代器与算法（06_iterators_algorithms.cpp）

容器和算法之间只认迭代器。`iterator_traits` 把"迭代器能干什么"抽出来，`std::distance` / `std::advance` 靠标签分派选不同实现。

算法部分重点看 `std::sort`：快速排序 + 堆排序兜底 + 小区间插入排序，这就是**内省排序（introsort）**。再看 `lower_bound` 的二分写法，理解它在随机访问迭代器上为什么比逐个 `find` 快一个数量级。

## 五、核心知识点梳理

### 1. STL 六大组件

```text
容器（container）  → 存数据：vector/list/deque/map/unordered_map...
算法（algorithm）  → 算数据：sort/find/transform...
迭代器（iterator） → 连接容器和算法的胶水
仿函数（functor）  → 比较、哈希、变换策略
适配器（adapter）  → stack/queue/priority_queue、reverse_iterator...
配置器（allocator）→ 管内存：allocate/deallocate + construct/destroy
```

很多人只把 STL 当"容器库"，其实算法、迭代器、仿函数、适配器、配置器这五件套才是它厉害的地方。

### 2. 配置器：分配 ≠ 构造

```cpp
Alloc a;
T* p = a.allocate(n);          // 只拿到 n 个 T 大小的原始内存，对象还没出生
std::allocator_traits<Alloc>::construct(a, p, args...); // 在这块内存上构造
// ...使用...
std::allocator_traits<Alloc>::destroy(a, p); // 只析构，不还内存
a.deallocate(p, n);            // 还内存
```

为什么要拆开？因为容器需要**先分配一大块内存，再按需构造**（比如 `vector::reserve`），也方便自定义内存池。

`allocator_traits` 是配置器的"统一门面"：它帮旧式配置器补齐 `rebind`、`propagate_on_container_copy_assignment`、`select_on_container_copy_construction` 等成员。容器只跟 traits 打交道，不直接碰配置器。

### 3. vector 的三根指针与扩容

```cpp
size()     = _M_finish - _M_start;
capacity() = _M_end_of_storage - _M_start;
```

扩容时：申请新内存 → 把旧元素搬过去 → 析构旧元素 → 释放旧内存 → 更新三根指针。GCC/libstdc++ 一般按 2 倍长，MSVC 一般按 1.5 倍长。

搬家时用 `std::move_if_noexcept`：如果元素的移动构造声明了 `noexcept`，就移动；否则（移动可能抛异常且元素可拷贝）就拷贝。这样扩容途中抛异常时，旧 vector 还是完整的，满足强异常安全。

### 4. list 的哨兵与 deque 的中控器

`list` 的 header 节点不存数据，它的 `next` 指向首元素，`prev` 指向尾元素；空链表时两个指针都指回自己。于是：

- `begin()` 就是 `header.next`
- `end()` 就是 `&header`
- `++end()` 在 libstdc++ 的实现里会绕回 `begin()`，但标准规定对 `end()` 自增是未定义行为，这只是实现细节，不要依赖

`deque` 是"分段连续"：中控器 `map` 是一个指针数组，每项指向一块固定大小的缓冲区（libstdc++ 常见为 512 字节）。迭代器 `{cur, first, last, node}` 记录当前缓冲区内的位置、缓冲区边界和在中控器里的位置。`push_front/push_back` 只在两端开新缓冲区，不搬旧元素；`insert` 到中间才会搬。

### 5. 红黑树的插入修复

新节点染红，从它往上修：

```text
叔叔是红        → 父/叔染黑，祖父染红，问题上移到祖父
叔叔是黑，一字型 → 父染黑，祖父染红，对祖父单旋
叔叔是黑，之字型 → 先对父单旋，变成一字型，再按上一条处理
```

修复完把根染黑。插入最多 2 次旋转，删除最多 3 次，这就是 `map/set` 插入删除稳定 O(log n) 的底气。

### 6. 哈希表的桶与 rehash

```text
bucket_count：桶的个数（质数序列，libstdc++ 大约每次翻倍）
load_factor = size / bucket_count
max_load_factor 默认 1.0
插入新元素会让 size 超过 bucket_count * max_load_factor 时触发 rehash
```

rehash 只按已有的哈希值重新分配每个节点落在哪个桶，**节点本身不搬家**。所以：

- 迭代器全部失效（链表顺序被重排）
- 指向元素的指针和引用仍然有效（节点地址没变）

### 7. 迭代器分类与内省排序

```cpp
input_iterator_tag → forward → bidirectional → random_access
```

`std::distance` 对随机访问迭代器直接相减 O(1)，对其他迭代器一步步走 O(n)。`std::sort` 只接受随机访问迭代器，`std::list` 只能用成员 `sort()`。

`std::sort` 的内部策略：

1. 快速排序，三数取中选枢轴
2. 递归深度超过 `2 * log2(n)` 就改用堆排序，避免最坏 O(n²)
3. 小区间（通常 ≤ 16）改用插入排序，减少函数调用开销

## 六、失效规则速查表

| 操作 | 迭代器 | 指针/引用 |
|------|--------|-----------|
| `vector::push_back` 未扩容 | 只有 `end()` 变 | 有效 |
| `vector::push_back` 触发扩容 | 全部失效 | 全部失效 |
| `vector::insert/erase` 中间 | 插入/删除点及之后失效 | 插入/删除点及之后失效 |
| `deque::push_front/push_back` | 全部失效 | 已有元素有效 |
| `deque::insert/erase` 中间 | 全部失效 | 全部失效（可能搬元素） |
| `list` 插入/删除 | 只有被删元素失效 | 只有被删元素失效 |
| `list::splice` | 被移动元素的迭代器仍有效（换容器后仍指向原元素） | 有效 |
| `map/set` 插入 | 不失效 | 不失效 |
| `map/set` 删除 | 只有被删元素失效 | 只有被删元素失效 |
| `unordered_*` 插入触发 rehash | 全部失效 | 有效 |
| `unordered_*` 删除 | 只有被删元素失效 | 只有被删元素失效 |

> 记不住没关系，但要记住一句话：**容器一改，先想迭代器**。

## 七、常见坑与注意事项

### 1. `reserve` 和 `resize` 不是一回事

`reserve(n)` 只把 `capacity` 提到至少 n，`size` 不变，不构造元素；`resize(n)` 会真的构造/析构元素，`size` 变成 n。`reserve` 之后不能直接 `v[0] = 1`。

### 2. `map::operator[]` 会插入

`m["不存在的 key"]` 会默认构造一个 value 插进去。只想查找用 `find()` 或 `at()`（`at` 找不到会抛 `out_of_range`）。

### 3. `unordered_map` rehash 后别再用旧迭代器

引用和指针还在（节点没搬家），但迭代器顺序全变了。循环里边遍历边插入，最容易踩这个坑。

### 4. `vector` 的 `erase` 不缩小容量

`erase` 只析构并搬移元素，`capacity` 不变。要还内存，C++11 起可以用 `shrink_to_fit()`（不保证一定生效），或者 `vector<T>(v).swap(v)` 这种老写法。

### 5. `std::remove` 不删元素

算法只能拿到迭代器，拿不到容器，所以 `remove` 只是把要保留的元素往前挪，返回新逻辑尾。真正的删除要用 erase-remove idiom：

```cpp
v.erase(std::remove(v.begin(), v.end(), 3), v.end());
```

### 6. 自定义比较器必须满足严格弱序

`std::sort` 的比较器如果写错（比如 `return a <= b;`），行为是未定义，可能直接崩。必须是 `a < b` 这种严格弱序。

### 7. `vector<bool>` 不是普通 vector

它是特化版本，一个 bit 存一个 bool，`operator[]` 返回代理对象，`data()` 都没有。别拿它当普通数组用。

### 8. `sizeof` 不等于内存占用

`sizeof(std::map<int,int>)` 只有几十字节，因为它只存 header、size 和比较器；真正的节点都在堆上。算内存要算"每个节点的布局 + 分配器开销"。

## 八、怎么读 STL 源码

### 源码地图（libstdc++，GCC）

| 主题 | 文件 | 关键类/函数 |
|------|------|-------------|
| 配置器门面 | `bits/alloc_traits.h` | `allocator_traits` |
| vector | `bits/stl_vector.h`、`bits/vector.tcc` | `_Vector_impl`、`_M_realloc_insert` |
| list | `bits/stl_list.h`、`bits/list.tcc` | `_List_node_base`、`_List_node<T>` |
| deque | `bits/stl_deque.h`、`bits/deque.tcc` | `_Deque_iterator`、`_Deque_base::_M_map` |
| map/set | `bits/stl_tree.h` | `_Rb_tree_node_base`、`_Rb_tree_insert_and_rebalance` |
| 哈希表 | `bits/hashtable.h`、`bits/hashtable_policy.h` | `_Hashtable`、`_Prime_rehash_policy` |
| 迭代器 | `bits/stl_iterator_base_types.h`、`bits/stl_iterator_base_funcs.h` | `iterator_traits`、`distance`、`advance` |
| 算法 | `bits/stl_algo.h`、`bits/stl_algobase.h` | `__introsort_loop`、`lower_bound` |

MSVC 的对应文件在 Visual Studio 安装目录的 `VC/Tools/MSVC/<版本>/include/` 下：`vector`、`list`、`deque`、`xtree`（红黑树）、`xhash`（哈希表）、`xutility`、`xmemory`。名字不同，套路一样。

### 读源码的四把工具

1. **先看成员变量**：一个类怎么工作，先看它存了什么。`vector` 三个指针、`deque` 一个 map + 两个迭代器、`map` 一个 header + size。
2. **用调试器单步**：在 `push_back` 里打断点，看三根指针怎么变；在 `rehash` 里看桶数组怎么换。比干读快十倍。
3. **写探针程序**：用 `sizeof`、`&元素`、`bucket_count()`、自定义配置器的计数器，把内部状态"打印"出来。本模块每章都是这么干的。
4. **对照 cppreference**：源码是实现，标准是契约。先知道标准保证什么，再看实现怎么做到，别把实现细节当标准。

> 别想一次读完。挑一个容器、一个函数，顺着调用链看下去，看懂就停。贪多嚼不烂。

## 九、编译与接入现有仓库

### 单文件编译

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic stl_internals/01_allocator_traits.cpp -o demo
./demo
```

Windows + MinGW 同理：

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic .\stl_internals\02_vector_internals.cpp -o demo.exe
.\demo.exe
```

### 接入根目录 CMakeLists.txt

在 `CMakeLists.txt` 的 "中级进阶 Demo" 段后面加：

```cmake
# ============================================================================
# STL 源码解剖（内功期）
# ============================================================================
set(STL_INTERNALS_SOURCES
    stl_internals/01_allocator_traits.cpp
    stl_internals/02_vector_internals.cpp
    stl_internals/03_list_deque_internals.cpp
    stl_internals/04_rbtree_map_internals.cpp
    stl_internals/05_hashtable_internals.cpp
    stl_internals/06_iterators_algorithms.cpp
)

message(STATUS "\n=== STL Internals Demos (内功期) ===")
foreach(src ${STL_INTERNALS_SOURCES})
    get_filename_component(target ${src} NAME_WE)
    add_executable(stl_${target} ${src})
    target_link_libraries(stl_${target} PRIVATE Threads::Threads)
    message(STATUS "  Added target: stl_${target}")
endforeach()

add_custom_target(all_stl_internals
    COMMENT "Building all STL internals demos..."
)
foreach(src ${STL_INTERNALS_SOURCES})
    get_filename_component(target ${src} NAME_WE)
    add_dependencies(all_stl_internals stl_${target})
endforeach()
```

再把 `all_stl_internals` 加进 `all_demos` 的 `DEPENDS`，构建摘要里补一行 "STL internals: 6 targets"。

图解页都放在 `visuals/`：`index.html` 是图解首页，六章各有一页（`allocator` / `dynamic_array` / `list_deque` / `rb_tree` / `hash_table` / `iterators`），共用已有的 `visual.css` 和 `visual.js`。新增图解页时，照 `rb_tree.html` 的骨架写一个 `window.STEPS` 数组，再在首页对应分组加一张卡片即可。

## 十、老师傅的话

徒弟，读源码这事，一开始肯定难受：模板套模板，下划线开头的名字一堆，一个 `push_back` 能跳五层。

但你记住三句话：

1. **源码不是用来"读完"的，是用来"查证"的。** 你心里先有一个猜测——"vector 扩容大概是两倍"——再去源码里找证据。带着问题读，比从头啃效率高得多。
2. **先看数据布局，再看算法。** 一个容器存了哪些指针、节点长什么样，决定了它能做什么、不能做什么。红黑树难，难在旋转规则；但你先记住每个节点存了颜色和三个指针，就没那么玄了。
3. **标准是契约，实现是手段。** `map` 保证有序和 O(log n)，红黑树只是 libstdc++ 选的手段；换个实现可能用别的平衡树。别把 `_M_start` 这种私有名字当成标准。

最后一句：**光看不动手，等于没学。** 每章的探针程序，你都要自己改一改：换个类型、换个数量、换个配置器，看看输出怎么变。源码里的结论，只有你自己验证过，才真正是你的。

### 小练习（回去自己做）

1. 给 `02_vector_internals.cpp` 的 `MiniVector` 加上拷贝构造和移动构造，用计数器验证移动后的源对象状态。
2. 用自定义配置器统计 `std::map<int,int>` 插入 1000 个元素一共分配了多少字节，再算平均每元素开销。
3. 给 `03_list_deque_internals.cpp` 加一段：把元素类型换成本文件的 `Tracked`，用 `std::deque<Tracked>` 和 `std::vector<Tracked>` 各做 10000 次头插，比较拷贝/移动次数。
4. 给红黑树加一个 `erase`，先只实现"删除后重新验证五条规矩"，再补删除修复的四种情况。
5. 用 `unordered_set<int, BadHash>` 换几种坏哈希（`x % 7`、`x & 15`、恒等哈希），画出桶长分布。
6. 给 `06_iterators_algorithms.cpp` 的自定义迭代器补上 `operator--`、`operator+=`，把它升级成随机访问迭代器，再让 `std::sort` 接受它。
7. 用调试器在 `std::vector::push_back` 里单步，记录扩容前后三根指针的值。

> 内功期就到这儿。过了这一关，你看 STL 就不再是"一堆 API"，而是一套**可以推演、可以验证、可以自己实现**的数据结构与算法。后面的并发、性能优化，都会用到这份底气。
