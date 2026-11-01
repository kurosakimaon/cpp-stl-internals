// ============================================================
//  02_vector_internals.cpp - vector：三根指针和一条增长曲线
// ============================================================
//  徒弟，vector 是你用得最多的容器，没有之一。
//  但很多人用了三年，还说不清 size 和 capacity 的区别，
//  更不知道为什么 push_back 有时候 O(1)、有时候 O(n)，
//  以及为什么扩容之后迭代器全废了。
//
//  这一章把 vector 拆成三根指针，再手写一个 MiniVector。
//  你看完就明白：所谓动态数组，一点都不神秘。
// ============================================================

#include <iostream>
#include <iomanip>
#include <vector>
#include <memory>
#include <utility>
#include <string>
#include <algorithm>
#include <cstddef>
#include <type_traits>

using namespace std;

template <class V>
void dump(const char* tag, const V& v)
{
    cout << "  " << tag << "：size=" << v.size()
         << "，capacity=" << v.capacity()
         << "，data=" << static_cast<const void*>(v.data()) << endl;
}

// ============================================================
// 一、三根指针：vector 的全部家当
// ============================================================
//  libstdc++ 的 vector 内部只有一个 _Vector_impl，里面三根指针：
//
//      T* _M_start;           // 首元素
//      T* _M_finish;          // 尾后（最后一个元素的下一个位置）
//      T* _M_end_of_storage;  // 已分配内存的尾后
//
//  于是：
//      size()     = _M_finish - _M_start
//      capacity() = _M_end_of_storage - _M_start
//      empty()    = _M_start == _M_finish
//      data()     = _M_start
//
//  MSVC 叫 _Myfirst / _Mylast / _Myend，名字不同，布局一模一样。
//  一个 vector 对象本身通常只有 24 字节（64 位下），
//  元素都在堆上，所以 sizeof(vector) 不能代表它占的内存。
// ============================================================

void threePointers()
{
    cout << "============================================================" << endl;
    cout << "  一、三根指针" << endl;
    cout << "============================================================" << endl;

    vector<int> v{1, 2, 3};
    cout << "  vector<int> 对象大小 = " << sizeof(v) << " 字节（三根指针）" << endl;
    cout << "  size()     = " << v.size() << endl;
    cout << "  capacity() = " << v.capacity() << endl;
    cout << "  data()     = " << static_cast<const void*>(v.data()) << endl;
    cout << "  地址差验证：" << endl;
    cout << "    &v[1] - &v[0] = " << (&v[1] - &v[0]) << "（相邻元素差 1 个 int）" << endl;
    cout << "    sizeof(int)   = " << sizeof(int) << " 字节" << endl;
    cout << endl;
    cout << "  注意：size 是“住了几个”，capacity 是“能住几个”。" << endl;
    cout << "  capacity 之后的槽位还没构造对象，访问就是 UB。" << endl;
}

// ============================================================
// 二、扩容曲线：为什么 push_back 是摊还 O(1)
// ============================================================
//  满了就申请一块更大的内存，把旧元素搬过去，
//  析构旧元素、释放旧内存，最后更新三根指针。
//
//  增长倍数由实现决定：
//      libstdc++（GCC）：2 倍
//      MSVC          ：约 1.5 倍
//      libc++（Clang）：2 倍
//
//  2 倍增长下，n 次 push_back 的扩容搬运总量是 1+2+4+...+2^k
//  （2^k 是不小于 n 的最小 2 的幂），总和 < 2n，摊到每次插入仍是 O(1)。
//  这就是“摊还 O(1)”的来历。
// ============================================================

void growthCurve()
{
    cout << "\n============================================================" << endl;
    cout << "  二、扩容曲线" << endl;
    cout << "============================================================" << endl;

    vector<int> v;
    size_t lastCap = v.capacity();
    cout << "  插入 40 个元素，记录 capacity 变化：" << endl;
    cout << "    ";

    for (int i = 1; i <= 40; ++i)
    {
        v.push_back(i);
        if (v.capacity() != lastCap)
        {
            cout << "[" << v.size() << "→" << v.capacity() << "] ";
            lastCap = v.capacity();
        }
    }
    cout << endl;
    cout << "  最终：size=" << v.size() << "，capacity=" << v.capacity()
         << "，浪费 " << (v.capacity() - v.size()) << " 个槽位" << endl;
    cout << "  方括号里是“第几个元素触发扩容 → 新容量”。" << endl;
    cout << "  可以看到是 1、2、4、8、16、32、64 的翻倍序列。" << endl;
    cout << "  扩容那一次要搬 n 个元素，是 O(n)；但摊到前面 n 次插入，每次 O(1)。" << endl;
}

