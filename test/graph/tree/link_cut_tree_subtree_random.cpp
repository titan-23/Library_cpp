#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <tuple>
#include <vector>

#include "titan_cpplib/graph/tree/link_cut_tree_subtree.cpp"

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

int main() {
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
