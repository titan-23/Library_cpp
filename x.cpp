// #pragma GCC target("avx2")
// #pragma GCC optimize("O3")
// #pragma GCC optimize("unroll-loops")

#include <bits/stdc++.h>
using namespace std;

using ll = long long;
#define rep(i, n) for (ll i = 0; i < (ll)(n); ++i)

const ll dy[] = {-1, 0, 0, 1};
const ll dx[] = {0, -1, 1, 0};

template <class T, class T1, class T2> bool isrange(T target, T1 low, T2 high) { return low <= target && target < high; }
template <class T, class U> T min(const T &t, const U &u) { return t < u ? t : u; }
template <class T, class U> T max(const T &t, const U &u) { return t < u ? u : t; }
template <class T, class U> bool chmin(T &t, const U &u) { if (t > u) { t = u; return true; } return false; }
template <class T, class U> bool chmax(T &t, const U &u) { if (t < u) { t = u; return true; } return false; }

// #include "titan_cpplib/others/print.cpp"
// #include "titan_cpplib/ds/persistent_wbtree.cpp"
/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/persistent_wbtree.cpp
#include <new>
#include <type_traits>
#include <utility>

#include <iostream>
#include <vector>
#include <cassert>
#include <memory>
#include <algorithm>
#include <cstdint>
// #include "titan_cpplib/others/print.cpp"
/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/others/print.cpp

#include <bits/stdc++.h>
using namespace std;

// print

// -------------------------
// pair<K, V>
template<typename K, typename V> ostream& operator<<(ostream& os, const pair<K, V>& p);
// tuple<T1, T2, T3>
template<typename T1, typename T2, typename T3> ostream &operator<<(ostream &os, const tuple<T1, T2, T3> &t);
// tuple<T1, T2, T3, T4>
template<typename T1, typename T2, typename T3, typename T4> ostream &operator<<(ostream &os, const tuple<T1, T2, T3, T4> &t);
// vector<T>
template<typename T> ostream& operator<<(ostream& os, const vector<T>& a);
// vector<vector<T>>
template<typename T> ostream& operator<<(ostream& os, const vector<vector<T>>& a);
// array<T, N>
template <typename T, size_t N> ostream& operator<<(ostream &os, const array<T, N> &a);
// stack<T>
template<typename T> ostream& operator<<(ostream& os, const stack<T>& s);
// queue<T>
template<typename T> ostream& operator<<(ostream& os, const queue<T>& q);
// deque<T>
template<typename T> ostream& operator<<(ostream& os, const deque<T>& dq);
// priority_queue<T, Container, Compare>
template<typename T, typename Container, typename Compare> ostream& operator<<(ostream& os, const priority_queue<T, Container, Compare>& pq);
// set<T>
template<typename T> ostream& operator<<(ostream& os, const set<T>& s);
// multiset<T>
template<typename T> ostream& operator<<(ostream& os, const multiset<T>& s);
// unordered_set<T>
template<typename T> ostream& operator<<(ostream& os, const unordered_set<T>& a);
// map<K, V>
template<typename K, typename V> ostream& operator<<(ostream& os, const map<K, V>& mp);
// unordered_map<K, V>
template<typename K, typename V> ostream& operator<<(ostream& os, const unordered_map<K, V>& mp);
// __int128_t
ostream& operator<<(ostream& os, __int128_t x);
// __uint128_t
ostream& operator<<(ostream& os, __uint128_t x);

// -------------------------

// color
static const string PRINT_RED    = "\033[91m"; // 赤字
static const string PRINT_GREEN  = "\033[92m"; // 緑字
static const string PRINT_YELLOW = "\033[93m"; // 黄字
static const string PRINT_BLUE   = "\033[94m"; // 青字
static const string PRINT_DIM    = "\033[2m";  // 薄字
static const string PRINT_NONE   = "\033[m";   // 色を元に戻す
static const string PRINT_BOLD = "\u001b[1m"; // 太字

string to_red   (const string &s) { return PRINT_RED    + s + PRINT_NONE; }
string to_green (const string &s) { return PRINT_GREEN  + s + PRINT_NONE; }
string to_yellow(const string &s) { return PRINT_YELLOW + s + PRINT_NONE; }
string to_blue  (const string &s) { return PRINT_BLUE   + s + PRINT_NONE; }
string to_dim   (const string &s) { return PRINT_DIM    + s + PRINT_NONE; }
string to_bold  (const string &s) { return PRINT_BOLD   + s + PRINT_NONE; }

