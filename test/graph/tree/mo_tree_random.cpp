#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

#include "titan_cpplib/graph/tree/mo_tree_vertex.cpp"
#include "titan_cpplib/graph/tree/mo_tree_edge.cpp"
#include "titan_cpplib/graph/tree/mo_tree_vertex_edge.cpp"

using namespace std;
using titan23::MoTreeEdge;
using titan23::MoTreeVertex;
using titan23::MoTreeVertexEdge;

using Graph = vector<vector<int>>;
using Edge = pair<int, int>;
using Query = pair<int, int>;

static_assert(is_constructible_v<MoTreeVertex, const Graph &, int>);
static_assert(is_constructible_v<MoTreeEdge, const Graph &, int>);
static_assert(is_constructible_v<MoTreeVertexEdge, const Graph &, int>);
static_assert(is_constructible_v<MoTreeVertex, const Graph &>);
static_assert(is_constructible_v<MoTreeEdge, const Graph &>);
static_assert(is_constructible_v<MoTreeVertexEdge, const Graph &>);
static_assert(is_constructible_v<MoTreeVertex, int, int>);
static_assert(is_constructible_v<MoTreeEdge, int, int>);
static_assert(is_constructible_v<MoTreeVertexEdge, int, int>);
static_assert(is_constructible_v<MoTreeVertex, int>);
static_assert(is_constructible_v<MoTreeEdge, int>);
static_assert(is_constructible_v<MoTreeVertexEdge, int>);

struct Path {
    vector<int> vertices, edges;
};

// BFS で経路を求めて照合
vector<Path> naive_paths(int n, const vector<Edge> &edges, const vector<Query> &queries) {
    vector<vector<Edge>> g(n);
    for (int e = 0; e < (int)edges.size(); ++e) {
        auto [u, v] = edges[e];
        g[u].emplace_back(v, e);
        g[v].emplace_back(u, e);
    }
    vector<Path> paths;
    for (auto [s, t] : queries) {
        vector<int> parent(n, -1), parent_edge(n, -1);
        queue<int> que;
        parent[s] = s;
        que.push(s);
        while (!que.empty() && parent[t] == -1) {
            int v = que.front();
            que.pop();
            for (auto [u, e] : g[v]) {
                if (parent[u] != -1) continue;
                parent[u] = v;
                parent_edge[u] = e;
                que.push(u);
            }
        }
        assert(parent[t] != -1);
        Path path{vector<int>(n), vector<int>(edges.size())};
        for (int v = t; v != s; v = parent[v]) {
            path.vertices[v] = 1;
            path.edges[parent_edge[v]] = 1;
        }
        path.vertices[s] = 1;
        paths.push_back(move(path));
    }
    return paths;
}

struct ActiveSet {
    vector<int> active, value, frequency;
    int distinct = 0;
    int callback_count = 0;

    ActiveSet(int n) : active(n), value(n), frequency(7) {
        for (int i = 0; i < n; ++i) value[i] = (i * 5 + i / 3) % 7;
    }

    void add(int id) {
        assert(0 <= id && id < (int)active.size());
        assert(active[id] == 0);
        active[id] = 1;
        if (frequency[value[id]]++ == 0) ++distinct;
        ++callback_count;
    }

    void del(int id) {
        assert(0 <= id && id < (int)active.size());
        assert(active[id] == 1);
        active[id] = 0;
        if (--frequency[value[id]] == 0) --distinct;
        ++callback_count;
    }

    void check(const vector<int> &expected) const {
        assert(active == expected);
        vector<int> counts(frequency.size());
        for (int i = 0; i < (int)expected.size(); ++i) {
            if (expected[i]) ++counts[value[i]];
        }
        assert(frequency == counts);
        assert(distinct == count_if(counts.begin(), counts.end(), [] (int x) { return x > 0; }));
    }
};

