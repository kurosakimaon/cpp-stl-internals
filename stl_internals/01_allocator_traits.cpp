// ============================================================
//  01_allocator_traits.cpp - 配置器：容器的地基
// ============================================================
//  徒弟啊，很多人用 STL 用了好几年，都没正眼看过 allocator。
//  觉得那是库作者才关心的东西，自己写业务代码用默认的就行。
//
//  这话对一半。默认配置器确实够用，但你不懂它，
//  就永远理解不了：为什么 vector 扩容要搬家、list 每个元素为什么那么胖、
//  为什么 map 插入 5 个 int 却分配了 200 字节。
//
//  这一章把配置器拆开，你就明白容器手里的"内存"到底从哪来。
//
//  核心就一句话：
//      分配内存（allocate） ≠ 构造对象（construct）
//      析构对象（destroy）  ≠ 释放内存（deallocate）
//
//  STL 把这两件事拆成了两对函数，这是它一切灵活性的根。
// ============================================================

#include <iostream>
#include <iomanip>
#include <memory>
#include <vector>
#include <list>
#include <map>
#include <string>
#include <atomic>
#include <cstddef>
#include <type_traits>
#include <new>

using namespace std;

// ============================================================
// 一、STL 六大组件：先建立全局观
// ============================================================
//  容器 container   ：存数据      vector/list/deque/map...
//  算法 algorithm   ：算数据      sort/find/transform...
//  迭代器 iterator  ：连接两者的胶水
//  仿函数 functor   ：策略        less/greater/hash...
//  适配器 adapter   ：改造接口    stack/queue/reverse_iterator...
//  配置器 allocator ：管内存      allocate/deallocate/construct/destroy
//
//  前五个你天天见，第六个藏在所有容器的模板参数最后一位：
//      template<class T, class Alloc = allocator<T>> class vector;
//  平时不写，不代表它不存在。
// ============================================================

void sixComponents()
{
    cout << "============================================================" << endl;
    cout << "  一、STL 六大组件" << endl;
    cout << "============================================================" << endl;
    cout << "  容器     → 存：vector / list / deque / map / unordered_map" << endl;
    cout << "  算法     → 算：sort / find / transform / lower_bound" << endl;
    cout << "  迭代器   → 连：begin/end、随机访问/双向/单向" << endl;
    cout << "  仿函数   → 策略：less / greater / hash / equal_to" << endl;
    cout << "  适配器   → 改接口：stack / queue / reverse_iterator" << endl;
    cout << "  配置器   → 管内存：allocate / deallocate / construct / destroy" << endl;
    cout << endl;
    cout << "  容器模板的最后一个参数就是配置器，默认 std::allocator<T>。" << endl;
    cout << "  你看不见它，但每个节点、每块缓冲区都经它的手。" << endl;
}

// ============================================================
// 二、allocate 与 construct 是两件事
// ============================================================
//  allocate(n) 只得到"能放 n 个 T 的原始内存"，里面没有活对象。
//  这块内存上可以：
//      - 一个对象都不构造（vector::reserve 就是这样）
//      - 构造一部分（vector::size < capacity）
//      - 用 placement new 在指定地址构造
//
//  construct(p, args...) 才是真正调用 T 的构造函数。
//  destroy(p) 只调用析构函数，不还内存。
//  deallocate(p, n) 才把内存还给分配器。
//
//  为什么拆开？三个理由：
//      1. reserve 只要内存，不要对象
//      2. 容器扩容时可以先分配新家，再把旧元素一个个搬过去
//      3. 自定义内存池只管理内存，不关心对象类型
// ============================================================