string spacefill(const string s, const int f) {
    int n = s.size();
    string t;
    for (int i = 0; i < f-n; ++i) t += " ";
    t += s;
    return t;
}

template<class T> string spacefill(const T &x, const int f) {
    return spacefill(to_string(x), f);
}

string zfill(const string s, const int f) {
    int n = s.size();
    string t;
    for (int i = 0; i < f-n; ++i) t += "0";
    t += s;
    return t;
}

string bin(unsigned long long s) {
    if (s == 0) return "0";
    string t;
    while (s) {
        t += (s & 1) ? '1' : '0';
        s >>= 1;
    }
    reverse(t.begin(), t.end());
    return t;
}

#define DEBUG_LINE() (cerr << "[LINE] : " << __LINE__ << endl)

string to_string_int128(__int128_t x) {
    if (x == 0) return "0";
    bool neg = false;
    if (x < 0) { neg = true; x = -x; }
    string s;
    while (x > 0) {
        s += char('0' + (int)(x % 10));
        x /= 10;
    }
    if (neg) s += '-';
    reverse(s.begin(), s.end());
    return s;
}

string to_string_uint128(__uint128_t x) {
    if (x == 0) return "0";
    string s;
    while (x > 0) {
        s += char('0' + (int)(x % 10));
        x /= 10;
    }
    reverse(s.begin(), s.end());
    return s;
}

// __int128_t
ostream& operator<<(ostream& os, __int128_t x) {
    os << to_string_int128(x);
    return os;
}

// __uint128_t
ostream& operator<<(ostream& os, __uint128_t x) {
    os << to_string_uint128(x);
    return os;
}

// pair<K, V>
template<typename K, typename V>
ostream& operator<<(ostream& os, const pair<K, V>& p) {
    os << "(" << p.first << ", " << p.second << ")";
    return os;
}

// tuple<T1, T2, T3>
template<typename T1, typename T2, typename T3>
ostream &operator<<(ostream &os, const tuple<T1, T2, T3> &t) {
    os << "(" << get<0>(t) << ", " << get<1>(t) << ", " << get<2>(t) << ")";
    return os;
}

// tuple<T1, T2, T3, T4>
template<typename T1, typename T2, typename T3, typename T4>
ostream &operator<<(ostream &os, const tuple<T1, T2, T3, T4> &t) {
    os << "(" << get<0>(t) << ", " << get<1>(t) << ", " << get<2>(t) << ", " << get<3>(t) << ")";
    return os;
}

// array
template <typename T, size_t N>
ostream& operator<<(ostream &os, const array<T, N> &a) {
    os << "[";
    for (int i = 0; i < (int)N; ++i) {
        if (i > 0) os << ", ";
        os << a[i];
    }
    os << "]";
    return os;
}

// vector<T>
template<typename T>
ostream& operator<<(ostream& os, const vector<T>& a) {
    os << "[";
    for (int i = 0; i < (int)a.size(); ++i) {
        if (i > 0) os << ", ";
        os << a[i];
    }
    os << "]";
    return os;
}

// vector<vector<T>>
template<typename T>
ostream& operator<<(ostream& os, const vector<vector<T>>& a) {
    os << "[\n";
    int h = (int)a.size();
    for (int i = 0; i < h; ++i) {
        os << "  " << a[i] << '\n';
    }
    os << "]";
    return os;
}

// stack<T>
template<typename T> ostream& operator<<(ostream& os, const stack<T>& s) {
    stack<T> tmp = s;
    vector<T> v;
    while (!tmp.empty()) {
        v.push_back(tmp.top());
        tmp.pop();
    }
    os << "[";
    for (int i = (int)v.size()-1; i >= 0; --i) {
        if (i != (int)v.size()-1) os << ", ";
        os << v[i];
    }
    os << "]";
    return os;
}

// queue<T>
template<typename T> ostream& operator<<(ostream& os, const queue<T>& q) {
    queue<T> tmp = q;
    os << "[";
    bool first = true;
    while (!tmp.empty()) {
        if (!first) os << ", ";
        first = false;
        os << tmp.front();
        tmp.pop();
    }
    os << "]";
    return os;
}

