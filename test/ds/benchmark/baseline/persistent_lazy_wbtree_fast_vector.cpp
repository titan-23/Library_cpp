/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/persistent_lazy_wbtree_fast.cpp
#pragma once

#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <stack>
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
// Drop-in alternative to persistent_lazy_wbtree.cpp; include one implementation.
// Updates never mutate older nodes. During one operation, newly allocated nodes
// have a single owning occurrence and can be reused by split/join/propagation.
// reverse() requires a reversal-invariant aggregate; reset/rebuild invalidates
// every other version using the same allocator specialization.
class PersistentLazyWBTree {
public:
    struct MemoryAllocator {

        // #pragma pack(push, 1)
        struct Node {
            SizeType left, right, size;
            Node() : left(0), right(0), size(0) {}
            Node(SizeType l, SizeType r, SizeType s) : left(l), right(r), size(s) {}
        };
        // #pragma pack(pop)

        // #pragma pack(push, 1)
        struct Data {
            T key, data; F lazy; int8_t rev;
            Data() : rev(0) {}
            Data(const T &k, const T &d, const F &l, int8_t r) : key(k), data(d), lazy(l), rev(r) {}
        };
        // #pragma pack(pop)

        vector<Node> tree;
        vector<Data> data;
        size_t ptr, cap;

        MemoryAllocator() : ptr(1), cap(1) {
            tree.emplace_back(0, 0, 0);
            data.emplace_back(T{}, T{}, F{}, 0);
        }

        SizeType copy(SizeType node) {
            if (!node) return 0;
            if (tree.size() > ptr) {
                tree[ptr] = tree[node];
                data[ptr] = data[node];
            } else {
                // push_back permits the source to be an element of the vector.
                tree.push_back(tree[node]);
                data.push_back(data[node]);
                cap = min(tree.capacity(), data.capacity());
            }
            return ptr++;
        }

        SizeType new_node(const T key, const F f) {
            if (tree.size() > ptr) {
                tree[ptr] = {0, 0, 1};
                data[ptr] = {key, key, f, 0};
            } else {
                tree.emplace_back(0, 0, 1);
                data.emplace_back(key, key, f, 0);
                cap = min(tree.capacity(), data.capacity());
            }
            ptr++;
            return ptr - 1;
        }

        void reserve(SizeType n) {
            tree.reserve(n);
            data.reserve(n);
            cap = min(tree.capacity(), data.capacity());
        }

        void reset() {
            ptr = 1;
        }

        bool almost_full() const {
            return (double)ptr > max(1.0, cap*0.99);
        }
    };

    static MemoryAllocator ma;

private:
    using PLTM = PersistentLazyWBTree<SizeType, T, F, op, mapping, composition, e, id>;
    static constexpr int DELTA = 3;
    static constexpr int GAMMA = 2;
    SizeType root;

    inline SizeType weight_right(SizeType node) const {
        return ma.tree[ma.tree[node].right].size + 1;
    }

    inline SizeType weight_left(SizeType node) const {
        return ma.tree[ma.tree[node].left].size + 1;
    }

    inline SizeType weight(SizeType node) const {
        return ma.tree[node].size + 1;
    }

    void update(SizeType node) {
        ma.tree[node].size = 1 + ma.tree[ma.tree[node].left].size + ma.tree[ma.tree[node].right].size;
        ma.data[node].data = ma.data[node].key;
        if (ma.tree[node].left) ma.data[node].data = op(ma.data[ma.tree[node].left].data, ma.data[node].key);
        if (ma.tree[node].right) ma.data[node].data = op(ma.data[node].data, ma.data[ma.tree[node].right].data);
    }

    void push(SizeType node, F f) {
        ma.data[node].key = mapping(f, ma.data[node].key);
        ma.data[node].data = mapping(f, ma.data[node].data);
        ma.data[node].lazy = composition(f, ma.data[node].lazy);
    }

    SizeType own(SizeType node, size_t first) {
        return node && static_cast<size_t>(node) < first ? ma.copy(node) : node;
    }