void allocateVsConstruct()
{
    cout << "\n============================================================" << endl;
    cout << "  二、allocate 与 construct 分离" << endl;
    cout << "============================================================" << endl;

    allocator<string> alloc;
    using Traits = allocator_traits<allocator<string>>;

    // 只要内存：此时 p[0]、p[1] 指向的是未构造的原始内存
    string* p = alloc.allocate(3);
    cout << "  allocate(3) 返回原始内存: " << static_cast<const void*>(p)
         << "（此时还没有 string 对象）" << endl;

    // 在指定地址上构造对象
    Traits::construct(alloc, p + 0, "hello");
    Traits::construct(alloc, p + 1, "stl");
    Traits::construct(alloc, p + 2, "world");

    cout << "  construct 之后：" << endl;
    for (int i = 0; i < 3; ++i)
    {
        cout << "    p[" << i << "] = \"" << p[i] << "\""
             << "，地址 " << static_cast<const void*>(p + i)
             << "，size " << p[i].size() << endl;
    }

    // 只析构，不还内存
    for (int i = 0; i < 3; ++i)
    {
        Traits::destroy(alloc, p + i);
    }
    cout << "  destroy 之后对象已析构，但内存还在（还没 deallocate）" << endl;

    // 还内存
    alloc.deallocate(p, 3);
    cout << "  deallocate 之后，这块内存才真正还给分配器" << endl;

    cout << endl;
    cout << "  记住顺序：allocate → construct → ... → destroy → deallocate" << endl;
    cout << "  少一步就是 UB 或泄漏，顺序错了也是 UB。" << endl;
}

// ============================================================
// 三、一个会计数的小配置器
// ============================================================
//  标准要求配置器提供 value_type、allocate、deallocate，
//  以及可跨类型比较的 == / !=。
//  其余成员（construct/destroy/rebind/max_size...）由
//  std::allocator_traits 提供默认实现。
//
//  这个 CountingAlloc 不管理内存池，只是在 ::operator new/delete
//  外面包一层计数，让我们能看见容器到底要了多少内存。
//
//  注意：计数器是全局的，不按 T 区分。因为 list<int> 会把
//  CountingAlloc<int> 重绑定成 CountingAlloc<_List_node<int>>，
//  如果按 T 分开计数，我们就看不到节点分配了。
// ============================================================

struct AllocStats
{
    static atomic<long> allocCalls;
    static atomic<long> deallocCalls;
    static atomic<long> bytes;

    static void reset()
    {
        allocCalls.store(0);
        deallocCalls.store(0);
        bytes.store(0);
    }

    static void print(const string& title)
    {
        cout << "  " << title << "：allocate " << allocCalls.load()
             << " 次，deallocate " << deallocCalls.load()
             << " 次，累计申请 " << bytes.load() << " 字节" << endl;
    }
};

atomic<long> AllocStats::allocCalls{0};
atomic<long> AllocStats::deallocCalls{0};
atomic<long> AllocStats::bytes{0};

template <class T>
struct CountingAlloc
{
    using value_type = T;

    CountingAlloc() noexcept = default;

    // 跨类型拷贝构造：让 CountingAlloc<int> 能变成 CountingAlloc<Node>
    template <class U>
    CountingAlloc(const CountingAlloc<U>&) noexcept {}

    T* allocate(size_t n)
    {
        AllocStats::allocCalls.fetch_add(1);
        AllocStats::bytes.fetch_add(static_cast<long>(n * sizeof(T)));
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, size_t)
    {
        AllocStats::deallocCalls.fetch_add(1);
        ::operator delete(p);
    }

    template <class U>
    bool operator==(const CountingAlloc<U>&) const noexcept { return true; }

    template <class U>
    bool operator!=(const CountingAlloc<U>&) const noexcept { return false; }
};

// ============================================================
// 四、allocator_traits：配置器的统一门面
// ============================================================
//  C++11 之前，容器直接调用 Alloc::rebind<T>::other，
//  旧式配置器少一个成员就编译不过。
//
//  C++11 起有了 allocator_traits<Alloc>，它像一个适配层：
//      - 配置器有 rebind，就用配置器的
//      - 没有，就自动推导 Alloc::value_type，生成重绑定版本
//      - construct 没有，就用 placement new
//      - propagate_on_container_* 没有，就默认 false_type
//
//  容器只跟 traits 打交道，所以旧式配置器也能用。
// ============================================================