// deque<T>
template<typename T>
ostream& operator<<(ostream& os, const deque<T>& dq) {
    vector<T> a(dq.begin(), dq.end());
    os << a;
    return os;
}

// priority_queue<T, Container, Compare>
template<typename T, typename Container, typename Compare> ostream& operator<<(ostream& os, const priority_queue<T, Container, Compare>& pq) {
    priority_queue<T, Container, Compare> tmp = pq;
    os << "[";
    bool first = true;
    while (!tmp.empty()) {
        if (!first) os << ", ";
        first = false;
        os << tmp.top();
        tmp.pop();
    }
    os << "]";
    return os;
}

// set<T>
template<typename T>
ostream& operator<<(ostream& os, const set<T>& s) {
    os << "{";
    auto it = s.begin();
    while (it != s.end()) {
        os << *it;
        ++it;
        if (it != s.end()) {
            os << ", ";
        }
    }
    os << "}";
    return os;
}

// multiset<T>
template<typename T>
ostream& operator<<(ostream& os, const multiset<T>& s) {
    os << "{";
    auto it = s.begin();
    while (it != s.end()) {
        os << *it;
        ++it;
        if (it != s.end()) {
            os << ", ";
        }
    }
    os << "}";
    return os;
}

// unordered_set<T>
template<typename T>
ostream& operator<<(ostream& os, const unordered_set<T>& a) {
    set<T> s(a.begin(), a.end());
    os << s;
    return os;
}

// map<K, V>
template<typename K, typename V>
ostream& operator<<(ostream& os, const map<K, V>& mp) {
    os << "{";
    auto it = mp.begin();
    while (it != mp.end()) {
        os << it->first << ": " << it->second;
        ++it;
        if (it != mp.end()) {
            os << ", ";
        }
    }
    os << "}";
    return os;
}

// unordered_map<K, V>
template<typename K, typename V>
ostream& operator<<(ostream& os, const unordered_map<K, V>& mp) {
    map<K, V> m(mp.begin(), mp.end());
    os << m;
    return os;
}

using namespace std;

namespace titan23 {

template <class T, typename SizeType = int>
class PersistentWBTree {
public:
    struct MemoryAllocator {
        struct Node {
            SizeType left, right, size, left_size;

            /// @brief メタデータをゼロ初期化する / O(1)
            Node() : left(0), right(0), size(0), left_size(0) {}

            /// @brief 子と部分木サイズを設定する / O(1)
            Node(SizeType l, SizeType r, SizeType s) : left(l), right(r), size(s), left_size(0) {}
        };
        template <class U, unsigned BlockBits>
        class BlockVector {
            static constexpr size_t block_size = size_t(1) << BlockBits;
            static constexpr size_t block_mask = block_size - 1;
            static constexpr size_t block_alignment = alignof(U) > 64 ? alignof(U) : 64;
            static_assert(block_size <= size_t(-1) / sizeof(U), "block too large");
            using Alloc = std::allocator<U>;
            using Traits = std::allocator_traits<Alloc>;
            Alloc allocator_;
            std::vector<U*> blocks_;
            size_t size_ = 0;

            void add_block() {
                U *block = static_cast<U*>(::operator new(sizeof(U) * block_size, std::align_val_t(block_alignment)));
                try {
                    blocks_.push_back(block);
                } catch (...) {
                    ::operator delete(block, std::align_val_t(block_alignment));
                    throw;
                }
            }

            void clear_storage() noexcept {
                if constexpr (!std::is_trivially_destructible<U>::value) {
                    for (size_t i = 0; i < size_; ++i) {
                        Traits::destroy(allocator_, &(*this)[i]);
                    }
                }
                for (U *block : blocks_) {
                    ::operator delete(block, std::align_val_t(block_alignment));
                }
                blocks_.clear();
                size_ = 0;
            }

        public:
            /// @brief 空の領域を初期化する / O(1)
            BlockVector() = default;

            /// @brief コピー構築を禁止する
            BlockVector(const BlockVector&) = delete;

            /// @brief コピー代入を禁止する
            BlockVector& operator=(const BlockVector&) = delete;

            /// @brief 領域の所有権を移動する / O(1)
            BlockVector(BlockVector&& other) noexcept
                : blocks_(std::move(other.blocks_)), size_(std::exchange(other.size_, 0)) {
                other.blocks_.clear();
            }

