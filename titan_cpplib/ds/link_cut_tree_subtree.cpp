/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/link_cut_tree_subtree.cpp
#pragma once

#include <cassert>
#include <utility>
#include <vector>
using namespace std;

// LinkCutTreeSubtree
// op と inv は可換群をなすこと
namespace titan23 {

template <class T, T (*op)(T, T), T (*e)(), T (*inv)(T)>
class LinkCutTreeSubtree {
private:
    struct Node {
        int l, r, p, sz, vsz, asz;
        bool rev;
        int tmp;
        T key, data, vir, all;

        Node(const T &v, int nil, int count = 1) :
                l(nil), r(nil), p(nil), sz(count), vsz(0), asz(count), rev(false), tmp(nil),
                key(v), data(v), vir(e()), all(v) {}
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
        x.all = op(l.all, op(x.key, op(x.vir, r.all)));
    }

    void _push_path(int v) {
        int top = n;
        for (int x = v;; x = pool[x].p) {
            pool[x].tmp = top;
            top = x;
            if (_is_root(x)) break;
        }
        while (top != n) {
            int x = top;
            top = pool[x].tmp;
            _push(x);
        }
    }

    void _rotate(int v) {
        int p = pool[v].p;
        int g = pool[p].p;
        Node &x = pool[v], &y = pool[p];
        bool d = y.r == v;
        int c = d ? x.l : x.r;
        if (!_is_root(p)) {
            if (pool[g].l == p) pool[g].l = v;
            else pool[g].r = v;
        }
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

    template<bool PULL_ROOT = true>
    void _splay(int v) {
        _push_path(v);
        bool moved = false;
        while (!_is_root(v)) {
            if constexpr (PULL_ROOT) moved = true;
            int p = pool[v].p;
            int g = pool[p].p;
            if (!_is_root(p)) {
                if ((pool[p].l == v) == (pool[g].l == p)) _rotate(p);
                else _rotate(v);
            }
            _rotate(v);
        }
        if constexpr (PULL_ROOT) {
            if (moved) _pull(v);
        }
    }

    int _expose(int v) {
        int last = n;
        for (int x = v; x != n; x = pool[x].p) {
            _splay<false>(x);
            int r = pool[x].r;
            if (r != n) {
                pool[x].vir = op(pool[x].vir, pool[r].all);
                pool[x].vsz += pool[r].asz;
            }
            if (last != n) {
                pool[x].vir = op(pool[x].vir, inv(pool[last].all));
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
        _expose(c);
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
        return op(pool[v].key, pool[v].vir);
    }

    T component_prod(int v) {
        assert(0 <= v && v < n);
        _expose(v);
        return pool[v].all;
    }

    bool merge(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        _evert(u);
        if (_root(v) == u) return false;
        _expose(v);
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
