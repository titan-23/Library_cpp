/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/persistent_lazy_wbtree.cpp
#pragma once
#include <new>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <iostream>
#include <vector>
#include <cassert>
#include <memory>
#include <algorithm>
#include <cstdint>
#include "titan_cpplib/others/print.cpp"
using namespace std;

namespace titan23 {

/// @brief 遅延作用と区間反転に対応する永続重み平衡木 / 0-indexed
/// 全インスタンスが静的なノード領域 ma を共有する
template <class SizeType, class T, class F,
        T (*op)(T, T),
        T (*mapping)(F, T),
        F (*composition)(F, F),
        T (*e)(),
        F (*id)()>
class PersistentLazyWBTree {
    using Index = conditional_t<(sizeof(SizeType) > 4), uint32_t, typename make_unsigned<SizeType>::type>;
    static constexpr unsigned index_bits = sizeof(SizeType) > 4 ? 31 : numeric_limits<SizeType>::digits;
    using Wide = conditional_t<(sizeof(SizeType) < 8), uint64_t, __uint128_t>;
public:
    /// @brief 全ての木が共有するノード領域
    struct MemoryAllocator {
        /// @brief 子の番号、反転フラグ、部分木サイズを持つ
        struct Node {
            Index left : index_bits;
            Index rev : 1;
            Index right;
            SizeType size, left_size;

            /// @brief オブジェクトを構築する / O(1)
            Node() : left(0), rev(0), right(0), size(0), left_size(0) {}

            /// @brief オブジェクトを構築する / O(1)
            Node(SizeType l, SizeType r, SizeType s) : left(l), rev(0), right(r), size(s), left_size(0) {}
        };
        /// @brief 部分木の総積と遅延作用を持つ
        struct Data {
            T data;
            F lazy;

            /// @brief オブジェクトを構築する / O(1)
            Data() = default;

            /// @brief オブジェクトを構築する / O(1)
            Data(const T& value, const F& f) : data(value), lazy(f) {}
        };
        /// @brief 2^BlockBits 要素ずつのブロックで領域を確保する可変長配列
        template <class U, unsigned BlockBits>
        class BlockVector {
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

            /// @brief オブジェクトを構築する / O(1)
            BlockVector() = default;

            /// @brief コピーを禁止する
            BlockVector(const BlockVector&) = delete;

            /// @brief コピーを禁止する
            BlockVector& operator=(const BlockVector&) = delete;

            /// @brief オブジェクトを構築する / O(1)
            BlockVector(BlockVector&& other) noexcept
                : blocks_(move(other.blocks_)), size_(exchange(other.size_, 0)) {
                other.blocks_.clear();
            }

            /// @brief 現在の N 要素と M ブロックを破棄して移動代入する / O(N + M)
            BlockVector& operator=(BlockVector&& other) noexcept {
                if (this != &other) {
                    clear_storage();
                    blocks_ = move(other.blocks_);
                    size_ = exchange(other.size_, 0);
                    other.blocks_.clear();
                }
                return *this;
            }

            /// @brief 構築済みの N 要素と M ブロックを破棄する / O(N + M)
            ~BlockVector() { clear_storage(); }

            /// @brief 要素への参照を返す / O(1)
            U& operator[](size_t i) noexcept {
                return blocks_[i >> BlockBits][i & block_mask];
            }

            /// @brief 要素への参照を返す / O(1)
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

        /// @brief 1 ノード分の Node と Data
        struct Slot {
            Node tree;
            Data data;

            /// @brief オブジェクトを構築する / O(1)
            Slot(SizeType l, SizeType r, SizeType s, const T& value, const F& lazy, int8_t rev)
                : tree(l, r, s), data(value, lazy) { tree.rev = rev != 0; }
        };
        using Pool = BlockVector<Slot, 16>;
        Pool pool;

        /// @brief pool の各 Slot の Member を配列として参照する
        template<class U, U Slot::* Member>
        struct View {
            Pool *pool;

            /// @brief 要素への参照を返す / O(1)
            U& operator[](size_t i) noexcept { return (*pool)[i].*Member; }

            /// @brief 要素への参照を返す / O(1)
            const U& operator[](size_t i) const noexcept { return (*pool)[i].*Member; }

            /// @brief 構築済み要素数を返す / O(1)
            size_t size() const noexcept { return pool->size(); }

            /// @brief 確保済み容量を返す / O(1)
            size_t capacity() const noexcept { return pool->capacity(); }

