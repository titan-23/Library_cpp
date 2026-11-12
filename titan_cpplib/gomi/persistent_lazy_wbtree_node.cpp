/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/gomi/persistent_lazy_wbtree_node.cpp
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

template <class SizeType, class T, class F,
        T (*op)(T, T),
        T (*mapping)(F, T),
        F (*composition)(F, F),
        T (*e)(),
        F (*id)()>
class PersistentLazyWBTree {
    using Index = conditional_t<(sizeof(SizeType) > 4), uint32_t, typename make_unsigned<SizeType>::type>;
    static constexpr unsigned index_bits = sizeof(SizeType) > 4 ? 31 : numeric_limits<SizeType>::digits;
public:
    struct MemoryAllocator {
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
        struct Data {
            T key, data;
            F lazy;

            /// @brief オブジェクトを構築する / O(1)
            Data() = default;

            /// @brief オブジェクトを構築する / O(1)
            Data(const T& k, const T& d, const F& f)
                : key(k), data(d), lazy(f) {}
        };
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

            /// @brief 領域の所有権を移動する / O(N)
            BlockVector& operator=(BlockVector&& other) noexcept {
                if (this != &other) {
                    clear_storage();
                    blocks_ = move(other.blocks_);
                    size_ = exchange(other.size_, 0);
                    other.blocks_.clear();
                }
                return *this;
            }

            /// @brief 構築済み要素と領域を破棄する / O(N)
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

            /// @brief N ノード分の容量を確保する / O(N)
            void reserve(size_t n) {
                const size_t required = n / block_size + (n % block_size != 0);
                blocks_.reserve(required);
                while (blocks_.size() < required) add_block();
            }

            template <class... Args>

            /// @brief 末尾に要素を構築する / 償却 O(1)
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

        struct Slot {
            Node tree;
            Data data;

            /// @brief オブジェクトを構築する / O(1)
            Slot(SizeType l, SizeType r, SizeType s, const T& key,
                 const T& aggregate, const F& lazy, int8_t rev)
                : tree(l, r, s), data(key, aggregate, lazy) { tree.rev = rev != 0; }
        };
        using Pool = BlockVector<Slot, 16>;
        Pool pool;

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

            /// @brief N ノード分の容量を確保する / O(N)
            void reserve(size_t n) { pool->reserve(n); }
        };
        View<Node, &Slot::tree> tree;
        View<Data, &Slot::data> data;
        size_t ptr, cap;

        /// @brief オブジェクトを構築する / O(1)
        MemoryAllocator() : tree{&pool}, data{&pool}, ptr(1), cap(0) {
            pool.emplace_back(0, 0, 0, T{}, T{}, F{}, 0);
            cap = pool.capacity();
        }

        /// @brief オブジェクトを構築する / O(1)
        MemoryAllocator(MemoryAllocator&& other) noexcept

            : pool(move(other.pool)), tree{&pool}, data{&pool},
              ptr(other.ptr), cap(other.cap) {}

        /// @brief 領域の所有権を移動する / O(N)
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
                pool[ptr].data = {key, key, f};
            } else {
                try {
                    pool.emplace_back(0, 0, 1, key, key, f, 0);
                } catch (...) {
                    cap = pool.capacity();
                    throw;
                }
                cap = pool.capacity();
            }
            return ptr++;
        }

        /// @brief N ノード分の容量を確保する / O(N)
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

    static MemoryAllocator ma;

