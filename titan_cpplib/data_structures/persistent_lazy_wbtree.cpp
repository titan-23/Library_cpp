#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <stack>
#include <memory>
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
    struct MemoeyAllocator {
        vector<SizeType> left, right, size;
        vector<T> keys, data;
        vector<F> lazy;
        vector<short> rev;
        size_t ptr, cap;

        MemoeyAllocator() : ptr(1), cap(1) {
            left.emplace_back(0);
            right.emplace_back(0);
            size.emplace_back(0);
            keys.emplace_back(T{});
            data.emplace_back(T{});
            lazy.emplace_back(F{});
            rev.emplace_back(0);
        }

        SizeType copy(SizeType node) {
            SizeType idx = new_node(keys[node], lazy[node]);
            left[idx] = left[node];
            right[idx] = right[node];
            size[idx] = size[node];
            data[idx] = data[node];
            rev[idx] = rev[node];
            return idx;
        }

        SizeType new_node(T key, F f) {
            if (left.size() > ptr) {
                left[ptr] = 0;
                right[ptr] = 0;
                size[ptr] = 1;
                keys[ptr] = key;
                data[ptr] = key;
                lazy[ptr] = f;
                rev[ptr] = 0;
            } else {
                left.emplace_back(0);
                right.emplace_back(0);
                size.emplace_back(1);
                keys.emplace_back(key);
                data.emplace_back(key);
                lazy.emplace_back(f);
                rev.emplace_back(0);
                if (left.size() >= cap) {
                    cap++;
                }
            }
            ptr++;
            return ptr - 1;
        }

        void reserve(SizeType n) {
            left.reserve(n);
            right.reserve(n);
            keys.reserve(n);
            size.reserve(n);
            data.reserve(n);
            lazy.reserve(n);
            rev.reserve(n);
            cap += n;
        }

        void reset() {
            ptr = 1;
        }

        bool almost_full() const {
            return (double)ptr > max(1.0, cap*0.99);
        }
    };

    static MemoeyAllocator ma;

