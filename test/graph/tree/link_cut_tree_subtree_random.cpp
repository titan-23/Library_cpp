#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <tuple>
#include <vector>

#ifdef LCTS_HEADER
#include LCTS_HEADER
#else
#include "titan_cpplib/graph/tree/link_cut_tree_subtree.cpp"
#endif

using namespace std;

long long op(long long x, long long y) {
    return x + y;
}

long long e() {
    return 0;
}

long long inv(long long x) {
    return -x;
}

struct NaiveForest {
    int n;
    vector<long long> a;
    vector<set<int>> g;

    NaiveForest(const vector<long long> &values) : n(int(values.size())), a(values), g(values.size()) {}

    vector<int> parents(int root) const {
        vector<int> par(n, -2);
        queue<int> q;
        par[root] = -1;
        q.push(root);
        while (!q.empty()) {
            int v = q.front();
            q.pop();
            for (int x : g[v]) {
                if (x == par[v]) continue;
                par[x] = v;
                q.push(x);
            }
        }
        return par;
    }

    bool same(int u, int v) const {
        return parents(u)[v] != -2;
    }

    vector<int> path(int u, int v) const {
        vector<int> par = parents(u);
        assert(par[v] != -2);
        vector<int> res;
        while (v != -1) {
            res.push_back(v);
            if (v == u) break;
            v = par[v];
        }
        reverse(res.begin(), res.end());
        return res;
    }

    vector<int> subtree(int root, int v) const {
        vector<int> par = parents(root);
        assert(par[v] != -2);
        vector<int> res;
        vector<pair<int, int>> stk = {{v, par[v]}};
        while (!stk.empty()) {
            auto [x, p] = stk.back();
            stk.pop_back();
            res.push_back(x);
            for (int y : g[x]) {
                if (y != p) stk.emplace_back(y, x);
            }
        }
        return res;
    }

    long long sum(const vector<int> &vs) const {
        long long res = 0;
        for (int v : vs) res += a[v];
        return res;
    }

    int lca(int root, int u, int v) const {
        vector<int> par = parents(root);
        vector<int> dep(n);
        queue<int> q;
        q.push(root);
        while (!q.empty()) {
            int x = q.front();
            q.pop();
            for (int y : g[x]) {
                if (y == par[x]) continue;
                dep[y] = dep[x] + 1;
                q.push(y);
            }
        }
        while (dep[u] > dep[v]) u = par[u];
        while (dep[v] > dep[u]) v = par[v];
        while (u != v) {
            u = par[u];
            v = par[v];
        }
        return u;
    }

    vector<pair<int, int>> edges() const {
        vector<pair<int, int>> res;
        for (int u = 0; u < n; ++u) {
            for (int v : g[u]) {
                if (u < v) res.emplace_back(u, v);
            }
        }
        return res;
    }

    void link(int u, int v) {
        assert(!same(u, v));
        g[u].insert(v);
        g[v].insert(u);
    }

    void cut(int u, int v) {
        assert(g[u].contains(v));
        g[u].erase(v);
        g[v].erase(u);
    }
};

void verify(titan23::LinkCutTreeSubtree<long long, op, e, inv> &lct, const NaiveForest &naive) {
    int n = naive.n;
    for (int u = 0; u < n; ++u) {
        assert(lct.get(u) == naive.a[u]);
        vector<int> comp = naive.subtree(u, u);
        assert(lct.component_prod(u) == naive.sum(comp));
        assert(lct.component_size(u) == (int)comp.size());
        for (int v = 0; v < n; ++v) {
            assert(lct.same(u, v) == naive.same(u, v));
            if (!naive.same(u, v)) continue;
            vector<int> path = naive.path(u, v);
            assert(lct.path_prod(u, v) == naive.sum(path));
            assert(lct.path_length(u, v) == (int)path.size());
            for (int k = 0; k < (int)path.size(); ++k) {
                assert(lct.path_kth_elm(u, v, k) == path[k]);
            }
            assert(lct.path_kth_elm(u, v, -1) == -1);
            assert(lct.path_kth_elm(u, v, int(path.size())) == -1);
            vector<int> sub = naive.subtree(u, v);
            assert(lct.subtree_prod(u, v) == naive.sum(sub));
            assert(lct.subtree_size(u, v) == (int)sub.size());
            assert(lct.lca(u, v, u) == u);
        }
    }
}

