// ============================================================
//  06_iterators_algorithms.cpp - 迭代器与算法：STL 的胶水
// ============================================================
//  徒弟，容器和算法本来不认识彼此。
//  vector 不知道 sort 长什么样，sort 也不知道 vector 内部有三根指针。
//  它们中间只隔着一层“迭代器”。
//
//  这一章把胶水拆开：
//      1. iterator_traits 怎么把“迭代器能干什么”抽出来
//      2. distance / advance 怎么靠标签分派选 O(1) 还是 O(n)
//      3. std::sort 为什么叫“内省排序”
//      4. lower_bound 为什么比 find 快一个数量级
//      5. 适配器怎么把一种迭代器改造成另一种
//      6. remove 为什么不删元素，erase-remove 到底在干什么
//
//  这一章没有新容器，但它是前面五章能成立的原因。
// ============================================================

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <deque>
#include <list>
#include <unordered_map>
#include <algorithm>
#include <numeric>
#include <iterator>
#include <functional>
#include <random>
#include <type_traits>
#include <cmath>
#include <cstddef>

using namespace std;

void section(const string& title)
{
    cout << "\n============================================================" << endl;
    cout << "  " << title << endl;
    cout << "============================================================" << endl;
}

// ============================================================
// 一、五个分类与 iterator_traits
// ============================================================
//  C++ 把迭代器按“能做什么”分成五类。前四类能力只增不减：
//
//      输入迭代器   input_iterator_tag          只能读，单遍
//      前向迭代器   forward_iterator_tag        能读，能 ++，多遍
//      双向迭代器   bidirectional_iterator_tag  能 ++ 也能 --
//      随机访问     random_access_iterator_tag  能 +n、-n、比大小
//
//  输出迭代器 output_iterator_tag 是独立的一支（只能写），
//  不在上面这条能力递增链上。
//
//  算法不关心你是 vector 还是 deque，只关心 traits 里写的标签。
//  iterator_traits 就是那张“能力说明书”。
// ============================================================

template <class It>
string categoryName()
{
    using Cat = typename iterator_traits<It>::iterator_category;

    if constexpr (is_same_v<Cat, random_access_iterator_tag>) return "random_access";
    else if constexpr (is_same_v<Cat, bidirectional_iterator_tag>) return "bidirectional";
    else if constexpr (is_same_v<Cat, forward_iterator_tag>) return "forward";
    else if constexpr (is_same_v<Cat, input_iterator_tag>) return "input";
    else return "output";
}

void traitsIntro()
{
    section("一、五个分类与 iterator_traits");

    cout << "  常见迭代器的分类：" << endl;
    cout << "    vector<int>::iterator           → "
         << categoryName<vector<int>::iterator>() << endl;
    cout << "    deque<int>::iterator            → "
         << categoryName<deque<int>::iterator>() << endl;
    cout << "    list<int>::iterator             → "
         << categoryName<list<int>::iterator>() << endl;
    cout << "    unordered_map<int,int>::iterator→ "
         << categoryName<unordered_map<int, int>::iterator>() << endl;
    cout << "    int*（裸指针也是迭代器）        → "
         << categoryName<int*>() << endl;

    using Traits = iterator_traits<vector<int>::iterator>;
    cout << endl;
    cout << "  iterator_traits<vector<int>::iterator> 里有什么：" << endl;
    cout << "    value_type        = int" << endl;
    cout << "    difference_type   = ptrdiff_t（"
         << (is_same_v<Traits::difference_type, ptrdiff_t> ? "是" : "不是")
         << "）" << endl;
    cout << "    pointer           = int*" << endl;
    cout << "    reference         = int&" << endl;
    cout << "    iterator_category = random_access_iterator_tag" << endl;

    cout << endl;
    cout << "  算法只读这张说明书，不看容器的具体类型。" << endl;
    cout << "  所以同一个 std::sort 能排 vector、deque、原生数组，" << endl;
    cout << "  但排不了 list——因为 list 的迭代器没有随机访问能力。" << endl;
}

