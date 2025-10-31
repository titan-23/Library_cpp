#include <vector>
using namespace std;

namespace titan23 {
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

    int find_centroid(int v, int p, int mid) {
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
    void inner_solve(int v, int p, F& func) {
        func(v, solve_banned);
        solve_banned[v] = true;
        for (int x : T[v]) if (x != p) {
            inner_solve(x, v, func);
        }
    }

public:
    CentroidDecomposition(vector<vector<int>> G) : G(G) {
        int n = G.size();
        T.resize(n);
        sub.resize(n);
        banned.resize(n, false);
        solve_banned.resize(n);
        C = build(0);
    }

    pair<int, vector<vector<int>>> get_result() {
        return {C, T};
    }

    // vをまたぐパスについて考える
    // vのみ / vを端点 / vを端点としない などの場合分け
    // 元の木と重心分解の木の混同に注意
    // bannedを見てはいけない
    // cd.solve([&] (int v, const vector<bool>& banned) {});
    template <typename F>
    void solve(F func) {
        fill(solve_banned.begin(), solve_banned.end(), false);
        inner_solve(C, -1, func);
    }
};
} // namespace titan23