            /// @brief n 要素分の容量を確保する / O(n)
            void reserve(size_t n) { pool->reserve(n); }
        };
        View<Node, &Slot::tree> tree;
        View<Data, &Slot::data> data;
        size_t ptr, cap;

        /// @brief オブジェクトを構築する / O(1)
        MemoryAllocator() : tree{&pool}, data{&pool}, ptr(1), cap(0) {
            pool.emplace_back(0, 0, 0, T{}, F{}, 0);
            cap = pool.capacity();
        }

        /// @brief オブジェクトを構築する / O(1)
        MemoryAllocator(MemoryAllocator&& other) noexcept

            : pool(move(other.pool)), tree{&pool}, data{&pool},
              ptr(other.ptr), cap(other.cap) {}

        /// @brief 現在の N 要素と M ブロックを破棄して移動代入する / O(N + M)
        MemoryAllocator& operator=(MemoryAllocator&& other) noexcept {
            if (this != &other) {
                pool = move(other.pool);
                ptr = other.ptr;
                cap = other.cap;
            }
            return *this;
        }

        /// @brief ノードを複製する / 償却 O(1)
        SizeType copy(SizeType node) {
            if (!node) return 0;
            if (ptr >= (uint64_t(1) << index_bits)) throw length_error("node index overflow");
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

        /// @brief ノードを作成する / 償却 O(1)
        SizeType new_node(const T& key, const F& f) {
            if (ptr >= (uint64_t(1) << index_bits)) throw length_error("node index overflow");
            if (pool.size() > ptr) {
                pool[ptr].tree = {0, 0, 1};
                pool[ptr].data = {key, f};
            } else {
                try {
                    pool.emplace_back(0, 0, 1, key, f, 0);
                } catch (...) {
                    cap = pool.capacity();
                    throw;
                }
                cap = pool.capacity();
            }
            return ptr++;
        }

        /// @brief n ノード分の容量を確保する / O(n)
        void reserve(SizeType n) {
            pool.reserve(n);
            cap = pool.capacity();
        }

        /// @brief 確保済み領域を再利用する / O(1)
        /// @note 同じ領域を共有する他の木は無効になる
        void reset() { ptr = 1; }

        /// @brief 領域がほぼ埋まっているか判定する / O(1)
        bool almost_full() const { return (double)ptr > max(1.0, cap * 0.99); }

        /// @brief 確保済み領域のバイト数を返す / O(1)
        size_t capacity_bytes() const { return pool.capacity() * sizeof(Slot); }
    };

    /// @brief 全インスタンスが共有するノード領域
    static MemoryAllocator ma;

private:
    using PLTM = PersistentLazyWBTree<SizeType, T, F, op, mapping, composition, e, id>;
    static constexpr int DELTA = 3, GAMMA = 2;
    Index root;

    SizeType size_of(SizeType node) const { return ma.pool[node].tree.size; }

    void update(SizeType node) {
        const SizeType l = ma.pool[node].tree.left, r = ma.pool[node].tree.right;
        assert(l && r);
        ma.pool[node].tree.left_size = size_of(l);
        ma.pool[node].tree.size = size_of(l) + size_of(r);
        ma.pool[node].data.data = op(ma.pool[l].data.data, ma.pool[r].data.data);
    }

    SizeType own(SizeType node, size_t first) {
        return node && static_cast<size_t>(node) < first ? ma.copy(node) : node;
    }

    void push(SizeType node, const F &f) {
        ma.pool[node].data.data = mapping(f, ma.pool[node].data.data);
        if (ma.pool[node].tree.size > 1)
            ma.pool[node].data.lazy = composition(f, ma.pool[node].data.lazy);
    }

    void propagate(SizeType node, size_t first) {
        assert(static_cast<size_t>(node) >= first);
        if (ma.pool[node].tree.size == 1) return;
        const bool rev = ma.pool[node].tree.rev;
        const bool lazy = ma.pool[node].data.lazy != id();
        if (!rev && !lazy) return;
        SizeType l = ma.pool[node].tree.left, r = ma.pool[node].tree.right;
        if (rev) swap(l, r);
        if (lazy || size_of(l) > 1) l = own(l, first);
        if (lazy || size_of(r) > 1) r = own(r, first);
        if (rev) {
            if (size_of(l) > 1) ma.pool[l].tree.rev ^= 1;
            if (size_of(r) > 1) ma.pool[r].tree.rev ^= 1;
            ma.pool[node].tree.left_size = ma.pool[node].tree.size - ma.pool[node].tree.left_size;
        }
        if (lazy) {

            push(l, ma.pool[node].data.lazy);

            push(r, ma.pool[node].data.lazy);
        }
        ma.pool[node].tree.left = l;
        ma.pool[node].tree.right = r;
        ma.pool[node].tree.rev = 0;
        ma.pool[node].data.lazy = id();
    }

