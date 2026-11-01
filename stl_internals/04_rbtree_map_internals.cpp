// ============================================================
//  04_rbtree_map_internals.cpp - map/set：红黑树的平衡术
// ============================================================
//  徒弟，map 和 set 为什么能“有序 + O(log n)”？
//  因为 libstdc++ 用红黑树（_Rb_tree）在背后撑着。
//
//  这一章分两部分：
//      前半：手写一棵小红黑树，把插入修复的三种情况跑给你看
//      后半：用 std::map 验证 header、operator[]、迭代器稳定性、节点开销
//
//  别怕旋转。你把五条规矩记住，把三种情况走一遍，
//  红黑树就没有传说中那么玄。
// ============================================================

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <random>
#include <stdexcept>
#include <cstddef>

using namespace std;

// ============================================================
// 一、红黑树的五条规矩
// ============================================================
//  1. 每个节点不是红就是黑
//  2. 根节点是黑
//  3. 每个 NIL（空叶子）是黑
//  4. 红节点的两个孩子必须是黑（不能红红相连）
//  5. 从任一节点到它所有 NIL 的路径上，黑节点数量相同
//
//  由第 4、5 条可以推出：最长路径 ≤ 2 × 最短路径，
//  所以树高被限制在 O(log n)，查找/插入/删除都是 O(log n)。
//
//  libstdc++ 的节点：
//      struct _Rb_tree_node_base {
//          color _M_color;              // 红/黑
//          _Rb_tree_node_base* _M_parent;
//          _Rb_tree_node_base* _M_left;
//          _Rb_tree_node_base* _M_right;
//      };
//      template<class T>
//      struct _Rb_tree_node : _Rb_tree_node_base {
//          T _M_value;                  // pair<const Key, T> 或 Key
//      };
//
//  64 位下，map<int,int> 的节点约 40 字节：
//      颜色 4 + 对齐 4 + 三个指针 24 + pair 8 = 40。
//  而元素本身只有 8 字节，剩下都是树的“骨架”。
// ============================================================

enum class Color
{
    Red,
    Black
};

struct Node
{
    int key;
    Color color;
    Node* parent;
    Node* left;
    Node* right;

    Node(int k, Color c)
        : key(k), color(c), parent(nullptr), left(nullptr), right(nullptr) {}
};

// ============================================================
// 二、一棵可以验证的小红黑树
// ============================================================
//  NIL 用 nullptr 表示，colorOf(nullptr) 返回 Black。
//  插入分两步：
//      1. 按 BST 规则挂到叶子，新节点染红
//      2. 从新节点往上修复，直到父节点是黑或到达根
//
//  修复的三种情况（以父节点是祖父的左孩子为例）：
//      情况一：叔叔是红
//          父、叔染黑，祖父染红，问题上移到祖父
//      情况二：叔叔是黑，当前节点是“一字型”（父左、我也左）
//          父染黑，祖父染红，对祖父右旋
//      情况三：叔叔是黑，当前节点是“之字型”（父左、我右）
//          先对父左旋，变成情况二，再按情况二处理
//
//  父节点是祖父右孩子时，左右完全镜像。
// ============================================================

class RBTree
{
public:
    RBTree() = default;

    ~RBTree()
    {
        destroy(root_);
    }

    RBTree(const RBTree&) = delete;
    RBTree& operator=(const RBTree&) = delete;

    void insert(int key)
    {
        Node* z = new Node(key, Color::Red);
        Node* parent = nullptr;
        Node* cur = root_;

        while (cur)
        {
            parent = cur;
            if (key < cur->key)
            {
                cur = cur->left;
            }
            else if (key > cur->key)
            {
                cur = cur->right;
            }
            else
            {
                delete z; // 重复 key，set 语义：不插入
                return;
            }
        }

        z->parent = parent;
        if (!parent)
        {
            root_ = z;
        }
        else if (key < parent->key)
        {
            parent->left = z;
        }
        else
        {
            parent->right = z;
        }

        if (z->parent)
        {
            insertFix(z);
        }
        root_->color = Color::Black;
    }

    bool validate() const
    {
        if (!root_) return true;
        if (root_->color != Color::Black) return false;

        bool colorsOk = true;
        long blackHeight = 0;
        bool heightOk = blackHeightOk(root_, blackHeight, colorsOk);
        if (!colorsOk || !heightOk) return false;

        vector<int> keys;
        collect(root_, keys);
        for (size_t i = 1; i < keys.size(); ++i)
        {
            if (keys[i - 1] > keys[i]) return false;
        }
        return true;
    }