            /// @brief 現在の N 要素と M ブロックを破棄して移動代入する / O(N + M)
            BlockVector& operator=(BlockVector&& other) noexcept {
                if (this != &other) {
                    clear_storage();
                    blocks_ = std::move(other.blocks_);
                    size_ = std::exchange(other.size_, 0);
                    other.blocks_.clear();
                }
                return *this;
            }

            /// @brief 構築済みの N 要素と M ブロックを破棄する / O(N + M)
            ~BlockVector() { clear_storage(); }

            /// @brief i 番目の要素への参照を返す / O(1)
            U& operator[](size_t i) noexcept {
                return blocks_[i >> BlockBits][i & block_mask];
            }

            /// @brief i 番目の要素への定数参照を返す / O(1)
            const U& operator[](size_t i) const noexcept {
                return blocks_[i >> BlockBits][i & block_mask];
            }

            /// @brief 構築済み要素数を返す / O(1)
            size_t size() const noexcept { return size_; }

            /// @brief 確保済み容量を返す / O(1)
            size_t capacity() const noexcept { return blocks_.size() * block_size; }

            /// @brief N 要素分の容量を確保する / O(N)
            void reserve(size_t n) {
                const size_t required = n / block_size + (n % block_size != 0);
                blocks_.reserve(required);
                while (blocks_.size() < required) add_block();
            }

            /// @brief 末尾に要素を構築する / 償却 O(1)
            template <class... Args>
            U& emplace_back(Args&&... args) {
                if (size_ == capacity()) add_block();
                U *value = blocks_[size_ >> BlockBits] + (size_ & block_mask);
                Traits::construct(allocator_, value, std::forward<Args>(args)...);
                ++size_;
                return *value;
            }

            /// @brief 末尾に要素をコピーする / 償却 O(1)
            void push_back(const U& value) { emplace_back(value); }

            /// @brief 末尾の要素を破棄する / O(1)
            void pop_back() noexcept {
                assert(size_ != 0);
                --size_;
                Traits::destroy(allocator_, &(*this)[size_]);
            }
        };

        struct Slot {
            Node tree;
            T key;

            /// @brief ノードを初期化する / O(1)
            Slot(SizeType l, SizeType r, SizeType s, const T& value)
                : tree(l, r, s), key(value) {}
        };
        using Pool = BlockVector<Slot, 16>;
        Pool pool;

        template<class U, U Slot::* Member>
        struct View {
            Pool *pool;

            /// @brief i 番目の要素への参照を返す / O(1)
            U& operator[](size_t i) noexcept { return (*pool)[i].*Member; }

            /// @brief i 番目の要素への定数参照を返す / O(1)
            const U& operator[](size_t i) const noexcept { return (*pool)[i].*Member; }

            /// @brief 構築済み要素数を返す / O(1)
            size_t size() const noexcept { return pool->size(); }

            /// @brief 確保済み容量を返す / O(1)
            size_t capacity() const noexcept { return pool->capacity(); }

            /// @brief N 要素分の容量を確保する / O(N)
            void reserve(size_t n) { pool->reserve(n); }
        };
        View<Node, &Slot::tree> d;
        View<T, &Slot::key> keys;
        size_t ptr, cap;

        /// @brief 番兵を持つ領域を初期化する / O(1)
        MemoryAllocator() : d{&pool}, keys{&pool}, ptr(1), cap(0) {
            pool.emplace_back(0, 0, 0, T{});
            cap = pool.capacity();
        }

        /// @brief 領域の所有権を移動する / O(1)
        MemoryAllocator(MemoryAllocator&& other) noexcept
            : pool(std::move(other.pool)), d{&pool}, keys{&pool},
              ptr(other.ptr), cap(other.cap) {}

        /// @brief 現在の N 要素と M ブロックを破棄して移動代入する / O(N + M)
        MemoryAllocator& operator=(MemoryAllocator&& other) noexcept {
            if (this != &other) {
                pool = std::move(other.pool);
                ptr = other.ptr;
                cap = other.cap;
            }
            return *this;
        }