template <class A>
void printTraits(const string& name)
{
    using Traits = allocator_traits<A>;
    cout << "  " << name << "：" << endl;
    cout << "    value_type 大小        = " << sizeof(typename Traits::value_type) << " 字节" << endl;
    cout << "    propagate_on_container_copy_assignment = "
         << boolalpha << Traits::propagate_on_container_copy_assignment::value << endl;
    cout << "    propagate_on_container_move_assignment = "
         << Traits::propagate_on_container_move_assignment::value << endl;
    cout << "    propagate_on_container_swap            = "
         << Traits::propagate_on_container_swap::value << endl;
    cout << "    is_always_equal                        = "
         << Traits::is_always_equal::value << noboolalpha << endl;
    cout << "    max_size(alloc)                        = " << Traits::max_size(A{}) << endl;
}

void traitsDemo()
{
    cout << "\n============================================================" << endl;
    cout << "  三、allocator_traits 统一门面" << endl;
    cout << "============================================================" << endl;

    printTraits<allocator<int>>("std::allocator<int>");
    printTraits<CountingAlloc<int>>("CountingAlloc<int>");

    cout << endl;
    cout << "  propagate_on_container_* 决定容器在拷贝/移动/swap 时" << endl;
    cout << "  要不要把配置器一起搬过去。自己写的配置器默认是 false。" << endl;
    cout << "  std::allocator 无状态，libstdc++ 把它的 move 传播设成 true，" << endl;
    cout << "  传不传播都不影响释放；带状态的配置器才必须认真处理这组开关。" << endl;
}

// ============================================================
// 五、rebind：为什么 list<int> 要的是节点配置器
// ============================================================
//  vector<int> 存的就是 int，配置器 value_type = int 正好。
//  list<int> 真正分配的是 _List_node<int>（prev/next + 数据），
//  所以要拿 int 配置器"重绑定"成节点配置器：
//
//      using NodeAlloc = allocator_traits<Alloc<int>>::rebind_alloc<Node>;
//
//  老式写法是 Alloc<int>::rebind<Node>::other，
//  新写法统一走 traits，旧配置器也能兼容。
//
//  这就是为什么你给 list 传 CountingAlloc<int>，
//  实际分配时 T 变成节点类型，sizeof(T) 不是 4 而是 24 左右。
// ============================================================

void rebindDemo()
{
    cout << "\n============================================================" << endl;
    cout << "  四、rebind：从 int 配置器到节点配置器" << endl;
    cout << "============================================================" << endl;

    using IntAlloc = CountingAlloc<int>;
    using PairAlloc = allocator_traits<IntAlloc>::rebind_alloc<pair<const int, int>>;

    cout << "  CountingAlloc<int>::value_type 大小 = " << sizeof(IntAlloc::value_type) << " 字节" << endl;
    cout << "  rebind 成 pair<const int,int> 后大小 = " << sizeof(PairAlloc::value_type) << " 字节" << endl;

    cout << "  但 map 真正分配的是 _Rb_tree_node<pair<const int,int>>，" << endl;
    cout << "  节点里除了 pair，还有颜色、parent、left、right 三个指针。" << endl;
    cout << "  下一节的计数会把这个开销量出来。" << endl;
}

// ============================================================
// 六、用计数配置器量一量各种容器的真实开销
// ============================================================
//  注意：这里统计的是配置器看到的字节数，不含 malloc 自身的
//  块头、对齐填充等开销。真实进程占用只会更大。
// ============================================================

