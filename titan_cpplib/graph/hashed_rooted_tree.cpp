#include <vector>
#include <random>
#include <algorithm>
using namespace std;

namespace titan23 {

class HashedRootedTree {
private:
    using u64 = unsigned long long;
    using u128 = __uint128_t;
    static const u64 MOD = (1ull<<61)-1;
    vector<u64> hs;

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
    using HashType = u64;
    HashedRootedTree() {}
    HashedRootedTree(vector<vector<int>> G, int root, int seed=-1) {
        int n = G.size();
        hs.resize(n, 1);
        vector<u64> R(n+1);
        random_device rd;
        mt19937_64 rnd(seed == -1 ? rd() : seed);
        uniform_int_distribution<u64> dist(47, (1ull<<61)-1);
        rep(i, n+1) {
            R[i] = dist(rnd);
        }

        auto dfs = [&] (auto &&dfs, int v, int p) -> int {
            int d = 0;
            for (int x : G[v]) if (x != p) {
                int dx = dfs(dfs, x, v);
                d = max(d, dx);
                hs[v] = mul(hs[v], mod(R[dx] + hs[x]));
            }
            return d+1;
        };

        dfs(dfs, root, -1);
    }

    HashType get(int v) const { return hs[v]; }
};
} // namespace titan23