private:
    using PLTM = PersistentLazyWBTree<SizeType, T, F, op, mapping, composition, e, id>;
    static constexpr int DELTA = 3;
    static constexpr int GAMMA = 2;
    Index root;

    inline SizeType weight_right(SizeType node) const {
        return ma.pool[node].tree.size - ma.pool[node].tree.left_size + (node == 0);
    }

    inline SizeType weight_left(SizeType node) const {
        return ma.pool[node].tree.left_size + 1;
    }

    inline SizeType weight(SizeType node) const {
        return ma.pool[node].tree.size + 1;
    }

    void update(SizeType node) {
        ma.pool[node].tree.left_size = ma.pool[ma.pool[node].tree.left].tree.size;
        ma.pool[node].tree.size = 1 + ma.pool[node].tree.left_size + ma.pool[ma.pool[node].tree.right].tree.size;
        ma.pool[node].data.data = ma.pool[node].data.key;
        if (ma.pool[node].tree.left) ma.pool[node].data.data = op(ma.pool[ma.pool[node].tree.left].data.data, ma.pool[node].data.key);
        if (ma.pool[node].tree.right) ma.pool[node].data.data = op(ma.pool[node].data.data, ma.pool[ma.pool[node].tree.right].data.data);
    }

    void push(SizeType node, F f) {
        ma.pool[node].data.key = mapping(f, ma.pool[node].data.key);
        ma.pool[node].data.data = mapping(f, ma.pool[node].data.data);
        ma.pool[node].data.lazy = composition(f, ma.pool[node].data.lazy);
    }

    SizeType own(SizeType node, size_t first) {
        return node && static_cast<size_t>(node) < first ? ma.copy(node) : node;
    }

    void propagate(SizeType node, size_t first) {
        assert(static_cast<size_t>(node) >= first);
        const bool rev = ma.pool[node].tree.rev;
        const bool lazy = ma.pool[node].data.lazy != id();
        if (!rev && !lazy) return;
        SizeType l = ma.pool[node].tree.left, r = ma.pool[node].tree.right;
        if (rev) swap(l, r);
        if (l) {
            l = own(l, first);
            if (rev) ma.pool[l].tree.rev ^= 1;
            if (lazy) push(l, ma.pool[node].data.lazy);
        }
        if (r) {
            r = own(r, first);
            if (rev) ma.pool[r].tree.rev ^= 1;
            if (lazy) push(r, ma.pool[node].data.lazy);
        }
        if (rev) ma.pool[node].tree.left_size = ma.pool[node].tree.size - 1 - ma.pool[node].tree.left_size;
        ma.pool[node].tree.left = l;
        ma.pool[node].tree.right = r;
        ma.pool[node].tree.rev = 0;
        ma.pool[node].data.lazy = id();
    }

    void balance_check(SizeType node) const {
        if (!(weight_left(node)*DELTA >= weight_right(node))) {
            assert(false);
        }
        if (!(weight_right(node) * DELTA >= weight_left(node))) {
            assert(false);
        }
    }

    void _build(vector<T> const &a) {
        auto build = [&] (auto &&build, SizeType l, SizeType r) -> SizeType {
            SizeType mid = (l + r) >> 1;
            SizeType node = ma.new_node(a[mid], id());
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

    SizeType balance(SizeType node, size_t first) {
        if (weight_left(node) * DELTA < weight_right(node)) {
            SizeType u = own(ma.pool[node].tree.right, first);
            ma.pool[node].tree.right = u;

            propagate(u, first);
            if (weight_left(u) >= weight_right(u) * GAMMA) {
                SizeType v = own(ma.pool[u].tree.left, first);
                ma.pool[u].tree.left = v;
                propagate(v, first);
                ma.pool[node].tree.right = _rotate_right(u);
            }
            return _rotate_left(node);
        }
        if (weight_right(node) * DELTA < weight_left(node)) {
            SizeType u = own(ma.pool[node].tree.left, first);
            ma.pool[node].tree.left = u;

            propagate(u, first);
            if (weight_right(u) >= weight_left(u) * GAMMA) {
                SizeType v = own(ma.pool[u].tree.right, first);
                ma.pool[u].tree.right = v;
                propagate(v, first);
                ma.pool[node].tree.left = _rotate_left(u);
            }
            return _rotate_right(node);
        }
        return node;
    }

    SizeType _merge_with_root(SizeType l, SizeType pivot, SizeType r, size_t first) {
        if (weight(r) * DELTA < weight(l)) {
            l = own(l, first);

            propagate(l, first);
            ma.pool[l].tree.right = _merge_with_root(ma.pool[l].tree.right, pivot, r, first);

            update(l);
            return balance(l, first);
        }
        if (weight(l) * DELTA < weight(r)) {
            r = own(r, first);

            propagate(r, first);
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

    SizeType insert_node(SizeType node, SizeType k, const T &value, size_t first) {
        if (!node) return ma.new_node(value, id());
        node = own(node, first);

        propagate(node, first);
        SizeType left_size = ma.pool[node].tree.left_size;
        if (k <= left_size) {
            ma.pool[node].tree.left = insert_node(ma.pool[node].tree.left, k, value, first);
        } else {
            ma.pool[node].tree.right = insert_node(ma.pool[node].tree.right, k-left_size-1, value, first);
        }

        update(node);
        return balance(node, first);
    }

    SizeType erase_extreme(SizeType node, T &value, bool rightmost, size_t first) {
        SizeType next = rightmost ? ma.pool[node].tree.right : ma.pool[node].tree.left;
        if (!next && !ma.pool[node].tree.rev && !(ma.pool[node].data.lazy != id())) {
            value = ma.pool[node].data.key;
            return rightmost ? ma.pool[node].tree.left : ma.pool[node].tree.right;
        }
        node = own(node, first);

        propagate(node, first);
        next = rightmost ? ma.pool[node].tree.right : ma.pool[node].tree.left;
        if (!next) {
            value = ma.pool[node].data.key;
            return rightmost ? ma.pool[node].tree.left : ma.pool[node].tree.right;
        }
        next = erase_extreme(next, value, rightmost, first);
        if (rightmost) ma.pool[node].tree.right = next;
        else ma.pool[node].tree.left = next;

        update(node);
        return balance(node, first);
    }

    SizeType erase_node(SizeType node, SizeType k, T &value, size_t first) {
        if (ma.pool[node].tree.size == 1) {
            value = ma.pool[node].data.key;
            return 0;
        }
        node = own(node, first);

        propagate(node, first);
        SizeType left = ma.pool[node].tree.left, right = ma.pool[node].tree.right;
        SizeType left_size = ma.pool[node].tree.left_size;
        if (k < left_size) {
            ma.pool[node].tree.left = erase_node(left, k, value, first);
        } else if (k > left_size) {
            ma.pool[node].tree.right = erase_node(right, k-left_size-1, value, first);
        } else {
            value = ma.pool[node].data.key;
            if (!left) return right;
            if (!right) return left;
            T replacement;
            if (left_size >= ma.pool[node].tree.size-left_size-1) {
                ma.pool[node].tree.left = erase_extreme(left, replacement, true, first);
            } else {
                ma.pool[node].tree.right = erase_extreme(right, replacement, false, first);
            }
            ma.pool[node].data.key = replacement;
        }

        update(node);
        return balance(node, first);
    }

    pair<SizeType, SizeType> extract_right_node(SizeType node, size_t first) {
        node = own(node, first);

        propagate(node, first);
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

        propagate(node, first);
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

        propagate(node, first);
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

        propagate(node, first);
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

        propagate(node, first);
        SizeType lch = ma.pool[node].tree.left, rch = ma.pool[node].tree.right;
        SizeType left_size = ma.pool[node].tree.left_size;
        if (k > left_size) return _split_node_right(rch, k-left_size-1, first);
        SizeType l = _split_node_right(lch, k, first);
        return _merge_with_root(l, node, rch, first);
    }

    SizeType reverse_node(SizeType node, SizeType l, SizeType r, size_t first) {
        if (r-l <= 1) return node;
        node = own(node, first);
        if (l == 0 && r == ma.pool[node].tree.size) {
            ma.pool[node].tree.rev ^= 1;
            return node;
        }

        propagate(node, first);
        SizeType left_size = ma.pool[node].tree.left_size;
        if (r <= left_size) {
            ma.pool[node].tree.left = reverse_node(ma.pool[node].tree.left, l, r, first);

            update(node);
            return node;
        }
        if (l > left_size) {
            ma.pool[node].tree.right = reverse_node(ma.pool[node].tree.right, l-left_size-1, r-left_size-1, first);

            update(node);
            return node;
        }
        auto [prefix, suffix] = _split_node(node, r, first);
        auto [left, middle] = _split_node(prefix, l, first);
        middle = own(middle, first);
        ma.pool[middle].tree.rev ^= 1;
        return _merge_node(_merge_node(left, middle, first), suffix, first);
    }

    static PLTM _new(SizeType root) {
        return PLTM(root);
    }

    PersistentLazyWBTree(SizeType root) : root(root) {}

  public:

    /// @brief 空の列を構築する / O(1)
    PersistentLazyWBTree() : root(0) {}

    /// @brief 配列 a から構築する / O(N)
    PersistentLazyWBTree(const vector<T> &a) { _build(a); }

    /// @brief 末尾に other を連結した列を返す / O(log(N + M))
    PLTM merge(PLTM other) {
        SizeType root = _merge_node(this->root, other.root, ma.ptr);
        return _new(root);
    }

    /// @brief 先頭 k 要素と残りに分割して返す / O(log N)
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

    /// @brief 区間 [l, r) からなる列を返す / O(log N)
    PLTM split_range(SizeType l, SizeType r) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return _new(0);
        const size_t first = ma.ptr;
        SizeType node = _split_node_left(root, r, first);
        return _new(_split_node_right(node, l, first));
    }

    /// @brief 区間 [l, r) に作用を適用した列を返す / O(log N)
    PLTM apply(SizeType l, SizeType r, F f) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r || !(f != id())) return _new(root);
        const size_t first = ma.ptr;
        auto dfs = [&] (auto &&dfs, SizeType node, SizeType l, SizeType r) -> SizeType {
            node = own(node, first);
            if (l == 0 && r == ma.pool[node].tree.size) {
                push(node, f);
                return node;
            }

            propagate(node, first);
            SizeType left_size = ma.pool[node].tree.left_size;
            if (l < left_size) {
                ma.pool[node].tree.left = dfs(dfs, ma.pool[node].tree.left, l, min(r, left_size));
            }
            if (l <= left_size && left_size < r) ma.pool[node].data.key = mapping(f, ma.pool[node].data.key);
            if (left_size+1 < r) {
                SizeType start = l > left_size+1 ? l-left_size-1 : 0;
                ma.pool[node].tree.right = dfs(dfs, ma.pool[node].tree.right, start, r-left_size-1);
            }

            update(node);
            return node;
        };
        return _new(dfs(dfs, root, l, r));
    }

    /// @brief 区間 [l, r) の総積を返す / O(log N)
    T prod(SizeType l, SizeType r) const {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return e();
        auto dfs = [&] (auto &&dfs, SizeType node, SizeType l, SizeType r, F acc, bool rev) -> T {
            if (l == 0 && r == ma.pool[node].tree.size) {
                return acc != id() ? mapping(acc, ma.pool[node].data.data) : ma.pool[node].data.data;
            }
            rev ^= ma.pool[node].tree.rev;
            SizeType lch = rev ? ma.pool[node].tree.right : ma.pool[node].tree.left;
            SizeType rch = rev ? ma.pool[node].tree.left : ma.pool[node].tree.right;
            SizeType left_size = rev ? ma.pool[node].tree.size - 1 - ma.pool[node].tree.left_size : ma.pool[node].tree.left_size;
            F next = ma.pool[node].data.lazy != id() ? composition(acc, ma.pool[node].data.lazy) : acc;
            if (r <= left_size) return dfs(dfs, lch, l, r, next, rev);
            if (l > left_size) return dfs(dfs, rch, l-left_size-1, r-left_size-1, next, rev);
            T result = acc != id() ? mapping(acc, ma.pool[node].data.key) : ma.pool[node].data.key;
            if (l < left_size) result = op(dfs(dfs, lch, l, left_size, next, rev), result);
            if (left_size+1 < r) result = op(result, dfs(dfs, rch, 0, r-left_size-1, next, rev));
            return result;
        };
        return dfs(dfs, root, l, r, id(), false);
    }

    /// @brief 位置 k に値を挿入した列を返す / O(log N)
    PLTM insert(SizeType k, T key) {
        assert(0 <= k && k <= len());
        const size_t first = ma.ptr;
        return _new(insert_node(root, k, key, first));
    }

    /// @brief 位置 k の値を削除した列と削除値を返す / O(log N)
    pair<PLTM, T> pop(SizeType k) {
        assert(0 <= k && k < len());
        const size_t first = ma.ptr;
        T value;
        SizeType result = erase_node(root, k, value, first);
        return {_new(result), value};
    }

    /// @brief 区間 [l, r) を反転した列を返す / O(log N)
    /// @note 総積が反転で変化しない場合に使える
    PLTM reverse(SizeType l, SizeType r) {
        assert(0 <= l && l <= r && r <= len());
        return _new(reverse_node(root, l, r, ma.ptr));
    }

    /// @brief 全要素を配列にして返す / O(N)
    vector<T> tovector() const {
        vector<T> a;
        a.reserve(len());
        auto dfs = [&] (auto &&dfs, SizeType node, F acc, bool rev) -> void {
            if (!node) return;
            rev ^= ma.pool[node].tree.rev;
            SizeType lch = rev ? ma.pool[node].tree.right : ma.pool[node].tree.left;
            SizeType rch = rev ? ma.pool[node].tree.left : ma.pool[node].tree.right;
            F next = ma.pool[node].data.lazy != id() ? composition(acc, ma.pool[node].data.lazy) : acc;
            if (lch) dfs(dfs, lch, next, rev);
            a.emplace_back(acc != id() ? mapping(acc, ma.pool[node].data.key) : ma.pool[node].data.key);
            if (rch) dfs(dfs, rch, next, rev);
        };

        dfs(dfs, root, id(), false);
        return a;
    }

    /// @brief 同じ列を共有するコピーを返す / O(1)
    PLTM copy() const {
        return _new(root);
    }

    /// @brief 位置 k の値を変更した列を返す / O(log N)
    PLTM set(SizeType k, T v) {
        assert(0 <= k && k < len());
        const size_t first = ma.ptr;
        auto dfs = [&] (auto &&dfs, SizeType node, SizeType k) -> SizeType {
            node = own(node, first);

            propagate(node, first);
            SizeType t = ma.pool[node].tree.left_size;
            if (t == k) {
                ma.pool[node].data.key = v;
            } else if (k < t) {
                ma.pool[node].tree.left = dfs(dfs, ma.pool[node].tree.left, k);
            } else {
                ma.pool[node].tree.right = dfs(dfs, ma.pool[node].tree.right, k-t-1);
            }

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
            if (slot.tree.rev) k = slot.tree.size - 1 - k;
            const SizeType left_size = slot.tree.left_size;
            if (k == left_size) {
                return acc != identity ? mapping(acc, slot.data.key) : slot.data.key;
            }
            if (slot.data.lazy != identity) acc = composition(acc, slot.data.lazy);
            using Unsigned = typename make_unsigned<SizeType>::type;
            const Unsigned mask = Unsigned(0) - Unsigned(k > left_size);
            const Unsigned left = static_cast<Unsigned>(slot.tree.left);
            const Unsigned right = static_cast<Unsigned>(slot.tree.right);
            node = static_cast<SizeType>(left ^ ((left ^ right) & mask));
            k -= static_cast<SizeType>(static_cast<Unsigned>(left_size + 1) & mask);
        }
    }

    /// @brief 全要素を標準出力に出力する / O(N)
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

    /// @brief 部分木サイズと平衡条件を検査する / O(N)
    void check() const {
        auto rec = [&] (auto &&rec, SizeType node) -> pair<SizeType, SizeType> {
            SizeType ls = 0, rs = 0;
            SizeType height = 0;
            SizeType h;
            if (ma.pool[node].tree.left) {
                pair<SizeType, SizeType> res = rec(rec, ma.pool[node].tree.left);
                ls = res.first;
                h = res.second;
                height = max(height, h);
            }
            if (ma.pool[node].tree.right) {
                pair<SizeType, SizeType> res = rec(rec, ma.pool[node].tree.right);
                rs = res.first;
                h = res.second;
                height = max(height, h);
            }
            SizeType s = ls + rs + 1;
            assert(s == ma.pool[node].tree.size);
            assert(ls == ma.pool[node].tree.left_size);

            balance_check(node);
            return {s, height+1};
        };
        if (root == 0) return;

        rec(rec, root);
    }

    /// @brief 全要素をストリームへ出力する / O(N)
    friend ostream& operator<<(ostream& os, const PersistentLazyWBTree<SizeType, T, F, op, mapping, composition, e, id> &tree) {
        vector<T> a = tree.tovector();
        os << "[";
        for (size_t i = 0; i < a.size(); ++i) {
            if (i) os << ", ";
            os << a[i];
        }
        os << "]";
        return os;
    }

    /// @brief 木を再構築する / O(N)
    /// @note 同じ領域を共有する他の木は無効になる
    static void rebuild(PLTM &tree) {
        vector<T> a = tree.tovector();
        PLTM::ma.reset();
        tree = PLTM(a);
    }
};

template <class SizeType, class T, class F,
        T (*op)(T, T),
        T (*mapping)(F, T),
        F (*composition)(F, F),
        T (*e)(),
        F (*id)()>
typename PersistentLazyWBTree<SizeType, T, F, op, mapping, composition, e, id>::MemoryAllocator PersistentLazyWBTree<SizeType, T, F, op, mapping, composition, e, id>::ma;

}  // namespace titan23