struct Payload {
    long long sum;
    uint64_t bits;
    bool operator==(const Payload &) const = default;
};

Payload payload_op(Payload a, Payload b) { return {a.sum + b.sum, a.bits ^ b.bits}; }
Payload payload_e() { return {0, 0}; }
Payload payload_inv(Payload a) { return {-a.sum, a.bits}; }

// Interleave rerooting, topology changes and value changes. Unlike verify(), this
// does not expose every pair between updates, so pending reversals accumulate.
void test_mixed_group() {
    using Tree = titan23::LinkCutTreeSubtree<Payload, payload_op, payload_e, payload_inv>;
    for (unsigned seed : {17, 81, 2026, 998244353}) {
        mt19937_64 rng(seed);
        const int n = 128;
        vector<Payload> values(n);
        vector<long long> sums(n);
        for (int v = 0; v < n; ++v) {
            sums[v] = int(rng() % 201) - 100;
            values[v] = {sums[v], rng()};
        }
        Tree tree(values);
        NaiveForest naive(sums);
        for (int v = 1; v < n; ++v) {
            int p = seed % 2 ? v - 1 : int(rng() % v);
            tree.link(v, p);
            naive.link(v, p);
        }
        auto fold = [&](const vector<int> &vs) {
            Payload result = payload_e();
            for (int v : vs) result = payload_op(result, values[v]);
            return result;
        };
        for (int step = 0; step < 12000; ++step) {
            int u = int(rng() % n), v = int(rng() % n);
            int kind = int(rng() % 9);
            if (kind == 0) {
                auto edges = naive.edges();
                tie(u, v) = edges[rng() % edges.size()];
                tree.split(u, v);
                naive.cut(u, v);
                auto a = naive.subtree(u, u), b = naive.subtree(v, v);
                assert(!tree.same(u, v));
                assert(tree.component_prod(u) == fold(a));
                assert(tree.component_prod(v) == fold(b));
                u = a[rng() % a.size()];
                v = b[rng() % b.size()];
                assert(tree.merge(u, v));
                naive.link(u, v);
            } else if (kind == 1) {
                values[u] = {int(rng() % 2001) - 1000, rng()};
                tree.set(u, values[u]);
                assert(tree.get(u) == values[u]);
            } else if (kind == 2) {
                auto path = naive.path(u, v);
                assert(tree.path_prod(u, v) == fold(path));
                assert(tree.path_length(u, v) == int(path.size()));
                int k = int(rng() % path.size());
                assert(tree.path_kth_elm(u, v, k) == path[k]);
            } else if (kind == 3) {
                auto sub = naive.subtree(u, v);
                assert(tree.subtree_prod(u, v) == fold(sub));
                assert(tree.subtree_size(u, v) == int(sub.size()));
            } else if (kind == 4) {
                tree.evert(u);
                assert(tree.root(v) == u);
                int w = int(rng() % n);
                assert(tree.lca(v, w) == naive.lca(u, v, w));
            } else if (kind == 5) {
                assert(!tree.merge(u, v));
                // Even a failed merge reroots the component at u.
                assert(tree.root(v) == u);
            } else if (kind == 6) {
                assert(tree.component_prod(u) == fold(naive.subtree(u, u)));
                assert(tree.component_size(u) == n);
            } else {
                tree.evert(u);
                tree.expose(v);
            }
            if (step % 997 == 0) {
                Tree copy = tree;
                Payload replacement = {123456, rng()};
                copy.set(u, replacement);
                assert(tree.get(u) == values[u]);
                Payload total = fold(naive.subtree(u, u));
                assert(tree.component_prod(u) == total);
                assert(copy.component_prod(u) ==
                       payload_op(total, payload_op(payload_inv(values[u]), replacement)));
            }
        }
    }
}