    void print(const string& title) const
    {
        cout << "  " << title << "（R=红，B=黑）：" << endl;
        printNode(root_, 4);
    }

    int height() const { return heightOf(root_); }
    long rotations() const { return rotations_; }
    size_t size() const { return count(root_); }

private:
    Node* root_ = nullptr;
    long rotations_ = 0;

    static Color colorOf(Node* n)
    {
        return n ? n->color : Color::Black;
    }

    static bool isRed(Node* n)
    {
        return n && n->color == Color::Red;
    }

    void rotateLeft(Node* x)
    {
        Node* y = x->right;
        x->right = y->left;
        if (y->left) y->left->parent = x;

        y->parent = x->parent;
        if (!x->parent)
        {
            root_ = y;
        }
        else if (x == x->parent->left)
        {
            x->parent->left = y;
        }
        else
        {
            x->parent->right = y;
        }

        y->left = x;
        x->parent = y;
        ++rotations_;
    }

    void rotateRight(Node* x)
    {
        Node* y = x->left;
        x->left = y->right;
        if (y->right) y->right->parent = x;

        y->parent = x->parent;
        if (!x->parent)
        {
            root_ = y;
        }
        else if (x == x->parent->right)
        {
            x->parent->right = y;
        }
        else
        {
            x->parent->left = y;
        }

        y->right = x;
        x->parent = y;
        ++rotations_;
    }

    void insertFix(Node* z)
    {
        while (z->parent && z->parent->color == Color::Red)
        {
            Node* p = z->parent;
            Node* g = p->parent;

            if (p == g->left)
            {
                Node* uncle = g->right;

                if (colorOf(uncle) == Color::Red)
                {
                    // 情况一：叔叔红，只变色，问题上移
                    p->color = Color::Black;
                    uncle->color = Color::Black;
                    g->color = Color::Red;
                    z = g;
                }
                else
                {
                    if (z == p->right)
                    {
                        // 情况三：之字型，先左旋父，变成一字型
                        z = p;
                        rotateLeft(z);
                    }
                    // 情况二：一字型，父黑祖父红，右旋祖父
                    z->parent->color = Color::Black;
                    z->parent->parent->color = Color::Red;
                    rotateRight(z->parent->parent);
                }
            }
            else
            {
                Node* uncle = g->left;

                if (colorOf(uncle) == Color::Red)
                {
                    p->color = Color::Black;
                    uncle->color = Color::Black;
                    g->color = Color::Red;
                    z = g;
                }
                else
                {
                    if (z == p->left)
                    {
                        z = p;
                        rotateRight(z);
                    }
                    z->parent->color = Color::Black;
                    z->parent->parent->color = Color::Red;
                    rotateLeft(z->parent->parent);
                }
            }
        }

        root_->color = Color::Black;
    }

    bool blackHeightOk(Node* n, long& height, bool& colorsOk) const
    {
        if (!n)
        {
            height = 1; // NIL 算一个黑
            return true;
        }

        if (n->color == Color::Red && (isRed(n->left) || isRed(n->right)))
        {
            colorsOk = false;
        }

        long leftHeight = 0;
        long rightHeight = 0;
        bool leftOk = blackHeightOk(n->left, leftHeight, colorsOk);
        bool rightOk = blackHeightOk(n->right, rightHeight, colorsOk);

        if (!leftOk || !rightOk || leftHeight != rightHeight)
        {
            height = leftHeight;
            return false;
        }

        height = leftHeight + (n->color == Color::Black ? 1 : 0);
        return true;
    }

    void collect(Node* n, vector<int>& keys) const
    {
        if (!n) return;
        collect(n->left, keys);
        keys.push_back(n->key);
        collect(n->right, keys);
    }

    void printNode(Node* n, int indent) const
    {
        if (!n) return;
        printNode(n->right, indent + 4);
        cout << string(static_cast<size_t>(indent), ' ')
             << setw(3) << n->key << (n->color == Color::Red ? "R" : "B") << endl;
        printNode(n->left, indent + 4);
    }

    int heightOf(Node* n) const
    {
        if (!n) return 0;
        return 1 + max(heightOf(n->left), heightOf(n->right));
    }

    size_t count(Node* n) const
    {
        if (!n) return 0;
        return 1 + count(n->left) + count(n->right);
    }

    void destroy(Node* n)
    {
        if (!n) return;
        destroy(n->left);
        destroy(n->right);
        delete n;
    }
};

// ============================================================
// 一、二、三：先打印规矩和思路，再把三种情况跑一遍
// ============================================================