void reserveVsResize()
{
    cout << "\n============================================================" << endl;
    cout << "  三、reserve 和 resize 不是一回事" << endl;
    cout << "============================================================" << endl;

    vector<int> a;
    a.reserve(10);
    cout << "  a.reserve(10) 后：size=" << a.size()
         << "，capacity=" << a.capacity() << "（没有构造任何元素）" << endl;

    vector<int> b;
    b.resize(10);
    cout << "  b.resize(10)  后：size=" << b.size()
         << "，capacity=" << b.capacity() << "（10 个元素已默认构造）" << endl;

    b.push_back(11);
    cout << "  b.push_back(11) 后：size=" << b.size()
         << "，capacity=" << b.capacity() << "（刚好超容量，触发扩容）" << endl;

    vector<int> c;
    c.reserve(10);
    for (int i = 0; i < 10; ++i) c.push_back(i);
    cout << "  c 先 reserve(10) 再插 10 个：capacity=" << c.capacity()
         << "（没有扩容，一次分配到位）" << endl;

    cout << endl;
    cout << "  reserve 只改容量，不构造元素；resize 真的改元素个数。" << endl;
    cout << "  reserve 之后不能写 v[0]——那里还没有对象。" << endl;
}

// ============================================================
// 四、扩容时到底是移动还是拷贝？
// ============================================================
//  vector 扩容要把旧元素搬到新内存。用移动还是拷贝？
//  标准库的答案是 std::move_if_noexcept：
//
//      - 移动构造是 noexcept        → 移动
//      - 移动构造可能抛异常，且有拷贝构造 → 拷贝
//      - 只能移动不能拷贝            → 只能移动
//
//  为什么？因为扩容途中如果移动抛异常，旧元素可能已经被“搬走一半”，
//  旧 vector 不再完整，无法恢复。拷贝则不会破坏源对象，能保证
//  强异常安全（失败时原容器不变）。
//
//  所以：给你的移动构造函数加 noexcept，不是可选项，是性能关键。
// ============================================================

struct ThrowingMove
{
    int value;
    static int copies;
    static int moves;

    explicit ThrowingMove(int v) : value(v) {}
    ThrowingMove(const ThrowingMove& other) : value(other.value) { ++copies; }
    ThrowingMove(ThrowingMove&& other) noexcept(false) : value(other.value) { ++moves; }
};

int ThrowingMove::copies = 0;
int ThrowingMove::moves = 0;

struct NoThrowMove
{
    int value;
    static int copies;
    static int moves;

    explicit NoThrowMove(int v) : value(v) {}
    NoThrowMove(const NoThrowMove& other) : value(other.value) { ++copies; }
    NoThrowMove(NoThrowMove&& other) noexcept : value(other.value) { ++moves; }
};

int NoThrowMove::copies = 0;
int NoThrowMove::moves = 0;

void moveIfNoexcept()
{
    cout << "\n============================================================" << endl;
    cout << "  四、扩容时：move_if_noexcept 的选择" << endl;
    cout << "============================================================" << endl;

    {
        vector<ThrowingMove> v;
        v.reserve(2);
        v.emplace_back(1);
        v.emplace_back(2);
        ThrowingMove::copies = 0;
        ThrowingMove::moves = 0;
        v.emplace_back(3); // size==capacity，触发扩容，搬 2 个旧元素

        cout << "  移动构造 noexcept(false)：" << endl;
        cout << "    扩容搬运 2 个旧元素 → 拷贝 " << ThrowingMove::copies
             << " 次，移动 " << ThrowingMove::moves << " 次" << endl;
    }

    {
        vector<NoThrowMove> v;
        v.reserve(2);
        v.emplace_back(1);
        v.emplace_back(2);
        NoThrowMove::copies = 0;
        NoThrowMove::moves = 0;
        v.emplace_back(3);

        cout << "  移动构造 noexcept(true)：" << endl;
        cout << "    扩容搬运 2 个旧元素 → 拷贝 " << NoThrowMove::copies
             << " 次，移动 " << NoThrowMove::moves << " 次" << endl;
    }

    cout << endl;
    cout << "  结论：移动构造不标 noexcept，vector 扩容会退回拷贝，" << endl;
    cout << "  复杂对象上可能慢一个数量级。能标 noexcept 就标上。" << endl;
}

