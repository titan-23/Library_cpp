/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/tree/centroid_decomposition.cpp
#pragma once

#include <vector>
using namespace std;

namespace titan23 {

/// @brief 木を重心分解し、重心分解木を作る
class CentroidDecomposition {
private:
    int C;
    vector<vector<int>> G, T;
    vector<int> sub;
    vector<bool> banned, solve_banned;

    int dfs(int v, int p) {
        sub[v] = 1;
        for (int x : G[v]) {
            if (x == p || banned[x]) continue;
            sub[v] += dfs(x, v);
        }
        return sub[v];
    }

    int find_centroid(int v, int p, int mid) const {
        for (int x : G[v]) {
            if (x == p || banned[x]) continue;
            if (sub[x] > mid) return find_centroid(x, v, mid);
        }
        return v;
    }

    int build(int v) {
        int total = dfs(v, -1);
        int c = find_centroid(v, -1, total/2);
        banned[c] = true;
        for (int x : G[c]) if (!banned[x]) {
            int w = build(x);
            T[c].push_back(w);
            T[w].push_back(c);
        }
        return c;
    }

    template <typename F>
    void inner_solve(int v, int p, F &&func) {
        func(v, solve_banned);
        solve_banned[v] = true;
        for (int x : T[v]) if (x != p) {
            inner_solve(x, v, func);
        }
    }

public:
    CentroidDecomposition() : C(-1) {}
    /// @brief 木の隣接リスト G から重心分解する / O(n log n)
    CentroidDecomposition(const vector<vector<int>> &G) : G(G) {
        int n = G.size();
        if (n == 0) return;
        T.resize(n);
        sub.resize(n);
        banned.resize(n, false);
        solve_banned.resize(n);
        C = build(0);
    }

    /// @brief 重心分解木の根 C と隣接リスト T を返す
    pair<int, vector<vector<int>>> get_result() {
        return {C, T};
    }

    /// @brief 重心分解木を根から葉へたどり、各頂点 v で func(v, banned) を呼ぶ / banned は上位の中心で true / func は元の木 G を探索し、banned が true の頂点には入らない
    // cd.solve([&] (int v, const vector<bool>& banned) { ... });
    template <typename F>
    void solve(F &&func) {
        if (G.empty()) return;
        fill(solve_banned.begin(), solve_banned.end(), false);
        inner_solve(C, -1, func);
    }
};
} // namespace titan23