        /// @brief ノードを複製して番号を返す / 償却 O(1)
        SizeType copy(SizeType node) {
            if (!node) return 0;
            if (pool.size() > ptr) {
                pool[ptr] = pool[node];
            } else {
                try {
                    pool.push_back(pool[node]);
                } catch (...) {
                    cap = pool.capacity();
                    throw;
                }
                cap = pool.capacity();
            }
            return ptr++;
        }

        /// @brief 値 key を持つノードを作成する / 償却 O(1)
        SizeType new_node(const T& key) {
            if (pool.size() > ptr) {
                pool[ptr].tree = {0, 0, 1};
                pool[ptr].key = key;
            } else {
                try {
                    pool.emplace_back(0, 0, 1, key);
                } catch (...) {
                    cap = pool.capacity();
                    throw;
                }
                cap = pool.capacity();
            }
            return ptr++;
        }

        /// @brief N ノード分の容量を確保する / O(N)
        /// @note 容量はブロック単位で切り上げる
        void reserve(SizeType n) {
            try { pool.reserve(n); }
            catch (...) { cap = pool.capacity(); throw; }
            cap = pool.capacity();
        }

        /// @brief 確保済み領域を再利用できる状態に戻す / O(1)
        /// @note この領域を使う既存の木は無効になる
        void reset() { ptr = 1; }

        /// @brief 確保済み領域がほぼ埋まっているか判定する / O(1)
        bool almost_full() const { return (double)ptr > max(1.0, cap * 0.99); }

        /// @brief 確保済みノード領域のバイト数を返す / O(1)
        size_t capacity_bytes() const { return pool.capacity() * sizeof(Slot); }
    };

    static MemoryAllocator ma;

private:
    using PLTM = PersistentWBTree<T, SizeType>;
    static constexpr int DELTA = 3;
    static constexpr int GAMMA = 2;
    SizeType root;

    inline size_t weight_right(SizeType node) const {
        return size_t(ma.pool[node].tree.size) - ma.pool[node].tree.left_size + (node == 0);
    }

    inline size_t weight_left(SizeType node) const {
        return size_t(ma.pool[node].tree.left_size) + 1;
    }

    inline size_t weight(SizeType node) const {
        return size_t(ma.pool[node].tree.size) + 1;
    }

    void update(SizeType node) {
        ma.pool[node].tree.left_size = ma.pool[ma.pool[node].tree.left].tree.size;
        ma.pool[node].tree.size = 1 + ma.pool[node].tree.left_size + ma.pool[ma.pool[node].tree.right].tree.size;
    }

    SizeType own(SizeType node, size_t first) {
        return node && static_cast<size_t>(node) < first ? ma.copy(node) : node;
    }

    void _build(vector<T> const &a) {
        auto build = [&] (auto &&build, SizeType l, SizeType r) -> SizeType {
            SizeType mid = l + (r - l) / 2;
            SizeType node = ma.new_node(a[mid]);
            if (l != mid) ma.pool[node].tree.left = build(build, l, mid);
            if (mid+1 != r) ma.pool[node].tree.right = build(build, mid+1, r);
            update(node);
            return node;
        };
        if (a.empty()) {
            root = 0;
            return;
        }
        root = build(build, 0, (SizeType)a.size());
    }

    SizeType _rotate_right(SizeType node) {
        SizeType u = ma.pool[node].tree.left;
        ma.pool[node].tree.left = ma.pool[u].tree.right;
        ma.pool[u].tree.right = node;
        update(node);
        update(u);
        return u;
    }

    SizeType _rotate_left(SizeType node) {
        SizeType u = ma.pool[node].tree.right;
        ma.pool[node].tree.right = ma.pool[u].tree.left;
        ma.pool[u].tree.left = node;
        update(node);
        update(u);
        return u;
    }

    // node is owned; its children may still belong to old versions.
    SizeType balance(SizeType node, size_t first) {
        if (weight_left(node) * DELTA < weight_right(node)) {
            SizeType u = own(ma.pool[node].tree.right, first);
            ma.pool[node].tree.right = u;
            if (weight_left(u) >= weight_right(u) * GAMMA) {
                SizeType v = own(ma.pool[u].tree.left, first);
                ma.pool[u].tree.left = v;
                ma.pool[node].tree.right = _rotate_right(u);
            }
            return _rotate_left(node);
        }
        if (weight_right(node) * DELTA < weight_left(node)) {
            SizeType u = own(ma.pool[node].tree.left, first);
            ma.pool[node].tree.left = u;
            if (weight_right(u) >= weight_left(u) * GAMMA) {
                SizeType v = own(ma.pool[u].tree.right, first);
                ma.pool[u].tree.right = v;
                ma.pool[node].tree.left = _rotate_left(u);
            }
            return _rotate_right(node);
        }
        return node;
    }