// ============================================================
// 五、迭代器失效：扩容把家搬了
// ============================================================
//  扩容后元素在新内存里，旧的指针、引用、迭代器全部指向已释放的旧家。
//  这就是“迭代器失效”的本质。
//
//  规则：
//      push_back 未扩容     → 只有 end() 变，其他迭代器/引用有效
//      push_back 触发扩容   → 全部失效
//      insert/erase 中间    → 插入/删除点及之后失效
//      reserve 之后容量够    → 后续 push_back 不搬家，引用保持有效
//
//  下面故意只比较地址，不解引用失效指针——解引用就是 UB。
// ============================================================

void iteratorInvalidation()
{
    cout << "\n============================================================" << endl;
    cout << "  五、迭代器失效" << endl;
    cout << "============================================================" << endl;

    vector<int> v{1, 2, 3};
    const int* oldData = v.data();
    size_t oldCap = v.capacity();

    while (v.size() < oldCap)
    {
        v.push_back(0);
    }
    cout << "  填满到 capacity=" << oldCap << "，data 还是 " << static_cast<const void*>(oldData) << endl;

    v.push_back(99); // 触发扩容
    cout << "  push_back(99) 触发扩容：" << endl;
    cout << "    旧 data=" << static_cast<const void*>(oldData)
         << "，新 data=" << static_cast<const void*>(v.data()) << endl;
    cout << "    地址" << (oldData == v.data() ? "没变" : "变了，旧指针/迭代器/引用全部失效") << endl;

    vector<int> w;
    w.reserve(8);
    const int* p = w.data();
    for (int i = 0; i < 8; ++i) w.push_back(i);
    cout << "  先 reserve(8) 再插 8 个：旧 data == 新 data ? "
         << (p == w.data() ? "true，没有搬家" : "false") << endl;

    cout << endl;
    cout << "  实战建议：循环里如果可能扩容，别提前存迭代器/指针；" << endl;
    cout << "  要么先 reserve，要么用下标，要么每轮重新取 begin()/end()。" << endl;
}

// ============================================================
// 六、push_back 与 emplace_back：少一次搬运
// ============================================================
//  push_back(Point(1,2))：先构造临时 Point，再移动进容器。
//  emplace_back(3,4)   ：把 3、4 直接转发给 Point 构造函数，
//                        在容器内存上原地构造，少一次移动。
//
//  对 int 这种平凡类型差别不大，对“构造代价高”的对象就很明显。
// ============================================================

struct Point
{
    int x;
    int y;
    static int ctors;
    static int copies;
    static int moves;

    Point(int a, int b) : x(a), y(b) { ++ctors; }
    Point(const Point& other) : x(other.x), y(other.y) { ++copies; }
    Point(Point&& other) noexcept : x(other.x), y(other.y) { ++moves; }
};

int Point::ctors = 0;
int Point::copies = 0;
int Point::moves = 0;

void emplaceVsPush()
{
    cout << "\n============================================================" << endl;
    cout << "  六、push_back vs emplace_back" << endl;
    cout << "============================================================" << endl;

    Point::ctors = Point::copies = Point::moves = 0;
    vector<Point> a;
    a.reserve(4);
    a.push_back(Point(1, 2));
    cout << "  push_back(Point(1,2))：构造 " << Point::ctors
         << " 次，拷贝 " << Point::copies
         << " 次，移动 " << Point::moves << " 次" << endl;

    Point::ctors = Point::copies = Point::moves = 0;
    vector<Point> b;
    b.reserve(4);
    b.emplace_back(3, 4);
    cout << "  emplace_back(3,4)   ：构造 " << Point::ctors
         << " 次，拷贝 " << Point::copies
         << " 次，移动 " << Point::moves << " 次" << endl;

    cout << endl;
    cout << "  能直接给构造参数，就用 emplace_back；" << endl;
    cout << "  已经有现成对象，就 push_back/insert，别为了 emplace 硬凑参数。" << endl;
}