// ============================================================
// 二、distance / advance：标签分派怎么选实现
// ============================================================
//  同一个 std::distance，对随机访问迭代器直接做减法，
//  对前向/双向迭代器只能一步一步走。
//  这不是重载，而是“标签分派”：拿 category 当第三个参数选函数。
//
//  下面这个 CountingIt 把 ++ 和 - 的次数记下来，
//  好让我们看见算法到底走了哪条路。
// ============================================================

template <class Tag>
class CountingIt
{
public:
    using iterator_category = Tag;
    using value_type = int;
    using difference_type = ptrdiff_t;
    using pointer = const int*;
    using reference = const int&;

    inline static long increments = 0;
    inline static long minusCalls = 0;

    explicit CountingIt(const int* p = nullptr) : p_(p) {}

    reference operator*() const { return *p_; }

    CountingIt& operator++()
    {
        ++p_;
        ++increments;
        return *this;
    }

    CountingIt operator++(int)
    {
        CountingIt tmp = *this;
        ++*this;
        return tmp;
    }

    CountingIt& operator--()
    {
        --p_;
        ++increments;
        return *this;
    }

    CountingIt operator--(int)
    {
        CountingIt tmp = *this;
        --*this;
        return tmp;
    }

    CountingIt& operator+=(difference_type n)
    {
        p_ += n;
        return *this;
    }

    CountingIt operator+(difference_type n) const { return CountingIt(p_ + n); }
    CountingIt& operator-=(difference_type n)
    {
        p_ -= n;
        return *this;
    }

    difference_type operator-(const CountingIt& other) const
    {
        ++minusCalls;
        return p_ - other.p_;
    }

    bool operator==(const CountingIt& other) const { return p_ == other.p_; }
    bool operator!=(const CountingIt& other) const { return p_ != other.p_; }

private:
    const int* p_;
};

void distanceDispatch()
{
    section("二、distance / advance：标签分派怎么选实现");

    vector<int> v(1000);
    iota(v.begin(), v.end(), 0);

    {
        using RIt = CountingIt<random_access_iterator_tag>;
        RIt first(v.data());
        RIt last(v.data() + v.size());
        RIt::increments = 0;
        RIt::minusCalls = 0;

        auto n = distance(first, last);
        cout << "  【随机访问】distance(begin, end) = " << n << endl;
        cout << "    ++ 调用 " << RIt::increments << " 次，"
             << "- 运算 " << RIt::minusCalls << " 次" << endl;
        cout << "    直接相减，O(1)。" << endl;
    }

    {
        using FIt = CountingIt<forward_iterator_tag>;
        FIt first(v.data());
        FIt last(v.data() + v.size());
        FIt::increments = 0;
        FIt::minusCalls = 0;

        auto n = distance(first, last);
        cout << "\n  【前向】distance(begin, end) = " << n << endl;
        cout << "    ++ 调用 " << FIt::increments << " 次，"
             << "- 运算 " << FIt::minusCalls << " 次" << endl;
        cout << "    只能一步步走，O(n)。" << endl;
    }

    {
        using BIt = CountingIt<bidirectional_iterator_tag>;
        BIt first(v.data());
        BIt::increments = 0;

        advance(first, 500);
        cout << "\n  【双向】advance(it, 500)：" << endl;
        cout << "    ++ 调用 " << BIt::increments << " 次" << endl;

        BIt::increments = 0;
        advance(first, -200);
        cout << "    再 advance(it, -200)：++/-- 调用 "
             << BIt::increments << " 次（负数走 --）" << endl;
    }

    {
        using RIt = CountingIt<random_access_iterator_tag>;
        RIt first(v.data());
        RIt::increments = 0;

        advance(first, 500);
        cout << "\n  【随机访问】advance(it, 500)：" << endl;
        cout << "    ++ 调用 " << RIt::increments << " 次" << endl;
        cout << "    直接用 +=，O(1)。" << endl;
    }

    cout << endl;
    cout << "  同一句 std::distance，对不同迭代器走了不同代码路径。" << endl;
    cout << "  这就是为什么 list 的 size() 在 C++11 之后是 O(1)，" << endl;
    cout << "  而 std::distance(list.begin(), list.end()) 仍然是 O(n)。" << endl;
}