    SizeType _merge_with_root(SizeType l, SizeType pivot, SizeType r, size_t first) {
        if (weight(r) * DELTA < weight(l)) {
            l = own(l, first);
            ma.pool[l].tree.right = _merge_with_root(ma.pool[l].tree.right, pivot, r, first);
            update(l);
            return balance(l, first);
        }
        if (weight(l) * DELTA < weight(r)) {
            r = own(r, first);
            ma.pool[r].tree.left = _merge_with_root(l, pivot, ma.pool[r].tree.left, first);
            update(r);
            return balance(r, first);
        }
        pivot = own(pivot, first);
        ma.pool[pivot].tree.left = l;
        ma.pool[pivot].tree.right = r;
        update(pivot);
        return pivot;
    }

    // Internal helpers for insertion/deletion with one copied search path.
    // The caller captures first = ma.ptr once for the entire public operation.
    // own()/balance() follow the ownership-frontier contract.
    // balance() is called after update() and may copy heavy children.
    SizeType insert_node(SizeType node, SizeType k, const T &value, size_t first) {
        if (!node) return ma.new_node(value);
        node = own(node, first);
        SizeType left_size = ma.pool[node].tree.left_size;
        if (k <= left_size) {
            ma.pool[node].tree.left = insert_node(ma.pool[node].tree.left, k, value, first);
        } else {
            ma.pool[node].tree.right = insert_node(ma.pool[node].tree.right, k-left_size-1, value, first);
        }
        update(node);
        return balance(node, first);
    }

    // Remove an extreme value without allocating a detached singleton node.
    // An old clean root at the endpoint can be omitted without copying it.
    SizeType erase_extreme(SizeType node, T &value, bool rightmost, size_t first) {
        const SizeType next = rightmost ? ma.pool[node].tree.right : ma.pool[node].tree.left;
        if (!next) {
            value = ma.pool[node].key;
            return rightmost ? ma.pool[node].tree.left : ma.pool[node].tree.right;
        }
        node = own(node, first);
        const SizeType child = erase_extreme(next, value, rightmost, first);
        if (rightmost) ma.pool[node].tree.right = child;
        else ma.pool[node].tree.left = child;
        update(node);
        return balance(node, first);
    }

    SizeType erase_node(SizeType node, SizeType k, T &value, size_t first) {
        const SizeType left = ma.pool[node].tree.left, right = ma.pool[node].tree.right;
        const SizeType left_size = ma.pool[node].tree.left_size;
        if (k == left_size && (!left || !right)) {
            value = ma.pool[node].key;
            return left ? left : right;
        }
        node = own(node, first);
        if (k < left_size) {
            ma.pool[node].tree.left = erase_node(left, k, value, first);
        } else if (k > left_size) {
            ma.pool[node].tree.right = erase_node(right, k-left_size-1, value, first);
        } else {
            value = ma.pool[node].key;
            T replacement;
            if (left_size >= ma.pool[node].tree.size-left_size-1) {
                ma.pool[node].tree.left = erase_extreme(left, replacement, true, first);
            } else {
                ma.pool[node].tree.right = erase_extreme(right, replacement, false, first);
            }
            ma.pool[node].key = replacement;
        }
        update(node);
        return balance(node, first);
    }

    // Merge can use this specialized extraction instead of split(size-1).
    // It returns a consumed singleton pivot, ready for join().
    pair<SizeType, SizeType> extract_right_node(SizeType node, size_t first) {
        node = own(node, first);
        if (!ma.pool[node].tree.right) {
            SizeType rest = ma.pool[node].tree.left;
            ma.pool[node].tree.left = 0;
            update(node);
            return {rest, node};
        }
        auto result = extract_right_node(ma.pool[node].tree.right, first);
        ma.pool[node].tree.right = result.first;
        update(node);
        return {balance(node, first), result.second};
    }