// ============================================================
// 七、clear / erase / shrink_to_fit：容量不会自己还
// ============================================================
//  clear()  只析构元素，capacity 不变。
//  erase()  只搬移元素，capacity 不变。
//  shrink_to_fit() 请求把容量降到 size，标准不保证一定生效。
//
//  为什么 erase 不缩容量？因为缩容又要分配+搬家，
//  频繁 erase 的场景会被拖垮。标准把选择权留给调用者。
// ============================================================

void capacityAfterErase()
{
    cout << "\n============================================================" << endl;
    cout << "  七、clear / erase / shrink_to_fit" << endl;
    cout << "============================================================" << endl;

    vector<int> v(1000, 7);
    dump("初始 1000 个元素", v);

    v.erase(v.begin() + 10, v.end());
    dump("erase 掉后 990 个", v);

    v.shrink_to_fit();
    dump("shrink_to_fit 之后", v);

    v.clear();
    dump("clear 之后", v);
    cout << "  clear 只析构元素，内存块还留着，方便下次复用。" << endl;
}

// ============================================================
// 八、手写 MiniVector：把三根指针落到实处
// ============================================================
//  只用 std::allocator + placement new（allocator_traits::construct），
//  实现 reserve / push_back / pop_back / clear。
//  这不是生产级 vector（没有拷贝、移动、异常安全细节），
//  但足够你看清扩容时三根指针怎么变。
// ============================================================

template <class T>
class MiniVector
{
public:
    using Alloc = std::allocator<T>;
    using Traits = std::allocator_traits<Alloc>;

    MiniVector() = default;

    ~MiniVector()
    {
        clear();
        if (begin_)
        {
            alloc_.deallocate(begin_, capacity());
        }
    }

    MiniVector(const MiniVector&) = delete;
    MiniVector& operator=(const MiniVector&) = delete;

    size_t size() const { return begin_ ? static_cast<size_t>(end_ - begin_) : 0; }
    size_t capacity() const { return begin_ ? static_cast<size_t>(cap_ - begin_) : 0; }
    bool empty() const { return size() == 0; }

    T& operator[](size_t i) { return begin_[i]; }
    const T& operator[](size_t i) const { return begin_[i]; }

    void reserve(size_t n)
    {
        if (n <= capacity()) return;

        T* newBegin = alloc_.allocate(n);
        T* newEnd = newBegin;

        try
        {
            for (T* p = begin_; p != end_; ++p)
            {
                // 和标准库同一个策略：能移动且 noexcept 就移动，否则拷贝
                Traits::construct(alloc_, newEnd, std::move_if_noexcept(*p));
                ++newEnd;
            }
        }
        catch (...)
        {
            while (newEnd != newBegin)
            {
                --newEnd;
                Traits::destroy(alloc_, newEnd);
            }
            alloc_.deallocate(newBegin, n);
            throw;
        }

        for (T* p = begin_; p != end_; ++p)
        {
            Traits::destroy(alloc_, p);
        }
        if (begin_)
        {
            alloc_.deallocate(begin_, capacity());
        }

        begin_ = newBegin;
        end_ = newEnd;
        cap_ = newBegin + n;
    }

    void push_back(const T& value)
    {
        if (end_ == cap_) reserve(capacity() ? capacity() * 2 : 1);
        Traits::construct(alloc_, end_, value);
        ++end_;
    }

    void push_back(T&& value)
    {
        if (end_ == cap_) reserve(capacity() ? capacity() * 2 : 1);
        Traits::construct(alloc_, end_, std::move(value));
        ++end_;
    }

    void pop_back()
    {
        --end_;
        Traits::destroy(alloc_, end_);
    }

    void clear()
    {
        while (end_ != begin_)
        {
            --end_;
            Traits::destroy(alloc_, end_);
        }
    }

private:
    T* begin_ = nullptr;
    T* end_ = nullptr;
    T* cap_ = nullptr;
    Alloc alloc_{};
};