int main() {
    test_mixed_group();
    {
        titan23::LinkCutTreeSubtree<long long, op, e, inv> lct(1);
        assert(lct.get(0) == 0);
        assert(lct.component_prod(0) == 0);
        assert(lct.component_size(0) == 1);
    }
    {
        constexpr int n = 100000;
        constexpr int m = n / 2;
        vector<long long> a(n, 1);
        titan23::LinkCutTreeSubtree<long long, op, e, inv> lct(a);
        for (int v = 1; v < n; ++v) lct.link(v, v - 1);
        assert(lct.component_size(0) == n);
        assert(lct.component_prod(n - 1) == n);
        assert(lct.path_length(0, n - 1) == n);
        assert(lct.path_prod(0, n - 1) == n);
        assert(lct.path_kth_elm(0, n - 1, 0) == 0);
        assert(lct.path_kth_elm(0, n - 1, m) == m);
        assert(lct.path_kth_elm(0, n - 1, n - 1) == n - 1);
        assert(lct.subtree_size(0, m) == n - m);
        assert(lct.subtree_prod(0, m) == n - m);
        lct.split(m - 1, m);
        assert(!lct.same(m - 1, m));
        assert(lct.component_size(0) == m);
        assert(lct.component_size(n - 1) == n - m);
        assert(lct.merge(m - 1, m));
        assert(lct.same(0, n - 1));
        assert(lct.component_size(m) == n);
    }

    mt19937 rng(123456789);
    for (int tc = 0; tc < 80; ++tc) {
        int n = int(rng() % 15) + 1;
        vector<long long> a(n);
        for (long long &v : a) v = int(rng() % 101) - 50;
        titan23::LinkCutTreeSubtree<long long, op, e, inv> lct(a);
        NaiveForest naive(a);

        for (int step = 0; step < 1500; ++step) {
            int type = int(rng() % 12);
            int u = int(rng() % n);
            int v = int(rng() % n);
            if (type == 0 || type == 1) {
                bool connected = naive.same(u, v);
                assert(lct.merge(u, v) == !connected);
                if (!connected) naive.link(u, v);
            } else if (type == 2) {
                if (!naive.same(u, v)) {
                    lct.evert(u);
                    lct.link(u, v);
                    naive.link(u, v);
                }
            } else if (type == 3 || type == 4) {
                vector<pair<int, int>> es = naive.edges();
                if (!es.empty()) {
                    tie(u, v) = es[rng() % es.size()];
                    if (type == 3) {
                        lct.split(u, v);
                    } else {
                        lct.evert(v);
                        lct.cut(u);
                    }
                    naive.cut(u, v);
                }
            } else if (type == 5) {
                long long x = int(rng() % 201) - 100;
                lct.set(u, x);
                naive.a[u] = x;
            } else if (type == 6) {
                assert(lct.same(u, v) == naive.same(u, v));
            } else if (type == 7 && naive.same(u, v)) {
                vector<int> path = naive.path(u, v);
                assert(lct.path_prod(u, v) == naive.sum(path));
            } else if (type == 8 && naive.same(u, v)) {
                vector<int> sub = naive.subtree(u, v);
                assert(lct.subtree_prod(u, v) == naive.sum(sub));
                assert(lct.subtree_size(u, v) == (int)sub.size());
            } else if (type == 9) {
                vector<int> comp = naive.subtree(u, u);
                assert(lct.component_prod(u) == naive.sum(comp));
                assert(lct.component_size(u) == (int)comp.size());
            } else if (type == 10) {
                lct.evert(u);
                for (int x = 0; x < n; ++x) {
                    if (naive.same(u, x)) assert(lct.root(x) == u);
                }
            } else if (naive.same(u, v)) {
                int root = int(rng() % n);
                while (!naive.same(root, u)) root = int(rng() % n);
                assert(lct.lca(u, v, root) == naive.lca(root, u, v));
            }
            if (step % 100 == 0) verify(lct, naive);
        }
        verify(lct, naive);
    }
    cout << "LinkCutTreeSubtree random test: OK\n";
}
