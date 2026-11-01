// ============================================================
//  05_hashtable_internals.cpp - unordered_map：桶、链表与 rehash
// ============================================================
//  徒弟，unordered_map 为什么不叫 hash_map，又为什么“无序”？
//  因为它的迭代顺序既不是插入顺序，也不是排序顺序，
//  而是“节点在桶链上的物理顺序”。
//
//  这一章我们做五件事：
//      1. 看清 libstdc++ _Hashtable 的布局：桶数组 + 全局单链表
//      2. 量一量 bucket_count 怎么长、load_factor 怎么算
//      3. 验证 rehash 之后：迭代器失效，指针和引用还活着
//      4. 对比好哈希和坏哈希，看桶链怎么退化
//      5. 手写一个极简哈希表，把桶数组 + 拉链 + rehash 自己实现一遍
//
//  看完你会明白：哈希表的 O(1) 不是白给的，
//  它拿“顺序”和“最坏情况”换来的。
// ============================================================

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <cstddef>

using namespace std;

// 统一的分节横幅，省得每章都手写三行 cout
void section(const string& title)
{
    cout << "\n============================================================" << endl;
    cout << "  " << title << endl;
    cout << "============================================================" << endl;
}

// ============================================================
// 一、布局：一个桶数组，一条全局单链表
// ============================================================
//  libstdc++ 的 _Hashtable 里有两个核心成员：
//      _Bucket*   _M_buckets;    // 桶数组
//      _Hash_node* _M_before_begin;  // 全局单链表的“头之前”
//
//  所有节点同时挂在一条单链表上；桶数组不存节点，存的是
//  “该桶第一个节点的前一个节点”的指针。为什么要存前驱？
//  因为单链表删除节点需要前驱，存了前驱才能在 O(1) 里摘链。
//
//  MSVC 的 xhash 不一样：它用 std::list 存节点，
//  桶数组里放的是 list 迭代器（指向前一个节点）。套路相同。
//
//  查找一个 key：
//      hash(key) % bucket_count  →  找到桶  →  沿这条桶链比较 key
//  所以哈希函数的质量，直接决定桶链长度。
// ============================================================

void layoutIntro()
{
    section("一、布局：桶数组 + 全局单链表");

    cout << "  逻辑结构（拉链法）：每个桶挂一条属于它的节点链" << endl;
    cout << endl;
    cout << "    桶数组 buckets[]" << endl;
    cout << "      [0] → nullptr" << endl;
    cout << "      [1] → (节点A) → (节点A') → nullptr" << endl;
    cout << "      [2] → nullptr" << endl;
    cout << "      [3] → (节点B) → nullptr" << endl;
    cout << endl;
    cout << "  但实现上不是每个桶各存一条独立链表：所有节点同时串在" << endl;
    cout << "  一条全局单链表上，桶数组存的是“该桶第一个节点的前驱”：" << endl;
    cout << endl;
    cout << "    全局链： before_begin → A → A' → B → nullptr" << endl;
    cout << "    buckets[0] = nullptr" << endl;
    cout << "    buckets[1] = &before_begin   （桶 1 从 A 开始）" << endl;
    cout << "    buckets[2] = nullptr" << endl;
    cout << "    buckets[3] = &A'             （桶 3 从 B 开始）" << endl;
    cout << endl;
    cout << "  存前驱是为了 O(1) 删除：单链表摘节点必须先拿到前驱。" << endl;
    cout << "  遍历 unordered_map 的顺序 = 节点在全局链上的顺序，" << endl;
    cout << "  既不是插入顺序，也不是 key 的顺序。" << endl;
    cout << "  rehash 会重挂这条链，遍历顺序随之改变。" << endl;
}

// ============================================================
// 二、桶数组怎么长：bucket_count 与 load_factor
// ============================================================

void growthCurve()
{
    section("二、桶数组怎么长：bucket_count 与 load_factor");

    unordered_map<int, int> m;

    cout << "  默认构造：size=" << m.size()
         << "，bucket_count=" << m.bucket_count()
         << "，load_factor=" << m.load_factor()
         << "，max_load_factor=" << m.max_load_factor() << endl;
    cout << endl;
    cout << "  逐个插入，只打印“桶数发生变化”的那几步：" << endl;

    size_t last = m.bucket_count();
    for (int i = 1; i <= 100; ++i)
    {
        m[i] = i;
        if (m.bucket_count() != last)
        {
            last = m.bucket_count();
            cout << "    插入第 " << setw(3) << i << " 个后：bucket_count="
                 << setw(4) << last << "，size=" << m.size()
                 << "，load_factor=" << fixed << setprecision(3)
                 << m.load_factor() << defaultfloat << endl;
        }
    }

    cout << endl;
    cout << "  libstdc++ 从一张质数表里取下一个桶数，不是严格的 2 倍。" << endl;
    cout << "  桶数选质数，是为了让 hash % bucket_count 分布更均匀；" << endl;
    cout << "  如果桶数是 2 的幂，低位重复的哈希值就会挤在同一个桶里。" << endl;
    cout << "  MSVC 反而喜欢用 2 的幂，各有各的理由，别记死数字。" << endl;
}