void miniVectorDemo()
{
    cout << "\n============================================================" << endl;
    cout << "  八、手写 MiniVector" << endl;
    cout << "============================================================" << endl;

    MiniVector<int> mv;
    cout << "  初始：size=" << mv.size() << "，capacity=" << mv.capacity() << endl;

    for (int i = 1; i <= 10; ++i)
    {
        mv.push_back(i * 10);
        cout << "  push_back(" << i * 10 << ") → size=" << mv.size()
             << "，capacity=" << mv.capacity()
             << "，首元素地址=" << static_cast<const void*>(&mv[0]) << endl;
    }

    cout << "  元素：";
    for (size_t i = 0; i < mv.size(); ++i) cout << mv[i] << " ";
    cout << endl;

    mv.pop_back();
    mv.clear();
    cout << "  pop_back + clear 后：size=" << mv.size()
         << "，capacity=" << mv.capacity() << "（容量仍然留着）" << endl;
    cout << "  析构时会 deallocate 这块容量。" << endl;
}

// ============================================================
// 老师傅的话
// ============================================================

void masterWords()
{
    cout << "\n============================================================" << endl;
    cout << "  老师傅的话" << endl;
    cout << "============================================================" << endl;
    cout << "  徒弟，vector 就三根指针，但它是 STL 里最值得先吃透的容器。" << endl;
    cout << endl;
    cout << "  1. size 和 capacity 千万别混。" << endl;
    cout << "     遍历按 size，扩容看 capacity。按 capacity 遍历是 UB。" << endl;
    cout << endl;
    cout << "  2. 扩容会搬家，搬完旧的全废。" << endl;
    cout << "     循环里存迭代器、存引用，再 push_back，是新手经典翻车现场。" << endl;
    cout << "     要么先 reserve，要么用下标，要么每轮重新取迭代器。" << endl;
    cout << endl;
    cout << "  3. 移动构造一定标 noexcept。" << endl;
    cout << "     你不标，vector 扩容就退回拷贝；对象一大，性能差得离谱。" << endl;
    cout << "     这不是玄学，是 move_if_noexcept 的硬规则。" << endl;
    cout << endl;
    cout << "  4. 别迷信“list 插入快”。" << endl;
    cout << "     list 插入是 O(1)，但你得先 O(n) 找到位置；" << endl;
    cout << "     vector 中间插入是 O(n) 搬元素，但内存连续、缓存友好。" << endl;
    cout << "     数据量不大时，vector 往往更快。默认就用 vector。" << endl;
    cout << endl;
    cout << "  5. reserve 是你唯一能提前控制扩容的手段。" << endl;
    cout << "     知道大概要装多少，就先 reserve，省掉一路翻倍搬家。" << endl;
    cout << "     但别乱 reserve 巨大容量，内存也是要还的。" << endl;
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
    cout << "  1. 给 MiniVector 加上拷贝构造和移动构造，" << endl;
    cout << "     用计数器验证移动后源对象的指针状态。" << endl;
    cout << "  2. 把 MiniVector 的增长因子改成 1.5 倍，观察扩容次数变化。" << endl;
    cout << "  3. 写一个类型，移动构造 noexcept(false)，但禁用拷贝构造，" << endl;
    cout << "     看看 vector 扩容时还能不能编译、走哪条路。" << endl;
    cout << "  4. 用 chrono 测一测 reserve(100000) 和不 reserve 的插入耗时，" << endl;
    cout << "     各跑十次取平均，别只跑一次。" << endl;
    cout << "  5. 验证 vector 的 erase 会不会缩容量，" << endl;
    cout << "     再对比 shrink_to_fit 前后 data() 地址是否变化。" << endl;
    cout << "  6. 思考：为什么 vector 不在每次 erase 后自动缩容？" << endl;
    cout << "============================================================" << endl;
}

int main()
{
    threePointers();
    growthCurve();
    reserveVsResize();
    moveIfNoexcept();
    iteratorInvalidation();
    emplaceVsPush();
    capacityAfterErase();
    miniVectorDemo();
    masterWords();
    exercises();
    return 0;
}