void fiveRules()
{
    cout << "============================================================" << endl;
    cout << "  一、红黑树的五条规矩" << endl;
    cout << "============================================================" << endl;
    cout << "  1. 每个节点不是红就是黑" << endl;
    cout << "  2. 根节点是黑" << endl;
    cout << "  3. 每个 NIL（空叶子）是黑" << endl;
    cout << "  4. 红节点的两个孩子必须是黑（不能红红相连）" << endl;
    cout << "  5. 从任一节点到它所有 NIL 的路径，黑节点数量相同" << endl;
    cout << "  由第 4、5 条推出：最长路径 ≤ 2 × 最短路径，树高 O(log n)。" << endl;
}

void rbTreeIntro()
{
    cout << "\n============================================================" << endl;
    cout << "  二、一棵可以验证的小红黑树" << endl;
    cout << "============================================================" << endl;
    cout << "  libstdc++ 的节点：颜色 + 三个指针（parent/left/right）+ 数据。" << endl;
    cout << "  插入分两步：按 BST 挂到叶子并染红，再从下往上修复。" << endl;
    cout << "  修复只有三种情况：叔叔红 → 变色上移；叔叔黑一字 → 单旋；" << endl;
    cout << "  叔叔黑之字 → 双旋。下面逐个跑给你看。" << endl;
}

void rbtreeCases()
{
    cout << "\n============================================================" << endl;
    cout << "  三、插入修复的三种情况" << endl;
    cout << "============================================================" << endl;

    {
        cout << "\n  【情况二 · 一字型】依次插入 10、20、30" << endl;
        RBTree t;
        for (int k : {10, 20, 30}) t.insert(k);
        t.print("插入 30 后");
        cout << "    旋转次数=" << t.rotations()
             << "，树高=" << t.height()
             << "，校验=" << (t.validate() ? "通过" : "失败") << endl;
        cout << "    10-20-30 形成右-右“一字型”，对 10 左旋一次，20 变黑当根。" << endl;
    }

    {
        cout << "\n  【情况三 · 之字型】依次插入 10、30、20" << endl;
        RBTree t;
        for (int k : {10, 30, 20}) t.insert(k);
        t.print("插入 20 后");
        cout << "    旋转次数=" << t.rotations()
             << "，树高=" << t.height()
             << "，校验=" << (t.validate() ? "通过" : "失败") << endl;
        cout << "    10-30-20 是之字型：先对 30 右旋变成一字型，再对 10 左旋。" << endl;
        cout << "    两次旋转后 20 变黑当根，10、30 染红。" << endl;
    }

    {
        cout << "\n  【情况一 · 叔叔红】依次插入 20、10、30、25" << endl;
        RBTree t;
        for (int k : {20, 10, 30, 25}) t.insert(k);
        t.print("插入 25 后");
        cout << "    旋转次数=" << t.rotations()
             << "，树高=" << t.height()
             << "，校验=" << (t.validate() ? "通过" : "失败") << endl;
        cout << "    25 的叔叔是 10（红），只变色：10/30 变黑、20 变红，" << endl;
        cout << "    问题移到 20；20 是根，最后再染黑。一次旋转都没有。" << endl;
    }

    {
        cout << "\n  【情况二 · 左-左】在上一棵树基础上再插入 22" << endl;
        RBTree t;
        for (int k : {20, 10, 30, 25, 22}) t.insert(k);
        t.print("插入 22 后");
        cout << "    旋转次数=" << t.rotations()
             << "，树高=" << t.height()
             << "，校验=" << (t.validate() ? "通过" : "失败") << endl;
        cout << "    22 的叔叔是 NIL（黑），22-25-30 是左-左一字型；" << endl;
        cout << "    25 染黑、30 染红，对 30 右旋，25 升为子树根。" << endl;
    }

    {
        cout << "\n  连续插入 1..100 的乱序，每次插入后都校验五条规矩" << endl;
        RBTree t;
        vector<int> keys(100);
        for (int i = 0; i < 100; ++i) keys[i] = i + 1;
        mt19937 rng(2026);
        shuffle(keys.begin(), keys.end(), rng);

        bool allOk = true;
        for (int k : keys)
        {
            t.insert(k);
            if (!t.validate())
            {
                allOk = false;
                cout << "    插入 " << k << " 后校验失败！" << endl;
                break;
            }
        }
        t.print("100 个 key 的最终树");
        cout << "    节点数=" << t.size() << "，树高=" << t.height()
             << "，旋转次数=" << t.rotations()
             << "，全部校验=" << (allOk ? "通过" : "失败") << endl;
        cout << "    100 个节点的理论树高约 log2(100)≈7，红黑树最多 2 倍：" << endl;
        cout << "    实际树高 " << t.height() << "，仍然是对数级。" << endl;
    }
}