// ============================================================
// 三、reserve 与 max_load_factor：提前把桶备好
// ============================================================

void reserveDemo()
{
    section("三、reserve 与 max_load_factor");

    {
        unordered_map<int, int> m;
        m.reserve(100);

        cout << "  只调用 reserve(100)，还没插入元素：" << endl;
        cout << "    bucket_count=" << m.bucket_count()
             << "，size=" << m.size() << endl;

        for (int i = 0; i < 100; ++i) m[i] = i;

        cout << "  再插入 100 个元素后：" << endl;
        cout << "    bucket_count=" << m.bucket_count()
             << "，size=" << m.size()
             << "，load_factor=" << m.load_factor() << endl;
        cout << "  桶已经够用，整个插入过程一次 rehash 都没发生。" << endl;
    }

    {
        unordered_map<int, int> m;
        m.max_load_factor(0.5f);

        cout << endl;
        cout << "  把 max_load_factor 调到 0.5 之后：" << endl;
        cout << "    max_load_factor=" << m.max_load_factor() << endl;

        size_t last = m.bucket_count();
        for (int i = 0; i < 30; ++i)
        {
            m[i] = i;
            if (m.bucket_count() != last)
            {
                last = m.bucket_count();
                cout << "    插入第 " << setw(2) << i + 1 << " 个：bucket_count="
                     << setw(3) << last << "，load_factor="
                     << fixed << setprecision(3) << m.load_factor()
                     << defaultfloat << endl;
            }
        }
        cout << "  装填因子上限调低，桶会更早扩容，桶链更短，查询更快；" << endl;
        cout << "  代价是桶数组占的内存更多。这是典型的空间换时间。" << endl;
    }
}

// ============================================================
// 四、rehash：迭代器死了，指针和引用还活着
// ============================================================

void rehashSurvival()
{
    section("四、rehash：迭代器死了，指针和引用还活着");

    unordered_map<int, int> m;
    for (int i = 0; i < 10; ++i) m[i * 10] = i;

    int& ref = m[70];
    int* p = &ref;
    auto it = m.find(70);
    auto keyOfIt = it->first;

    cout << "  rehash 前：" << endl;
    cout << "    bucket_count=" << m.bucket_count()
         << "，&m[70]=" << static_cast<void*>(p)
         << "，值=" << ref
         << "，迭代器指向 key=" << keyOfIt << endl;

    cout << "    遍历顺序（10 个）：";
    int n = 0;
    for (const auto& kv : m)
    {
        cout << kv.first << " ";
        if (++n == 10) break;
    }
    cout << endl;

    m.rehash(0);   // 参数 0：缩到“刚好装下当前元素”的最小桶数

    cout << "  rehash(0) 后：" << endl;
    cout << "    bucket_count=" << m.bucket_count()
         << "，&m[70]=" << static_cast<void*>(&m[70])
         << "，引用值=" << ref << endl;
    cout << "    遍历顺序（10 个）：";
    n = 0;
    for (const auto& kv : m)
    {
        cout << kv.first << " ";
        if (++n == 10) break;
    }
    cout << endl;

    cout << endl;
    cout << "  节点地址没变，引用还能用 → rehash 只是重写 next 指针，" << endl;
    cout << "  节点本身没有搬家。" << endl;
    cout << "  但标准规定 rehash 后迭代器全部作废：节点没搬家，" << endl;
    cout << "  链上的位置却被重排了，旧迭代器可能指向错误的邻居。" << endl;
    cout << "  这就是 unordered_map 的失效规则和 vector 不一样的地方：" << endl;
    cout << "    vector 扩容：指针、引用、迭代器全部失效（搬家了）" << endl;
    cout << "    unordered_map rehash：迭代器失效，指针和引用仍然有效" << endl;
}

// ============================================================
// 五、哈希质量：好哈希与坏哈希的桶链
// ============================================================

