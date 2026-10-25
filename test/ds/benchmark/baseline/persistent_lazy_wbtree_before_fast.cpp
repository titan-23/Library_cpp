/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/persistent_lazy_wbtree.cpp
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

    void propagate(SizeType node) {
        const bool rev = ma.data[node].rev;
        const bool lazy = ma.data[node].lazy != id();
        if (!rev && !lazy) return;
        SizeType l = ma.tree[node].left, r = ma.tree[node].right;
        if (rev) swap(l, r);
        if (l) {
            l = ma.copy(l);
            if (rev) ma.data[l].rev ^= 1;
            if (lazy) push(l, ma.data[node].lazy);
        }
        if (r) {
            r = ma.copy(r);
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

    SizeType _rotate_right(SizeType node, bool owned_child = false) {
        SizeType u = owned_child ? ma.tree[node].left : ma.copy(ma.tree[node].left);
        ma.tree[node].left = ma.tree[u].right;
        ma.tree[u].right = node;
        update(node);
        update(u);
        return u;
    }

    SizeType _rotate_left(SizeType node, bool owned_child = false) {
        SizeType u = owned_child ? ma.tree[node].right : ma.copy(ma.tree[node].right);
        ma.tree[node].right = ma.tree[u].left;
        ma.tree[u].left = node;
        update(node);
        update(u);
        return u;
    }

    SizeType _balance_left(SizeType node) {
        propagate(ma.tree[node].right);
        SizeType u = ma.tree[node].right;
        if (weight_left(ma.tree[node].right) >= weight_right(ma.tree[node].right) * GAMMA) {
            propagate(ma.tree[u].left);
            ma.tree[node].right = _rotate_right(u);
        }
        // This child was created by the recursive merge or the inner rotation.
        u = _rotate_left(node, true);
        return u;
    }

    SizeType _balance_right(SizeType node) {
        propagate(ma.tree[node].left);
        SizeType u = ma.tree[node].left;
        if (weight_right(ma.tree[node].left) >= weight_left(ma.tree[node].left) * GAMMA) {
            propagate(ma.tree[u].right);
            ma.tree[node].left = _rotate_left(u);
        }
        u = _rotate_right(node, true);
        return u;
    }

    SizeType _merge_with_root(SizeType l, SizeType root, SizeType r, bool owned_root = false) {
        if (weight(r) * DELTA < weight(l)) {
            propagate(l);
            l = ma.copy(l);
            ma.tree[l].right = _merge_with_root(ma.tree[l].right, root, r, owned_root);
            update(l);
            if (weight(ma.tree[l].left) * DELTA < weight(ma.tree[l].right)) {
                return _balance_left(l);
            }
            return l;
        } else if (weight(l) * DELTA < weight(r)) {
            propagate(r);
            r = ma.copy(r);
            ma.tree[r].left = _merge_with_root(l, root, ma.tree[r].left, owned_root);
            update(r);
            if (weight(ma.tree[r].right) * DELTA < weight(ma.tree[r].left)) {
                return _balance_right(r);
            }
            return r;
        }
        if (!owned_root) root = ma.copy(root);
        ma.tree[root].left = l;
        ma.tree[root].right = r;
        update(root);
        return root;
    }

    pair<SizeType, SizeType> _pop_right(SizeType node) {
        return _split_node(node, ma.tree[node].size-1);
    }

    SizeType _merge_node(SizeType l, SizeType r) {
        if ((!l) && (!r)) { return 0; }
        if (!l) { return r; }
        if (!r) { return l; }
        auto [l_, root_] = _pop_right(l);
        return _merge_with_root(l_, root_, r, true);
    }

    pair<SizeType, SizeType> _split_node(SizeType node, SizeType k) {
        // The nonempty right result has a fresh root, including at k == 0.
        // reverse() and _pop_right() rely on this ownership guarantee.
        if (!node) { return {0, 0}; }
        propagate(node);
        SizeType lch = ma.tree[node].left, rch = ma.tree[node].right;
        SizeType tmp = lch ? k-ma.tree[lch].size : k;
        if (tmp == 0) {
            return {lch, _merge_with_root(0, node, rch)};
        } else if (tmp < 0) {
            auto [l, r] = _split_node(lch, k);
            return {l, _merge_with_root(r, node, rch)};
        } else {
            auto [l, r] = _split_node(rch, tmp-1);
            return {_merge_with_root(lch, node, l), r};
        }
    }

    SizeType _split_node_left(SizeType node, SizeType k) {
        if (!node) { return 0; }
        propagate(node);
        SizeType lch = ma.tree[node].left, rch = ma.tree[node].right;
        SizeType tmp = lch ? k-ma.tree[lch].size : k;
        if (tmp == 0) {
            return lch;
        } else if (tmp < 0) {
            SizeType l = _split_node_left(lch, k);
            return l;
        } else {
            SizeType l = _split_node_left(rch, tmp-1);
            return _merge_with_root(lch, node, l);
        }
    }

    SizeType _split_node_right(SizeType node, SizeType k) {
        if (!node) { return 0; }
        propagate(node);
        SizeType lch = ma.tree[node].left, rch = ma.tree[node].right;
        SizeType tmp = lch ? k-ma.tree[lch].size : k;
        if (tmp == 0) {
            return _merge_with_root(0, node, rch);
        } else if (tmp < 0) {
            SizeType r = _split_node_right(lch, k);
            return _merge_with_root(r, node, rch);
        } else {
            SizeType r = _split_node_right(rch, tmp-1);
            return r;
        }
    }

    static PLTM _new(SizeType root) {
        return PLTM(root);
    }

    PersistentLazyWBTree(SizeType root) : root(root) {}

  public:
    PersistentLazyWBTree() : root(0) {}

    PersistentLazyWBTree(const vector<T> &a) { _build(a); }

    PLTM merge(PLTM other) {
        SizeType root = _merge_node(this->root, other.root);
        return _new(root);
    }

    pair<PLTM, PLTM> split(SizeType k) {
        assert(0 <= k && k <= len());
        if (k == 0) return {_new(0), _new(root)};
        if (k == len()) return {_new(root), _new(0)};
        auto [l, r] = _split_node(this->root, k);
        return {_new(l), _new(r)};
    }

    PLTM split_left(SizeType k) {
        assert(0 <= k && k <= len());
        if (k == 0) return _new(0);
        if (k == len()) return _new(root);
        SizeType l = _split_node_left(this->root, k);
        return _new(l);
    }

    PLTM split_right(SizeType k) {
        assert(0 <= k && k <= len());
        if (k == 0) return _new(root);
        if (k == len()) return _new(0);
        SizeType r = _split_node_right(this->root, k);
        return _new(r);
    }

    PLTM split_range(SizeType l, SizeType r) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return _new(0);
        return split_left(r).split_right(l);
    }

    PLTM apply(SizeType l, SizeType r, F f) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r || !(f != id())) return _new(root);
        auto dfs = [&] (auto &&dfs, SizeType node, SizeType left, SizeType right) -> SizeType {
            if (right <= l || r <= left) return node;
            if (l <= left && right <= r) {
                SizeType nnode = ma.copy(node);
                push(nnode, f);
                return nnode;
            }
            propagate(node);
            SizeType nnode = ma.copy(node);
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
        auto [s, t] = _split_node(root, k);
        SizeType new_node = ma.new_node(key, id());
        return _new(_merge_with_root(s, new_node, t, true));
    }

    pair<PLTM, T> pop(SizeType k) {
        assert(0 <= k && k < len());
        auto [s_, t] = _split_node(this->root, k+1);
        auto [s, tmp] = _pop_right(s_);
        T res = ma.data[tmp].key;
        SizeType root = _merge_node(s, t);
        return {_new(root), res};
    }

    PLTM reverse(SizeType l, SizeType r) {
        assert(0 <= l && l <= r && r <= len());
        if (r-l <= 1) return _new(root);
        if (l == 0 && r == len()) {
            SizeType node = ma.copy(root);
            ma.data[node].rev ^= 1;
            return _new(node);
        }
        auto [s_, t] = _split_node(root, r);
        auto [u, s] = _split_node(s_, l);
        ma.data[s].rev ^= 1;
        SizeType root = _merge_node(_merge_node(u, s), t);
        return _new(root);
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
        auto dfs = [&] (auto &&dfs, SizeType node, SizeType k) -> SizeType {
            propagate(node);
            node = ma.copy(node);
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
        auto [_, h] = rec(rec, root);
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