void measureContainerOverhead()
{
    cout << "\n============================================================" << endl;
    cout << "  五、量一量：5 个 int 元素，各容器要了多少内存" << endl;
    cout << "============================================================" << endl;

    {
        AllocStats::reset();
        vector<int, CountingAlloc<int>> v;
        for (int i = 0; i < 5; ++i) v.push_back(i);
        cout << "  vector<int> 插入 5 个（capacity=" << v.capacity() << "）：" << endl;
        AllocStats::print("    配置器视角");
        cout << "    平均每元素 " << static_cast<double>(AllocStats::bytes.load()) / v.size()
             << " 字节（含预留容量）" << endl;
    }
    cout << "  容器析构后：";
    AllocStats::print("vector 释放");
    cout << endl;

    {
        AllocStats::reset();
        list<int, CountingAlloc<int>> l;
        for (int i = 0; i < 5; ++i) l.push_back(i);
        cout << "  list<int> 插入 5 个：" << endl;
        AllocStats::print("    配置器视角");
        cout << "    平均每元素 " << static_cast<double>(AllocStats::bytes.load()) / l.size()
             << " 字节（每个节点单独分配）" << endl;
    }
    cout << "  容器析构后：";
    AllocStats::print("list 释放");
    cout << endl;

    {
        AllocStats::reset();
        map<int, int, less<int>, CountingAlloc<pair<const int, int>>> m;
        for (int i = 0; i < 5; ++i) m[i] = i;
        cout << "  map<int,int> 插入 5 个：" << endl;
        AllocStats::print("    配置器视角");
        cout << "    平均每元素 " << static_cast<double>(AllocStats::bytes.load()) / m.size()
             << " 字节（红黑树节点）" << endl;
    }
    cout << "  容器析构后：";
    AllocStats::print("map 释放");
    cout << endl;

    cout << "  结论：" << endl;
    cout << "    - vector 按块分配，申请次数等于扩容次数（这里 4 次），" << endl;
    cout << "      不是每个元素一次；最终 capacity=8，累计 1+2+4+8 个槽位" << endl;
    cout << "    - list 每个元素 1 次分配，节点 = 2 个指针 + 数据 + 对齐" << endl;
    cout << "    - map  每个元素 1 次分配，节点 = 颜色 + 3 个指针 + pair" << endl;
    cout << "    - 所以“5 个 int”从来不是 20 字节，容器有自己的骨架" << endl;
}

// ============================================================
// 七、一个带状态的配置器：为什么传播 traits 很重要
// ============================================================
//  无状态配置器（std::allocator、CountingAlloc）谁都能互换。
//  带状态配置器（内存池、arena）各自管理一块内存，
//  两个容器 swap 时，配置器必须跟着一起换，否则
//  A 的节点会被 B 的配置器释放，直接炸。
//
//  这里不实现完整内存池，只打印一个带 id 的示意配置器，
//  让你看到 propagate_on_container_swap 打开后容器的行为差异。
// ============================================================

template <class T>
struct TaggedAlloc
{
    using value_type = T;
    using propagate_on_container_swap = true_type;
    using propagate_on_container_move_assignment = true_type;

    int id = 0;

    TaggedAlloc() noexcept = default;
    explicit TaggedAlloc(int i) noexcept : id(i) {}
    template <class U>
    TaggedAlloc(const TaggedAlloc<U>& other) noexcept : id(other.id) {}

    T* allocate(size_t n) { return static_cast<T*>(::operator new(n * sizeof(T))); }
    void deallocate(T* p, size_t) { ::operator delete(p); }

    template <class U>
    bool operator==(const TaggedAlloc<U>& other) const noexcept { return id == other.id; }
    template <class U>
    bool operator!=(const TaggedAlloc<U>& other) const noexcept { return id != other.id; }
};

void statefulAllocatorDemo()
{
    cout << "\n============================================================" << endl;
    cout << "  六、带状态配置器与传播 traits" << endl;
    cout << "============================================================" << endl;

    using V = vector<int, TaggedAlloc<int>>;
    V a(TaggedAlloc<int>(1));
    V b(TaggedAlloc<int>(2));
    a.push_back(10);
    b.push_back(20);

    cout << "  swap 前：a.alloc.id=" << a.get_allocator().id
         << "，b.alloc.id=" << b.get_allocator().id << endl;

    a.swap(b);

    cout << "  swap 后：a.alloc.id=" << a.get_allocator().id
         << "，b.alloc.id=" << b.get_allocator().id << endl;
    cout << "  propagate_on_container_swap = true，配置器跟着元素一起换：" << endl;
    cout << "  a 拿到 b 的元素，配置器也变成 id=2，元素和配置器始终配套，" << endl;
    cout << "  释放时不会找错人。" << endl;
    cout << "  如果 traits 是 false，标准要求两个配置器相等才能 swap，" << endl;
    cout << "  否则行为未定义——这也是自定义配置器最容易踩的坑。" << endl;
}

// ============================================================
// 八、C++17 的 pmr：标准库自带的内存资源
// ============================================================
//  std::pmr 把配置器换成了运行期的 memory_resource 指针：
//      monotonic_buffer_resource  → 只增不减的线性池
//      unsynchronized_pool_resource → 不分线程的池
//      synchronized_pool_resource   → 线程安全池
//  适合大量小对象、短生命周期、想减少 malloc 次数的场景。
//
//  这里只做一个引子，让你知道：配置器不是只能编译期指定，
//  C++17 起也可以运行期注入。真正的内存池实践放在
//  performance/ 模块里展开。
// ============================================================