struct CountingEqual
{
    static long calls;
    bool operator()(int a, int b) const
    {
        ++calls;
        return a == b;
    }
};
long CountingEqual::calls = 0;

struct ConstHash
{
    size_t operator()(int) const { return 0; }
};

void hashQuality()
{
    section("五、哈希质量：好哈希与坏哈希的桶链");

    const int N = 100;

    {
        unordered_map<int, int, hash<int>, CountingEqual> good;
        for (int i = 0; i < N; ++i) good[i] = i;

        cout << "  【好哈希】std::hash<int>，插入 0.." << N - 1 << "：" << endl;
        cout << "    bucket_count=" << good.bucket_count()
             << "，load_factor=" << fixed << setprecision(3)
             << good.load_factor() << defaultfloat << endl;

        size_t maxLen = 0;
        for (size_t b = 0; b < good.bucket_count(); ++b)
        {
            maxLen = max(maxLen, good.bucket_size(b));
        }
        cout << "    最长的一条桶链 = " << maxLen << " 个节点" << endl;

        CountingEqual::calls = 0;
        good.find(N - 1);
        cout << "    查 key=" << N - 1 << "：相等比较 "
             << CountingEqual::calls << " 次" << endl;

        CountingEqual::calls = 0;
        good.find(N + 1);
        cout << "    查不存在的 key=" << N + 1 << "：相等比较 "
             << CountingEqual::calls << " 次" << endl;
    }

    {
        unordered_map<int, int, ConstHash, CountingEqual> bad;
        for (int i = 0; i < N; ++i) bad[i] = i;

        cout << endl;
        cout << "  【坏哈希】所有 key 都返回 0，插入 0.." << N - 1 << "：" << endl;
        cout << "    bucket_count=" << bad.bucket_count()
             << "，load_factor=" << fixed << setprecision(3)
             << bad.load_factor() << defaultfloat << endl;

        size_t maxLen = 0;
        size_t maxBucket = 0;
        for (size_t b = 0; b < bad.bucket_count(); ++b)
        {
            if (bad.bucket_size(b) > maxLen)
            {
                maxLen = bad.bucket_size(b);
                maxBucket = b;
            }
        }
        cout << "    最长的一条桶链 = " << maxLen
             << " 个节点（bucket " << maxBucket << "）" << endl;

        CountingEqual::calls = 0;
        bad.find(0);   // 最早插入的 key，要走过大半条桶链
        cout << "    查 key=0（较早插入）：相等比较 "
             << CountingEqual::calls << " 次" << endl;

        CountingEqual::calls = 0;
        bad.find(N - 1);   // 最后插入的节点，正好在链头
        cout << "    查 key=" << N - 1 << "（链头）：相等比较 "
             << CountingEqual::calls << " 次" << endl;

        CountingEqual::calls = 0;
        bad.find(N + 1);   // 不存在的 key，要扫完整条链
        cout << "    查不存在的 key=" << N + 1 << "：相等比较 "
             << CountingEqual::calls << " 次" << endl;
    }

    cout << endl;
    cout << "  桶链退化成长链表，O(1) 就变成了 O(n)。" << endl;
    cout << "  所以自定义类型做 key 时，哈希函数要尽量打散，" << endl;
    cout << "  别只返回某一个固定字段的低几位。" << endl;
}

// ============================================================
// 六、自定义 hash / equal 到底被调用几次
// ============================================================

struct StringHash
{
    static long calls;
    size_t operator()(const string& s) const
    {
        ++calls;
        return hash<string>{}(s);
    }
};
long StringHash::calls = 0;

struct StringEqual
{
    static long calls;
    bool operator()(const string& a, const string& b) const
    {
        ++calls;
        return a == b;
    }
};
long StringEqual::calls = 0;

