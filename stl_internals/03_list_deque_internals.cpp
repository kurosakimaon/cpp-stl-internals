// ============================================================
//  03_list_deque_internals.cpp - list 与 deque：节点和分段
// ============================================================
//  徒弟，vector 是“一整块连续内存”，list 是“一堆散落的节点”，
//  deque 夹在中间，是“分段连续”。
//
//  这三种布局决定了它们的迭代器长什么样、插入删除什么代价、
//  什么时候迭代器/引用会失效。这一章把三种布局都量一遍。
// ============================================================

#include <iostream>
#include <iomanip>
#include <list>
#include <deque>
#include <vector>
#include <iterator>
#include <algorithm>
#include <cstddef>

using namespace std;

// ============================================================
// 一、list 的哨兵：一条首尾相接的环
// ============================================================
//  libstdc++ 的 list 节点分两层：
//
//      struct _List_node_base {
//          _List_node_base* _M_next;
//          _List_node_base* _M_prev;
//      };
//      template<class T>
//      struct _List_node : _List_node_base {
//          T _M_data;
//      };
//
//  链表本身不存 head/tail 指针，只存一个“哨兵节点”（header）：
//
//           +--------------------------------------+
//           |                                      |
//           v                                      |
//      [header] <-> [1] <-> [2] <-> [3] <-> [4] --+
//        ^                                      |
//        +--------------------------------------+
//
//  - header 不存数据，next 指向首元素，prev 指向尾元素
//  - 空链表时 header.next = header.prev = &header
//  - begin() = header.next，end() = &header
//  - ++end() 在 libstdc++ 实现上会绕回 begin()，但标准规定对 end() 自增是 UB，
//    这只是实现细节，生产代码不要依赖它
//
//  有了哨兵，头插、尾插、删除、splice 都不用特判空链表，
//  代码里少了一大堆 if，这就是哨兵的价值。
//
//  C++11 起标准要求 size() 是 O(1)，所以 list 对象里还缓存了 size。
//  每个节点 = 2 个指针 + 数据 + 对齐，64 位下一个 int 节点约 24 字节。
// ============================================================

void listLayout()
{
    cout << "============================================================" << endl;
    cout << "  一、list 的哨兵与节点布局" << endl;
    cout << "============================================================" << endl;

    list<int> l{10, 20, 30, 40};
    cout << "  sizeof(list<int>) = " << sizeof(l) << " 字节"
         << "（header 节点 + size，header 本身不存数据）" << endl;

    cout << "  各元素的数据地址：" << endl;
    for (auto it = l.begin(); it != l.end(); ++it)
    {
        cout << "    值 " << setw(3) << *it
             << "，&元素 = " << static_cast<const void*>(&*it) << endl;
    }
    cout << "  地址不保证连续，也不保证按插入顺序递增——每个节点单独 new。" << endl;
    cout << "  这就是 list 缓存不友好的根源：遍历时 CPU 要跳着取数据。" << endl;

    cout << endl;
    cout << "  验证哨兵：从 begin() 走 size() 步，正好到 end()，而 end() 就是 header。" << endl;
    auto it = l.begin();
    for (size_t i = 0; i < l.size(); ++i) ++it;
    cout << "    走了 size() = " << l.size() << " 步后，it == end() ? "
         << (it == l.end() ? "true" : "false") << endl;
    cout << "  header.next 就是 begin()，所以在 libstdc++ 里再 ++ 一次会绕回首元素；" << endl;
    cout << "  但标准规定对 end() 自增是 UB，生产代码不要依赖这个实现细节。" << endl;
}

// ============================================================
// 二、splice：只改指针，不搬元素
// ============================================================
//  list::splice 把一个链表的一段节点“剪”下来，接到另一个链表。
//  它只改几个 next/prev 指针，O(1)（给定位置时），不构造、不析构、不拷贝。
//  被移动元素的迭代器仍然有效，只是现在属于新链表。
//
//  这是 list 相对 vector 的真正优势场景：
//      把一段元素从一个容器挪到另一个容器，零成本。
// ============================================================