    void propagate(SizeType node, size_t first) {
        assert(static_cast<size_t>(node) >= first);
        const bool rev = ma.data[node].rev;
        const bool lazy = ma.data[node].lazy != id();
        if (!rev && !lazy) return;
        SizeType l = ma.tree[node].left, r = ma.tree[node].right;
        if (rev) swap(l, r);
        if (l) {
            l = own(l, first);
            if (rev) ma.data[l].rev ^= 1;
            if (lazy) push(l, ma.data[node].lazy);
        }
        if (r) {
            r = own(r, first);
            if (rev) ma.data[r].rev ^= 1;
            if (lazy) push(r, ma.data[node].lazy);
        }
        ma.tree[node].left = l;
        ma.tree[node].right = r;
        ma.data[node].rev = 0;
        ma.data[node].lazy = id();
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
            if (l != mid) ma.tree[node].left = build(build, l, mid);
            if (mid+1 != r) ma.tree[node].right = build(build, mid+1, r);
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
        SizeType u = ma.tree[node].left;
        ma.tree[node].left = ma.tree[u].right;
        ma.tree[u].right = node;
        update(node);
        update(u);
        return u;
    }

    SizeType _rotate_left(SizeType node) {
        SizeType u = ma.tree[node].right;
        ma.tree[node].right = ma.tree[u].left;
        ma.tree[u].left = node;
        update(node);
        update(u);
        return u;
    }

    // node is owned and normalized. Children may still belong to old versions.
    SizeType balance(SizeType node, size_t first) {
        if (weight_left(node) * DELTA < weight_right(node)) {
            SizeType u = own(ma.tree[node].right, first);
            ma.tree[node].right = u;
            propagate(u, first);
            if (weight_left(u) >= weight_right(u) * GAMMA) {
                SizeType v = own(ma.tree[u].left, first);
                ma.tree[u].left = v;
                propagate(v, first);
                ma.tree[node].right = _rotate_right(u);
            }
            return _rotate_left(node);
        }
        if (weight_right(node) * DELTA < weight_left(node)) {
            SizeType u = own(ma.tree[node].left, first);
            ma.tree[node].left = u;
            propagate(u, first);
            if (weight_right(u) >= weight_left(u) * GAMMA) {
                SizeType v = own(ma.tree[u].right, first);
                ma.tree[u].right = v;
                propagate(v, first);
                ma.tree[node].left = _rotate_left(u);
            }
            return _rotate_right(node);
        }
        return node;
    }

    SizeType _merge_with_root(SizeType l, SizeType pivot, SizeType r, size_t first) {
        if (weight(r) * DELTA < weight(l)) {
            l = own(l, first);
            propagate(l, first);
            ma.tree[l].right = _merge_with_root(ma.tree[l].right, pivot, r, first);
            update(l);
            return balance(l, first);
        }
        if (weight(l) * DELTA < weight(r)) {
            r = own(r, first);
            propagate(r, first);
            ma.tree[r].left = _merge_with_root(l, pivot, ma.tree[r].left, first);
            update(r);
            return balance(r, first);
        }
        pivot = own(pivot, first);
        ma.tree[pivot].left = l;
        ma.tree[pivot].right = r;
        update(pivot);
        return pivot;
    }

    // Internal helpers for insertion/deletion with one copied search path.
    // The caller captures first = ma.ptr once for the entire public operation.
    // own()/propagate()/balance() follow the ownership-frontier contract.
    // balance() is called after update() and may normalize/copy heavy children.
    SizeType insert_node(SizeType node, SizeType k, const T &value, size_t first) {
        if (!node) return ma.new_node(value, id());
        node = own(node, first);
        propagate(node, first);
        SizeType left_size = ma.tree[ma.tree[node].left].size;
        if (k <= left_size) {
            ma.tree[node].left = insert_node(ma.tree[node].left, k, value, first);
        } else {
            ma.tree[node].right = insert_node(ma.tree[node].right, k-left_size-1, value, first);
        }
        update(node);
        return balance(node, first);
    }

    // Remove an extreme value without allocating a detached singleton node.
    // An old clean root at the endpoint can be omitted without copying it.
    SizeType erase_extreme(SizeType node, T &value, bool rightmost, size_t first) {
        SizeType next = rightmost ? ma.tree[node].right : ma.tree[node].left;
        if (!next && !ma.data[node].rev && !(ma.data[node].lazy != id())) {
            value = ma.data[node].key;
            return rightmost ? ma.tree[node].left : ma.tree[node].right;
        }
        node = own(node, first);
        propagate(node, first);
        next = rightmost ? ma.tree[node].right : ma.tree[node].left;
        if (!next) {
            value = ma.data[node].key;
            return rightmost ? ma.tree[node].left : ma.tree[node].right;
        }
        next = erase_extreme(next, value, rightmost, first);
        if (rightmost) ma.tree[node].right = next;
        else ma.tree[node].left = next;
        update(node);
        return balance(node, first);
    }

