#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "titan_cpplib/graph/tree/mo_tree_edge.cpp"

using namespace std;
using titan23::MoTreeEdge;

template<typename T>
using Graph = vector<vector<pair<int, T>>>;

static_assert(is_constructible_v<MoTreeEdge, const Graph<string> &>);
static_assert(is_constructible_v<MoTreeEdge, const Graph<string> &, int>);
static_assert(is_constructible_v<MoTreeEdge, const Graph<unique_ptr<int>> &, int>);

template<typename T>
void check(const Graph<T> &g, int root) {
    const int n = g.size();
    Graph<int> edges(n);
    int edge_count = 0;
    for (int u = 0; u < n; ++u) {
        for (const auto &edge : g[u]) {
            const int v = edge.first;
            if (u >= v) continue;
            edges[u].emplace_back(v, edge_count);
            edges[v].emplace_back(u, edge_count++);
        }
    }

    vector<vector<int>> expected;
    MoTreeEdge mo(g, root);
    auto unexpected = [] (int) { assert(false); };
    mo.run(unexpected, unexpected, unexpected);
    for (int s = 0; s < n; ++s) {
        vector<int> parent(n, -1), parent_edge(n, -1);
        queue<int> que;
        parent[s] = s;
        que.push(s);
        while (!que.empty()) {
            const int v = que.front();
            que.pop();
            for (auto [u, id] : edges[v]) {
                if (parent[u] != -1) continue;
                parent[u] = v;
                parent_edge[u] = id;
                que.push(u);
            }
        }
        for (int t = 0; t < n; ++t) {
            assert(parent[t] != -1);
            vector<int> path(edge_count);
            for (int v = t; v != s; v = parent[v]) path[parent_edge[v]] = 1;
            const int id = mo.add_query(s, t);
            assert(id == s * n + t);
            expected.push_back(move(path));
        }
    }

    for (int run = 0; run < 2; ++run) {
        vector<int> active(edge_count), seen(expected.size());
        auto add = [&] (int id) {
            assert(0 <= id && id < edge_count);
            assert(active[id] == 0);
            active[id] = 1;
        };
        auto del = [&] (int id) {
            assert(0 <= id && id < edge_count);
            assert(active[id] == 1);
            active[id] = 0;
        };
        auto out = [&] (int id) {
            assert(0 <= id && id < n * n);
            assert(seen[id]++ == 0);
            assert(active == expected[id]);
        };
        mo.run(add, del, out);
        for (int count : seen) assert(count == 1);
    }
}

int main() {
    mt19937 rng(20260928);
    for (int n = 1; n <= 40; ++n) {
        vector<int> labels(n);
        iota(labels.begin(), labels.end(), 0);
        for (int trial = 0; trial < 3; ++trial) {
            shuffle(labels.begin(), labels.end(), rng);
            Graph<string> g(n);
            for (int i = 1; i < n; ++i) {
                const int u = labels[i], v = labels[rng() % i];
                g[u].emplace_back(v, "weight " + to_string(i));
                g[v].emplace_back(u, "reverse " + to_string(i));
            }
            for (auto &adj : g) shuffle(adj.begin(), adj.end(), rng);
            check(g, 0);
            check(g, n - 1);
        }
    }

    Graph<unique_ptr<int>> g(5);
    for (auto [u, v] : vector<pair<int, int>>{{3, 4}, {1, 3}, {0, 3}, {2, 1}}) {
        g[u].emplace_back(v, make_unique<int>(100 + u));
        g[v].emplace_back(u, make_unique<int>(100 + v));
    }
    check(g, 3);
    cout << "mo_tree_weighted_graph: OK\n";
}