    pair<SizeType, SizeType> extract_left_node(SizeType node, size_t first) {
        node = own(node, first);
        if (!ma.pool[node].tree.left) {
            SizeType rest = ma.pool[node].tree.right;
            ma.pool[node].tree.right = 0;
            update(node);
            return {rest, node};
        }
        auto result = extract_left_node(ma.pool[node].tree.left, first);
        ma.pool[node].tree.left = result.first;
        update(node);
        return {balance(node, first), result.second};
    }

    SizeType _merge_node(SizeType l, SizeType r, size_t first) {
        if (!l) return r;
        if (!r) return l;
        if (ma.pool[r].tree.size < ma.pool[l].tree.size) {
            auto [rest, pivot] = extract_left_node(r, first);
            return _merge_with_root(l, pivot, rest, first);
        }
        auto [rest, pivot] = extract_right_node(l, first);
        return _merge_with_root(rest, pivot, r, first);
    }

    pair<SizeType, SizeType> _split_node(SizeType node, SizeType k, size_t first) {
        if (!k) return {0, node};
        if (k == ma.pool[node].tree.size) return {node, 0};
        node = own(node, first);
        SizeType lch = ma.pool[node].tree.left, rch = ma.pool[node].tree.right;
        SizeType left_size = ma.pool[node].tree.left_size;
        if (k <= left_size) {
            auto [l, r] = _split_node(lch, k, first);
            return {l, _merge_with_root(r, node, rch, first)};
        }
        auto [l, r] = _split_node(rch, k-left_size-1, first);
        return {_merge_with_root(lch, node, l, first), r};
    }

    SizeType _split_node_left(SizeType node, SizeType k, size_t first) {
        if (!k) return 0;
        if (k == ma.pool[node].tree.size) return node;
        node = own(node, first);
        SizeType lch = ma.pool[node].tree.left, rch = ma.pool[node].tree.right;
        SizeType left_size = ma.pool[node].tree.left_size;
        if (k <= left_size) return _split_node_left(lch, k, first);
        SizeType r = _split_node_left(rch, k-left_size-1, first);
        return _merge_with_root(lch, node, r, first);
    }

    SizeType _split_node_right(SizeType node, SizeType k, size_t first) {
        if (!k) return node;
        if (k == ma.pool[node].tree.size) return 0;
        node = own(node, first);
        SizeType lch = ma.pool[node].tree.left, rch = ma.pool[node].tree.right;
        SizeType left_size = ma.pool[node].tree.left_size;
        if (k > left_size) return _split_node_right(rch, k-left_size-1, first);
        SizeType l = _split_node_right(lch, k, first);
        return _merge_with_root(l, node, rch, first);
    }

    static PLTM _new(SizeType root) {
        return PLTM(root);
    }

    PersistentWBTree(SizeType root) : root(root) {}

  public:
    /// @brief 空の列を作成する / O(1)
    PersistentWBTree() : root(0) {}

    /// @brief 配列から木を構築する / O(N)
    PersistentWBTree(const vector<T> &a) { _build(a); }

    /// @brief 末尾に other を連結した列を返す / O(log(N + M))
    PLTM merge(PLTM other) {
        SizeType root = _merge_node(this->root, other.root, ma.ptr);
        return _new(root);
    }

    /// @brief 先頭 k 要素と残りに分割した列を返す / O(log N)
    pair<PLTM, PLTM> split(SizeType k) {
        assert(0 <= k && k <= len());
        if (k == 0) return {_new(0), _new(root)};
        if (k == len()) return {_new(root), _new(0)};
        auto [l, r] = _split_node(this->root, k, ma.ptr);
        return {_new(l), _new(r)};
    }

    /// @brief 先頭 k 要素からなる列を返す / O(log N)
    PLTM split_left(SizeType k) {
        assert(0 <= k && k <= len());
        if (k == 0) return _new(0);
        if (k == len()) return _new(root);
        SizeType l = _split_node_left(this->root, k, ma.ptr);
        return _new(l);
    }

    /// @brief 先頭 k 要素を除いた列を返す / O(log N)
    PLTM split_right(SizeType k) {
        assert(0 <= k && k <= len());
        if (k == 0) return _new(root);
        if (k == len()) return _new(0);
        SizeType r = _split_node_right(this->root, k, ma.ptr);
        return _new(r);
    }