    SizeType erase_node(SizeType node, SizeType k, T &value, size_t first) {
        // A leaf's key already contains its own pending action; no children
        // remain to receive lazy/reversal flags, and no copy is necessary.
        if (ma.tree[node].size == 1) {
            value = ma.data[node].key;
            return 0;
        }
        node = own(node, first);
        propagate(node, first);
        SizeType left = ma.tree[node].left, right = ma.tree[node].right;
        SizeType left_size = ma.tree[left].size;
        if (k < left_size) {
            ma.tree[node].left = erase_node(left, k, value, first);
        } else if (k > left_size) {
            ma.tree[node].right = erase_node(right, k-left_size-1, value, first);
        } else {
            value = ma.data[node].key;
            if (!left) return right;
            if (!right) return left;
            // Remove the replacement from the heavier side to avoid
            // increasing the imbalance at this node.
            T replacement;
            if (left_size >= ma.tree[right].size) {
                ma.tree[node].left = erase_extreme(left, replacement, true, first);
            } else {
                ma.tree[node].right = erase_extreme(right, replacement, false, first);
            }
            ma.data[node].key = replacement;
        }
        update(node);
        return balance(node, first);
    }

    // Merge can use this specialized extraction instead of split(size-1).
    // It returns a consumed, normalized singleton pivot, ready for join().
    pair<SizeType, SizeType> extract_right_node(SizeType node, size_t first) {
        node = own(node, first);
        propagate(node, first);
        if (!ma.tree[node].right) {
            SizeType rest = ma.tree[node].left;
            ma.tree[node].left = 0;
            update(node);
            return {rest, node};
        }
        auto result = extract_right_node(ma.tree[node].right, first);
        ma.tree[node].right = result.first;
        update(node);
        return {balance(node, first), result.second};
    }

    pair<SizeType, SizeType> _pop_right(SizeType node, size_t first) {
        return extract_right_node(node, first);
    }

    SizeType _merge_node(SizeType l, SizeType r, size_t first) {
        if (!l) return r;
        if (!r) return l;
        auto [rest, pivot] = _pop_right(l, first);
        return _merge_with_root(rest, pivot, r, first);
    }

    pair<SizeType, SizeType> _split_node(SizeType node, SizeType k, size_t first) {
        if (!k) return {0, node};
        if (k == ma.tree[node].size) return {node, 0};
        node = own(node, first);
        propagate(node, first);
        SizeType lch = ma.tree[node].left, rch = ma.tree[node].right;
        SizeType left_size = ma.tree[lch].size;
        if (k <= left_size) {
            auto [l, r] = _split_node(lch, k, first);
            return {l, _merge_with_root(r, node, rch, first)};
        }
        auto [l, r] = _split_node(rch, k-left_size-1, first);
        return {_merge_with_root(lch, node, l, first), r};
    }

    SizeType _split_node_left(SizeType node, SizeType k, size_t first) {
        if (!k) return 0;
        if (k == ma.tree[node].size) return node;
        node = own(node, first);
        propagate(node, first);
        SizeType lch = ma.tree[node].left, rch = ma.tree[node].right;
        SizeType left_size = ma.tree[lch].size;
        if (k <= left_size) return _split_node_left(lch, k, first);
        SizeType r = _split_node_left(rch, k-left_size-1, first);
        return _merge_with_root(lch, node, r, first);
    }

    SizeType _split_node_right(SizeType node, SizeType k, size_t first) {
        if (!k) return node;
        if (k == ma.tree[node].size) return 0;
        node = own(node, first);
        propagate(node, first);
        SizeType lch = ma.tree[node].left, rch = ma.tree[node].right;
        SizeType left_size = ma.tree[lch].size;
        if (k > left_size) return _split_node_right(rch, k-left_size-1, first);
        SizeType l = _split_node_right(lch, k, first);
        return _merge_with_root(l, node, rch, first);
    }

    static PLTM _new(SizeType root) {
        return PLTM(root);
    }