void customHashCalls()
{
    section("六、自定义 hash / equal 到底被调用几次");

    using Map = unordered_map<string, int, StringHash, StringEqual>;
    Map m;
    m.reserve(32);

    vector<string> words = {
        "apple", "banana", "cherry", "date", "elderberry",
        "fig", "grape", "honeydew", "kiwi", "lemon"
    };

    StringHash::calls = 0;
    StringEqual::calls = 0;
    for (size_t i = 0; i < words.size(); ++i) m[words[i]] = int(i);

    cout << "  插入 " << words.size() << " 个单词：" << endl;
    cout << "    hash 调用 " << StringHash::calls << " 次" << endl;
    cout << "    equal 调用 " << StringEqual::calls << " 次" << endl;
    cout << "    每个 key 先 hash 一次定位桶，桶空就不用比；" << endl;
    cout << "    桶里已有节点才逐个比较，直到命中或链尾。" << endl;

    StringHash::calls = 0;
    StringEqual::calls = 0;
    m.find("kiwi");
    m.find("mango");
    cout << endl;
    cout << "  再查一个命中（kiwi）和一个未命中（mango）：" << endl;
    cout << "    hash 调用 " << StringHash::calls << " 次（每次查找一次）" << endl;
    cout << "    equal 调用 " << StringEqual::calls << " 次" << endl;
    cout << "    命中时比较到目标就停；未命中时如果落在空桶，一次都不用比，" << endl;
    cout << "    落在有节点的桶，就要把整条桶链走完。这就是坏哈希最疼的地方。" << endl;

    cout << endl;
    cout << "  注意：rehash 不会重新计算每个 key 的哈希，" << endl;
    cout << "  它只把已有节点按新桶数重新挂链（用的还是原来的 hash 值）。" << endl;
}

// ============================================================
// 七、手写一个极简哈希表
// ============================================================
//  真实实现为了从单链表里 O(1) 删除节点，桶里存的是“前驱指针”。
//  这里先用最直观的“桶里存头指针”写法，把流程跑通：
//      insert → 算桶号 → 查重 → 头插 → 检查装填因子
//      rehash → 换一个更大的桶数组 → 把节点重新挂一遍
//
//  pool_ 用 vector 存节点，节点里存的是“下标”而不是指针，
//  这样 rehash 时下标不变，相当于真实实现里节点地址不变。
// ============================================================

class MiniHashTable
{
public:
    explicit MiniHashTable(size_t buckets = 4)
        : buckets_(buckets, -1)
    {
        pool_.reserve(64);
    }

    size_t size() const { return size_; }
    size_t bucketCount() const { return buckets_.size(); }
    double loadFactor() const
    {
        return static_cast<double>(size_) / static_cast<double>(buckets_.size());
    }

    bool find(int key, int& out) const
    {
        for (int i = buckets_[bucketOf(key)]; i != -1; i = pool_[i].next)
        {
            if (pool_[i].key == key)
            {
                out = pool_[i].value;
                return true;
            }
        }
        return false;
    }

    void insert(int key, int value)
    {
        int& head = buckets_[bucketOf(key)];

        for (int i = head; i != -1; i = pool_[i].next)
        {
            if (pool_[i].key == key)
            {
                pool_[i].value = value;   // 已存在，改值
                return;
            }
        }

        pool_.push_back({key, value, head});
        head = static_cast<int>(pool_.size()) - 1;
        ++size_;

        if (loadFactor() > 1.0)
        {
            rehash(buckets_.size() * 2 + 1);
        }
    }

    size_t bucketSize(size_t b) const
    {
        size_t n = 0;
        for (int i = buckets_[b]; i != -1; i = pool_[i].next) ++n;
        return n;
    }

    void print(const string& title) const
    {
        cout << "  " << title << "：size=" << size_
             << "，bucket_count=" << bucketCount()
             << "，load_factor=" << fixed << setprecision(2)
             << loadFactor() << defaultfloat << endl;
        for (size_t b = 0; b < buckets_.size(); ++b)
        {
            cout << "    [" << setw(2) << b << "]";
            for (int i = buckets_[b]; i != -1; i = pool_[i].next)
            {
                cout << " → " << pool_[i].key;
            }
            cout << endl;
        }
    }

private:
    struct Entry
    {
        int key;
        int value;
        int next;   // 同桶下一个节点的下标，-1 表示链尾
    };

    size_t bucketOf(int key) const
    {
        return static_cast<size_t>(key) % buckets_.size();
    }

    void rehash(size_t newCount)
    {
        vector<int> fresh(newCount, -1);

        for (size_t i = 0; i < pool_.size(); ++i)
        {
            size_t b = static_cast<size_t>(pool_[i].key) % newCount;
            pool_[i].next = fresh[b];
            fresh[b] = static_cast<int>(i);
        }

        buckets_ = move(fresh);
    }

    vector<Entry> pool_;      // 节点池：模拟堆上的节点
    vector<int> buckets_;     // 桶数组：存每个桶的头节点下标
    size_t size_ = 0;
};

