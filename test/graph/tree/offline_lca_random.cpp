#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

#include "titan_cpplib/graph/tree/offline_lca.cpp"

using namespace std;
using titan23::OfflineLCA;

using Graph = vector<vector<int>>;
using Query = pair<int, int>;

static_assert(is_constructible_v<OfflineLCA, const Graph &>);
static_assert(is_constructible_v<OfflineLCA, const Graph &, int>);
static_assert(is_same_v<decltype(declval<const OfflineLCA &>().run()), vector<int>>);

struct NaiveLCA {
    vector<int> parent, depth;

    NaiveLCA(const Graph &g, int root) : parent(g.size(), -1), depth(g.size()) {
        queue<int> que;
        parent[root] = root;
        que.push(root);
        while (!que.empty()) {
            const int v = que.front();
            que.pop();
            for (int u : g[v]) {
                if (parent[u] != -1) continue;
                parent[u] = v;
                depth[u] = depth[v] + 1;
                que.push(u);
            }
        }
    }

    int lca(int u, int v) const {
        while (depth[u] > depth[v]) u = parent[u];
        while (depth[v] > depth[u]) v = parent[v];
        while (u != v) {
            u = parent[u];
            v = parent[v];
        }
        return u;
    }
};

void add_query(OfflineLCA &lca, vector<int> &expected, int u, int v, int answer) {
    const int id = lca.add_query(u, v);
    const int next_id = expected.size();
    assert(id == next_id);
    expected.push_back(answer);
}

void check(const Graph &g, int root, const vector<Query> &queries) {
    OfflineLCA lca(g, root);
    const OfflineLCA &view = lca;
    assert(view.run().empty());
    assert(view.run().empty());
    NaiveLCA naive(g, root);
    vector<int> expected;
    for (auto [u, v] : queries) {
        add_query(lca, expected, u, v, naive.lca(u, v));
    }
    const int n = g.size();
    for (int v = 0; v < n; ++v) {
        add_query(lca, expected, v, v, v);
        add_query(lca, expected, root, v, root);
        add_query(lca, expected, v, root, root);
        add_query(lca, expected, v, naive.parent[v], naive.parent[v]);
    }
    assert(view.run() == expected);
    assert(view.run() == expected);
    for (int i = 0; i < 5; ++i) {
        const int u = i % n, v = (n - 1 - i % n);
        add_query(lca, expected, u, v, naive.lca(u, v));
        add_query(lca, expected, v, u, naive.lca(u, v));
        add_query(lca, expected, u, v, naive.lca(u, v));
        assert(view.run() == expected);
    }
    if (root == 0) {
        OfflineLCA default_root(g);
        vector<int> default_expected;
        for (auto [u, v] : queries) {
            add_query(default_root, default_expected, u, v, naive.lca(u, v));
        }
        assert(default_root.run() == default_expected);
    }
}

Graph make_tree(int n, int shape, mt19937 &rng) {
    Graph g(n);
    for (int v = 1; v < n; ++v) {
        int p = 0;
        if (shape == 0) p = v - 1;
        if (shape == 1) p = (v - 1) / 2;
        if (shape == 2) p = rng() % v;
        g[v].push_back(p);
        g[p].push_back(v);
    }
    return g;
}

Graph relabel(const Graph &g, mt19937 &rng) {
    const int n = g.size();
    vector<int> label(n);
    iota(label.begin(), label.end(), 0);
    shuffle(label.begin(), label.end(), rng);
    Graph result(n);
    for (int v = 0; v < n; ++v) {
        for (int u : g[v]) result[label[v]].push_back(label[u]);
    }
    for (auto &neighbors : result) shuffle(neighbors.begin(), neighbors.end(), rng);
    return result;
}

vector<Query> all_pairs(int n) {
    vector<Query> queries;
    for (int u = 0; u < n; ++u) {
        for (int v = 0; v < n; ++v) queries.emplace_back(u, v);
    }
    return queries;
}

void check_long_chain(mt19937 &rng) {
    constexpr int n = 200000;
    const Graph g = make_tree(n, 0, rng);
    for (int root : {0, n / 2, n - 1}) {
        OfflineLCA lca(g, root);
        vector<int> expected;
        for (int i = 0; i < n; ++i) {
            int u = rng() % n, v = rng() % n;
            if (i % 5 == 0) u = 0;
            if (i % 5 == 1) v = n - 1;
            if (i % 5 == 2) v = u;
            const int answer = clamp(root, min(u, v), max(u, v));
            add_query(lca, expected, u, v, answer);
        }
        assert(lca.run() == expected);
        assert(lca.run() == expected);
        add_query(lca, expected, 0, n - 1, root);
        assert(lca.run() == expected);
    }
}

int main() {
    mt19937 rng(238461);
    for (int n = 1; n <= 20; ++n) {
        const auto queries = all_pairs(n);
        for (int shape = 0; shape < 4; ++shape) {
            const auto g = relabel(make_tree(n, shape, rng), rng);
            for (int root = 0; root < n; ++root) check(g, root, queries);
        }
    }
    for (int shape = 0; shape < 4; ++shape) {
        const auto g = make_tree(127, shape, rng);
        const auto queries = all_pairs(127);
        for (int root : {0, 63, 126}) check(g, root, queries);
    }
    for (int trial = 0; trial < 400; ++trial) {
        const int n = 1 + rng() % 200;
        const auto g = relabel(make_tree(n, 2, rng), rng);
        vector<Query> queries;
        for (int i = 0; i < 500; ++i) {
            const int u = rng() % n, v = rng() % n;
            queries.emplace_back(u, v);
            queries.emplace_back(v, u);
        }
        shuffle(queries.begin(), queries.end(), rng);
        check(g, rng() % n, queries);
    }
    check_long_chain(rng);
    cout << "offline_lca_random: OK\n";
}
