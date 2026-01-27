#include <vector>
#include <algorithm>
#include <queue>
#include "titan_cpplib/string/hash_string.cpp"
using namespace std;

namespace titan23 {

class Namori {
private:
    int n;
    vector<vector<int>> G;
    vector<bool> is_cycle;
    vector<int> cycle;
    vector<vector<vector<int>>> forest;

    using u64 = unsigned long long;
    using u128 = __uint128_t;
    const u64 MOD = (1ull<<61)-1;

    u64 mul(u64 a, u64 b) {
        u128 t = (u128)a * b;
        t = (t>>61) + (t&MOD);
        if (t >= MOD) t -= MOD;
        return t;
    }

    u64 mod(u64 a) {
        a = (a>>61) + (a&MOD);
        if (a >= MOD) a -= MOD;
        return a;
    }

    u64 pow_mod(u64 a, u64 b) {
        u64 res = 1ULL;
        while (b) {
            if (b & 1ULL) {
                res = mul(res, a);
            }
            a = mul(a, a);
            b >>= 1ULL;
        }
        return res;
    }

public:
    Namori(vector<vector<int>> G) : n(G.size()), G(G) {
        vector<int> deg(n, 0);
        for (int v = 0; v < n; ++v) for (int x : G[v]) deg[x]++;
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
        vector<vector<vector<int>>> F(N);
        vector<int> seen(n, false);
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
            forest[v] = tree;
        }
        int v = -1;
        for (int i = 0; i < n; ++i) if (is_cycle[i]) {
            v = i;
            break;
        }
        vector<int> visit(n, false);
        visit[v] = true;
        cycle.push_back(v);
        forest.push_back(F[v]);
        while (1) {
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
            forest.push_back(F[v]);
        }
    }

    u64 get_hash(int seed) {
        int M = 2*n+1;
        mt19937_64 rnd(seed);
        vector<u64> hs(M, 1), R.resize(M+1);
        uniform_int_distribution<u64> dist(47, (1ull<<61)-1);
        for (int i = 0; i < M+1; ++i) R[i] = dist(rnd);
        u64 base = uniform_int_distribution<>(414123123, 1000000000)(rnd);
        u64 invpow = pow_mod(base, MOD-2);
        vector<u64> powb(M+1, 1ULL);
        vector<u64> invb(M+1, 1ULL);
        for (int i = 1; i <= M; ++i) {
            powb[i] = mul(powb[i-1], base);
            invb[i] = mul(invb[i-1], invpow);
        }
        for (int v = 0; v < n; ++v) if (is_cycle[v]) {
            auto dfs = [&] (auto &&dfs, int v, int p) -> int {
                hs[v] = 1;
                int d = 0;
                for (int x : G[v]) if (x != p && !is_cycle[x]) {
                    int dx = dfs(dfs, x, v);
                    d = max(d, dx);
                    hs[v] = mul(hs[v], mod(R[dx] + hs[x]));
                }
                return d+1;
            };
            dfs(dfs, v, -1);
        }
        vector<u64> H(cycle.size());
        for (int i = 0; i < cycle.size(); ++i) {
            H[i] = hs[cycle[i]];
        }
        for (int i = 0; i < cycle.size(); ++i) {
            H.push_back(H[i]);
        }
        // 数列Hをロリハすることを考える
        // rotate,reverseをして得られるロリハのうち最小のものをグラフのハッシュとする
        u64 res = ULLONG_MAX;
        for (int r = 0; r < 2; ++r) {
            vector<u64> data(H.size()), acc(H.size()+1);
            for (int i = 0; i < H.size(); ++i) {
                data[i] = mul(powb[H.size()-i-1], H[i]+1);
                acc[i+1] = mod(acc[i] + data[i]);
            }
            auto get = [&] (int l, int r) -> u64 {
                return mul(mod(4*MOD+acc[r]-acc[l]), invb[H.size()-r]);
            };
            for (int i = 0; i < cycle.size(); ++i) {
                res = min(res, get(i, i+cycle.size()));
            }
            reverse(H.begin(), H.end());
        }
        return res;
    }
};
}  // namespace titan23
