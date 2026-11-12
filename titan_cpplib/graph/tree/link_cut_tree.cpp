/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/tree/link_cut_tree.cpp
#pragma once

#include <cassert>
#include <utility>
#include <vector>
using namespace std;

// Dynamic forest: updates and queries are amortized O(log n).
// Construction/copy and memory are O(n); these bounds assume O(1) value operations.
// Individual operations are not guaranteed worst-case O(log n).
namespace titan23 {

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
    // PULL_ROOT=false is only for callers that immediately rebuild v.
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
    LinkCutTree() : n(0) {
        pool.emplace_back(n, 0);
    }

    LinkCutTree(int size) : n(size) {
        assert(size >= 0);
        pool.reserve(size + 1);
        for (int i = 0; i < size; ++i) pool.emplace_back(n);
        pool.emplace_back(n, 0);
    }

    int expose(int v) {
        assert(0 <= v && v < n);
        return _expose(v);
    }

    // u, v (and root, if specified) must belong to the same component.
    int lca(int u, int v, int root = -1) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        if (root != -1) evert(root);
        _expose(u);
        return _expose(v);
    }

    // c must be a represented-tree root in a different component from p.
    void link(int c, int p) {
        assert(0 <= c && c < n && 0 <= p && p < n);
        _splay(c);
        assert(pool[c].l == n && pool[c].p == n);
        _expose(p);
        pool[c].p = p;
        pool[p].r = c;
        _pull(p);
    }

    // c must not be a represented-tree root.
    void cut(int c) {
        assert(0 <= c && c < n);
        _expose(c);
        int p = pool[c].l;
        assert(p != n);
        pool[c].l = n;
        pool[p].p = n;
        _pull(c);
    }

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

    bool same(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        if (u == v) return true;
        _expose(u);
        _expose(v);
        return pool[u].p != n;
    }

    void evert(int v) {
        assert(0 <= v && v < n);
        _expose(v);
        _toggle(v);
    }

    bool merge(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        if (u == v) return false;
        _expose(u);
        _expose(v);
        // A failed merge preserves the represented-tree root.
        if (pool[u].p != n) return false;
        // The components are disjoint, so exposing v left u exposed.
        _toggle(u);
        pool[u].p = v;
        pool[v].r = u;
        _pull(v);
        return true;
    }

    // u and v must be adjacent.
    void split(int u, int v) {
        evert(u);
        cut(v);
    }

    // u and v must belong to the same component.
    int path_length(int u, int v) {
        evert(u);
        expose(v);
        return pool[v].sz;
    }

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