void pmrIntro()
{
    cout << "\n============================================================" << endl;
    cout << "  七、C++17 pmr：运行期内存资源（了解）" << endl;
    cout << "============================================================" << endl;
    cout << "  pmr = polymorphic memory resource" << endl;
    cout << "  用法：std::pmr::vector<int> v{&pool};" << endl;
    cout << "  常见资源：" << endl;
    cout << "    monotonic_buffer_resource  线性递增，不回收单个对象" << endl;
    cout << "    unsynchronized_pool_resource  小对象池，单线程用" << endl;
    cout << "    synchronized_pool_resource    线程安全池" << endl;
    cout << "  好处：减少 malloc/free 次数，提升缓存局部性。" << endl;
    cout << "  代价：生命周期和 pool 绑定，pool 死了容器不能再用。" << endl;
}

// ============================================================
// 老师傅的话
// ============================================================

void masterWords()
{
    cout << "\n============================================================" << endl;
    cout << "  老师傅的话" << endl;
    cout << "============================================================" << endl;
    cout << "  徒弟，配置器这章看着最“底层”，其实最该先看。" << endl;
    cout << endl;
    cout << "  1. 容器不直接 new 对象，它先要内存，再构造。" << endl;
    cout << "     你记住这个顺序，就懂了 reserve 为什么不构造元素，" << endl;
    cout << "     也懂了 vector 扩容为什么要“先分配新家，再搬家”。" << endl;
    cout << endl;
    cout << "  2. 节点容器为什么胖？因为节点要自己存指针。" << endl;
    cout << "     list 一个 int 元素实际要 24 字节左右，map 要 40 字节左右。" << endl;
    cout << "     不是 STL 浪费，是链式结构必须付的“路费”。" << endl;
    cout << endl;
    cout << "  3. 自定义配置器，先想清楚状态。" << endl;
    cout << "     无状态配置器随便复制；带状态的必须处理 propagate_on_container_*，" << endl;
    cout << "     否则 swap / move 时会出现“A 的内存被 B 释放”的惨案。" << endl;
    cout << endl;
    cout << "  4. 别为了炫技乱换配置器。" << endl;
    cout << "     默认 allocator 已经很快；真遇到分配热点，先用性能工具量，" << endl;
    cout << "     再考虑 pmr 或自己写池。没有测量的优化都是猜。" << endl;
    cout << "============================================================" << endl;
}

// ============================================================
// 小练习
// ============================================================

void exercises()
{
    cout << "\n============================================================" << endl;
    cout << "  小练习（回去自己做）" << endl;
    cout << "============================================================" << endl;
    cout << "  1. 把 CountingAlloc 改成“按 T 分别计数”，观察 list/map 的" << endl;
    cout << "     节点类型到底是什么（提示：打印 typeid 或 sizeof）。" << endl;
    cout << "  2. 用 CountingAlloc 统计 map 插入 1000 个元素的总字节数，" << endl;
    cout << "     算平均每元素开销，再和 vector 比。" << endl;
    cout << "  3. 给 TaggedAlloc 把 propagate_on_container_swap 改成 false，" << endl;
    cout << "     让两个 id 不同的容器 swap，看看会发生什么（小心，可能是 UB）。" << endl;
    cout << "  4. 查 cppreference 的 std::pmr，把 pmrIntro 变成一个真能跑的" << endl;
    cout << "     pmr::vector 例子，打印 get_allocator().resource()。" << endl;
    cout << "  5. 思考：为什么 std::vector 不把 allocator 作为成员存起来，" << endl;
    cout << "     而是用空基类优化（EBO）？sizeof(vector<int>) 是多少？" << endl;
    cout << "============================================================" << endl;
}

int main()
{
    sixComponents();
    allocateVsConstruct();
    traitsDemo();
    rebindDemo();
    measureContainerOverhead();
    statefulAllocatorDemo();
    pmrIntro();
    masterWords();
    exercises();
    return 0;
}