    PersistentLazyWBTree(SizeType root) : root(root) {}

  public:
    PersistentLazyWBTree() : root(0) {}

    PersistentLazyWBTree(const vector<T> &a) { _build(a); }

    PLTM merge(PLTM other) {
        SizeType root = _merge_node(this->root, other.root, ma.ptr);
        return _new(root);
    }

    pair<PLTM, PLTM> split(SizeType k) {
        assert(0 <= k && k <= len());
        if (k == 0) return {_new(0), _new(root)};
        if (k == len()) return {_new(root), _new(0)};
        auto [l, r] = _split_node(this->root, k, ma.ptr);
        return {_new(l), _new(r)};
    }

    PLTM split_left(SizeType k) {
        assert(0 <= k && k <= len());
        if (k == 0) return _new(0);
        if (k == len()) return _new(root);
        SizeType l = _split_node_left(this->root, k, ma.ptr);
        return _new(l);
    }

    PLTM split_right(SizeType k) {
        assert(0 <= k && k <= len());
        if (k == 0) return _new(root);
        if (k == len()) return _new(0);
        SizeType r = _split_node_right(this->root, k, ma.ptr);
        return _new(r);
    }

    PLTM split_range(SizeType l, SizeType r) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return _new(0);
        const size_t first = ma.ptr;
        SizeType node = _split_node_left(root, r, first);
        return _new(_split_node_right(node, l, first));
    }

    PLTM apply(SizeType l, SizeType r, F f) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r || !(f != id())) return _new(root);
        const size_t first = ma.ptr;
        auto dfs = [&] (auto &&dfs, SizeType node, SizeType left, SizeType right) -> SizeType {
            if (right <= l || r <= left) return node;
            if (l <= left && right <= r) {
                SizeType nnode = own(node, first);
                push(nnode, f);
                return nnode;
            }
            SizeType nnode = own(node, first);
            propagate(nnode, first);
            SizeType lsize = ma.tree[ma.tree[nnode].left].size;
            if (ma.tree[nnode].left) ma.tree[nnode].left = dfs(dfs, ma.tree[nnode].left, left, left+lsize);
            if (l <= left+lsize && left+lsize < r) ma.data[nnode].key = mapping(f, ma.data[nnode].key);
            if (ma.tree[nnode].right) ma.tree[nnode].right = dfs(dfs, ma.tree[nnode].right, left+lsize+1, right);
            update(nnode);
            return nnode;
        };
        return _new(dfs(dfs, root, 0, len()));
    }

    T prod(SizeType l, SizeType r) const {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return e();
        auto dfs = [&] (auto &&dfs, SizeType node, SizeType left, SizeType right, F acc, bool rev) -> T {
            if (right <= l || r <= left) return e();
            if (l <= left && right <= r) {
                return acc != id() ? mapping(acc, ma.data[node].data) : ma.data[node].data;
            }
            rev ^= ma.data[node].rev;
            SizeType lch = rev ? ma.tree[node].right : ma.tree[node].left;
            SizeType rch = rev ? ma.tree[node].left : ma.tree[node].right;
            SizeType lsize = ma.tree[lch].size;
            F next = ma.data[node].lazy != id() ? composition(acc, ma.data[node].lazy) : acc;
            T res = e();
            if (lch) res = dfs(dfs, lch, left, left+lsize, next, rev);
            if (l <= left+lsize && left+lsize < r) {
                res = op(res, acc != id() ? mapping(acc, ma.data[node].key) : ma.data[node].key);
            }
            if (rch) res = op(res, dfs(dfs, rch, left+lsize+1, right, next, rev));
            return res;
        };
        return dfs(dfs, root, 0, len(), id(), false);
    }

    PLTM insert(SizeType k, T key) {
        assert(0 <= k && k <= len());
        const size_t first = ma.ptr;
        return _new(insert_node(root, k, key, first));
    }

    pair<PLTM, T> pop(SizeType k) {
        assert(0 <= k && k < len());
        const size_t first = ma.ptr;
        T value;
        SizeType result = erase_node(root, k, value, first);
        return {_new(result), value};
    }

    PLTM reverse(SizeType l, SizeType r) {
        assert(0 <= l && l <= r && r <= len());
        if (r-l <= 1) return _new(root);
        const size_t first = ma.ptr;
        if (l == 0 && r == len()) {
            SizeType node = own(root, first);
            ma.data[node].rev ^= 1;
            return _new(node);
        }
        auto [prefix, suffix] = _split_node(root, r, first);
        auto [left, middle] = _split_node(prefix, l, first);
        middle = own(middle, first);
        ma.data[middle].rev ^= 1;
        return _new(_merge_node(_merge_node(left, middle, first), suffix, first));
    }

    vector<T> tovector() const {
        vector<T> a;
        a.reserve(len());
        auto dfs = [&] (auto &&dfs, SizeType node, F acc, bool rev) -> void {
            if (!node) return;
            rev ^= ma.data[node].rev;
            SizeType lch = rev ? ma.tree[node].right : ma.tree[node].left;
            SizeType rch = rev ? ma.tree[node].left : ma.tree[node].right;
            F next = ma.data[node].lazy != id() ? composition(acc, ma.data[node].lazy) : acc;
            dfs(dfs, lch, next, rev);
            a.emplace_back(acc != id() ? mapping(acc, ma.data[node].key) : ma.data[node].key);
            dfs(dfs, rch, next, rev);
        };
        dfs(dfs, root, id(), false);
        return a;
    }

    PLTM copy() const {
        return _new(root);
    }

    PLTM set(SizeType k, T v) {
        assert(0 <= k && k < len());
        const size_t first = ma.ptr;
        auto dfs = [&] (auto &&dfs, SizeType node, SizeType k) -> SizeType {
            node = own(node, first);
            propagate(node, first);
            SizeType t = ma.tree[ma.tree[node].left].size;
            if (t == k) {
                ma.data[node].key = v;
            } else if (k < t) {
                ma.tree[node].left = dfs(dfs, ma.tree[node].left, k);
            } else {
                ma.tree[node].right = dfs(dfs, ma.tree[node].right, k-t-1);
            }
            update(node);
            return node;
        };
        return _new(dfs(dfs, root, k));
    }

    T get(SizeType k) const {
        assert(0 <= k && k < len());
        SizeType node = root;
        F acc = id();
        bool rev = false;
        while (1) {
            rev ^= ma.data[node].rev;
            SizeType lch = rev ? ma.tree[node].right : ma.tree[node].left;
            SizeType rch = rev ? ma.tree[node].left : ma.tree[node].right;
            SizeType t = ma.tree[lch].size;
            if (t == k) {
                return acc != id() ? mapping(acc, ma.data[node].key) : ma.data[node].key;
            }
            if (ma.data[node].lazy != id()) acc = composition(acc, ma.data[node].lazy);
            if (t < k) {
                k -= t + 1;
                node = rch;
            } else {
                node = lch;
            }
        }
    }

    void print() const {
        vector<T> a = tovector();
        cout << "[";
        for (SizeType i = 0; i < (SizeType)a.size()-1; ++i) {
            cout << a[i] << ", ";
        }
        if (!a.empty()) cout << a.back();
        cout << "]" << endl;
    }

    SizeType len() const {
        return ma.tree[root].size;
    }

    void check() const {
        auto rec = [&] (auto &&rec, SizeType node) -> pair<SizeType, SizeType> {
            SizeType ls = 0, rs = 0;
            SizeType height = 0;
            SizeType h;
            if (ma.tree[node].left) {
                pair<SizeType, SizeType> res = rec(rec, ma.tree[node].left);
                ls = res.first;
                h = res.second;
                height = max(height, h);
            }
            if (ma.tree[node].right) {
                pair<SizeType, SizeType> res = rec(rec, ma.tree[node].right);
                rs = res.first;
                h = res.second;
                height = max(height, h);
            }
            SizeType s = ls + rs + 1;
            assert(s == ma.tree[node].size);
            balance_check(node);
            return {s, height+1};
        };
        if (root == 0) return;
        rec(rec, root);
    }

    friend ostream& operator<<(ostream& os, const PersistentLazyWBTree<SizeType, T, F, op, mapping, composition, e, id> &tree) {
        vector<T> a = tree.tovector();
        os << "[";
        for (SizeType i = 0; i < (SizeType)a.size()-1; ++i) {
            os << a[i] << ", ";
        }
        if (!a.empty()) os << a.back();
        os << "]";
        return os;
    }

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
