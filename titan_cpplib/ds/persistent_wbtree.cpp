/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/persistent_wbtree.cpp
#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
using namespace std;

namespace titan23 {

/// @brief 列を管理する永続重み平衡木 / 0-indexed
/// 全インスタンスが静的なノード領域 ma を共有する
template<class T, typename SizeType = int>
class PersistentWBTree {
    using Id = uint32_t;
    static constexpr Id leaf_bit = Id(1) << 31;

public:
    /// @brief 全ての木が共有するノード領域 / 値は葉ごとに keys へ置く
    struct MemoryAllocator {
        /// @brief 内部ノード / 子の番号と左部分木の要素数を持つ
        struct Node { Id left, right; SizeType left_size; };
        /// @brief 2^BlockBits 要素ずつのブロックで領域を確保する可変長配列
        template <class U, unsigned BlockBits>
        class Blocks {
            static constexpr size_t block_size = size_t(1) << BlockBits;
            static constexpr size_t block_mask = block_size - 1;
            static constexpr size_t block_alignment = alignof(U) > 64 ? alignof(U) : 64;
            static_assert(block_size <= size_t(-1) / sizeof(U), "block too large");
            using Alloc = allocator<U>;
            using Traits = allocator_traits<Alloc>;
            Alloc allocator_;
            vector<U*> blocks_;
            size_t size_ = 0;

            void add_block() {
                U *block = static_cast<U*>(::operator new(sizeof(U) * block_size, align_val_t(block_alignment)));
                try {
                    blocks_.push_back(block);
                } catch (...) {
                    ::operator delete(block, align_val_t(block_alignment));
                    throw;
                }
            }

            void clear_storage() noexcept {
                if constexpr (!is_trivially_destructible<U>::value) {
                    for (size_t i = 0; i < size_; ++i) {
                        Traits::destroy(allocator_, &(*this)[i]);
                    }
                }
                for (U *block : blocks_) {
                    ::operator delete(block, align_val_t(block_alignment));
                }
                blocks_.clear();
                size_ = 0;
            }

        public:
            /// @brief 空の領域を初期化する / O(1)
            Blocks() = default;

            /// @brief コピー構築を禁止する
            Blocks(const Blocks&) = delete;

            /// @brief コピー代入を禁止する
            Blocks& operator=(const Blocks&) = delete;

            /// @brief 領域の所有権を移動する / O(1)
            Blocks(Blocks&& other) noexcept
                : blocks_(move(other.blocks_)), size_(exchange(other.size_, 0)) {
                other.blocks_.clear();
            }

            /// @brief 現在の N 要素と M ブロックを破棄して移動代入する / O(N + M)
            Blocks& operator=(Blocks&& other) noexcept {
                if (this != &other) {
                    clear_storage();
                    blocks_ = move(other.blocks_);
                    size_ = exchange(other.size_, 0);
                    other.blocks_.clear();
                }
                return *this;
            }

            /// @brief 構築済みの N 要素と M ブロックを破棄する / O(N + M)
            ~Blocks() { clear_storage(); }

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

            /// @brief n 要素分の容量を確保する / O(n)
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
                Traits::construct(allocator_, value, forward<Args>(args)...);
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
    public:
        Blocks<Node, 16> nodes;
        Blocks<T, 16> keys;
        size_t ptr = 1, leaf_ptr = 0;

        /// @brief 空の領域を作成する / O(1)
        MemoryAllocator() { nodes.emplace_back(Node{}); }

        /// @brief 内部ノードを作成する / 償却 O(1)
        Id branch(Node node) {
            if (ptr >= leaf_bit) throw length_error("node index overflow");
            if (ptr < nodes.size()) nodes[ptr] = node;
            else nodes.emplace_back(node);
            return static_cast<Id>(ptr++);
        }

        /// @brief 葉を作成する / 償却 O(1)
        Id leaf(const T& value) {
            if (leaf_ptr >= leaf_bit) throw length_error("leaf index overflow");
            if (leaf_ptr < keys.size()) keys[leaf_ptr] = value;
            else keys.emplace_back(value);
            return leaf_bit | static_cast<Id>(leaf_ptr++);
        }

        /// @brief 領域を再利用する / O(1)
        /// @note この領域を使う既存の木は無効になる
        void reset() { ptr = 1; leaf_ptr = 0; }

        /// @brief n 個の内部ノードの容量を確保する / O(n)
        void reserve(size_t n) { nodes.reserve(n); }

        /// @brief 確保済み領域のバイト数を返す / O(1)
        size_t capacity_bytes() const {
            return nodes.capacity() * sizeof(Node) + keys.capacity() * sizeof(T);
        }
    };

    /// @brief 全インスタンスが共有するノード領域
    inline static MemoryAllocator ma;

private:
    using Node = typename MemoryAllocator::Node;
    struct Part { Id id = 0; SizeType n = 0; };
    Part root;
    using Wide = conditional_t<(sizeof(SizeType) < 8), uint64_t, __uint128_t>;

    explicit PersistentWBTree(Part p) : root(p) {}