    /// @brief 位置 k に key を挿入した列を返す / O(log N)
    PLTM insert(SizeType k, T key) {
        assert(0 <= k && k <= len());
        const size_t first = ma.ptr;
        return _new(insert_node(root, k, key, first));
    }

    /// @brief 位置 k の要素を削除した列と削除値を返す / O(log N)
    pair<PLTM, T> pop(SizeType k) {
        assert(0 <= k && k < len());
        const size_t first = ma.ptr;
        T value;
        SizeType result = erase_node(root, k, value, first);
        return {_new(result), value};
    }

    /// @brief 列を vector に変換する / O(N)
    vector<T> tovector() const {
        vector<T> result;
        result.reserve(len());
        auto dfs = [&] (auto &&dfs, SizeType node) -> void {
            if (!node) return;
            const auto &slot = ma.pool[node];
            if (slot.tree.left) dfs(dfs, slot.tree.left);
            result.emplace_back(slot.key);
            if (slot.tree.right) dfs(dfs, slot.tree.right);
        };
        dfs(dfs, root);
        return result;
    }

    /// @brief 同じ列を共有するコピーを返す / O(1)
    PLTM copy() const {
        return _new(root);
    }

    /// @brief 位置 k の値を返す / O(log N)
    T get(SizeType k) const {
        assert(0 <= k && k < len());
        SizeType node = root;
        while (true) {
            const auto &slot = ma.pool[node];
            const SizeType left_size = slot.tree.left_size;
            if (k == left_size) return slot.key;
            using Unsigned = typename std::make_unsigned<SizeType>::type;
            const Unsigned mask = Unsigned(0) - Unsigned(k > left_size);
            const Unsigned left = static_cast<Unsigned>(slot.tree.left);
            const Unsigned right = static_cast<Unsigned>(slot.tree.right);
            node = static_cast<SizeType>(left ^ ((left ^ right) & mask));
            k -= static_cast<SizeType>(static_cast<Unsigned>(left_size + 1) & mask);
        }
    }

    /// @brief 列を出力する / O(N)
    void print() const {
        vector<T> a = tovector();
        cout << "[";
        for (size_t i = 0; i < a.size(); ++i) {
            if (i) cout << ", ";
            cout << a[i];
        }
        cout << "]" << endl;
    }

    /// @brief 要素数を返す / O(1)
    SizeType len() const {
        return ma.pool[root].tree.size;
    }

    /// @brief 列を出力する / O(N)
    friend ostream& operator<<(ostream& os, const PersistentWBTree<T, SizeType> &tree) {
        vector<T> a = tree.tovector();
        os << "[";
        for (size_t i = 0; i < a.size(); ++i) {
            if (i) os << ", ";
            os << a[i];
        }
        os << "]";
        return os;
    }

    /// @brief N ノード分の容量を確保する / O(N)
    /// @note 容量はブロック単位で切り上げる
    static void reserve(SizeType n) { ma.reserve(n); }

    /// @brief 木を再構築して確保済み領域を再利用する / O(N)
    /// @note 同じ領域を共有する他の木は無効になる
    static void rebuild(PLTM &tree) {
        vector<T> a = tree.tovector();
        PLTM::ma.reset();
        tree = PLTM(a);
    }
};

template <class T, typename SizeType>
typename PersistentWBTree<T, SizeType>::MemoryAllocator PersistentWBTree<T, SizeType>::ma;

}  // namespace titan23



void solve() {
    int q; cin >> q;
    using Tree = titan23::PersistentWBTree<char, ll>;
    // Tree::reserve(1e7);
    vector<Tree> T(q+2);
    vector<char> s0 = {'0'};
    vector<char> s1 = {'1'};
    T[0] = Tree(s0);
    T[1] = Tree(s1);
    rep(i, q) {
        ll l, r, x; cin >> l >> r >> x;
        --x;
        T[i+2] = T[l].merge(T[r]);
        if (T[i+2].len() >= 1e18) {
            T[i+2] = T[i+2].split_left((ll)1e18);
        }
        cout << T[i+2].get(x) << "\n";
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(0);
    cout << fixed << setprecision(15);
    cerr << fixed << setprecision(15);

    int t = 1;
    // cin >> t;
    for (int i = 0; i < t; ++i) {
        solve();
    }

    return 0;
}

// MLE