void spliceDemo()
{
    cout << "\n============================================================" << endl;
    cout << "  二、splice：零成本搬家" << endl;
    cout << "============================================================" << endl;

    list<int> a{1, 2, 3};
    list<int> b{4, 5, 6};

    auto pos = a.begin();
    ++pos; // 指向 2
    a.splice(pos, b); // 把 b 全部接到 2 前面

    cout << "  a.splice(指向2的位置, b) 后：" << endl;
    cout << "    a = ";
    for (int x : a) cout << x << " ";
    cout << "（size=" << a.size() << "）" << endl;
    cout << "    b = ";
    for (int x : b) cout << x << " ";
    cout << "（size=" << b.size() << "，已被搬空）" << endl;

    // 把 a 中的 4 单独挪到 c
    list<int> c;
    auto it4 = a.begin();
    ++it4; // 指向 4
    c.splice(c.begin(), a, it4);

    cout << "  再把 4 单独 splice 到 c：" << endl;
    cout << "    a = ";
    for (int x : a) cout << x << " ";
    cout << "（size=" << a.size() << "）" << endl;
    cout << "    c = ";
    for (int x : c) cout << x << " ";
    cout << "（size=" << c.size() << "）" << endl;
    cout << "  注意：指向 4 的迭代器 it4 仍然有效，只是现在属于 c。" << endl;
    cout << "  这不叫“移动”，叫“换了个链表挂上去”。" << endl;
}

void listPointerStability()
{
    cout << "\n============================================================" << endl;
    cout << "  三、list 的指针/引用稳定性" << endl;
    cout << "============================================================" << endl;

    list<int> l{1, 2, 3, 4};
    auto it2 = l.begin();
    ++it2;
    int* p = &*it2; // 指向 2
    cout << "  取指向元素 2 的指针，值 = " << *p << endl;

    l.erase(l.begin());   // 删 1，不影响 2
    l.push_back(5);       // 尾插，不影响 2
    l.push_front(0);      // 头插，不影响 2

    cout << "  删除 1、尾插 5、头插 0 之后，*p = " << *p
         << "（地址 " << static_cast<const void*>(p) << "）" << endl;
    cout << "  list 只在“删掉这个元素本身”时才让它的指针/引用失效。" << endl;
    cout << "  这是节点式容器最大的好处：其他元素的地址永远不变。" << endl;
}

// ============================================================
// 四、deque 的分段连续：中控器 + 缓冲区
// ============================================================
//  deque 不是“一整块连续内存”，也不是“每个元素一个节点”。
//  它是“一堆固定大小的缓冲区 + 一个中控器数组”：
//
//       map（中控器，指针数组）
//        +----+----+----+----+----+
//        |    |  * |  * |  * |    |
//        +----+----+----+----+----+
//                 |    |    |
//                 v    v    v
//            [buffer][buffer][buffer]   每块 512 字节（libstdc++ 对 int 是 128 个）
//
//  迭代器有四个指针：
//      T* cur;    // 当前元素
//      T* first;  // 当前缓冲区起点
//      T* last;   // 当前缓冲区终点
//      T** node;  // 当前缓冲区在中控器里的位置
//
//  operator++ 的逻辑：
//      ++cur;
//      if (cur == last) { node++; first = *node; last = first + buf_size; cur = first; }
//
//  所以 deque 的迭代器是“跨缓冲区”的随机访问迭代器：
//      it + n 要先算跨过几个缓冲区，再算落在哪。
//
//  两端插入 O(1)：push_front 只在新缓冲区头部构造，push_back 只在尾部构造，
//  都不搬旧元素。中间插入才要挑“离哪端近”搬哪边，O(min(i, n-i))。
// ============================================================

void dequeLayout()
{
    cout << "\n============================================================" << endl;
    cout << "  四、deque 的中控器与缓冲区" << endl;
    cout << "============================================================" << endl;

    deque<int> d;
    for (int i = 0; i < 1000; ++i) d.push_back(i);

    cout << "  sizeof(deque<int>) = " << sizeof(d) << " 字节"
         << "（中控器指针 + map_size + 两个四指针迭代器）" << endl;
    cout << "  size = " << d.size() << "，front 地址 = "
         << static_cast<const void*>(&d.front())
         << "，back 地址 = " << static_cast<const void*>(&d.back()) << endl;

    cout << "  扫描相邻元素地址，找缓冲区边界（int 的缓冲区通常是 128 个）：" << endl;
    const int* prev = &d[0];
    int boundaries = 0;
    for (size_t i = 1; i < d.size() && boundaries < 6; ++i)
    {
        const int* cur = &d[i];
        if (cur != prev + 1)
        {
            ptrdiff_t delta = reinterpret_cast<const char*>(cur)
                            - reinterpret_cast<const char*>(prev);
            cout << "    d[" << (i - 1) << "] → d[" << i << "]：地址跳变 "
                 << delta << " 字节（跨缓冲区）" << endl;
            ++boundaries;
        }
        prev = cur;
    }

    cout << "  同一缓冲区内地址连续，跨缓冲区时地址会跳。" << endl;
    cout << "  跳变多少不重要（缓冲区是单独分配的，可能恰好挨着），" << endl;
    cout << "  重要的是它不再是 sizeof(int)=4 的固定步长。" << endl;
    cout << "  所以 deque 支持 O(1) 随机访问，但比 vector 多一层“算在哪个缓冲区”。" << endl;
}