// ============================================================
// 三、自己写一遍标签分派
// ============================================================
//  标准库的写法很朴素：给 category 加一个重载版本，
//  最后再包一层，把 iterator_traits 里的标签取出来当参数。
// ============================================================

template <class It>
void myAdvanceImpl(It& it, ptrdiff_t n, random_access_iterator_tag)
{
    it += n;
}

template <class It>
void myAdvanceImpl(It& it, ptrdiff_t n, bidirectional_iterator_tag)
{
    if (n >= 0)
    {
        while (n--) ++it;
    }
    else
    {
        while (n++) --it;
    }
}

template <class It>
void myAdvanceImpl(It& it, ptrdiff_t n, forward_iterator_tag)
{
    while (n--) ++it;
}

template <class It>
void myAdvance(It& it, ptrdiff_t n)
{
    using Cat = typename iterator_traits<It>::iterator_category;
    myAdvanceImpl(it, n, Cat{});
}

void manualDispatch()
{
    section("三、自己写一遍标签分派");

    vector<int> v(1000);
    iota(v.begin(), v.end(), 0);

    {
        using RIt = CountingIt<random_access_iterator_tag>;
        RIt it(v.data());
        RIt::increments = 0;
        myAdvance(it, 700);
        cout << "  随机访问：myAdvance(it, 700) → ++ 调用 "
             << RIt::increments << " 次，*it=" << *it << endl;
    }

    {
        using FIt = CountingIt<forward_iterator_tag>;
        FIt it(v.data());
        FIt::increments = 0;
        myAdvance(it, 700);
        cout << "  前向：    myAdvance(it, 700) → ++ 调用 "
             << FIt::increments << " 次，*it=" << *it << endl;
    }

    cout << endl;
    cout << "  三个重载函数体几乎一样，差别只在“怎么走”。" << endl;
    cout << "  C++20 的 concepts 可以把这套标签分派写得更短，" << endl;
    cout << "  但底层思路没变：按能力选实现。" << endl;
}

// ============================================================
// 四、std::sort 的内省排序
// ============================================================
//  std::sort 不是纯快排，它叫 introsort（内省排序）：
//      1. 三数取中选 pivot，递归快排
//      2. 递归深度超过 2 × log2(n) 就改用堆排序，防止最坏 O(n²)
//      3. 小区间（通常 ≤ 16 个元素）改用插入排序，减少递归开销
//
//  所以它的复杂度稳定在 O(n log n)，最坏也是。
//  代价是“不稳定”：相等元素的相对顺序不保证。
//  要稳定就用 stable_sort（归并排序，可能额外分配内存）。
// ============================================================

struct CountingLess
{
    inline static long comparisons = 0;
    bool operator()(int a, int b) const
    {
        ++comparisons;
        return a < b;
    }
};

void countingSort(vector<int>& v, const string& label)
{
    CountingLess::comparisons = 0;
    sort(v.begin(), v.end(), CountingLess{});
    cout << "    " << left << setw(10) << label
         << "比较 " << setw(7) << CountingLess::comparisons
         << " 次，有序=" << (is_sorted(v.begin(), v.end()) ? "是" : "否") << endl;
}