    SizeType branch(SizeType l, SizeType r, SizeType spare = 0) {
        assert(l && r);
        if (!spare) spare = ma.new_node(op(ma.pool[l].data.data, ma.pool[r].data.data), id());
        else ma.pool[spare].data.data = op(ma.pool[l].data.data, ma.pool[r].data.data);
        ma.pool[spare].tree.left = l;
        ma.pool[spare].tree.right = r;
        ma.pool[spare].tree.left_size = size_of(l);
        ma.pool[spare].tree.size = size_of(l) + size_of(r);
        ma.pool[spare].data.lazy = id();
        ma.pool[spare].tree.rev = 0;
        return spare;
    }

    SizeType rotate_left(SizeType node) {
        SizeType pivot = ma.pool[node].tree.right;
        ma.pool[node].tree.right = ma.pool[pivot].tree.left;
        ma.pool[pivot].tree.left = node;

        update(node); update(pivot);
        return pivot;
    }

    SizeType rotate_right(SizeType node) {
        SizeType pivot = ma.pool[node].tree.left;
        ma.pool[node].tree.left = ma.pool[pivot].tree.right;
        ma.pool[pivot].tree.right = node;

        update(node); update(pivot);
        return pivot;
    }

    SizeType balance(SizeType node, size_t first) {
        const SizeType left_size = ma.pool[node].tree.left_size;
        const SizeType right_size = ma.pool[node].tree.size - left_size;
        if (Wide(left_size) * DELTA < Wide(right_size)) {
            SizeType u = own(ma.pool[node].tree.right, first);
            ma.pool[node].tree.right = u;

            propagate(u, first);
            if (Wide(ma.pool[u].tree.left_size) >= Wide(ma.pool[u].tree.size-ma.pool[u].tree.left_size) * GAMMA) {
                SizeType v = own(ma.pool[u].tree.left, first);
                ma.pool[u].tree.left = v;
                propagate(v, first);
                ma.pool[node].tree.right = rotate_right(u);
            }
            return rotate_left(node);
        }
        if (Wide(right_size) * DELTA < Wide(left_size)) {
            SizeType u = own(ma.pool[node].tree.left, first);
            ma.pool[node].tree.left = u;

            propagate(u, first);
            if (Wide(ma.pool[u].tree.size-ma.pool[u].tree.left_size) >= Wide(ma.pool[u].tree.left_size) * GAMMA) {
                SizeType v = own(ma.pool[u].tree.right, first);
                ma.pool[u].tree.right = v;
                propagate(v, first);
                ma.pool[node].tree.left = rotate_left(u);
            }
            return rotate_right(node);
        }
        return node;
    }

    SizeType join(SizeType l, SizeType r, size_t first, SizeType spare = 0) {
        if (!l) return r;
        if (!r) return l;
        if (Wide(size_of(l)) > Wide(size_of(r)) * DELTA) {
            l = own(l, first); propagate(l, first);
            ma.pool[l].tree.right = join(ma.pool[l].tree.right, r, first, spare);

            update(l);
            return balance(l, first);
        }
        if (Wide(size_of(r)) > Wide(size_of(l)) * DELTA) {
            r = own(r, first); propagate(r, first);
            ma.pool[r].tree.left = join(l, ma.pool[r].tree.left, first, spare);

            update(r);
            return balance(r, first);
        }
        return branch(l, r, spare);
    }

    pair<SizeType, SizeType> split_node(SizeType node, SizeType k, size_t first) {
        if (k == 0) return {0, node};
        if (k == size_of(node)) return {node, 0};
        if (k == ma.pool[node].tree.left_size && !ma.pool[node].tree.rev && !(ma.pool[node].data.lazy != id()))
            return {static_cast<SizeType>(ma.pool[node].tree.left), static_cast<SizeType>(ma.pool[node].tree.right)};
        node = own(node, first); propagate(node, first);
        SizeType l = ma.pool[node].tree.left, r = ma.pool[node].tree.right;
        SizeType left_size = ma.pool[node].tree.left_size;
        if (k == left_size) return {l, r};
        if (k < left_size) {
            auto parts = split_node(l, k, first);
            return {parts.first, join(parts.second, r, first, node)};
        }
        auto parts = split_node(r, k-left_size, first);
        return {join(l, parts.first, first, node), parts.second};
    }

