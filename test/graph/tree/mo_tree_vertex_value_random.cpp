#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "titan_cpplib/graph/tree/mo_tree_vertex_value.cpp"

using namespace std;
using titan23::MoTreeVertexValue;

using Graph = vector<vector<int>>;
using Edge = pair<int, int>;
using Query = pair<int, int>;

static_assert(is_constructible_v<MoTreeVertexValue<int>, const vector<int> &>);
static_assert(is_constructible_v<MoTreeVertexValue<int>, const vector<int> &, int>);
static_assert(is_constructible_v<MoTreeVertexValue<int>, const Graph &, const vector<int> &>);
static_assert(is_constructible_v<MoTreeVertexValue<int>, const Graph &, const vector<int> &, int>);

template<typename T, typename KEY>
auto naive_counts(const Graph &g, const vector<T> &values,
                  const vector<Query> &queries, KEY key) {
    using Key = decltype(key(values.front()));
    vector<map<Key, int>> expected;
    const int n = g.size();
    for (auto [s, t] : queries) {
        vector<int> parent(n, -1);
        queue<int> que;
        parent[s] = s;
        que.push(s);
        while (parent[t] == -1) {
            const int v = que.front();
            que.pop();
            for (int u : g[v]) {
                if (parent[u] != -1) continue;
                parent[u] = v;
                que.push(u);
            }
        }
        map<Key, int> counts;
        for (int v = t; v != s; v = parent[v]) ++counts[key(values[v])];
        ++counts[key(values[s])];
        expected.push_back(move(counts));
    }
    return expected;
}

template<typename T, typename Key, typename KEY>
void check_run(MoTreeVertexValue<T> &mo, const vector<map<Key, int>> &expected, KEY key) {
    map<Key, int> counts;
    const int q = expected.size();
    vector<int> seen(q);
    int callbacks = 0;
    mo.run(
        [&] (const T &value) {
            ++counts[key(value)];
            ++callbacks;
        },
        [&] (const T &value) {
            auto it = counts.find(key(value));
            assert(it != counts.end());
            if (--it->second == 0) counts.erase(it);
            ++callbacks;
        },
        [&] (int i) {
            assert(0 <= i && i < q);
            assert(seen[i]++ == 0);
            assert(counts == expected[i]);
        }
    );
    for (int count : seen) assert(count == 1);
    if (expected.empty()) assert(callbacks == 0);
}

template<typename T, typename Key, typename KEY>
void exercise(MoTreeVertexValue<T> &mo, const vector<Query> &queries,
              const vector<map<Key, int>> &expected, KEY key) {
    vector<map<Key, int>> prefix;
    check_run(mo, prefix, key);
    const int q = queries.size();
    int registered = 0;
    for (int end : {q / 2, q}) {
        for (; registered < end; ++registered) {
            auto [u, v] = queries[registered];
            const int id = mo.add_query(u, v);
            assert(id == registered);
            prefix.push_back(expected[registered]);
        }
        check_run(mo, prefix, key);
        check_run(mo, prefix, key);
    }
}

template<typename T, typename KEY>
void check_constructors(const Graph &g, const vector<Edge> &edges,
                        const vector<T> &values, const vector<Query> &queries,
                        const vector<int> &roots, const T &replacement, KEY key) {
    const auto expected = naive_counts(g, values, queries, key);
    for (int root : roots) {
        {
            vector<T> source = values;
            MoTreeVertexValue<T> mo(g, source, root);
            fill(source.begin(), source.end(), replacement);
            source.clear();
            source.shrink_to_fit();
            exercise(mo, queries, expected, key);
        }
        {
            vector<T> source = values;
            MoTreeVertexValue<T> mo(source, root);
            fill(source.begin(), source.end(), replacement);
            source.clear();
            source.shrink_to_fit();
            int next_id = 0;
            for (auto [u, v] : edges) {
                const int id = mo.add_edge(u, v);
                assert(id == next_id++);
            }
            exercise(mo, queries, expected, key);
        }
    }
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
    vector<int> values(n);
    for (int &value : values) {
        value = rng() % 9;
        value -= 4;
    }
    vector<Query> queries;
    for (int v = 0; v < n; ++v) {
        queries.emplace_back(v, v);
        queries.emplace_back(0, v);
        queries.emplace_back(v, 0);
    }
    for (int i = 0; i < 80; ++i) {
        const int u = rng() % n, v = rng() % n;
        queries.emplace_back(u, v);
        queries.emplace_back(v, u);
        queries.emplace_back(u, v);
    }
    if (n <= 8) {
        for (int u = 0; u < n; ++u) {
            for (int v = 0; v < n; ++v) queries.emplace_back(u, v);
        }
    }
    shuffle(queries.begin(), queries.end(), rng);
    check_constructors(g, edges, values, queries, {0, n / 2}, 1000007,
                       [] (const int &value) { return value; });
}

struct Payload {
    int color;
    string label;

    Payload(int color, string label) : color(color), label(move(label)) {}
};

static_assert(!is_default_constructible_v<Payload>);
static_assert(is_copy_constructible_v<Payload>);
static_assert(!is_convertible_v<int, Payload>);

void check_payload() {
    const vector<Edge> edges = {{0, 1}, {0, 2}, {1, 3}, {1, 4}, {2, 5}, {2, 6}};
    Graph g(7);
    for (auto [u, v] : edges) {
        g[u].push_back(v);
        g[v].push_back(u);
    }
    const vector<Payload> values = {
        {7, "root"}, {2, "branch"}, {2, "branch"}, {4, "leaf"},
        {7, "leaf"}, {4, "leaf"}, {2, "last"}
    };
    vector<Query> queries;
    for (int u = 0; u < 7; ++u) {
        for (int v = 0; v < 7; ++v) queries.emplace_back(u, v);
    }
    check_constructors(g, edges, values, queries, {0, 3, 6}, Payload(-1, "changed"),
                       [] (const Payload &value) { return make_pair(value.color, value.label); });
}

void check_default_root() {
    const Graph g = {{1, 2}, {0}, {0}};
    const vector<int> values = {30, 10, 20};
    const vector<Query> queries = {{1, 2}, {0, 0}, {2, 2}};
    auto key = [] (const int &value) { return value; };
    const auto expected = naive_counts(g, values, queries, key);
    MoTreeVertexValue<int> graph_mo(g, values), edge_mo(values);
    edge_mo.add_edge(0, 1);
    edge_mo.add_edge(0, 2);
    exercise(graph_mo, queries, expected, key);
    exercise(edge_mo, queries, expected, key);
}

void check_long_chain() {
    constexpr int n = 200000;
    Graph g(n);
    vector<Edge> edges;
    vector<int> values(n);
    for (int v = 0; v < n; ++v) {
        values[v] = v % 7 - 3;
        if (v == 0) continue;
        g[v - 1].push_back(v);
        g[v].push_back(v - 1);
        edges.emplace_back(v, v - 1);
    }
    const vector<Query> queries = {
        {0, n - 1}, {n - 1, 0}, {n / 3, n / 3}, {n / 4, 3 * n / 4},
        {0, 0}, {n - 1, n - 1}, {0, n / 3}, {n / 3, n - 1}
    };
    check_constructors(g, edges, values, queries, {n / 3}, 1000007,
                       [] (const int &value) { return value; });
}

int main() {
    mt19937_64 rng(0x4d6f56616c7565ULL);
    check_default_root();
    check_payload();
    for (int n : {1, 2, 3, 7, 31, 70}) {
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
    cout << "mo_tree_vertex_value_random: OK\n";
}