template <class Mo>
void check_run(Mo &mo, int n, const vector<Path> &expected) {
    // run ごとに状態を初期化
    ActiveSet vertices(n), edges(n - 1);
    vector<int> seen(expected.size());
    auto av = [&] (int v) { vertices.add(v); };
    auto dv = [&] (int v) { vertices.del(v); };
    auto ae = [&] (int e) { edges.add(e); };
    auto de = [&] (int e) { edges.del(e); };
    auto out = [&] (int qid) {
        assert(0 <= qid && qid < (int)expected.size());
        assert(seen[qid]++ == 0);
        if constexpr (!is_same_v<Mo, MoTreeEdge>) vertices.check(expected[qid].vertices);
        if constexpr (!is_same_v<Mo, MoTreeVertex>) edges.check(expected[qid].edges);
    };
    if constexpr (is_same_v<Mo, MoTreeVertex>) {
        mo.run(av, dv, out);
    } else if constexpr (is_same_v<Mo, MoTreeEdge>) {
        mo.run(ae, de, out);
    } else {
        mo.run(av, dv, ae, de, out);
    }
    for (int count : seen) assert(count == 1);
    if (expected.empty()) {
        assert(vertices.callback_count == 0);
        assert(edges.callback_count == 0);
    }
}

template <class Mo>
void add_queries(Mo &mo, const vector<Query> &queries) {
    for (int i = 0; i < (int)queries.size(); ++i) {
        auto [u, v] = queries[i];
        const int qid = mo.add_query(u, v);
        assert(qid == i);
    }
}

template <class Mo>
void exercise(Mo &mo, int n, const vector<Edge> &edges, const vector<Query> &queries,
              bool empty_run, bool repeat) {
    if (empty_run) check_run(mo, n, {});
    add_queries(mo, queries);
    auto expected = naive_paths(n, edges, queries);
    check_run(mo, n, expected);
    if (!repeat) return;
    check_run(mo, n, expected);

    if (!queries.empty()) {
        auto [u, v] = queries.front();
        const int qid = mo.add_query(u, v);
        assert(qid == (int)expected.size());
        expected.push_back(expected.front());
        check_run(mo, n, expected);
    }
}

template <class Mo>
void check_type(const Graph &g, const vector<Query> &queries,
                int root, int new_root, bool repeat = true) {
    const int n = g.size();
    // 辺番号は u < v の走査順
    vector<Edge> graph_edges;
    for (int u = 0; u < n; ++u) {
        for (int v : g[u]) {
            if (u < v) graph_edges.emplace_back(u, v);
        }
    }
    {
        Mo mo(g, root);
        exercise(mo, n, graph_edges, queries, true, repeat);
    }
    if (!repeat) return;
    {
        Mo mo(g, new_root);
        vector<Query> reversed_queries = queries;
        reverse(reversed_queries.begin(), reversed_queries.end());
        for (auto &[u, v] : reversed_queries) swap(u, v);
        exercise(mo, n, graph_edges, reversed_queries, false, repeat);
    }
    Mo mo(g);
    exercise(mo, n, graph_edges, queries, false, false);
}

template <class Mo>
void add_edges(Mo &mo, const vector<Edge> &edges) {
    int expected = 0;
    for (auto [u, v] : edges) {
        const int id = mo.add_edge(u, v);
        assert(id == expected++);
    }
}

template <class Mo>
void check_insertion_type(int n, const vector<Edge> &edges, const vector<Query> &queries,
                          int root, int new_root, bool repeat = true) {
    {
        Mo mo(n, root);
        add_edges(mo, edges);
        exercise(mo, n, edges, queries, true, repeat);
    }
    if (!repeat) return;
    {
        Mo mo(n, new_root);
        add_edges(mo, edges);
        vector<Query> reversed_queries = queries;
        reverse(reversed_queries.begin(), reversed_queries.end());
        for (auto &[u, v] : reversed_queries) swap(u, v);
        exercise(mo, n, edges, reversed_queries, false, repeat);
    }
    Mo mo(n);
    add_edges(mo, edges);
    exercise(mo, n, edges, queries, false, false);
}

