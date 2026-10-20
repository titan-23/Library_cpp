#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <tuple>
#include <vector>

#include "titan_cpplib/graph/tree/kruskal_reconstruction_tree.cpp"

using namespace std;
using titan23::KruskalReconstructionTree;
using ll = long long;

template<typename T>
vector<int> reachable(int n, const vector<tuple<int, int, T>> &edges, int s, T x) {
    vector<vector<int>> graph(n);
    for (const auto &[u, v, w] : edges) {
        if (x < w) continue;
        graph[u].push_back(v);
        graph[v].push_back(u);
    }
    vector<bool> seen(n, false);
    queue<int> que;
    seen[s] = true;
    que.push(s);
    while (!que.empty()) {
        const int v = que.front();
        que.pop();
        for (int u : graph[v]) {
            if (seen[u]) continue;
            seen[u] = true;
            que.push(u);
        }
    }
    vector<int> result;
    for (int v = 0; v < n; ++v) {
        if (seen[v]) result.push_back(v);
    }
    return result;
}

template<typename T>
void check_graph(int n, const vector<tuple<int, int, T>> &edges, const vector<T> &thresholds) {
    const KruskalReconstructionTree<T> tree(n, edges);
    assert(tree.vertex_count() == n);
    assert(tree.node_count() == 2 * n - tree.component_count());
    auto permutation = tree.leaf_order();
    sort(permutation.begin(), permutation.end());
    vector<int> expected_permutation(n);
    iota(expected_permutation.begin(), expected_permutation.end(), 0);
    assert(permutation == expected_permutation);

    // 全点対の minimax 距離を Floyd-Warshall で独立に計算する。
    vector<vector<optional<T>>> dist(n, vector<optional<T>>(n));
    for (int v = 0; v < n; ++v) dist[v][v] = numeric_limits<T>::lowest();
    for (const auto &[u, v, w] : edges) {
        if (!dist[u][v] || w < *dist[u][v]) dist[u][v] = dist[v][u] = w;
    }
    for (int k = 0; k < n; ++k) {
        for (int u = 0; u < n; ++u) {
            for (int v = 0; v < n; ++v) {
                if (!dist[u][k] || !dist[k][v]) continue;
                const T candidate = max(*dist[u][k], *dist[k][v]);
                if (!dist[u][v] || candidate < *dist[u][v]) dist[u][v] = candidate;
            }
        }
    }
    for (int u = 0; u < n; ++u) {
        assert(tree.leaf_order()[tree.leaf_index(u)] == u);
        for (int v = 0; v < n; ++v) {
            assert(tree.same(u, v) == dist[u][v].has_value());
            const optional<T> expected = u == v ? nullopt : dist[u][v];
            assert(tree.merge_threshold(u, v) == expected);
        }
    }

    // 部分木区間が実際の子孫の葉を過不足なく含むことを確認する。
    vector<int> roots;
    for (int node = 0; node < tree.node_count(); ++node) {
        if (tree.parent(node) == -1) roots.push_back(node);
        if (node < n) {
            assert((tree.children(node) == array<int, 2>{-1, -1}));
        } else {
            for (int c : tree.children(node)) {
                assert(0 <= c && c < node);
                assert(tree.parent(c) == node);
                if (c >= n) assert(!(tree.merge_weight(node) < tree.merge_weight(c)));
            }
        }
        vector<int> descendants, st{node};
        while (!st.empty()) {
            const int v = st.back();
            st.pop_back();
            if (v < n) {
                descendants.push_back(v);
            } else {
                for (int c : tree.children(v)) st.push_back(c);
            }
        }
        const auto [l, r] = tree.subtree_range(node);
        assert(0 <= l && l < r && r <= n);
        vector<int> interval(tree.leaf_order().begin() + l, tree.leaf_order().begin() + r);
        sort(interval.begin(), interval.end());
        sort(descendants.begin(), descendants.end());
        assert(interval == descendants);
        assert(tree.leaf_count(node) == (int)descendants.size());
    }
    assert(tree.roots() == roots);

    // 合併頂点を含む LCA と、非連結な頂点間の -1 を確認する。
    for (int u = 0; u < tree.node_count(); ++u) {
        vector<bool> ancestor(tree.node_count(), false);
        for (int v = u; v != -1; v = tree.parent(v)) ancestor[v] = true;
        for (int v = 0; v < tree.node_count(); ++v) {
            int expected = v;
            while (expected != -1 && !ancestor[expected]) expected = tree.parent(expected);
            assert(tree.lca(u, v) == expected);
        }
    }

    // 各閾値で元のグラフを BFS し、成分の頂点集合そのものと比較する。
    for (T x : thresholds) {
        for (int u = 0; u < n; ++u) {
            const auto expected = reachable(n, edges, u, x);
            const int node = tree.component_node(u, x);
            assert(tree.component_size(u, x) == (int)expected.size());
            const auto [l, r] = tree.subtree_range(node);
            vector<int> actual(tree.leaf_order().begin() + l, tree.leaf_order().begin() + r);
            sort(actual.begin(), actual.end());
            assert(actual == expected);
            for (int v = 0; v < n; ++v) {
                const bool connected = binary_search(expected.begin(), expected.end(), v);
                assert(tree.same(u, v, x) == connected);
                assert((tree.component_node(v, x) == node) == connected);
            }
        }
    }
}