// ============================================================
// 四、std::map 的 header：end() 到底指向哪
// ============================================================
//  libstdc++ 的 _Rb_tree 有一个 header 节点（不是根）：
//      header.parent = 根
//      header.left   = 最左节点（begin()）
//      header.right  = 最右节点
//      header.color  = 红（用来区分 header 和普通节点）
//
//  end() 返回的就是 &header，不是 nullptr。
//  所以 --end() 能直接到最右节点，++最右节点也能回到 end()，
//  不需要每次都从根开始找。
//
//  begin() 是 header.left，O(1)；中序遍历的迭代器 ++ 是“找后继”：
//      有右子树 → 右子树最左
//      没有右子树 → 往上走，直到“我是父亲的左孩子”
//
//  这些细节你平时不用手写，但看源码时要知道 header 不是根。
// ============================================================

// ============================================================
// 五、std::map 实战：operator[]、at、try_emplace、稳定性
// ============================================================

struct Loud
{
    Loud() { cout << "    Loud() 默认构造" << endl; }
    Loud(int v) { cout << "    Loud(int " << v << ") 构造" << endl; }
    Loud(const Loud&) { cout << "    Loud 拷贝构造" << endl; }
    Loud(Loud&&) noexcept { cout << "    Loud 移动构造" << endl; }
};

void headerLayout()
{
    cout << "\n============================================================" << endl;
    cout << "  四、std::map 的 header：end() 到底指向哪" << endl;
    cout << "============================================================" << endl;
    cout << "  libstdc++ 的 _Rb_tree 有一个 header 节点（不是根）：" << endl;
    cout << "    header.parent = 根，header.left = 最左（begin()），" << endl;
    cout << "    header.right  = 最右，header.color = 红（用来区分 header）。" << endl;
    cout << "  end() 返回 &header，不是 nullptr；--end() 直接到最右节点。" << endl;
    cout << "  begin() 是 header.left，O(1)；++ 是“找后继”，O(log n)。" << endl;
}

void mapSemantics()
{
    cout << "\n============================================================" << endl;
    cout << "  五、std::map 的语义细节" << endl;
    cout << "============================================================" << endl;

    {
        cout << "  operator[] vs at：" << endl;
        map<string, int> m;
        m["apple"] = 1;
        cout << "    m[\"apple\"]=1 后 size=" << m.size() << endl;

        try
        {
            m.at("banana");
        }
        catch (const out_of_range&)
        {
            cout << "    m.at(\"banana\") 抛 out_of_range，size 仍=" << m.size()
                 << "（at 只查找，不插入）" << endl;
        }

        m["banana"];
        cout << "    改成 m[\"banana\"] 之后再查，size=" << m.size()
             << "（operator[] 找不到就插入默认值）" << endl;
    }

    {
        cout << "\n  try_emplace vs operator[]：" << endl;
        map<int, Loud> m;
        cout << "    try_emplace(1, 42)：" << endl;
        m.try_emplace(1, 42);
        cout << "    try_emplace(1, 43)（key 已存在）：" << endl;
        m.try_emplace(1, 43);
        cout << "    m[2]（等价于插入默认构造）：" << endl;
        m[2];
        cout << "    size=" << m.size() << endl;
        cout << "    try_emplace 在 key 已存在时连 value 都不构造，零浪费；" << endl;
        cout << "    operator[] 找不到就默认构造一个再插入。" << endl;
    }

    {
        cout << "\n  insert 的返回值与引用稳定性：" << endl;
        map<int, int> m;
        m[1] = 10;
        int* p = &m[1];

        for (int i = 2; i <= 100; ++i) m[i] = i;
        auto result = m.insert({1, 999});

        cout << "    插入 99 个新元素后，&m[1] 仍指向原值：" << *p << endl;
        cout << "    再 insert({1,999})：inserted=" << result.second
             << "，值仍为 " << result.first->second << endl;
        cout << "    map 插入/删除其他节点不会移动已有节点，指针和引用稳定。" << endl;
    }

    {
        cout << "\n  有序性与范围查询：" << endl;
        map<int, int> m;
        for (int i = 0; i < 20; ++i) m[i] = i * i;

        cout << "    中序遍历（前 10 个）：";
        int n = 0;
        for (const auto& kv : m)
        {
            cout << kv.first << " ";
            if (++n == 10) break;
        }
        cout << endl;

        auto lo = m.lower_bound(5);   // 第一个 >= 5
        auto hi = m.upper_bound(15);  // 第一个 > 15
        cout << "    lower_bound(5)=" << lo->first
             << "，upper_bound(15)=" << hi->first << endl;
        cout << "    [5,15) 区间元素个数=" << distance(lo, hi) << endl;
        cout << "    这就是 map 能做范围查询、unordered_map 做不到的原因。" << endl;
    }
}