void dequeEndInsert()
{
    cout << "\n============================================================" << endl;
    cout << "  五、deque 两端插入：引用为什么还活着" << endl;
    cout << "============================================================" << endl;

    deque<int> d;
    for (int i = 0; i < 200; ++i) d.push_back(i);

    int& ref = d[100];       // 绑定到“值为 100 的那个对象”
    int* addr = &ref;
    int valueBefore = ref;

    for (int i = 0; i < 100; ++i) d.push_front(-i);

    cout << "  往头部插 100 个之后：" << endl;
    cout << "    引用仍然指向原对象，值 = " << ref
         << "（插入前也是 " << valueBefore << "，没被搬走）" << endl;
    cout << "    地址变了吗？" << (addr == &ref ? "没变" : "变了") << endl;
    cout << "    但它现在的下标已经不是 100 了，因为前面的元素整体后移。" << endl;

    cout << endl;
    cout << "  规则：" << endl;
    cout << "    push_front/push_back  → 迭代器全失效，但已有元素的引用/指针有效" << endl;
    cout << "    insert/erase 中间     → 标准说迭代器和引用全部失效" << endl;
    cout << "  因为中间插入可能把某一边的元素整体搬走，引用就不一定还指着原对象了。" << endl;

    // 中间插入后，检查原指针还能不能落在某个元素上（只比较地址，不解引用）
    deque<int> e;
    for (int i = 0; i < 200; ++i) e.push_back(i);
    int* p = &e[100];
    e.insert(e.begin(), 999);

    bool found = false;
    for (size_t i = 0; i < e.size(); ++i)
    {
        if (&e[i] == p) { found = true; break; }
    }
    cout << "  在 begin() 处 insert 后，原 &e[100] 还能在容器里找到吗？"
         << (found ? "能（实现上没搬它，但标准仍判失效）" : "不能（已被搬走）") << endl;
}

// ============================================================
// 六、中间插入的搬移代价：三种容器对比
// ============================================================
//  用一个会计数的 Tracked 类型，统计“插入一个元素到中间”
//  到底发生了多少次拷贝/移动。
//
//      vector：插入点之后的元素整体后移          → O(n)
//      deque ：挑离得近的一端搬                  → O(min(i, n-i))
//      list  ：只改指针，不搬元素，但找位置 O(n)  → 0 次搬移
//
//  注意：list 的“0 次搬移”不等于“快”。找位置要遍历，
//  而且每个节点单独分配，缓存不友好。数据量小的时候 vector 常常更快。
// ============================================================

struct Tracked
{
    int value;
    static long copies;
    static long moves;

    explicit Tracked(int v) : value(v) {}
    Tracked(const Tracked& other) : value(other.value) { ++copies; }
    Tracked(Tracked&& other) noexcept : value(other.value) { ++moves; }

    Tracked& operator=(const Tracked& other)
    {
        value = other.value;
        ++copies;
        return *this;
    }

    Tracked& operator=(Tracked&& other) noexcept
    {
        value = other.value;
        ++moves;
        return *this;
    }
};

long Tracked::copies = 0;
long Tracked::moves = 0;

template <class Container>
void middleInsert(const char* name, size_t n)
{
    Container c;
    for (size_t i = 0; i < n; ++i)
    {
        c.emplace_back(static_cast<int>(i));
    }

    Tracked::copies = 0;
    Tracked::moves = 0;

    auto pos = c.begin();
    std::advance(pos, static_cast<ptrdiff_t>(n / 2));
    c.emplace(pos, 999);

    cout << "  " << name << " 在中间插入 1 个元素：" << endl;
    cout << "    拷贝 " << Tracked::copies << " 次，移动 " << Tracked::moves
         << " 次，size=" << c.size() << endl;
}

void middleInsertCompare()
{
    cout << "\n============================================================" << endl;
    cout << "  六、中间插入的搬移代价（n=5000）" << endl;
    cout << "============================================================" << endl;

    vector<Tracked> v;
    v.reserve(5001); // 排除扩容干扰，只看插入搬移
    for (int i = 0; i < 5000; ++i) v.emplace_back(i);
    Tracked::copies = 0;
    Tracked::moves = 0;
    v.emplace(v.begin() + 2500, 999);
    cout << "  vector 在中间插入 1 个元素：" << endl;
    cout << "    拷贝 " << Tracked::copies << " 次，移动 " << Tracked::moves
         << " 次（后半段整体后移）" << endl;

    middleInsert<deque<Tracked>>("deque ", 5000);
    middleInsert<list<Tracked>>("list  ", 5000);

    cout << endl;
    cout << "  list 的 0 次搬移只说明“插入动作本身”便宜，" << endl;
    cout << "  它找位置仍然要从头走 n/2 步，而且每个节点一次分配。" << endl;
    cout << "  所以实际选型不能只看复杂度，要看数据量、访问模式和缓存。" << endl;
}