void check_large_chain() {
    constexpr int n = 200000;
    vector<tuple<int, int, ll>> edges;
    for (int v = 1; v < n; ++v) edges.emplace_back(v - 1, v, v);
    const KruskalReconstructionTree<ll> tree(n, move(edges));
    assert(tree.node_count() == 2 * n - 1);
    assert(tree.component_count() == 1);
    assert(tree.leaf_count(tree.roots()[0]) == n);
    mt19937 rng(172963);
    for (int i = 0; i < 5000; ++i) {
        const int u = rng() % n, v = rng() % n;
        const auto result = tree.merge_threshold(u, v);
        if (u == v) assert(!result);
        else assert(result == max(u, v));
        const int x = int(rng() % (n + 2)) - 1;
        const int expected_size = u <= x ? min(n, x + 1) : 1;
        assert(tree.component_size(u, x) == expected_size);
        assert(tree.same(u, v, x) == (u == v || max(u, v) <= x));
    }
}

int main() {
    const KruskalReconstructionTree<> empty;
    assert(empty.vertex_count() == 0 && empty.node_count() == 0);
    assert(empty.component_count() == 0 && empty.roots().empty() && empty.leaf_order().empty());
    check_graph<ll>(0, {}, {});
    check_graph<ll>(1, {}, {-1, 0, 1});
    check_graph<ll>(5, {}, {-1, 0, 1});
    check_graph<ll>(4, {{0, 1, 2}, {2, 3, 3}, {1, 2, 5}, {0, 3, 9}}, {1, 2, 3, 4, 5, 9});
    check_graph<ll>(6, {{0, 0, -10}, {0, 1, -3}, {0, 1, 2}, {1, 2, -1}, {3, 4, 0}}, {-11, -3, -2, -1, 0, 2});
    check_graph<ll>(5, {{0, 1, 7}, {1, 2, 7}, {2, 3, 7}, {3, 4, 7}, {4, 0, 7}}, {6, 7, 8});

    const ll lo = numeric_limits<ll>::lowest(), hi = numeric_limits<ll>::max();
    check_graph<ll>(4, {{0, 1, lo}, {1, 2, hi}}, {lo, lo + 1, -1, 0, hi - 1, hi});
    using ull = unsigned long long;
    const ull umax = numeric_limits<ull>::max();
    check_graph<ull>(4, {{0, 1, 0}, {1, 2, umax}}, {0, 1, umax - 1, umax});
    check_graph<double>(4, {{0, 1, -2.5}, {1, 2, 0.25}, {2, 3, 0.25}}, {-3, -2.5, 0, 0.25, 1});

    mt19937 rng(8761923);
    vector<ll> thresholds;
    for (ll x = -6; x <= 6; ++x) thresholds.push_back(x);
    for (int trial = 0; trial < 600; ++trial) {
        const int n = 1 + rng() % 14, m = rng() % (2 * n * n + 1);
        vector<tuple<int, int, ll>> edges;
        for (int i = 0; i < m; ++i) {
            const int u = rng() % n, v = rng() % n;
            const ll w = int(rng() % 11) - 5;
            edges.emplace_back(u, v, w);
        }
        check_graph(n, edges, thresholds);
        if (trial % 10 == 0) {
            shuffle(edges.begin(), edges.end(), rng);
            check_graph(n, edges, thresholds);
        }
    }
    check_large_chain();
    cout << "kruskal_reconstruction_tree_random: OK\n";
}