    SizeType prefix(SizeType node, SizeType k, size_t first) {
        if (!k) return 0;
        if (k == size_of(node)) return node;
        node = own(node, first); propagate(node, first);
        const SizeType left_size = ma.pool[node].tree.left_size;
        if (k <= left_size) return prefix(ma.pool[node].tree.left, k, first);
        return join(ma.pool[node].tree.left, prefix(ma.pool[node].tree.right, k-left_size, first), first, node);
    }

    SizeType suffix(SizeType node, SizeType k, size_t first) {
        if (!k) return node;
        if (k == size_of(node)) return 0;
        node = own(node, first); propagate(node, first);
        const SizeType left_size = ma.pool[node].tree.left_size;
        if (k >= left_size) return suffix(ma.pool[node].tree.right, k-left_size, first);
        return join(suffix(ma.pool[node].tree.left, k, first), ma.pool[node].tree.right, first, node);
    }

    SizeType insert_node(SizeType node, SizeType k, const T &value, size_t first) {
        if (!node) return ma.new_node(value, id());
        if (size_of(node) == 1) {
            SizeType leaf = ma.new_node(value, id());
            return k ? branch(node, leaf) : branch(leaf, node);
        }
        node = own(node, first); propagate(node, first);
        const SizeType left_size = ma.pool[node].tree.left_size;
        if (k < left_size) ma.pool[node].tree.left = insert_node(ma.pool[node].tree.left, k, value, first);
        else ma.pool[node].tree.right = insert_node(ma.pool[node].tree.right, k-left_size, value, first);

        update(node);
        return balance(node, first);
    }

    SizeType erase_node(SizeType node, SizeType k, T &value, size_t first) {
        if (size_of(node) == 1) {
            value = ma.pool[node].data.data;
            return 0;
        }
        node = own(node, first); propagate(node, first);
        const SizeType left_size = ma.pool[node].tree.left_size;
        if (k < left_size) ma.pool[node].tree.left = erase_node(ma.pool[node].tree.left, k, value, first);
        else ma.pool[node].tree.right = erase_node(ma.pool[node].tree.right, k-left_size, value, first);
        if (!ma.pool[node].tree.left) return ma.pool[node].tree.right;
        if (!ma.pool[node].tree.right) return ma.pool[node].tree.left;

        update(node);
        return balance(node, first);
    }

    SizeType reverse_node(SizeType node, SizeType l, SizeType r, size_t first) {
        if (r-l <= 1) return node;
        node = own(node, first);
        if (!l && r == size_of(node)) {
            ma.pool[node].tree.rev ^= 1;
            return node;
        }

        propagate(node, first);
        const SizeType left_size = ma.pool[node].tree.left_size;
        if (r <= left_size) {
            ma.pool[node].tree.left = reverse_node(ma.pool[node].tree.left, l, r, first);

            update(node);
            return node;
        }
        if (l >= left_size) {
            ma.pool[node].tree.right = reverse_node(ma.pool[node].tree.right, l-left_size, r-left_size, first);

            update(node);
            return node;
        }
        auto tail = split_node(node, r, first);
        auto head = split_node(tail.first, l, first);
        SizeType middle = own(head.second, first);
        if (size_of(middle) > 1) ma.pool[middle].tree.rev ^= 1;
        return join(join(head.first, middle, first), tail.second, first);
    }

    static PLTM _new(SizeType root) { return PLTM(root); }

    explicit PersistentLazyWBTree(SizeType root) : root(root) {}

public:

    /// @brief 空の列を構築する / O(1)
    PersistentLazyWBTree() : root(0) {}

    /// @brief 配列 a から構築する / O(N)
    PersistentLazyWBTree(const vector<T> &a) : root(0) {
        auto build = [&](auto &&self, SizeType l, SizeType r) -> SizeType {
            if (r-l == 1) return ma.new_node(a[l], id());
            SizeType mid = l + (r-l)/2;
            SizeType left = self(self, l, mid);
            SizeType right = self(self, mid, r);
            return branch(left, right);
        };
        if (!a.empty()) root = build(build, 0, static_cast<SizeType>(a.size()));
    }

