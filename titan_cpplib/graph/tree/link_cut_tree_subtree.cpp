/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/tree/link_cut_tree_subtree.cpp
#pragma once

#include <cassert>
#include <utility>
#include <vector>
using namespace std;

// LinkCutTreeSubtree
// op と inv は可換群をなすこと（群演算を O(1) として、各操作は償却 O(log n)）。
// get は O(1)、構築・オブジェクトのコピー・メモリは O(n)。
// 単一操作の最悪時間の保証ではない。
namespace titan23 {

template <class T, T (*op)(T, T), T (*e)(), T (*inv)(T)>
class LinkCutTreeSubtree {
private:
    struct Node {
        int l, r, p, sz, vsz, asz;
        bool rev;
        // sub includes key and all virtual children; data excludes virtual children.
        T key, data, sub, all;

        Node(const T &v, int nil, int count = 1) :
                l(nil), r(nil), p(nil), sz(count), vsz(0), asz(count), rev(false),
                key(v), data(v), sub(v), all(v) {}
    };

    int n;
    vector<Node> pool;

    bool _is_root(int v) const {
        int p = pool[v].p;
        return p == n || (pool[p].l != v && pool[p].r != v);
    }

    void _toggle(int v) {
        if (v == n) return;
        swap(pool[v].l, pool[v].r);
        pool[v].rev = !pool[v].rev;
    }

    void _push(int v) {
        if (!pool[v].rev) return;
        _toggle(pool[v].l);
        _toggle(pool[v].r);
        pool[v].rev = false;
    }

    void _pull(int v) {
        Node &x = pool[v];
        const Node &l = pool[x.l], &r = pool[x.r];
        x.sz = 1 + l.sz + r.sz;
        x.asz = 1 + x.vsz + l.asz + r.asz;
        x.data = op(l.data, op(x.key, r.data));
        x.all = op(l.all, op(x.sub, r.all));
    }

    // p is the root of its auxiliary tree; g is its path parent.
    void _rotate_root(int v, int p, int g) {
        Node &x = pool[v], &y = pool[p];
        bool d = y.r == v;
        int c = d ? x.l : x.r;
        x.p = g;
        if (d) {
            y.r = c;
            x.l = p;
        } else {
            y.l = c;
            x.r = p;
        }
        if (c != n) pool[c].p = p;
        y.p = v;
        _pull(p);
    }

    void _rotate2(int v, int p, int g) {
        Node &x = pool[v], &y = pool[p], &z = pool[g];
        int up = z.p;
        if (up != n) {
            if (pool[up].l == g) pool[up].l = v;
            else if (pool[up].r == g) pool[up].r = v;
        }
        x.p = up;
        bool a = y.r == v, b = z.r == p;
        if (a == b) {
            if (a) {
                int c = y.l, d = x.l;
                z.r = c;
                if (c != n) pool[c].p = g;
                y.r = d;
                if (d != n) pool[d].p = p;
                x.l = p;
                y.l = g;
            } else {
                int c = x.r, d = y.r;
                y.l = c;
                if (c != n) pool[c].p = p;
                z.l = d;
                if (d != n) pool[d].p = g;
                x.r = p;
                y.r = g;
            }
            y.p = v;
            z.p = p;
            _pull(g);
            _pull(p);
        } else {
            int c = x.l, d = x.r;
            if (a) {
                y.r = c;
                if (c != n) pool[c].p = p;
                z.l = d;
                if (d != n) pool[d].p = g;
                x.l = p;
                x.r = g;
            } else {
                z.r = c;
                if (c != n) pool[c].p = g;
                y.l = d;
                if (d != n) pool[d].p = p;
                x.l = g;
                x.r = p;
            }
            y.p = v;
            z.p = v;
            _pull(p);
            _pull(g);
        }
    }

    // Only reversal is lazy. A pending reversal above g mirrors the whole
    // rotated subtree, so pushing g, p, v locally is sufficient. This performs
    // the usual zig/zig-zig/zig-zag steps without walking the ancestor path twice.
    template<bool PULL_ROOT = true>
    void _splay(int v) {
        if (_is_root(v)) {
            _push(v);
            return;
        }
        do {
            int p = pool[v].p;
            int g = pool[p].p;
            if (!_is_root(p)) {
                _push(g);
                _push(p);
                _push(v);
                _rotate2(v, p, g);
            } else {
                _push(p);
                _push(v);
                _rotate_root(v, p, g);
                break;
            }
        } while (!_is_root(v));
        if constexpr (PULL_ROOT) _pull(v);
    }

    int _expose(int v) {
        int last = n;
        for (int x = v; x != n; x = pool[x].p) {
            _splay<false>(x);
            int r = pool[x].r;
            if (r != n) {
                pool[x].sub = op(pool[x].sub, pool[r].all);
                pool[x].vsz += pool[r].asz;
            }
            if (last != n) {
                pool[x].sub = op(pool[x].sub, inv(pool[last].all));
                pool[x].vsz -= pool[last].asz;
                pool[last].p = x;
            }
            pool[x].r = last;
            _pull(x);
            last = x;
        }
        _splay(v);
        return last;
    }

