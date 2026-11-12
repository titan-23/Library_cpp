#include <algorithm>
#include <cassert>
#include <cstdint>
#include <numeric>
#include <queue>
#include <random>
#include <utility>
#include <vector>
#include "titan_cpplib/graph/online_dynamic_connectivity.cpp"
using namespace std;

struct NaiveGraph {
    struct Edge { int u, v; bool active; };
    int n, issued;
    vector<Edge> edges;

    explicit NaiveGraph(int size) : n(size), issued(0) {}

    int add_edge(int u, int v) {
        int id = issued++;
        edges.push_back({u, v, true});
        return id;
    }

    bool erase_edge(int id) {
        if (id < 0 || id >= issued || !edges[id].active) return false;
        edges[id].active = false;
        return true;
    }

    vector<int> component(int s) const {
        vector<int> seen(n);
        seen[s] = 1;
        vector<int> que(1, s);
        for (int h = 0; h < (int)que.size(); ++h) {
            int v = que[h];
            for (const Edge &e : edges) {
                if (!e.active) continue;
                int u = -1;
                if (e.u == v) u = e.v;
                if (e.v == v) u = e.u;
                if (u != -1 && !seen[u]) {
                    seen[u] = 1;
                    que.push_back(u);
                }
            }
        }
        return seen;
    }

    bool same(int u, int v) const {
        return component(u)[v];
    }

    int size(int v) const {
        vector<int> seen = component(v);
        return accumulate(seen.begin(), seen.end(), 0);
    }

    int group_count() const {
        vector<int> seen(n);
        int ans = 0;
        for (int s = 0; s < n; ++s) {
            if (seen[s]) continue;
            ++ans;
            vector<int> cur = component(s);
            for (int v = 0; v < n; ++v) seen[v] |= cur[v];
        }
        return ans;
    }

    int edge_count() const {
        int ans = 0;
        for (const Edge &e : edges) ans += e.active;
        return ans;
    }
};

void check_all(titan23::OnlineDynamicConnectivity &dc, const NaiveGraph &g) {
    assert(dc.group_count() == g.group_count());
    assert(dc.edge_count() == g.edge_count());
    assert(dc.edge_id_count() == g.issued);
    for (int u = 0; u < g.n; ++u) {
        assert(dc.size(u) == g.size(u));
        for (int v = 0; v < g.n; ++v) assert(dc.same(u, v) == g.same(u, v));
    }
}

int root(vector<int> &par, int v) {
    if (par[v] < 0) return v;
    return par[v] = root(par, par[v]);
}

void check_dsu(titan23::OnlineDynamicConnectivity &dc, const NaiveGraph &g) {
    vector<int> par(g.n, -1);
    for (const NaiveGraph::Edge &e : g.edges) {
        if (!e.active || e.u == e.v) continue;
        int u = root(par, e.u), v = root(par, e.v);
        if (u == v) continue;
        if (par[u] > par[v]) swap(u, v);
        par[u] += par[v];
        par[v] = u;
    }
    int groups = 0;
    for (int v = 0; v < g.n; ++v) groups += par[v] < 0;
    assert(dc.group_count() == groups);
    assert(dc.edge_count() == g.edge_count());
    assert(dc.edge_id_count() == g.issued);
    for (int v = 0; v < g.n; ++v) {
        assert(dc.size(v) == -par[root(par, v)]);
        int u = (v * 11939 + 17) % g.n;
        assert(dc.same(u, v) == (root(par, u) == root(par, v)));
    }
}

void test_small() {
    titan23::OnlineDynamicConnectivity empty;
    assert(empty.group_count() == 0);
    assert(empty.edge_count() == 0);
    assert(empty.edge_id_count() == 0);
    assert(!empty.erase_edge(-1) && !empty.erase_edge(0));

    titan23::OnlineDynamicConnectivity one(1, 4);
    int a = one.add_edge(0, 0);
    int b = one.add_edge(0, 0);
    assert(one.edge_id_count() == 2);
    assert(a == 0 && b == 1 && one.same(0, 0) && one.size(0) == 1);
    assert(one.erase_edge(a));
    assert(!one.erase_edge(a));
    assert(one.edge_active(b));
    assert(one.erase_edge(b));

    titan23::OnlineDynamicConnectivity dc(2, 3);
    int e0 = dc.add_edge(0, 1);
    int e1 = dc.add_edge(0, 1);
    assert(dc.same(0, 1));
    assert(dc.erase_edge(e0));
    assert(dc.same(0, 1));
    assert(dc.erase_edge(e1));
    assert(!dc.same(0, 1));
}