    /// @brief 末尾に other を連結した列を返す / O(log(N + M))
    PLTM merge(PLTM other) { return _new(join(root, other.root, ma.ptr)); }

    /// @brief 先頭 k 要素と残りに分割して返す / O(log N)
    pair<PLTM, PLTM> split(SizeType k) {
        assert(0 <= k && k <= len());
        auto result = split_node(root, k, ma.ptr);
        return {_new(result.first), _new(result.second)};
    }

    /// @brief 先頭 k 要素からなる列を返す / O(log N)
    PLTM split_left(SizeType k) {
        assert(0 <= k && k <= len());
        return _new(prefix(root, k, ma.ptr));
    }

    /// @brief 先頭 k 要素を除いた列を返す / O(log N)
    PLTM split_right(SizeType k) {
        assert(0 <= k && k <= len());
        return _new(suffix(root, k, ma.ptr));
    }

    /// @brief 区間 [l, r) からなる列を返す / O(log N)
    PLTM split_range(SizeType l, SizeType r) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return _new(0);
        const size_t first = ma.ptr;
        return _new(suffix(prefix(root, r, first), l, first));
    }

    /// @brief 区間 [l, r) に作用を適用した列を返す / O(log N)
    PLTM apply(SizeType l, SizeType r, F f) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r || !(f != id())) return _new(root);
        const size_t first = ma.ptr;
        auto dfs = [&](auto &&self, SizeType node, SizeType l, SizeType r) -> SizeType {
            node = own(node, first);
            if (!l && r == size_of(node)) { push(node, f); return node; }

            propagate(node, first);
            const SizeType left_size = ma.pool[node].tree.left_size;
            if (l < left_size) ma.pool[node].tree.left = self(self, ma.pool[node].tree.left, l, min(r, left_size));
            if (r > left_size) ma.pool[node].tree.right = self(self, ma.pool[node].tree.right, l > left_size ? l-left_size : 0, r-left_size);

            update(node);
            return node;
        };
        return _new(dfs(dfs, root, l, r));
    }

    /// @brief 区間 [l, r) の総積を返す / O(log N)
    T prod(SizeType l, SizeType r) const {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return e();
        const F identity = id();
        auto dfs = [&](auto &&self, SizeType node, SizeType l, SizeType r, F acc) -> T {
            const auto &slot = ma.pool[node];
            if (l == 0 && r == slot.tree.size) {
                return acc != identity ? mapping(acc, slot.data.data) : slot.data.data;
            }
            if (slot.tree.rev) {
                const SizeType old_l = l;
                l = slot.tree.size - r;
                r = slot.tree.size - old_l;
            }
            const SizeType left_size = slot.tree.left_size;
            if (slot.data.lazy != identity) acc = composition(acc, slot.data.lazy);
            if (r <= left_size) return self(self, slot.tree.left, l, r, acc);
            if (l >= left_size) return self(self, slot.tree.right, l-left_size, r-left_size, acc);
            return op(self(self, slot.tree.left, l, left_size, acc),
                      self(self, slot.tree.right, 0, r-left_size, acc));
        };
        return dfs(dfs, root, l, r, identity);
    }

    /// @brief 位置 k に値を挿入した列を返す / O(log N)
    PLTM insert(SizeType k, T value) {
        assert(0 <= k && k <= len());
        return _new(insert_node(root, k, value, ma.ptr));
    }

    /// @brief 位置 k の値を削除した列と削除値を返す / O(log N)
    pair<PLTM, T> pop(SizeType k) {
        assert(0 <= k && k < len());
        T value;
        SizeType result = erase_node(root, k, value, ma.ptr);
        return {_new(result), value};
    }

    /// @brief 区間 [l, r) を反転した列を返す / O(log N)
    /// @note 総積が反転で変化しない場合に使える
    PLTM reverse(SizeType l, SizeType r) {
        assert(0 <= l && l <= r && r <= len());
        return _new(reverse_node(root, l, r, ma.ptr));
    }

    /// @brief 位置 k の値を変更した列を返す / O(log N)
    PLTM set(SizeType k, T value) {
        assert(0 <= k && k < len());
        const size_t first = ma.ptr;
        auto dfs = [&](auto &&self, SizeType node, SizeType k) -> SizeType {
            node = own(node, first);
            if (size_of(node) == 1) {
                ma.pool[node].data.data = value;
                ma.pool[node].data.lazy = id(); ma.pool[node].tree.rev = 0;
                return node;
            }

            propagate(node, first);
            const SizeType left_size = ma.pool[node].tree.left_size;
            if (k < left_size) ma.pool[node].tree.left = self(self, ma.pool[node].tree.left, k);
            else ma.pool[node].tree.right = self(self, ma.pool[node].tree.right, k-left_size);

            update(node);
            return node;
        };
        return _new(dfs(dfs, root, k));
    }