void sortDemo()
{
    section("四、std::sort 的内省排序");

    const int N = 5000;
    vector<int> base(N);
    iota(base.begin(), base.end(), 0);

    mt19937 rng(2026);
    shuffle(base.begin(), base.end(), rng);

    vector<int> randomData = base;
    vector<int> sortedData = base;
    sort(sortedData.begin(), sortedData.end());
    vector<int> reversedData = base;
    sort(reversedData.begin(), reversedData.end(), greater<int>{});

    cout << "  n=" << N << "，三种输入各排一次（比较次数由自定义比较器统计）：" << endl;
    countingSort(randomData, "随机");
    countingSort(sortedData, "已有序");
    countingSort(reversedData, "完全逆序");

    cout << endl;
    cout << "  n log2(n) ≈ " << static_cast<long>(N * log2(N)) << "，"
         << "实际比较次数在同一量级。" << endl;
    cout << "  已有序和逆序都没有退化到 O(n²)，因为三数取中和堆排序兜底。" << endl;

    vector<int> stableData = base;
    CountingLess::comparisons = 0;
    stable_sort(stableData.begin(), stableData.end(), CountingLess{});
    cout << "  stable_sort 比较 " << CountingLess::comparisons
         << " 次，结果有序=" << (is_sorted(stableData.begin(), stableData.end()) ? "是" : "否")
         << "（归并排序比较次数往往更少，但会额外分配内存、搬移更多数据）" << endl;

    cout << endl;
    cout << "  list 不能给 std::sort 用：它的迭代器没有随机访问能力，编译就过不去。" << endl;
    cout << "  list 有自己的成员函数 list::sort()，内部按链表归并，不要求随机访问。" << endl;

    list<int> l{5, 1, 4, 2, 3};
    l.sort();
    cout << "    list::sort 后：";
    for (int x : l) cout << x << " ";
    cout << endl;
}

// ============================================================
// 五、lower_bound 为什么快
// ============================================================
//  lower_bound 要求区间已经有序，每次砍掉一半，O(log n)。
//  find_if 只能从头扫，O(n)。
//  同样是“找一个数”，数据量一大就是数量级的差距。
// ============================================================

struct CountingEqual
{
    int target;
    inline static long calls = 0;
    bool operator()(int x) const
    {
        ++calls;
        return x == target;
    }
};

void lowerBoundDemo()
{
    section("五、lower_bound 为什么快");

    const int N = 1000;
    vector<int> v(N);
    iota(v.begin(), v.end(), 0);   // 0..999，天然有序

    CountingLess::comparisons = 0;
    auto it = lower_bound(v.begin(), v.end(), N - 1, CountingLess{});
    cout << "  在 0.." << N - 1 << " 里找 " << N - 1 << "：" << endl;
    cout << "    lower_bound 比较 " << CountingLess::comparisons
         << " 次，命中值=" << *it << endl;

    CountingEqual pred{N - 1};
    CountingEqual::calls = 0;
    auto found = find_if(v.begin(), v.end(), pred);
    cout << "    find_if 比较 " << CountingEqual::calls
         << " 次，命中值=" << *found << endl;

    cout << "    log2(" << N << ") ≈ 10，一个是 10 次，一个是 " << N << " 次。" << endl;

    cout << endl;
    cout << "  lower_bound 找“第一个 >= x”的位置，upper_bound 找“第一个 > x”的位置：" << endl;
    vector<int> dup{1, 2, 2, 2, 3, 4, 5};
    auto lo = lower_bound(dup.begin(), dup.end(), 2);
    auto hi = upper_bound(dup.begin(), dup.end(), 2);
    cout << "    有序数组 1 2 2 2 3 4 5，lower_bound(2) 下标="
         << distance(dup.begin(), lo)
         << "，upper_bound(2) 下标=" << distance(dup.begin(), hi)
         << "，区间长度=" << distance(lo, hi) << endl;

    cout << endl;
    cout << "  lower_bound 只要求“有序 + 前向迭代器”，所以 list 也能用；" << endl;
    cout << "  但非随机访问迭代器上，advance 只能一步步走，总步数是 O(n)。" << endl;
    cout << "  要在链表上二分，比较次数仍是 O(log n)，代价是搬不动指针只能走。" << endl;
}