void test_random(uint32_t seed) {
    mt19937 rng(seed);
    auto pick = [&](int m) { return uniform_int_distribution<int>(0, m - 1)(rng); };
    int n = 1 + pick(32);
    int steps = 4000;
    titan23::OnlineDynamicConnectivity dc(n);
    if (seed % 3 == 0) dc.prepare_dynamic();
    if (seed & 1) dc.reserve_edges(steps);
    NaiveGraph g(n);
    vector<int> active, pos(steps, -1);
    int active_count = 0;

    auto keep = [&](int id) {
        pos[id] = active_count++;
        active.push_back(id);
    };
    auto drop = [&](int id) {
        int p = pos[id], z = active.back();
        active[p] = z;
        pos[z] = p;
        active.pop_back();
        pos[id] = -1;
        --active_count;
    };

    for (int step = 0; step < steps; ++step) {
        if (step == steps / 3) dc.prepare_dynamic();
        if (step == steps / 2) dc.reserve_edges(steps + 100);
        int kind = pick(100);
        if (kind < 42 || active.empty()) {
            int u = pick(n), v = pick(n);
            int x = dc.add_edge(u, v);
            int y = g.add_edge(u, v);
            assert(x == y);
            keep(x);
        } else if (kind < 70) {
            int id = active[pick(active_count)];
            assert(dc.erase_edge(id) == g.erase_edge(id));
            assert(!dc.edge_active(id));
            drop(id);
        } else if (kind < 85) {
            int u = pick(n), v = pick(n);
            assert(dc.same(u, v) == g.same(u, v));
        } else if (kind < 95) {
            int v = pick(n);
            assert(dc.size(v) == g.size(v));
        } else {
            assert(dc.group_count() == g.group_count());
            assert(dc.edge_count() == g.edge_count());
        }
        if (step % 257 == 0) check_all(dc, g);
    }
    check_all(dc, g);
}

void test_path() {
    int n = 2000, q = 10000;
    titan23::OnlineDynamicConnectivity dc(n, n + q);
    vector<int> ids(n - 1);
    for (int v = 1; v < n; ++v) ids[v - 1] = dc.add_edge(v - 1, v);
    int m = (n - 1) / 2;
    for (int i = 0; i < q; ++i) {
        assert(dc.erase_edge(ids[m]));
        assert(!dc.same(0, n - 1));
        ids[m] = dc.add_edge(m, m + 1);
        assert(dc.same(0, n - 1));
        assert(dc.size(i % n) == n);
    }
}

void test_deep() {
    int n = 512, steps = 30000, cap = n * 3 + steps;
    mt19937 rng(1234567);
    auto pick = [&](int m) { return uniform_int_distribution<int>(0, m - 1)(rng); };
    titan23::OnlineDynamicConnectivity dc(n, cap);
    NaiveGraph g(n);
    vector<int> active, path, pos(cap, -1);
    int count = 0;

    auto keep = [&](int id) {
        pos[id] = count++;
        active.push_back(id);
    };
    auto drop = [&](int id) {
        int p = pos[id], z = active.back();
        active[p] = z;
        pos[z] = p;
        active.pop_back();
        pos[id] = -1;
        --count;
    };
    auto add = [&](int u, int v) {
        int x = dc.add_edge(u, v), y = g.add_edge(u, v);
        assert(x == y);
        keep(x);
        return x;
    };

    for (int v = 1; v < n; ++v) path.push_back(add(v - 1, v));
    for (int i = 0; i < n * 2; ++i) add(pick(n), pick(n));
    shuffle(path.begin(), path.end(), rng);
    int turn = 0;
    for (int id : path) {
        assert(dc.erase_edge(id) == g.erase_edge(id));
        drop(id);
        if (turn++ % 16 == 0) check_dsu(dc, g);
    }
    for (int step = 0; step < steps; ++step) {
        int kind = pick(100);
        if (kind < 45 || active.empty()) {
            add(pick(n), pick(n));
        } else if (kind < 80) {
            int id = active[pick(count)];
            assert(dc.erase_edge(id) == g.erase_edge(id));
            drop(id);
        } else {
            int u = pick(n), v = pick(n);
            (void)dc.same(u, v);
        }
        if (step % 1024 == 0) check_dsu(dc, g);
    }
    check_dsu(dc, g);
}