void nodeOverhead()
{
    cout << "\n============================================================" << endl;
    cout << "  六、节点开销与对象大小" << endl;
    cout << "============================================================" << endl;

    struct FakeRbNode
    {
        Color color;
        void* parent;
        void* left;
        void* right;
        pair<const int, int> value;
    };

    cout << "  模拟 map<int,int> 节点布局：" << endl;
    cout << "    sizeof(FakeRbNode) = " << sizeof(FakeRbNode) << " 字节" << endl;
    cout << "    其中 pair 只占 " << sizeof(pair<const int, int>) << " 字节，" << endl;
    cout << "    颜色 + 对齐 + 三个指针占 " << sizeof(FakeRbNode) - sizeof(pair<const int, int>) << " 字节" << endl;
    cout << "  sizeof(std::map<int,int>)    = " << sizeof(map<int, int>) << " 字节" << endl;
    cout << "  sizeof(std::set<int>)        = " << sizeof(set<int>) << " 字节" << endl;
    cout << "  注意：map 对象本身只有 header + size + 比较器，" << endl;
    cout << "  真正的节点都在堆上，sizeof(map) 不能代表总内存。" << endl;
}

// ============================================================
// 老师傅的话
// ============================================================

void masterWords()
{
    cout << "\n============================================================" << endl;
    cout << "  老师傅的话" << endl;
    cout << "============================================================" << endl;
    cout << "  徒弟，红黑树是 STL 里最硬的一块骨头，但别被它吓住。" << endl;
    cout << endl;
    cout << "  1. 先记规矩，再记旋转。" << endl;
    cout << "     五条规矩是“为什么”，旋转是“怎么修”。" << endl;
    cout << "     你把规矩忘了，旋转就成了背代码，换个场景就不会了。" << endl;
    cout << endl;
    cout << "  2. 插入修复只有三种情况。" << endl;
    cout << "     叔叔红 → 变色上移；叔叔黑一字 → 单旋；叔叔黑之字 → 双旋。" << endl;
    cout << "     镜像情况不用另背，左右反过来就行。" << endl;
    cout << endl;
    cout << "  3. 红黑树不是“更平衡”，是“够平衡且旋转少”。" << endl;
    cout << "     AVL 更严格，查找略快，但插入删除旋转多；" << endl;
    cout << "     红黑树放宽到 2 倍高度，换来插入最多 2 次、删除最多 3 次旋转。" << endl;
    cout << "     标准库选它，是综合权衡，不是它单科第一。" << endl;
    cout << endl;
    cout << "  4. map 的 operator[] 是“插入器”，不是“查找器”。" << endl;
    cout << "     只想查用 at/find；确定要插入用 try_emplace，避免先默认构造再覆盖。" << endl;
    cout << endl;
    cout << "  5. 节点容器的内存，别只看 sizeof。" << endl;
    cout << "     一个 map<int,int> 节点 40 字节，元素才 8 字节；" << endl;
    cout << "     数据量大时，这份“骨架开销”可能比数据本身还大。" << endl;
    cout << "     真要极致省内存，考虑连续存储 + 自己排序。" << endl;
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
    cout << "  1. 给 RBTree 加 erase，先实现“删除后校验五条规矩”，" << endl;
    cout << "     再补删除修复的四种情况（提示：找后继、看兄弟颜色）。" << endl;
    cout << "  2. 在 rbtreeCases 里打印每次插入后的树，观察旋转发生在哪一步。" << endl;
    cout << "  3. 把 RBTree 改成模板，支持 string key 和自定义比较器。" << endl;
    cout << "  4. 用 chrono 对比 map 和 unordered_map 插入 100 万个随机 key 的耗时，" << endl;
    cout << "     各跑三次取中位数。" << endl;
    cout << "  5. 用 map 实现一个“按分数范围查学生”的小程序，" << endl;
    cout << "     练习 lower_bound/upper_bound。" << endl;
    cout << "  6. 思考：为什么 map 的迭代器是双向的，而 unordered_map 只有前向？" << endl;
    cout << "============================================================" << endl;
}

int main()
{
    fiveRules();
    rbTreeIntro();
    rbtreeCases();
    headerLayout();
    mapSemantics();
    nodeOverhead();
    masterWords();
    exercises();
    return 0;
}