    static bool leaf(Part p) { return (p.id & leaf_bit) != 0; }

    static Part left(Part p) {
        const auto& x = ma.nodes[p.id];
        return {x.left, x.left_size};
    }

    static Part right(Part p) {
        const auto& x = ma.nodes[p.id];
        return {x.right, static_cast<SizeType>(p.n - x.left_size)};
    }

    static Part branch(Part l, Part r, Id spare = 0) {
        assert(l.id && r.id);
        assert(l.n <= numeric_limits<SizeType>::max() - r.n);
        Node node{l.id, r.id, l.n};
        if (spare) ma.nodes[spare] = node;
        else spare = ma.branch(node);
        return {spare, static_cast<SizeType>(l.n + r.n)};
    }

    static Part own(Part p, size_t first) {
        if (p.id < first) p.id = ma.branch(ma.nodes[p.id]);
        return p;
    }

    static Part rotate_left(Part p) {
        Part r = right(p), a = left(p), b = left(r), c = right(r);
        return branch(branch(a, b, p.id), c, r.id);
    }

    static Part rotate_right(Part p) {
        Part l = left(p), a = left(l), b = right(l), c = right(p);
        return branch(a, branch(b, c, p.id), l.id);
    }

    static Part balance(Part p, size_t first) {
        Part l = left(p), r = right(p);
        if (Wide(l.n) * 3 < Wide(r.n)) {
            r = own(r, first);
            Part b = left(r), c = right(r);
            if (Wide(b.n) >= Wide(c.n) * 2) {
                b = own(b, first);
                r = branch(b, c, r.id);
                r = rotate_right(r);
            }
            return rotate_left(branch(l, r, p.id));
        }
        if (Wide(r.n) * 3 < Wide(l.n)) {
            l = own(l, first);
            Part a = left(l), b = right(l);
            if (Wide(b.n) >= Wide(a.n) * 2) {
                b = own(b, first);
                l = branch(a, b, l.id);
                l = rotate_left(l);
            }
            return rotate_right(branch(l, r, p.id));
        }
        return p;
    }

    static Part join(Part l, Part r, size_t first) {
        if (!l.id) return r;
        if (!r.id) return l;
        if (Wide(l.n) > Wide(r.n) * 3) {
            l = own(l, first);
            Part a = left(l), b = right(l);
            return balance(branch(a, join(b, r, first), l.id), first);
        }
        if (Wide(r.n) > Wide(l.n) * 3) {
            r = own(r, first);
            Part a = left(r), b = right(r);
            return balance(branch(join(l, a, first), b, r.id), first);
        }
        return branch(l, r);
    }

    static pair<Part, Part> split_node(Part p, SizeType k, size_t first) {
        if (!k) return {{}, p};
        if (k == p.n) return {p, {}};
        Part l = left(p), r = right(p);
        if (k == l.n) return {l, r};
        if (k < l.n) {
            auto parts = split_node(l, k, first);
            return {parts.first, join(parts.second, r, first)};
        }
        auto parts = split_node(r, k-l.n, first);
        return {join(l, parts.first, first), parts.second};
    }

    static Part prefix(Part p, SizeType k, size_t first) {
        if (!k) return {};
        if (k == p.n) return p;
        Part l = left(p), r = right(p);
        if (k <= l.n) return prefix(l, k, first);
        return join(l, prefix(r, k-l.n, first), first);
    }

    static Part suffix(Part p, SizeType k, size_t first) {
        if (!k) return p;
        if (k == p.n) return {};
        Part l = left(p), r = right(p);
        if (k >= l.n) return suffix(r, k-l.n, first);
        return join(suffix(l, k, first), r, first);
    }

    static Part insert_node(Part p, SizeType k, const T& value, size_t first) {
        if (!p.id) return {ma.leaf(value), 1};
        if (leaf(p)) {
            Part q{ma.leaf(value), 1};
            return k ? branch(p, q) : branch(q, p);
        }
        p = own(p, first);
        Part l = left(p), r = right(p);
        if (k < l.n) l = insert_node(l, k, value, first);
        else r = insert_node(r, k-l.n, value, first);
        return balance(branch(l, r, p.id), first);
    }

    static Part erase_node(Part p, SizeType k, T& value, size_t first) {
        if (leaf(p)) { value = ma.keys[p.id & ~leaf_bit]; return {}; }
        Part l = left(p), r = right(p);
        p = own(p, first);
        if (k < l.n) l = erase_node(l, k, value, first);
        else r = erase_node(r, k-l.n, value, first);
        if (!l.id) return r;
        if (!r.id) return l;
        return balance(branch(l, r, p.id), first);
    }

public:
    /// @brief 空の列を構築する / O(1)
    PersistentWBTree() = default;

    /// @brief 配列 a から構築する / O(N)
    PersistentWBTree(const vector<T>& a) {
        assert(a.size() <= static_cast<size_t>(numeric_limits<SizeType>::max()));
        auto build = [&](auto&& self, size_t l, size_t r) -> Part {
            if (r-l == 1) return {ma.leaf(a[l]), 1};
            size_t m = l + (r-l)/2;
            Part x = self(self, l, m), y = self(self, m, r);
            return branch(x, y);
        };
        if (!a.empty()) root = build(build, 0, a.size());
    }

