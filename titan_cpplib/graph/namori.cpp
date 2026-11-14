/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/namori.cpp
#pragma once

#include <vector>
#include <algorithm>
#include <queue>
#include <random>
#include <climits>
#include <stdexcept>
#include <utility>
using namespace std;

namespace titan23 {

using u64 = unsigned long long;

/// @brief なもりグラフをサイクルと、サイクル上の各頂点を根とする木に分ける / 連結で N = M の無向グラフを扱う
class Namori {
private:
    int n;
    vector<vector<int>> G;
    vector<bool> is_cycle;

public:
    vector<int> cycle;                   // サイクルを構成する頂点列
    vector<vector<vector<int>>> forest;  // forest[i] は cycle[i] を根とする木の辺 {子, 親} の列

private:
    using u128 = __uint128_t;
    static constexpr u64 MOD = (1ull<<61)-1;

    static u64 mul(u64 a, u64 b) {
        u128 t = (u128)a * b;
        t = (t>>61) + (t&MOD);
        if (t >= MOD) t -= MOD;
        return t;
    }

    static u64 mod(u64 a) {
        a = (a>>61) + (a&MOD);
        if (a >= MOD) a -= MOD;
        return a;
    }

public:
    Namori() : n(0) {}

    /// @brief 隣接リスト G から分解する / 各辺は両端に入れ、自己ループは同じ頂点に 2 回入れる / 条件を満たさなければ invalid_argument / O(n)
    Namori(const vector<vector<int>> &G) : n(G.size()), G(G) {
        if (n == 0) throw invalid_argument("Namori: graph must not be empty");
        vector<int> deg(n, 0);
        long long deg_sum = 0;
        for (int v = 0; v < n; ++v) for (int x : G[v]) {
            if (x < 0 || x >= n) throw invalid_argument("Namori: vertex is out of range");
            deg[x]++;
            deg_sum++;
        }
        if (deg_sum != 2LL*n) throw invalid_argument("Namori: number of edges must equal number of vertices");
        for (int v = 0; v < n; ++v) {
            if (deg[v] != (int)G[v].size()) throw invalid_argument("Namori: adjacency lists must be symmetric");
        }
        // 次数1の頂点を削っていき、残った頂点がサイクル
        queue<int> todo;
        for (int v = 0; v < n; ++v) if (deg[v] == 1) todo.push(v);
        is_cycle.resize(n, true);
        while (!todo.empty()) {
            int v = todo.front(); todo.pop();
            is_cycle[v] = false;
            for (int x : G[v]) {
                deg[x]--;
                if (deg[x] == 1) todo.push(x);
            }
        }
        vector<vector<vector<int>>> F(n);
        vector<bool> seen(n, false);
        for (int v = 0; v < n; ++v) if (is_cycle[v]) {
            vector<vector<int>> tree;
            queue<pair<int, int>> qu; qu.push({v, -1});
            seen[v] = true;
            while (!qu.empty()) {
                auto [a, p] = qu.front(); qu.pop();
                if (p != -1) tree.push_back({a, p});
                for (int x : G[a]) if (!is_cycle[x] && !seen[x]) {
                    seen[x] = true;
                    qu.push({x, a});
                }
            }
            F[v] = move(tree);
        }
        for (int i = 0; i < n; ++i) {
            if (!seen[i]) throw invalid_argument("Namori: graph must be connected");
        }
        int v = -1;
        for (int i = 0; i < n; ++i) if (is_cycle[i]) {
            v = i;
            break;
        }
        if (v == -1) throw invalid_argument("Namori: graph must have a cycle");
        vector<bool> visit(n, false);
        visit[v] = true;
        cycle.push_back(v);
        forest.push_back(move(F[v]));
        while (true) {
            bool upd = false;
            for (int x : G[v]) {
                if (is_cycle[x] && !visit[x]) {
                    upd = true;
                    visit[x] = true;
                    v = x;
                    break;
                }
            }
            if (!upd) break;
            cycle.push_back(v);
            forest.push_back(move(F[v]));
        }
        int cycle_cnt = 0;
        for (int i = 0; i < n; ++i) if (is_cycle[i]) cycle_cnt++;
        if ((int)cycle.size() != cycle_cnt) throw invalid_argument("Namori: graph must have exactly one cycle");
    }

    /// @brief 同型判定用のハッシュ値を返す / 比べる 2 つのグラフには同じ seed を使う / O(n)
    u64 get_hash(int seed) const {
        mt19937_64 rnd(seed);
        uniform_int_distribution<u64> dist(47, MOD-1);
        vector<u64> R(n+1);
        for (u64 &r : R) r = dist(rnd);
        u64 base = uniform_int_distribution<u64>(414123123, 1000000000)(rnd);

        // 木のハッシュ hs[v] = prod_{子 x} (R[height[x]] + hs[x]) を求める。height は葉が 1
        vector<u64> hs(n, 1);
        vector<int> height(n, 1), parent(n, -1), order;
        order.reserve(n);
        for (int root : cycle) {
            int begin = order.size();
            order.push_back(root);
            for (int i = begin; i < (int)order.size(); ++i) {
                int v = order[i];
                for (int x : G[v]) if (x != parent[v] && !is_cycle[x]) {
                    parent[x] = v;
                    order.push_back(x);
                }
            }
            // 幅優先順の逆順に見れば、子は親より先に確定する
            for (int i = (int)order.size()-1; i > begin; --i) {
                int x = order[i], p = parent[x];
                hs[p] = mul(hs[p], mod(R[height[x]] + hs[x]));
                height[p] = max(height[p], height[x]+1);
            }
        }

        // 木のハッシュをサイクル順に並べた列 H の、回転と反転のうち最小のローリングハッシュを返す
        int c = cycle.size();
        vector<u64> H(2*c);
        for (int i = 0; i < c; ++i) H[i] = H[i+c] = hs[cycle[i]];
        u64 pow_c = 1;
        for (int i = 0; i < c; ++i) pow_c = mul(pow_c, base);
        u64 res = ULLONG_MAX;
        vector<u64> acc(2*c+1);
        for (int r = 0; r < 2; ++r) {
            // acc[i] = sum_{j<i} (H[j]+1) base^(i-1-j)
            for (int i = 0; i < 2*c; ++i) acc[i+1] = mod(mul(acc[i], base) + H[i] + 1);
            for (int i = 0; i < c; ++i) {
                // H[i, i+c) のローリングハッシュ
                res = min(res, mod(acc[i+c] + MOD - mul(acc[i], pow_c)));
            }
            reverse(H.begin(), H.end());
        }
        return res;
    }
};
}  // namespace titan23