// ============================================================
// 六、迭代器适配器：把一种迭代器改造成另一种
// ============================================================
//  back_inserter：把“赋值”翻译成 push_back
//  ostream_iterator：把“赋值”翻译成输出
//  reverse_iterator：把 ++ 翻译成 --
//  move_iterator：把 *it 翻译成 std::move(*it)
//
//  适配器不改变容器，只是给迭代器套一层壳。
// ============================================================

void adapterDemo()
{
    section("六、迭代器适配器");

    {
        vector<int> src{10, 20, 30, 40, 50};
        vector<int> dst;

        copy(src.begin(), src.end(), back_inserter(dst));
        cout << "  back_inserter：copy 到空 vector，目标 size="
             << dst.size() << "，值：";
        for (int x : dst) cout << x << " ";
        cout << endl;
        cout << "    普通迭代器要求“目标已有元素”，back_inserter 把赋值变成 push_back。"
             << endl;
    }

    {
        vector<int> v{1, 2, 3, 4, 5};
        cout << "\n  ostream_iterator：";
        copy(v.begin(), v.end(), ostream_iterator<int>(cout, " "));
        cout << endl;
        cout << "    赋值运算符被翻译成“往流里写一个值”。" << endl;
    }

    {
        vector<int> v{1, 2, 3, 4, 5};
        cout << "\n  reverse_iterator：";
        for (auto rit = v.rbegin(); rit != v.rend(); ++rit) cout << *rit << " ";
        cout << endl;

        auto rit = v.rbegin();
        cout << "    *rbegin()=" << *rit
             << "，rbegin().base()==v.end() ? "
             << (rit.base() == v.end() ? "是" : "否")
             << "（base 指向“当前元素的后一个”）" << endl;
    }

    {
        vector<string> words{"alpha", "beta", "gamma"};
        vector<string> moved(make_move_iterator(words.begin()),
                            make_move_iterator(words.end()));

        cout << "\n  move_iterator：目标 =";
        for (const string& s : moved) cout << " " << s;
        cout << endl;

        int stillThere = static_cast<int>(
            count_if(words.begin(), words.end(),
                     [](const string& s) { return !s.empty(); }));
        cout << "    源容器里还剩 " << stillThere
             << " 个非空字符串（被移走的对象处于“有效但未指定”状态）" << endl;
        cout << "    对 string 来说通常是空串，但标准不保证具体值。" << endl;
    }
}

// ============================================================
// 七、remove 不删元素：erase-remove 惯用法
// ============================================================
//  std::remove 只是把“要留下的元素”往前挪，返回新的逻辑尾。
//  它不知道容器类型，没法调用 erase，所以容器的 size 不会变。
//  真正删除要再调一次 v.erase(newEnd, v.end())。
// ============================================================

void eraseRemoveDemo()
{
    section("七、remove 不删元素：erase-remove 惯用法");

    {
        vector<int> v{1, 2, 3, 2, 4, 2, 5};
        cout << "  原 vector：";
        for (int x : v) cout << x << " ";
        cout << "（size=" << v.size() << "）" << endl;

        auto newEnd = remove(v.begin(), v.end(), 2);
        cout << "  remove(..., 2) 之后：";
        for (int x : v) cout << x << " ";
        cout << endl;
        cout << "    size 仍是 " << v.size()
             << "，逻辑尾之后有 " << distance(newEnd, v.end())
             << " 个元素（值已无意义）" << endl;

        v.erase(newEnd, v.end());
        cout << "  erase(newEnd, end) 之后：";
        for (int x : v) cout << x << " ";
        cout << "（size=" << v.size() << "）" << endl;
        cout << "    capacity 不变=" << v.capacity()
             << "：erase 只改 size，不缩容量。" << endl;
    }

    {
        vector<int> v{1, 2, 3, 4, 5, 6, 7, 8};
        v.erase(remove_if(v.begin(), v.end(),
                          [](int x) { return x % 2 == 0; }),
                v.end());
        cout << "\n  remove_if 删偶数后：";
        for (int x : v) cout << x << " ";
        cout << endl;
    }

    {
        vector<int> v{1, 1, 2, 2, 2, 3, 1};
        v.erase(unique(v.begin(), v.end()), v.end());
        cout << "\n  unique 只去“相邻重复”，1 1 2 2 2 3 1 → ";
        for (int x : v) cout << x << " ";
        cout << endl;
        cout << "    要全去重，先 sort 再 unique，或者用 set/unordered_set。" << endl;
    }

    cout << endl;
    cout << "  记住这个固定搭配：v.erase(std::remove(...), v.end())。" << endl;
    cout << "  C++20 起有 std::erase(v, value) 和 std::erase_if(v, pred)，" << endl;
    cout << "  一行搞定，内部就是同一套 erase-remove。" << endl;
}

