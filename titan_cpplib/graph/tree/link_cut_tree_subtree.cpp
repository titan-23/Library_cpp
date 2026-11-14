/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/tree/link_cut_tree_subtree.cpp
#pragma once

#include <cassert>
#include <utility>
#include <vector>
using namespace std;

namespace titan23 {

/// @brief 部分木と連結成分の集約に対応した Link Cut Tree / 各操作は償却 O(log n)
/// op と inv は可換群をなすこと
template <class T, T (*op)(T, T), T (*e)(), T (*inv)(T)>
class LinkCutTreeSubtree {
private:
    struct Node {
        int l, r, p, sz, vsz, asz;
        bool rev;
        // sub は key と全ての仮想の子を含み、data は仮想の子を含まない
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

    // p は補助木の根で、g はそのパスの親
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

    // 遅延するのは反転だけなので、g, p, v を push すれば足りる
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
        // c は表現木の根なので、splay すると左の子はない
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
    /// @brief 頂点数 0 で構築する
    LinkCutTreeSubtree() : n(0) {
        pool.emplace_back(e(), n, 0);
    }

    /// @brief 頂点数 size で構築する / 各頂点の値は e() / O(n)
    LinkCutTreeSubtree(int size) : n(size) {
        assert(size >= 0);
        pool.reserve(size + 1);
        for (int i = 0; i < size; ++i) pool.emplace_back(e(), n);
        pool.emplace_back(e(), n, 0);
    }

    /// @brief 頂点 i の値を a[i] として構築する / O(n)
    LinkCutTreeSubtree(const vector<T> &a) : n(int(a.size())) {
        pool.reserve(a.size() + 1);
        for (const T &v : a) pool.emplace_back(v, n);
        pool.emplace_back(e(), n, 0);
    }

    /// @brief 根から v へのパスを露出する / expose(u) の直後なら u と v の LCA を返す / 償却 O(log n)
    int expose(int v) {
        assert(0 <= v && v < n);
        return _expose(v);
    }

    /// @brief u と v の LCA を返す / root を指定すると先に root を根にする / 償却 O(log n)
    /// u と v は同じ連結成分に属すること / root を指定する場合は root も同じ連結成分に属すること
    int lca(int u, int v, int root = -1) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        if (root != -1) {
            assert(0 <= root && root < n);
            evert(root);
        }
        expose(u);
        return expose(v);
    }

    /// @brief 根 c を p の子にする / c は表現木の根で p とは異なる連結成分に属すること / 償却 O(log n)
    void link(int c, int p) {
        assert(0 <= c && c < n && 0 <= p && p < n);
        _link(c, p);
    }

    /// @brief c と親の間の辺を切る / c は表現木の根でないこと / 償却 O(log n)
    void cut(int c) {
        assert(0 <= c && c < n);
        _cut(c);
    }

    /// @brief v を含む木の根を返す / 償却 O(log n)
    int root(int v) {
        assert(0 <= v && v < n);
        return _root(v);
    }

    /// @brief u と v が同じ連結成分にあるかを返す / 償却 O(log n)
    bool same(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        if (u == v) return true;
        _expose(u);
        _expose(v);
        return pool[u].p != n;
    }

    /// @brief v を木の根にする / 償却 O(log n)
    void evert(int v) {
        assert(0 <= v && v < n);
        _evert(v);
    }

    /// @brief u から v へのパス上の値を op で集約して返す / u と v は同じ連結成分に属すること / 償却 O(log n)
    T path_prod(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        _evert(u);
        _expose(v);
        return pool[v].data;
    }

    /// @brief root を根としたときの v の部分木の値を op で集約して返す / root と v は同じ連結成分に属すること / 償却 O(log n)
    T subtree_prod(int root, int v) {
        assert(0 <= root && root < n && 0 <= v && v < n);
        _evert(root);
        _expose(v);
        return pool[v].sub;
    }

    /// @brief v を含む連結成分の値を op で集約して返す / 償却 O(log n)
    T component_prod(int v) {
        assert(0 <= v && v < n);
        _expose(v);
        return pool[v].all;
    }

    /// @brief u と v が別の連結成分なら辺 (u, v) を張って true を返す / 同じ成分なら false / 償却 O(log n)
    bool merge(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        _evert(u);
        _expose(v);
        // v を露出した後、異なる 2 頂点が連結なときに限り u に親がある
        if (u == v || pool[u].p != n) return false;
        pool[u].p = v;
        pool[v].r = u;
        _pull(v);
        return true;
    }

    /// @brief u と v の間の辺を切る / u と v の間に辺があること / 償却 O(log n)
    void split(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        _evert(u);
        _cut(v);
    }

    /// @brief 頂点 v の値を返す / O(1)
    T get(int v) const {
        assert(0 <= v && v < n);
        return pool[v].key;
    }

    /// @brief 頂点 v の値を key に変更する / 償却 O(log n)
    void set(int v, T key) {
        assert(0 <= v && v < n);
        _expose(v);
        pool[v].sub = pool[v].vsz ? op(pool[v].sub, op(inv(pool[v].key), key)) : key;
        pool[v].key = key;
        _pull(v);
    }

    /// @brief u から v へのパス上の頂点数を返す / u と v は同じ連結成分に属すること / 償却 O(log n)
    int path_length(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        _evert(u);
        _expose(v);
        return pool[v].sz;
    }

    /// @brief u から v へのパスで k 番目の頂点を返す / 0-indexed / 範囲外なら -1 / 償却 O(log n)
    /// u と v は同じ連結成分に属すること
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

    /// @brief root を根としたときの v の部分木の頂点数を返す / root と v は同じ連結成分に属すること / 償却 O(log n)
    int subtree_size(int root, int v) {
        assert(0 <= root && root < n && 0 <= v && v < n);
        _evert(root);
        _expose(v);
        return 1 + pool[v].vsz;
    }

    /// @brief v を含む連結成分の頂点数を返す / 償却 O(log n)
    int component_size(int v) {
        assert(0 <= v && v < n);
        _expose(v);
        return pool[v].asz;
    }
};
}  // namespace titan23