// ============================================================
// 七、怎么选：一张表收尾
// ============================================================

void summary()
{
    cout << "\n============================================================" << endl;
    cout << "  七、vector / deque / list 怎么选" << endl;
    cout << "============================================================" << endl;
    cout << "  容器     随机访问   两端插入   中间插入      每元素开销" << endl;
    cout << "  vector    O(1)      尾 O(1)*    O(n)          低" << endl;
    cout << "  deque     O(1)      两端 O(1)   O(min(i,n-i)) 中" << endl;
    cout << "  list      O(n)      两端 O(1)   O(1)+定位     高" << endl;
    cout << endl;
    cout << "  * 尾插是摊还 O(1)：绝大多数插入只挪一个元素，" << endl;
    cout << "    扩容那一次要搬 n 个，摊到每次仍是常数。" << endl;
    cout << endl;
    cout << "  经验法则：" << endl;
    cout << "    - 默认 vector。连续内存 + 缓存友好，大多数场景最快。" << endl;
    cout << "    - 频繁头插头删，用 deque。" << endl;
    cout << "    - 需要 splice、需要元素地址永远稳定，才考虑 list。" << endl;
    cout << "    - 别为了“插入 O(1)”三个字就选 list，先量一量。" << endl;
}

// ============================================================
// 老师傅的话
// ============================================================

void masterWords()
{
    cout << "\n============================================================" << endl;
    cout << "  老师傅的话" << endl;
    cout << "============================================================" << endl;
    cout << "  徒弟，容器选型这事儿，最怕背复杂度表背傻了。" << endl;
    cout << endl;
    cout << "  1. list 的哨兵，是为了让代码没有特判。" << endl;
    cout << "     空链表、头插、尾插、删除，全靠一个 header 绕成环。" << endl;
    cout << "     你写链表时也可以学这一手，能少一半 if。" << endl;
    cout << endl;
    cout << "  2. splice 是 list 的看家本领。" << endl;
    cout << "     把一段节点从一个链表摘下来挂到另一个，只改指针。" << endl;
    cout << "     需要“O(1) 挪动一段元素而不拷贝”的场景，vector/deque 都做不到。" << endl;
    cout << endl;
    cout << "  3. deque 的“连续”是假的，是分段连续。" << endl;
    cout << "     随机访问 O(1)，但比 vector 多一层中控器寻址；" << endl;
    cout << "     两端插入不搬旧元素，这是它比 vector 强的地方。" << endl;
    cout << endl;
    cout << "  4. 失效规则要分开记。" << endl;
    cout << "     deque 两端插入：迭代器全废，引用还活；中间插入：标准说全废。" << endl;
    cout << "     list：除了被删的那个，其他指针/引用都不动。" << endl;
    cout << "     记不住就记一句：容器一改，先想迭代器。" << endl;
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
    cout << "  1. 用 list::splice 实现“把链表按奇偶拆成两个链表”，" << endl;
    cout << "     要求不拷贝元素。" << endl;
    cout << "  2. 把元素类型换成本文件的 Tracked，用 deque<Tracked> 和" << endl;
    cout << "     vector<Tracked> 各做 10000 次头插，统计拷贝/移动次数，解释差距。" << endl;
    cout << "  3. 打印 list<int> 相邻节点的地址差，看看节点到底多大；" << endl;
    cout << "     再换 list<double>、list<string> 对比。" << endl;
    cout << "  4. 用 deque 存 1000 个元素，打印每个缓冲区的首地址，" << endl;
    cout << "     验证缓冲区大小是不是 512 字节。" << endl;
    cout << "  5. 写一段代码复现“deque 中间插入后旧引用失效”，" << endl;
    cout << "     用 AddressSanitizer 观察（如果环境支持）。" << endl;
    cout << "  6. 思考：为什么 stack/queue 默认用 deque，而不是 vector？" << endl;
    cout << "============================================================" << endl;
}

int main()
{
    listLayout();
    spliceDemo();
    listPointerStability();
    dequeLayout();
    dequeEndInsert();
    middleInsertCompare();
    summary();
    masterWords();
    exercises();
    return 0;
}