    /// @brief 末尾に other を連結した列を返す / O(log(N + M))
    PersistentWBTree merge(PersistentWBTree other) const {
        assert(root.n <= numeric_limits<SizeType>::max() - other.root.n);
        return PersistentWBTree(join(root, other.root, ma.ptr));
    }

    /// @brief 連結した列の先頭 limit 要素までを返す / O(log(N + M))
    PersistentWBTree merge_prefix(PersistentWBTree other, SizeType limit) const {
        assert(limit >= 0);
        const size_t first = ma.ptr;
        if (root.n >= limit) return PersistentWBTree(prefix(root, limit, first));
        Part r = other.root;
        if (r.n > limit-root.n) r = prefix(r, limit-root.n, first);
        return PersistentWBTree(join(root, r, first));
    }

    /// @brief 先頭 k 要素と残りに分割して返す / O(log N)
    pair<PersistentWBTree, PersistentWBTree> split(SizeType k) const {
        assert(0 <= k && k <= len());
        auto p = split_node(root, k, ma.ptr);
        return {PersistentWBTree(p.first), PersistentWBTree(p.second)};
    }

    /// @brief 先頭 k 要素からなる列を返す / O(log N)
    PersistentWBTree split_left(SizeType k) const {
        assert(0 <= k && k <= len());
        return PersistentWBTree(prefix(root, k, ma.ptr));
    }

    /// @brief 先頭 k 要素を除いた列を返す / O(log N)
    PersistentWBTree split_right(SizeType k) const {
        assert(0 <= k && k <= len());
        return PersistentWBTree(suffix(root, k, ma.ptr));
    }

    /// @brief 位置 k に値を挿入した列を返す / O(log N)
    PersistentWBTree insert(SizeType k, T value) const {
        assert(0 <= k && k <= len());
        return PersistentWBTree(insert_node(root, k, value, ma.ptr));
    }

    /// @brief 位置 k の値を削除した列と削除値を返す / O(log N)
    pair<PersistentWBTree, T> pop(SizeType k) const {
        assert(0 <= k && k < len());
        T value;
        Part p = erase_node(root, k, value, ma.ptr);
        return {PersistentWBTree(p), value};
    }

    /// @brief 位置 k の値を返す / O(log N)
    T get(SizeType k) const {
        assert(0 <= k && k < len());
        Id node = root.id;
        while (!(node & leaf_bit)) {
            const auto& x = ma.nodes[node];
            const bool r = k >= x.left_size;
            node = r ? x.right : x.left;
            k -= r ? x.left_size : 0;
        }
        return ma.keys[node & ~leaf_bit];
    }

    /// @brief 要素数を返す / O(1)
    SizeType len() const { return root.n; }

    /// @brief 列を共有するコピーを返す / O(1)
    PersistentWBTree copy() const { return *this; }

    /// @brief 全要素を配列にして返す / O(N)
    vector<T> tovector() const {
        vector<T> result;
        result.reserve(static_cast<size_t>(len()));
        auto dfs = [&](auto&& self, Id p) -> void {
            if (!p) return;
            if (p & leaf_bit) result.push_back(ma.keys[p & ~leaf_bit]);
            else { auto x = ma.nodes[p]; self(self, x.left); self(self, x.right); }
        };
        dfs(dfs, root.id);
        return result;
    }

    /// @brief 平衡条件と部分木サイズを検査する / O(N)
    void check() const {
        auto dfs = [&](auto&& self, Part p) -> void {
            if (!p.id) { assert(!p.n); return; }
            if (leaf(p)) { assert(p.n == 1); return; }
            Part l = left(p), r = right(p);
            assert(l.id && r.id && l.n > 0 && r.n > 0);
            assert(Wide(l.n)*3 >= Wide(r.n) && Wide(r.n)*3 >= Wide(l.n));
            self(self, l); self(self, r);
        };
        dfs(dfs, root);
    }

    /// @brief n 個の内部ノードの容量を確保する / O(n)
    static void reserve(SizeType n) { assert(n >= 0); ma.reserve(static_cast<size_t>(n)); }

    /// @brief 木を再構築する / O(N)
    /// @note 同じ領域を共有する他の木は無効になる
    static void rebuild(PersistentWBTree& tree) {
        auto a = tree.tovector();
        ma.reset();
        tree = PersistentWBTree(a);
    }

    /// @brief 全要素を標準出力に出力する / O(N)
    void print() const { cout << *this << endl; }

    /// @brief 全要素をストリームへ出力する / O(N)
    friend ostream& operator<<(ostream& out, const PersistentWBTree& tree) {
        auto a = tree.tovector(); out << '[';
        for (size_t i = 0; i < a.size(); ++i) { if (i) out << ", "; out << a[i]; }
        return out << ']';
    }
};

} // namespace titan23