    void _link(int c, int p) {
        // c is a represented-tree root: splaying it already leaves no left child.
        // Its preferred descendants may stay on the right when attaching to p.
        _splay(c);
        _expose(p);
        pool[c].p = p;
        pool[p].r = c;
        _pull(p);
    }

    void _cut(int c) {
        _expose(c);
        int p = pool[c].l;
        assert(p != n);
        pool[c].l = n;
        pool[p].p = n;
        _pull(c);
    }

    int _root(int v) {
        _expose(v);
        _push(v);
        while (pool[v].l != n) {
            v = pool[v].l;
            _push(v);
        }
        _splay(v);
        return v;
    }

    void _evert(int v) {
        _expose(v);
        _toggle(v);
    }

public:
    LinkCutTreeSubtree() : n(0) {
        pool.emplace_back(e(), n, 0);
    }

    LinkCutTreeSubtree(int size) : n(size) {
        assert(size >= 0);
        pool.reserve(size + 1);
        for (int i = 0; i < size; ++i) pool.emplace_back(e(), n);
        pool.emplace_back(e(), n, 0);
    }

    LinkCutTreeSubtree(const vector<T> &a) : n(int(a.size())) {
        pool.reserve(a.size() + 1);
        for (const T &v : a) pool.emplace_back(v, n);
        pool.emplace_back(e(), n, 0);
    }

    int expose(int v) {
        assert(0 <= v && v < n);
        return _expose(v);
    }

    // u と v は同じ連結成分に属すること
    // root を指定する場合は root も同じ連結成分に属すること
    int lca(int u, int v, int root = -1) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        if (root != -1) {
            assert(0 <= root && root < n);
            evert(root);
        }
        expose(u);
        return expose(v);
    }

    // c は表現木の根で p とは異なる連結成分に属すること
    void link(int c, int p) {
        assert(0 <= c && c < n && 0 <= p && p < n);
        _link(c, p);
    }

    // c は表現木の根でないこと
    void cut(int c) {
        assert(0 <= c && c < n);
        _cut(c);
    }

    int root(int v) {
        assert(0 <= v && v < n);
        return _root(v);
    }

    bool same(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        if (u == v) return true;
        _expose(u);
        _expose(v);
        return pool[u].p != n;
    }

    void evert(int v) {
        assert(0 <= v && v < n);
        _evert(v);
    }

    // u と v は同じ連結成分に属すること
    T path_prod(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        _evert(u);
        _expose(v);
        return pool[v].data;
    }

    // root と v は同じ連結成分に属すること
    T subtree_prod(int root, int v) {
        assert(0 <= root && root < n && 0 <= v && v < n);
        _evert(root);
        _expose(v);
        return pool[v].sub;
    }

    T component_prod(int v) {
        assert(0 <= v && v < n);
        _expose(v);
        return pool[v].all;
    }

    bool merge(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        _evert(u);
        _expose(v);
        // After exposing v, u has a parent iff the distinct vertices are connected.
        if (u == v || pool[u].p != n) return false;
        pool[u].p = v;
        pool[v].r = u;
        _pull(v);
        return true;
    }

    // u と v の間に辺があること
    void split(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        _evert(u);
        _cut(v);
    }

    T get(int v) const {
        assert(0 <= v && v < n);
        return pool[v].key;
    }

    void set(int v, T key) {
        assert(0 <= v && v < n);
        _expose(v);
        pool[v].sub = pool[v].vsz ? op(pool[v].sub, op(inv(pool[v].key), key)) : key;
        pool[v].key = key;
        _pull(v);
    }

    // u と v は同じ連結成分に属すること
    int path_length(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        _evert(u);
        _expose(v);
        return pool[v].sz;
    }

    // u と v は同じ連結成分に属すること
    int path_kth_elm(int u, int v, int k) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        _evert(u);
        _expose(v);
        if (k < 0 || pool[v].sz <= k) return -1;
        int x = v;
        while (true) {
            _push(x);
            int l = pool[x].l;
            int sz = pool[l].sz;
            if (k == sz) {
                _splay(x);
                return x;
            }
            if (k < sz) {
                x = l;
            } else {
                k -= sz + 1;
                x = pool[x].r;
            }
        }
    }

    int subtree_size(int root, int v) {
        assert(0 <= root && root < n && 0 <= v && v < n);
        _evert(root);
        _expose(v);
        return 1 + pool[v].vsz;
    }

    int component_size(int v) {
        assert(0 <= v && v < n);
        _expose(v);
        return pool[v].asz;
    }
};
}  // namespace titan23