void check_tree(int n, vector<Edge> edges, mt19937_64 &rng) {
    vector<int> labels(n);
    iota(labels.begin(), labels.end(), 0);
    shuffle(labels.begin(), labels.end(), rng);
    for (auto &[u, v] : edges) {
        u = labels[u];
        v = labels[v];
        if (rng() & 1) swap(u, v);
    }
    shuffle(edges.begin(), edges.end(), rng);
    Graph g(n);
    for (auto [u, v] : edges) {
        g[u].push_back(v);
        g[v].push_back(u);
    }
    for (auto &adj : g) shuffle(adj.begin(), adj.end(), rng);
    const int root = rng() % n, new_root = (root + n / 2) % n;
    vector<Query> queries;
    for (int v = 0; v < n; ++v) {
        queries.emplace_back(v, v);
        queries.emplace_back(root, v);
        queries.emplace_back(v, root);
    }
    for (int i = 0; i < 100; ++i) {
        const int u = rng() % n, v = rng() % n;
        queries.emplace_back(u, v);
        queries.emplace_back(v, u);
        queries.emplace_back(u, v);
    }
    if (n <= 10) {
        for (int u = 0; u < n; ++u) {
            for (int v = 0; v < n; ++v) queries.emplace_back(u, v);
        }
    }
    shuffle(queries.begin(), queries.end(), rng);
    check_type<MoTreeVertex>(g, queries, root, new_root);
    check_type<MoTreeEdge>(g, queries, root, new_root);
    check_type<MoTreeVertexEdge>(g, queries, root, new_root);
    check_insertion_type<MoTreeVertex>(n, edges, queries, root, new_root);
    check_insertion_type<MoTreeEdge>(n, edges, queries, root, new_root);
    check_insertion_type<MoTreeVertexEdge>(n, edges, queries, root, new_root);
}

template <class Mo>
void check_incremental_queries() {
    constexpr int n = 7;
    const vector<Edge> edges = {{0, 1}, {0, 2}, {1, 3}, {1, 4}, {2, 5}, {2, 6}};
    const vector<vector<Query>> batches = {
        {{3, 4}, {3, 3}},
        {{5, 6}, {3, 5}, {1, 4}},
        {{0, 6}, {6, 4}, {2, 2}}
    };
    Graph g(n);
    for (auto [u, v] : edges) {
        g[u].push_back(v);
        g[v].push_back(u);
    }
    auto check = [&] (Mo &mo) {
        check_run(mo, n, {});
        vector<Query> queries;
        for (const auto &batch : batches) {
            for (auto [u, v] : batch) {
                const int expected_id = queries.size();
                const int qid = mo.add_query(u, v);
                assert(qid == expected_id);
                queries.emplace_back(u, v);
            }
            auto expected = naive_paths(n, edges, queries);
            check_run(mo, n, expected);
            check_run(mo, n, expected);
        }
    };
    for (int root : {0, 3}) {
        Mo graph_mo(g, root), edge_mo(n, root);
        add_edges(edge_mo, edges);
        check(graph_mo);
        check(edge_mo);
    }
}

void check_long_chain() {
    constexpr int n = 200000;
    Graph g(n);
    for (int v = 1; v < n; ++v) {
        g[v - 1].push_back(v);
        g[v].push_back(v - 1);
    }
    const vector<Query> queries = {
        {0, n - 1}, {n - 1, 0}, {n / 3, n / 3},
        {n / 4, 3 * n / 4}, {0, 0}, {n - 1, n - 1}
    };
    check_type<MoTreeVertex>(g, queries, n / 3, 0, false);
    check_type<MoTreeEdge>(g, queries, n / 3, 0, false);
    check_type<MoTreeVertexEdge>(g, queries, n / 3, 0, false);
    vector<Edge> edges;
    for (int v = n - 1; v > 0; --v) edges.emplace_back(v, v - 1);
    check_insertion_type<MoTreeVertex>(n, edges, queries, n / 3, 0, false);
    check_insertion_type<MoTreeEdge>(n, edges, queries, n / 3, 0, false);
    check_insertion_type<MoTreeVertexEdge>(n, edges, queries, n / 3, 0, false);
}

int main() {
    mt19937_64 rng(0x4d6f54726565ULL);
    check_incremental_queries<MoTreeVertex>();
    check_incremental_queries<MoTreeEdge>();
    check_incremental_queries<MoTreeVertexEdge>();
    for (int n : {1, 2, 3, 8, 31, 70}) {
        vector<Edge> path, star, balanced;
        for (int v = 1; v < n; ++v) {
            path.emplace_back(v - 1, v);
            star.emplace_back(0, v);
            balanced.emplace_back((v - 1) / 2, v);
        }
        check_tree(n, path, rng);
        check_tree(n, star, rng);
        check_tree(n, balanced, rng);
    }
    for (int trial = 0; trial < 100; ++trial) {
        const int n = 1 + rng() % 80;
        vector<Edge> edges;
        for (int v = 1; v < n; ++v) edges.emplace_back(rng() % v, v);
        check_tree(n, edges, rng);
    }
    check_long_chain();
    cout << "mo_tree_random: OK\n";
}
