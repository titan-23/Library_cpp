/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/tree/link_cut_tree.cpp
#pragma once

#include <cassert>
#include <utility>
#include <vector>
using namespace std;

namespace titan23 {

/// @brief 動的な森を扱う Link Cut Tree / 各操作は償却 O(log n)
class LinkCutTree {
private:
    struct Node {
        int l, r, p, sz;
        bool rev;

        Node(int nil, int count = 1) :
                l(nil), r(nil), p(nil), sz(count), rev(false) {}
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
    // PULL_ROOT=false は直後に v を再計算する呼び出し元だけが使う
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
            pool[x].r = last;
            if (last != n) pool[last].p = x;
            _pull(x);
            last = x;
        }
        _splay(v);
        return last;
    }

public:
    /// @brief 頂点数 0 で構築する
    LinkCutTree() : n(0) {
        pool.emplace_back(n, 0);
    }

    /// @brief 頂点数 size で構築する / O(n)
    LinkCutTree(int size) : n(size) {
        assert(size >= 0);
        pool.reserve(size + 1);
        for (int i = 0; i < size; ++i) pool.emplace_back(n);
        pool.emplace_back(n, 0);
    }

    /// @brief 根から v へのパスを露出する / expose(u) の直後なら u と v の LCA を返す / 償却 O(log n)
    int expose(int v) {
        assert(0 <= v && v < n);
        return _expose(v);
    }

    /// @brief u と v の LCA を返す / root を指定すると先に root を根にする / 償却 O(log n)
    /// u, v と、指定した場合の root は同じ連結成分にあること
    int lca(int u, int v, int root = -1) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        if (root != -1) evert(root);
        _expose(u);
        return _expose(v);
    }

    /// @brief 根 c を p の子にする / c は p と別の連結成分にある表現木の根であること / 償却 O(log n)
    void link(int c, int p) {
        assert(0 <= c && c < n && 0 <= p && p < n);
        _splay(c);
        assert(pool[c].l == n && pool[c].p == n);
        _expose(p);
        pool[c].p = p;
        pool[p].r = c;
        _pull(p);
    }

    /// @brief c と親の間の辺を切る / c は表現木の根でないこと / 償却 O(log n)
    void cut(int c) {
        assert(0 <= c && c < n);
        _expose(c);
        int p = pool[c].l;
        assert(p != n);
        pool[c].l = n;
        pool[p].p = n;
        _pull(c);
    }

    /// @brief v を含む木の根を返す / 償却 O(log n)
    int root(int v) {
        assert(0 <= v && v < n);
        _expose(v);
        while (pool[v].l != n) {
            v = pool[v].l;
            _push(v);
        }
        _splay(v);
        return v;
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
        _expose(v);
        _toggle(v);
    }

    /// @brief u と v が別の連結成分なら辺 (u, v) を張って true を返す / 同じ成分なら根を変えずに false / 償却 O(log n)
    bool merge(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        if (u == v) return false;
        _expose(u);
        _expose(v);
        // 失敗時は表現木の根を変えない
        if (pool[u].p != n) return false;
        // 成分が異なるので、v を露出しても u は露出したまま
        _toggle(u);
        pool[u].p = v;
        pool[v].r = u;
        _pull(v);
        return true;
    }

    /// @brief 隣接する u と v の間の辺を切る / 償却 O(log n)
    void split(int u, int v) {
        evert(u);
        cut(v);
    }

    /// @brief u から v へのパス上の頂点数を返す / u, v は同じ連結成分にあること / 償却 O(log n)
    int path_length(int u, int v) {
        evert(u);
        expose(v);
        return pool[v].sz;
    }

    /// @brief u から v へのパスで k 番目の頂点を返す / 0-indexed / 範囲外なら -1 / 償却 O(log n)
    int path_kth_elm(int u, int v, int k) {
        evert(u);
        expose(v);
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
            if (k < sz) x = l;
            else {
                k -= sz + 1;
                x = pool[x].r;
            }
        }
    }
};
}  // namespace titan23