private:
    using PLTM = PersistentLazyWBTree<SizeType, T, F, op, mapping, composition, e, id>;
    static constexpr int DELTA = 3;
    static constexpr int GAMMA = 2;
    SizeType root;

    SizeType weight_right(SizeType node) const {
        return ma.size[ma.right[node]] + 1;
    }

    SizeType weight_left(SizeType node) const {
        return ma.size[ma.left[node]] + 1;
    }

    void update(SizeType node) {
        ma.size[node] = 1 + ma.size[ma.left[node]] + ma.size[ma.right[node]];
        ma.data[node] = ma.keys[node];
        if (ma.left[node]) ma.data[node] = op(ma.data[ma.left[node]], ma.keys[node]);
        if (ma.right[node]) ma.data[node] = op(ma.data[node], ma.data[ma.right[node]]);
    }

    void propagate(SizeType node) {
        if (ma.rev[node]) {
            SizeType l = ma.left[node] ? ma.copy(ma.left[node]) : 0;
            SizeType r = ma.right[node] ? ma.copy(ma.right[node]) : 0;
            ma.left[node] = r;
            ma.right[node] = l;
            if (ma.left[node]) ma.rev[ma.left[node]] ^= 1;
            if (ma.right[node]) ma.rev[ma.right[node]] ^= 1;
            ma.rev[node] = 0;
        }
        if (ma.lazy[node] != id()) {
            if (ma.left[node]) {
                ma.left[node] = ma.copy(ma.left[node]);
                ma.keys[ma.left[node]] = mapping(ma.lazy[node], ma.keys[ma.left[node]]);
                ma.data[ma.left[node]] = mapping(ma.lazy[node], ma.data[ma.left[node]]);
                ma.lazy[ma.left[node]] = composition(ma.lazy[node], ma.lazy[ma.left[node]]);
            }
            if (ma.right[node]) {
                ma.right[node] = ma.copy(ma.right[node]);
                ma.keys[ma.right[node]] = mapping(ma.lazy[node], ma.keys[ma.right[node]]);
                ma.data[ma.right[node]] = mapping(ma.lazy[node], ma.data[ma.right[node]]);
                ma.lazy[ma.right[node]] = composition(ma.lazy[node], ma.lazy[ma.right[node]]);
            }
            ma.lazy[node] = id();
        }
    }

    void balance_check(SizeType node) const {
        if (!(weight_left(node)*DELTA >= weight_right(node))) {
            cerr << weight_left(node) << ", " << weight_right(node) << endl;
            cerr << "not weight_left()*DELTA >= weight_right()." << endl;
            assert(false);
        }
        if (!(weight_right(node) * DELTA >= weight_left(node))) {
            cerr << weight_left(node) << ", " << weight_right(node) << endl;
            cerr << "not weight_right() * DELTA >= weight_left()." << endl;
            assert(false);
        }
    }

    void _build(vector<T> const &a) {
        auto build = [&] (auto &&build, SizeType l, SizeType r) -> SizeType {
            SizeType mid = (l + r) >> 1;
            SizeType node = ma.new_node(a[mid], id());
            if (l != mid) ma.left[node] = build(build, l, mid);
            if (mid+1 != r) ma.right[node] = build(build, mid+1, r);
            update(node);
            return node;
        };
        root = build(build, 0, (SizeType)a.size());
    }

    SizeType _rotate_right(SizeType node) {
        SizeType u = ma.copy(ma.left[node]);
        ma.left[node] = ma.right[u];
        ma.right[u] = node;
        update(node);
        update(u);
        return u;
    }

    SizeType _rotate_left(SizeType node) {
        SizeType u = ma.copy(ma.right[node]);
        ma.right[node] = ma.left[u];
        ma.left[u] = node;
        update(node);
        update(u);
        return u;
    }

    SizeType _balance_left(SizeType node) {
        propagate(ma.right[node]);
        SizeType u = ma.right[node];
        if (weight_left(ma.right[node]) >= weight_right(ma.right[node]) * GAMMA) {
            propagate(ma.left[u]);
            ma.right[node] = _rotate_right(u);
        }
        u = _rotate_left(node);
        return u;
    }

    SizeType _balance_right(SizeType &node) {
        propagate(ma.left[node]);
        SizeType u = ma.left[node];
        if (weight_right(ma.left[node]) >= weight_left(ma.left[node]) * GAMMA) {
            propagate(ma.right[u]);
            ma.left[node] = _rotate_left(u);
        }
        u = _rotate_right(node);
        return u;
    }

    SizeType weight(SizeType node) const {
        return ma.size[node] + 1;
    }

    SizeType _merge_with_root(SizeType l, SizeType root, SizeType r) {
        if (weight(r) * DELTA < weight(l)) {
            propagate(l);
            l = ma.copy(l);
            ma.right[l] = _merge_with_root(ma.right[l], root, r);
            update(l);
            if (weight(ma.left[l]) * DELTA < weight(ma.right[l])) {
                return _balance_left(l);
            }
            return l;
        } else if (weight(l) * DELTA < weight(r)) {
            propagate(r);
            r = ma.copy(r);
            ma.left[r] = _merge_with_root(l, root, ma.left[r]);
            update(r);
            if (weight(ma.right[r]) * DELTA < weight(ma.left[r])) {
                return _balance_right(r);
            }
            return r;
        }
        root = ma.copy(root);
        ma.left[root] = l;
        ma.right[root] = r;
        update(root);
        return root;
    }

    pair<SizeType, SizeType> _pop_right(SizeType node) {
        return _split_node(node, ma.size[node]-1);
    }

    SizeType _merge_node(SizeType l, SizeType r) {
        if ((!l) && (!r)) { return 0; }
        if (!l) { return ma.copy(r); }
        if (!r) { return ma.copy(l); }
        l = ma.copy(l);
        r = ma.copy(r);
        auto [l_, root_] = _pop_right(l);
        return _merge_with_root(l_, root_, r);
    }

    pair<SizeType, SizeType> _split_node(SizeType node, SizeType k) {
        if (!node) { return {0, 0}; }
        propagate(node);
        SizeType lch = ma.left[node], rch = ma.right[node];
        SizeType tmp = lch ? k-ma.size[lch] : k;
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
        SizeType lch = ma.left[node], rch = ma.right[node];
        SizeType tmp = lch ? k-ma.size[lch] : k;
        if (tmp == 0) {
            return lch;
        } else if (tmp < 0) {
            auto l = _split_node_left(lch, k);
            return l;
        } else {
            auto l = _split_node_left(rch, tmp-1);
            return _merge_with_root(lch, node, l);
        }
    }

    SizeType _split_node_right(SizeType node, SizeType k) {
        if (!node) { return 0; }
        propagate(node);
        SizeType lch = ma.left[node], rch = ma.right[node];
        SizeType tmp = lch ? k-ma.size[lch] : k;
        if (tmp == 0) {
            return _merge_with_root(0, node, rch);
        } else if (tmp < 0) {
            auto r = _split_node_right(lch, k);
            return _merge_with_root(r, node, rch);
        } else {
            auto r = _split_node_right(rch, tmp-1);
            return r;
        }
    }

    PLTM _new(SizeType root) {
        return PLTM(root);
    }

    PersistentLazyWBTree(SizeType root) : root(root) {}

  public:
    PersistentLazyWBTree() : root(0) {}

    PersistentLazyWBTree(vector<T> &a) { _build(a); }

    PLTM merge(PLTM other) {
        SizeType root = _merge_node(this->root, other.root);
        return _new(root);
    }

    pair<PLTM, PLTM> split(SizeType k) {
        auto [l, r] = _split_node(this->root, k);
        return {_new(l), _new(r)};
    }

    PLTM split_left(SizeType k) {
        auto l = _split_node_left(this->root, k);
        return _new(l);
    }

    PLTM split_right(SizeType k) {
        auto r = _split_node_right(this->root, k);
        return _new(r);
    }

    PLTM apply(SizeType l, SizeType r, F f) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return _new(ma.copy(root));
        auto dfs = [&] (auto &&dfs, SizeType node, SizeType left, SizeType right) -> SizeType {
            if (right <= l || r <= left) return node;
            propagate(node);
            SizeType nnode = ma.copy(node);
            if (l <= left && right < r) {
                ma.keys[nnode] = mapping(f, ma.keys[nnode]);
                ma.data[nnode] = mapping(f, ma.data[nnode]);
                ma.lazy[nnode] = composition(f, ma.lazy[nnode]);
                return nnode;
            }
            SizeType lsize = ma.size[ma.left[nnode]];
            if (ma.left[nnode]) ma.left[nnode] = dfs(dfs, ma.left[nnode], left, left+lsize);
            if (l <= left+lsize && left+lsize < r) ma.keys[nnode] = mapping(f, ma.keys[nnode]);
            if (ma.right[nnode]) ma.right[nnode] = dfs(dfs, ma.right[nnode], left+lsize+1, right);
            update(nnode);
            return nnode;
        };
        return _new(dfs(dfs, root, 0, len()));
    }

    T prod(SizeType l, SizeType r) {
        assert(0 <= l && l <= r && r <= len());
        if (l == r) return e();
        auto dfs = [&] (auto &&dfs, SizeType node, SizeType left, SizeType right) -> T {
            if (right <= l || r <= left) return e();
            propagate(node);
            if (l <= left && right < r) return ma.data[node];
            SizeType lsize = ma.size[ma.left[node]];
            T res = e();
            if (ma.left[node]) res = dfs(dfs, ma.left[node], left, left+lsize);
            if (l <= left+lsize && left+lsize < r) res = op(res, ma.keys[node]);
            if (ma.right[node]) res = op(res, dfs(dfs, ma.right[node], left+lsize+1, right));
            return res;
        };
        return dfs(dfs, root, 0, len());
    }

    PLTM insert(SizeType k, T key) {
        assert(0 <= k && k <= len());
        auto [s, t] = _split_node(root, k);
        SizeType new_node = ma.new_node(key, id());
        return _new(_merge_with_root(s, new_node, t));
    }

    pair<PLTM, T> pop(SizeType k) {
        assert(0 <= k && k < len());
        auto [s_, t] = _split_node(this->root, k+1);
        auto [s, tmp] = _pop_right(s_);
        T res = ma.keys[tmp];
        SizeType root = _merge_node(s, t);
        return {_new(root), res};
    }

    PLTM reverse(SizeType l, SizeType r) {
        assert(0 <= l && l <= r && r <= len());
        if (l >= r) return _new(ma.copy(root));
        auto [s_, t] = _split_node(root, r);
        auto [u, s] = _split_node(s_, l);
        ma.rev[s] ^= 1;
        SizeType root = _merge_node(_merge_node(u, s), t);
        return _new(root);
    }

    vector<T> tovector() {
        SizeType node = root;
        stack<SizeType> s;
        vector<T> a;
        a.reserve(len());
        while (!s.empty() || node) {
            if (node) {
                propagate(node);
                s.emplace(node);
                node = ma.left[node];
            } else {
                node = s.top(); s.pop();
                a.emplace_back(ma.keys[node]);
                node = ma.right[node];
            }
        }
        return a;
    }

    PLTM copy() const {
        return _new(copy(root));
    }

    PLTM set(SizeType k, T v) {
        assert(0 <= k && k < len());
        SizeType node = ma.copy(root);
        SizeType root = node;
        SizeType pnode = 0;
        int d = 0;
        stack<SizeType> path = {node};
        while (1) {
            propagate(node);
            SizeType t = ma.size[ma.left[node]];
            if (t == k) {
                node = ma.copy(node);
                ma.keys[node] = v;
                path.emplace(node);
                if (d) ma.left[pnode] = node;
                else ma.right[pnode] = node;
                while (!path.empty()) {
                    update(path.top());
                    path.pop();
                }
                return _new(root);
            }
            pnode = node;
            if (t < k) {
                k -= t + 1;
                node = ma.copy(ma.right[node]);
                d = 0;
            } else {
                d = 1;
                node = ma.copy(ma.left[node]);
            }
            path.emplace_back(node);
            if (d) ma.left[pnode] = node;
            else ma.right[pnode] = node;
        }
    }

    T get(SizeType k) {
        assert(0 <= k && k < len());
        SizeType node = root;
        while (1) {
            propagate(node);
            SizeType t = ma.size[ma.left[node]];
            if (t == k) {
                return ma.keys[node];
            }
            if (t < k) {
                k -= t + 1;
                node = ma.right[node];
            } else {
                node = ma.left[node];
            }
        }
    }

    void print() {
        vector<T> a = tovector();
        cout << "[";
        for (SizeType i = 0; i < (SizeType)a.size()-1; ++i) {
            cout << a[i] << ", ";
        }
        if (!a.empty()) cout << a.back();
        cout << "]" << endl;
    }

    SizeType len() const {
        return ma.size[root];
    }

    void check() const {
        auto rec = [&] (auto &&rec, SizeType node) -> pair<SizeType, SizeType> {
            SizeType ls = 0, rs = 0;
            SizeType height = 0;
            SizeType h;
            if (ma.left[node]) {
                pair<SizeType, SizeType> res = rec(rec, ma.left[node]);
                ls = res.first;
                h = res.second;
                height = max(height, h);
            }
            if (ma.right[node]) {
                pair<SizeType, SizeType> res = rec(rec, ma.right[node]);
                rs = res.first;
                h = res.second;
                height = max(height, h);
            }
            SizeType s = ls + rs + 1;
            assert(s == ma.size[node]);
            cerr << ls << " " << rs << " " << endl;
            balance_check(node);
            return {s, height+1};
        };
        if (root == 0) return;
        auto [_, h] = rec(rec, root);
        cerr << PRINT_GREEN << "OK : height=" << h << PRINT_NONE << endl;
    }

    friend ostream& operator<<(ostream& os, PersistentLazyWBTree<SizeType, T, F, op, mapping, composition, e, id> &tree) {
        vector<T> a = tree.tovector();
        os << "[";
        for (SizeType i = 0; i < (SizeType)a.size()-1; ++i) {
            os << a[i] << ", ";
        }
        if (!a.empty()) os << a.back();
        os << "]";
        return os;
    }
};

template <class SizeType, class T, class F,
        T (*op)(T, T),
        T (*mapping)(F, T),
        F (*composition)(F, F),
        T (*e)(),
        F (*id)()>
typename PersistentLazyWBTree<SizeType, T, F, op, mapping, composition, e, id>::MemoeyAllocator PersistentLazyWBTree<SizeType, T, F, op, mapping, composition, e, id>::ma;

template <class SizeType, class T, class F,
        T (*op)(T, T),
        T (*mapping)(F, T),
        F (*composition)(F, F),
        T (*e)(),
        F (*id)()>
void rebuild_pltm(PersistentLazyWBTree<SizeType, T, F, op, mapping, composition, e, id> &tree) {
    vector<T> a = tree.tovector();
    PersistentLazyWBTree<SizeType, T, F, op, mapping, composition, e, id>::ma.reset();
    tree = PersistentLazyWBTree<SizeType, T, F, op, mapping, composition, e, id>(a);
}

}  // namespace titan23