    /// @brief 位置 k の値を返す / O(log N)
    T get(SizeType k) const {
        assert(0 <= k && k < len());
        SizeType node = root;
        const F identity = id();
        F acc = identity;
        while (true) {
            const auto &slot = ma.pool[node];
            if (slot.tree.size == 1) {
                return acc != identity ? mapping(acc, slot.data.data) : slot.data.data;
            }
            if (slot.tree.rev) k = slot.tree.size - 1 - k;
            const SizeType left_size = slot.tree.left_size;
            if (slot.data.lazy != identity) acc = composition(acc, slot.data.lazy);
            using Unsigned = typename make_unsigned<SizeType>::type;
            const Unsigned mask = Unsigned(0) - Unsigned(k >= left_size);
            const Unsigned left = static_cast<Unsigned>(slot.tree.left);
            const Unsigned right = static_cast<Unsigned>(slot.tree.right);
            node = static_cast<SizeType>(left ^ ((left ^ right) & mask));
            k -= static_cast<SizeType>(static_cast<Unsigned>(left_size) & mask);
        }
    }

    /// @brief 全要素を配列にして返す / O(N)
    vector<T> tovector() const {
        vector<T> result; result.reserve(len());
        auto dfs = [&](auto &&self, SizeType node, F acc, bool rev) -> void {
            if (!node) return;
            if (size_of(node) == 1) {
                result.push_back(acc != id() ? mapping(acc, ma.pool[node].data.data) : ma.pool[node].data.data);
                return;
            }
            rev ^= ma.pool[node].tree.rev;
            if (ma.pool[node].data.lazy != id()) acc = composition(acc, ma.pool[node].data.lazy);

            self(self, rev ? ma.pool[node].tree.right : ma.pool[node].tree.left, acc, rev);

            self(self, rev ? ma.pool[node].tree.left : ma.pool[node].tree.right, acc, rev);
        };

        dfs(dfs, root, id(), false);
        return result;
    }

    /// @brief 同じ列を共有するコピーを返す / O(1)
    PLTM copy() const { return _new(root); }

    /// @brief 要素数を返す / O(1)
    SizeType len() const { return size_of(root); }

    /// @brief 部分木サイズと平衡条件を検査する / O(N)
    void check() const {
        auto dfs = [&](auto &&self, SizeType node) -> SizeType {
            const SizeType left = ma.pool[node].tree.left, right = ma.pool[node].tree.right;
            if (!left && !right) {
                assert(size_of(node) == 1 && ma.pool[node].tree.left_size == 0);
                return 1;
            }
            assert(left && right);
            SizeType ls = self(self, left), rs = self(self, right);
            assert(ls == ma.pool[node].tree.left_size);
            assert(ls+rs == size_of(node));
            assert(Wide(ls) <= Wide(rs)*DELTA && Wide(rs) <= Wide(ls)*DELTA);
            return ls+rs;
        };
        if (root) dfs(dfs, root);
    }

    /// @brief 木を再構築する / O(N)
    /// @note 同じ領域を共有する他の木は無効になる
    static void rebuild(PLTM &tree) {
        vector<T> values = tree.tovector();
        ma.reset(); tree = PLTM(values);
    }

    /// @brief 全要素を標準出力に出力する / O(N)
    void print() const { cout << *this << endl; }

    /// @brief 全要素をストリームへ出力する / O(N)
    friend ostream& operator<<(ostream& out, const PLTM &tree) {
        auto values = tree.tovector(); out << '[';
        for (size_t i = 0; i < values.size(); ++i) { if (i) out << ", "; out << values[i]; }
        return out << ']';
    }
};

template <class SizeType, class T, class F,
        T (*op)(T, T), T (*mapping)(F, T), F (*composition)(F, F), T (*e)(), F (*id)()>
typename PersistentLazyWBTree<SizeType, T, F, op, mapping, composition, e, id>::MemoryAllocator
PersistentLazyWBTree<SizeType, T, F, op, mapping, composition, e, id>::ma;
}  // namespace titan23
