/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/tree/offline_lca.cpp
#pragma once

#include <cassert>
#include <utility>
#include <vector>
#include "titan_cpplib/ds/union_find.cpp"
using namespace std;

namespace titan23 {

/// @brief Tarjan のオフライン LCA
class OfflineLCA {
private:
    int n, root, q = 0;
    vector<vector<int>> graph;
    vector<vector<pair<int, int>>> queries;

public:
    /// @brief 無向木 G と根 root で初期化 / O(N)
    OfflineLCA(const vector<vector<int>> &G, int root = 0)
        : n(G.size()), root(root), graph(G), queries(n) {}

    /// @brief クエリを登録し、登録順の番号を返す / O(1)
    int add_query(int u, int v) {
        assert(0 <= u && u < n && 0 <= v && v < n);
        queries[u].emplace_back(v, q);
        if (u != v) queries[v].emplace_back(u, q);
        return q++;
    }

    /// @brief 登録順に LCA を返す / O((N+Q)α(N)) 時間、O(N+Q) 空間
    vector<int> run() const {
        vector<int> ans(q);
        if (q == 0) return ans;
        titan23::UnionFind uf(n);
        vector<int> a(n), par(n, -1), next(n, 0), st;
        vector<bool> finish(n, false);
        st.reserve(n);
        st.push_back(root);
        a[root] = root;
        while (!st.empty()) {
            const int v = st.back();
            if (next[v] < (int)graph[v].size()) {
                const int u = graph[v][next[v]++];
                if (u == par[v]) continue;
                par[u] = v;
                a[u] = u;
                st.push_back(u);
                continue;
            }
            finish[v] = true;
            for (const auto &[u, id] : queries[v]) {
                if (finish[u]) ans[id] = a[uf.root(u)];
            }
            st.pop_back();
            if (par[v] != -1) {
                uf.unite(par[v], v);
                a[uf.root(par[v])] = par[v];
            }
        }
        return ans;
    }
};

}
