/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/tree/mo_tree_base.cpp
#pragma once

#include <cassert>
#include <utility>
#include <vector>
#include "titan_cpplib/alg/mo.cpp"
#include "titan_cpplib/graph/tree/offline_lca.cpp"
using namespace std;

namespace titan23 {

// 木上の Mo の共通実装
template<bool UseVertex>
class MoTreeBase {
private:
    int n, root, edge_count = 0, query_count = 0;
    bool built = false;
    vector<vector<pair<int, int>>> graph;
    vector<int> nodein, tour, par_edge;
    vector<pair<int, int>> queries;
    titan23::Mo mo;

protected:
    template<bool UseEdge, typename AV, typename DV, typename AE, typename DE, typename OUT>
    void run_impl(AV &add_vertex, DV &del_vertex, AE &add_edge, DE &del_edge, OUT &out) {
        const vector<int> values;
        run_impl<UseEdge, false>(values, add_vertex, del_vertex, add_edge, del_edge, out);
    }

    template<bool UseEdge, bool UseValue, typename T, typename AV, typename DV, typename AE, typename DE, typename OUT>
    void run_impl(const vector<T> &values, AV &add_vertex, DV &del_vertex, AE &add_edge, DE &del_edge, OUT &out) {
        if (!built) build();
        if (query_count == 0) return;
        const auto value_tour = [&] {
            vector<pair<int, T>> result;
            if constexpr (UseValue) {
                result.reserve(tour.size());
                for (int v : tour) result.emplace_back(v, values[v]);
            }
            return result;
        }();
        vector<int> lcas;
        if constexpr (UseVertex) {
            vector<vector<int>> G(n);
            for (int v = 0; v < n; ++v) {
                G[v].reserve(graph[v].size());
                for (const auto &[u, id] : graph[v]) G[v].push_back(u);
            }
            titan23::OfflineLCA lca(G, root);
            for (const auto &[u, v] : queries) lca.add_query(u, v);
            lcas = lca.run();
        }
        vector<bool> contain(n, false);
        auto change = [&] (int i) {
            const int v = UseValue ? value_tour[i].first : tour[i];
            if (contain[v]) {
                if constexpr (UseVertex) {
                    if constexpr (UseValue) del_vertex(value_tour[i].second);
                    else del_vertex(v);
                }
                if constexpr (UseEdge) del_edge(par_edge[v]);
            } else {
                if constexpr (UseVertex) {
                    if constexpr (UseValue) add_vertex(value_tour[i].second);
                    else add_vertex(v);
                }
                if constexpr (UseEdge) add_edge(par_edge[v]);
            }
            contain[v] = !contain[v];
        };
        auto answer = [&] (int i) {
            // パスから欠けている LCA の頂点だけを補う
            if constexpr (UseVertex) {
                if constexpr (UseValue) add_vertex(values[lcas[i]]);
                else add_vertex(lcas[i]);
            }
            out(i);
            if constexpr (UseVertex) {
                if constexpr (UseValue) del_vertex(values[lcas[i]]);
                else del_vertex(lcas[i]);
            }
        };
        mo.run_light(change, change, answer);
    }

public:
    /// @brief n 頂点、根 root で初期化
    MoTreeBase(int n, int root = 0) : n(n), root(root), graph(n), mo(0) {}

    /// @brief 無向木 G と根 root から構築
    MoTreeBase(const vector<vector<int>> &G, int root = 0) : MoTreeBase(G.size(), root) {
        for (int u = 0; u < n; ++u) {
            for (int v : G[u]) {
                if (u < v) add_edge(u, v);
            }
        }
        build();
    }

    /// @brief 辺属性付き無向木 G と根 root から構築
    template<typename T>
    MoTreeBase(const vector<vector<pair<int, T>>> &G, int root = 0) : MoTreeBase(G.size(), root) {
        for (int u = 0; u < n; ++u) {
            for (const auto &edge : G[u]) {
                if (u < edge.first) add_edge(u, edge.first);
            }
        }
        build();
    }

    /// @brief 無向辺を追加し、登録順の辺番号を返す / O(1)
    int add_edge(int u, int v) {
        assert(!built);
        graph[u].emplace_back(v, edge_count);
        graph[v].emplace_back(u, edge_count);
        return edge_count++;
    }

    /// @brief u-v パスを登録し、登録順のクエリ番号を返す / 前計算後は償却 O(1)
    int add_query(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        if (!built) build();
        int l = nodein[u], r = nodein[v];
        if (l > r) swap(l, r);
        mo.add_query(l, r);
        if constexpr (UseVertex) queries.emplace_back(u, v);
        return query_count++;
    }

private:
    void build() {
        nodein.resize(n);
        par_edge.assign(n, -1);
        tour.reserve(2 * (n - 1));
        vector<int> next(n, 0), stack;
        stack.reserve(n);
        stack.push_back(root);
        nodein[root] = 0;
        while (!stack.empty()) {
            const int v = stack.back();
            if (next[v] == (int)graph[v].size()) {
                stack.pop_back();
                if (!stack.empty()) tour.push_back(v);
                continue;
            }
            const auto [u, id] = graph[v][next[v]++];
            if (id == par_edge[v]) continue;
            par_edge[u] = id;
            tour.push_back(u);
            nodein[u] = tour.size();
            stack.push_back(u);
        }
        mo = titan23::Mo(tour.size());
        built = true;
    }
};

}