void miniHashDemo()
{
    section("七、手写一个极简哈希表");

    MiniHashTable t(4);
    for (int i = 1; i <= 6; ++i)
    {
        t.insert(i, i * 10);
        cout << "  插入 key=" << i
             << "：bucket_count=" << t.bucketCount()
             << "，load_factor=" << fixed << setprecision(2)
             << t.loadFactor() << defaultfloat << endl;
    }

    cout << endl;
    cout << "  再插入 10 和 12，它们会和 1、3 撞到同一个桶：" << endl;
    t.insert(10, 100);
    t.insert(12, 120);
    cout << "    10 % 9 = 1，12 % 9 = 3" << endl;

    cout << endl;
    t.print("最终的桶布局");

    int value = 0;
    cout << endl;
    cout << "  find(3) → " << (t.find(3, value) ? to_string(value) : "未找到") << endl;
    cout << "  find(99) → " << (t.find(99, value) ? to_string(value) : "未找到") << endl;

    cout << endl;
    cout << "  这段代码和真实实现差两处：" << endl;
    cout << "    1. 桶里存头指针，删节点要先找前驱；真实实现存前驱指针。"
         << endl;
    cout << "    2. 节点池用 vector，扩容可能搬家；真实实现用节点分配器，"
         << "地址天然稳定。" << endl;
    cout << "  但“桶数组 + 拉链 + 装填因子 + rehash”的骨架，是一样的。" << endl;
}

// ============================================================
// 老师傅的话
// ============================================================

void masterWords()
{
    section("老师傅的话");

    cout << "  徒弟，哈希表这东西，面试最爱问，实战最容易踩坑。" << endl;
    cout << endl;
    cout << "  1. 先想清楚“谁决定顺序”。" << endl;
    cout << "     unordered_map 的顺序由桶和链决定，rehash 一变就重排。" << endl;
    cout << "     要稳定顺序，别靠它；要么排序，要么换 map。" << endl;
    cout << endl;
    cout << "  2. 失效规则要按容器记，别背一句话。" << endl;
    cout << "     vector 扩容：指针、引用、迭代器全废。" << endl;
    cout << "     unordered_map rehash：迭代器废，指针和引用还活。" << endl;
    cout << "     list：除了被删的那个，其他都不动。" << endl;
    cout << endl;
    cout << "  3. 自定义 key，hash 和 equal 必须一致。" << endl;
    cout << "     两个 key 相等，hash 必须相同；" << endl;
    cout << "     否则 equal 说相等，hash 却把它们分到两个桶，永远找不到。" << endl;
    cout << endl;
    cout << "  4. reserve 是最好用的性能开关。" << endl;
    cout << "     知道大概要装多少个元素，先 reserve，省掉一串 rehash。" << endl;
    cout << "     但别乱 reserve 一个天量数字，桶数组也是要占内存的。" << endl;
    cout << endl;
    cout << "  5. 最坏情况 O(n) 不是理论吓唬人。" << endl;
    cout << "     哈希函数被恶意构造，桶链全挤在一起，查询就退化成链表。" << endl;
    cout << "     对不可信输入，考虑加盐哈希或限制链长。" << endl;
    cout << "============================================================" << endl;
}

// ============================================================
// 小练习
// ============================================================

void exercises()
{
    section("小练习（回去自己做）");

    cout << "  1. 用 bucket_size(i) 打印 1000 个随机 key 的桶长分布，" << endl;
    cout << "     画出直方图，看看 std::hash<int> 到底均不均匀。" << endl;
    cout << "  2. 写一个自定义 Person 结构体，实现 hash 和 equal，" << endl;
    cout << "     塞进 unordered_set，验证“相等对象必须同哈希”。" << endl;
    cout << "  3. 把 MiniHashTable 的桶改成“存前驱指针”，" << endl;
    cout << "     实现 O(1) 的 erase（提示：单链表删除要看前驱）。" << endl;
    cout << "  4. 用 chrono 对比 reserve 和不 reserve 插入 100 万个元素，" << endl;
    cout << "     各跑三次，看看差多少。" << endl;
    cout << "  5. 构造一个坏哈希（只返回 key % 2），" << endl;
    cout << "     统计 1 万个 key 的最长桶链和平均查找比较次数。" << endl;
    cout << "  6. 思考：为什么 unordered_map 的迭代器是前向迭代器，" << endl;
    cout << "     而不是双向迭代器？和它“全局单链表”的布局有什么关系？" << endl;
    cout << "============================================================" << endl;
}

int main()
{
    layoutIntro();
    growthCurve();
    reserveDemo();
    rehashSurvival();
    hashQuality();
    customHashCalls();
    miniHashDemo();
    masterWords();
    exercises();
    return 0;
}
