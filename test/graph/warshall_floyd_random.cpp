#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <tuple>
#include <vector>

#include "titan_cpplib/graph/warshall_floyd.cpp"
#include "titan_cpplib/graph/warshall_floyd_path.cpp"
#include "titan_cpplib/graph/warshall_floyd_simd.cpp"
#include "titan_cpplib/graph/warshall_floyd_simd_nonnegative.cpp"

using namespace std;

namespace {

using Edge = tuple<int, int, long long>;

// 素朴な Floyd 法による参照解。負閉路で値が際限なく小さくなるのを防ぐため -BIG で打ち切る
struct Reference {
    static constexpr long long UNREACHABLE = numeric_limits<long long>::max();
    static constexpr long long BIG = 1000000000000000LL;
    int n;
    vector<vector<long long>> dist;
    vector<vector<int>> neg_inf;
    vector<vector<long long>> edge;  // 頂点対ごとの最小の辺の重み
    bool neg_cycle = false;

    Reference(const int n, const vector<Edge> &edges)
        : n(n), dist(n, vector<long long>(n, UNREACHABLE)), neg_inf(n, vector<int>(n, 0)),
          edge(n, vector<long long>(n, UNREACHABLE)) {
        for (int v = 0; v < n; ++v) dist[v][v] = 0;
        for (const auto &[u, v, w] : edges) {
            dist[u][v] = min(dist[u][v], w);
            edge[u][v] = min(edge[u][v], w);
        }
        for (int k = 0; k < n; ++k) {
            for (int i = 0; i < n; ++i) {
                if (dist[i][k] == UNREACHABLE) continue;
                for (int j = 0; j < n; ++j) {
                    if (dist[k][j] == UNREACHABLE) continue;
                    dist[i][j] = min(dist[i][j], max(-BIG, dist[i][k] + dist[k][j]));
                }
            }
        }
        for (int k = 0; k < n; ++k) {
            if (dist[k][k] >= 0) continue;
            neg_cycle = true;
            for (int i = 0; i < n; ++i) {
                if (dist[i][k] == UNREACHABLE) continue;
                for (int j = 0; j < n; ++j) {
                    if (dist[k][j] != UNREACHABLE) neg_inf[i][j] = 1;
                }
            }
        }
    }

    bool reachable(const int s, const int t) const { return neg_inf[s][t] || dist[s][t] != UNREACHABLE; }
};

template<class T>
vector<vector<pair<int, T>>> to_graph(const int n, const vector<Edge> &edges) {
    vector<vector<pair<int, T>>> graph(n);
    for (const auto &[u, v, w] : edges) graph[u].emplace_back(v, T(w));
    return graph;
}

// 負閉路を扱う 3 クラスに共通の検査。inf は「到達不能」を表すそのクラスの値
template<class WF, class T>
void check_general(const WF &wf, const Reference &ref, const T inf, const bool has_neg_cycle) {
    assert(has_neg_cycle == ref.neg_cycle);
    for (int s = 0; s < ref.n; ++s) {
        for (int t = 0; t < ref.n; ++t) {
            assert(wf.is_neg_inf(s, t) == bool(ref.neg_inf[s][t]));
            assert(wf.reachable(s, t) == ref.reachable(s, t));
            if (ref.neg_inf[s][t]) continue;
            const long long expected = ref.dist[s][t] == Reference::UNREACHABLE ? (long long)inf : ref.dist[s][t];
            assert((long long)wf.get_dist(s, t) == expected);
        }
    }
}

template<class T>
void check_path(const titan23::WarshallFloydPath<T> &wf, const Reference &ref) {
    for (int s = 0; s < ref.n; ++s) {
        for (int t = 0; t < ref.n; ++t) {
            const vector<int> path = wf.get_path(s, t);
            if (ref.neg_inf[s][t] || ref.dist[s][t] == Reference::UNREACHABLE) {
                assert(path.empty());
                continue;
            }
            // 経路は s から t への辺の列で、重みの和が最短距離に等しい
            assert(!path.empty() && path.front() == s && path.back() == t);
            long long total = 0;
            for (int i = 0; i + 1 < (int)path.size(); ++i) {
                const long long w = ref.edge[path[i]][path[i + 1]];
                assert(w != Reference::UNREACHABLE);
                total += w;
            }
            assert(total == ref.dist[s][t]);
        }
    }
}

template<class T>
void run(const uint64_t seed, const int instances) {
    mt19937_64 rng(seed);
    const T path_infs[] = {numeric_limits<T>::max() / 2, numeric_limits<T>::max()};
    for (int instance = 0; instance < instances; ++instance) {
        const int n = 1 + rng() % 40;
        const int m = rng() % (3 * n + 1);
        // 負の辺の割合を変えて、負閉路のない場合とある場合の両方を作る
        const int negative_percent = instance % 3 == 0 ? 0 : instance % 3 == 1 ? 3 : 15;
        auto random_edge = [&](const bool nonnegative) -> Edge {
            const int u = rng() % n, v = rng() % n;
            long long w = rng() % 101;
            if (!nonnegative && int(rng() % 100) < negative_percent) w = -w;
            return {u, v, w};
        };
        vector<Edge> edges;
        for (int i = 0; i < m; ++i) edges.push_back(random_edge(false));
        vector<Edge> nonnegative_edges;
        for (int i = 0; i < m; ++i) nonnegative_edges.push_back(random_edge(true));

        const T plain_inf = path_infs[instance % 2];
        titan23::WarshallFloyd<T> plain(to_graph<T>(n, edges), plain_inf);
        titan23::WarshallFloydSIMD<T> simd(to_graph<T>(n, edges));
        titan23::WarshallFloydPath<T> path(to_graph<T>(n, edges), plain_inf);
        titan23::WarshallFloydSIMDNonnegative<T> nonnegative(to_graph<T>(n, nonnegative_edges));

        const int additions = rng() % 8;
        for (int step = 0; step <= additions; ++step) {
            if (step > 0) {
                const Edge e = random_edge(false);
                const auto [u, v, w] = e;
                edges.push_back(e);
                plain.add_edge(u, v, T(w));
                simd.add_edge(u, v, T(w));
                path.add_edge(u, v, T(w));
                const Edge f = random_edge(true);
                const auto [fu, fv, fw] = f;
                nonnegative_edges.push_back(f);
                nonnegative.add_edge(fu, fv, T(fw));
            }
            const Reference ref(n, edges);
            check_general(plain, ref, plain_inf, plain.has_neg_cycle());
            check_general(simd, ref, titan23::WarshallFloydSIMD<T>::INF, simd.has_neg_cycle());
            check_general(path, ref, plain_inf, path.has_negative_cycle());
            check_path(path, ref);

            const Reference nonnegative_ref(n, nonnegative_edges);
            for (int s = 0; s < n; ++s) {
                for (int t = 0; t < n; ++t) {
                    const long long d = nonnegative_ref.dist[s][t];
                    const long long expected = d == Reference::UNREACHABLE ? (long long)titan23::WarshallFloydSIMDNonnegative<T>::INF : d;
                    assert((long long)nonnegative.get_dist(s, t) == expected);
                }
            }
        }
    }
}

}  // namespace

int main() {
    run<long long>(1, 1500);
    run<int>(2, 1500);
    cout << "Warshall-Floyd random test: OK" << endl;
    return 0;
}