// ============================================================
// 老师傅的话
// ============================================================

void masterWords()
{
    section("老师傅的话");

    cout << "  徒弟，迭代器这层胶水，平时最不起眼，出事时最要命。" << endl;
    cout << endl;
    cout << "  1. 先看标签，再看接口。" << endl;
    cout << "     一个容器能不能用某个算法，不看它有没有 begin/end，" << endl;
    cout << "     而看它的迭代器属于哪一类。list 排不了 std::sort，就是这个原因。" << endl;
    cout << endl;
    cout << "  2. O(1) 和 O(n) 常常藏在同一行代码里。" << endl;
    cout << "     std::distance 对 vector 是减法，对 list 是走一遍。" << endl;
    cout << "     写模板代码时，别假设所有迭代器都一样快。" << endl;
    cout << endl;
    cout << "  3. sort 的“不稳定”不是 bug，是契约。" << endl;
    cout << "     相等元素的相对顺序不保证；要稳定就 stable_sort，" << endl;
    cout << "     或者把比较键做成全序（把原来的下标也纳入比较）。" << endl;
    cout << endl;
    cout << "  4. lower_bound 之前必须有序。" << endl;
    cout << "     忘了排序，二分不会报错，只会给你一个“看起来像对的错答案”。" << endl;
    cout << "     这是线上事故的常见来源。" << endl;
    cout << endl;
    cout << "  5. remove 不是 erase。" << endl;
    cout << "     remove 只搬元素、返回新尾；erase 才改 size。" << endl;
    cout << "     名字像，职责完全不同，别混。" << endl;
    cout << "============================================================" << endl;
}

// ============================================================
// 小练习
// ============================================================

void exercises()
{
    section("小练习（回去自己做）");

    cout << "  1. 给 CountingIt 加一个 operator<，验证 std::sort 在随机访问迭代器上" << endl;
    cout << "     确实能用，在 forward 标签上编译失败。" << endl;
    cout << "  2. 自己实现一个 input iterator（只能读、只能 ++），" << endl;
    cout << "     用 iterator_traits 打印它的五个 typedef。" << endl;
    cout << "  3. 用 CountingLess 对比 std::sort 和 std::stable_sort 在 n=1000/10000" << endl;
    cout << "     时的比较次数，画出增长趋势。" << endl;
    cout << "  4. 用 lower_bound/upper_bound 实现“查分数段 [60, 90) 的学生人数”，" << endl;
    cout << "     和逐个遍历的写法对比耗时。" << endl;
    cout << "  5. 用 back_inserter + transform 实现“把 vector<int> 转成 vector<string>”，" << endl;
    cout << "     不许写下标循环。" << endl;
    cout << "  6. 思考：为什么 std::remove 不能自己调用 erase？" << endl;
    cout << "     它拿到了容器吗？它只知道迭代器，还是连迭代器都不完整？" << endl;
    cout << "============================================================" << endl;
}

int main()
{
    traitsIntro();
    distanceDispatch();
    manualDispatch();
    sortDemo();
    lowerBoundDemo();
    adapterDemo();
    eraseRemoveDemo();
    masterWords();
    exercises();
    return 0;
}