// First split a tree with no replacement candidates; then introduce cycles
// and parallel edges and delete the old tree. Deferred work must stay valid
// when incidences are added later, including after complete fragmentation.
void test_split_then_add() {
    constexpr int n = 96;
    titan23::OnlineDynamicConnectivity dc(n);
    NaiveGraph g(n);
    mt19937 rng(20261112);
    auto add = [&](int u, int v) {
        int id = dc.add_edge(u, v);
        assert(id == g.add_edge(u, v));
        return id;
    };
    auto erase = [&](int id) { assert(dc.erase_edge(id) == g.erase_edge(id)); };
    for (int round = 0; round < 4; ++round) {
        vector<int> ids;
        for (int v = 1; v < n; ++v) ids.push_back(add(v - 1, v));
        erase(ids[n / 2 - 1]);
        check_dsu(dc, g);
        ids.push_back(add(0, n / 2 - 1));
        ids.push_back(add(n / 2, n - 1));
        ids.push_back(add(1, n - 2));
        ids.push_back(add(1, n - 2));
        ids.push_back(add(0, 0));
        for (int i = 0; i < n; ++i) ids.push_back(add(rng() % n, rng() % n));
        shuffle(ids.begin(), ids.end(), rng);
        for (size_t i = 0; i < ids.size(); ++i) {
            erase(ids[i]);
            if (i % 11 == 0) check_dsu(dc, g);
        }
        check_dsu(dc, g);
        assert(dc.group_count() == n && dc.edge_count() == 0);
    }
}

void test_balanced_cuts() {
    constexpr int n = 4096;
    titan23::OnlineDynamicConnectivity dc(n, n);
    NaiveGraph g(n);
    for (int v = 1; v < n; ++v) {
        assert(dc.add_edge(v - 1, v) == g.add_edge(v - 1, v));
    }
    vector<pair<int, int>> ranges{{0, n - 1}};
    int removed = 0;
    for (size_t i = 0; i < ranges.size(); ++i) {
        auto [lo, hi] = ranges[i];
        if (lo == hi) continue;
        int mid = (lo + hi) / 2;
        assert(dc.erase_edge(mid) == g.erase_edge(mid));
        ++removed;
        assert(dc.group_count() == removed + 1);
        assert(dc.size(lo) == mid - lo + 1);
        assert(dc.size(hi) == hi - mid);
        if (removed % 257 == 0) check_dsu(dc, g);
        ranges.emplace_back(lo, mid);
        ranges.emplace_back(mid + 1, hi);
    }
    check_dsu(dc, g);
}

void test_deferred_build() {
    titan23::OnlineDynamicConnectivity empty;
    empty.prepare_dynamic();
    empty.prepare_dynamic();
    assert(empty.group_count() == 0 && !empty.erase_edge(0));
    for (int mode = 0; mode < 3; ++mode) {
        constexpr int n = 40;
        titan23::OnlineDynamicConnectivity dc(n);
        NaiveGraph g(n);
        if (mode == 1) dc.prepare_dynamic();
        auto add = [&](int u, int v) {
            int id = dc.add_edge(u, v);
            assert(id == g.add_edge(u, v));
            return id;
        };
        auto erase = [&](int id) { assert(dc.erase_edge(id) == g.erase_edge(id)); };
        vector<int> ids;
        for (int offset : {0, 20}) for (int v = 0; v < 20; ++v)
            ids.push_back(add(offset + v, offset + (v + 1) % 20));
        int loop = add(3, 3), parallel = add(0, 1);
        erase(loop); erase(parallel); // Before automatic preparation: ID gaps.
        auto copy = dc;
        assert(copy.erase_edge(ids[0]));
        assert(dc.edge_active(ids[0]));
        check_dsu(dc, g);
        if (mode == 2) dc.prepare_dynamic();
        // Tree-edge replacements preserve the partition cache.
        for (int v = 0; v < 12; ++v) {
            erase(ids[v]);
            check_dsu(dc, g);
            ids[v] = add(v, v + 1);
        }
        // A real split must invalidate it, even if a later insertion reconnects.
        erase(ids[0]); erase(ids[1]);
        check_dsu(dc, g);
        assert(dc.size(1) == 1 && !dc.same(0, 1));
        dc.prepare_dynamic();
        add(0, 1); add(1, 2); add(0, 20);
        check_all(dc, g);
        dc.prepare_dynamic();
        for (int id = 0; id < g.issued; ++id) erase(id);
        check_dsu(dc, g);
    }
}

int main() {
    test_small();
    for (uint32_t seed = 1; seed <= 24; ++seed) test_random(seed * 998244353U);
    test_path();
    test_deep();
    test_split_then_add();
    test_balanced_cuts();
    test_deferred_build();
}
