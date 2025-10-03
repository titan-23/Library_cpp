#include <iostream>
#include <set>
#include <vector>
#include <algorithm>
#include <map>
using namespace std;

// Osa_k
namespace titan23 {

class Osa_k {
private:
    vector<int> min_factor;

    vector<pair<int, int>> rle(const vector<int> &A) const {
        vector<pair<int, int>> ret;
        if (A.empty()) return ret;
        int now = A[0];
        int cnt = 1;
        for (int i = 1; i < (int)A.size(); ++i) {
            if (A[i] == now) {
                ++cnt;
            } else {
                ret.emplace_back(now, cnt);
                now = A[i];
                cnt = 1;
            }
        }
        ret.emplace_back(now, cnt);
        return ret;
    }
public:
    Osa_k() : min_factor(0) {}

    Osa_k(const int n) : min_factor(n+1) {
        for (int i = 0; i <= n; ++i) {
            min_factor[i] = i;
        }
        for (int i = 2; i*i <= n; ++i) {
            if (min_factor[i] == i) {
                for (int j = 2; j <= n/i; ++j) {
                    if (min_factor[i*j] > i) {
                        min_factor[i*j] = i;
                    }
                }
            }
        }
    }

    vector<int> p_factorization(int n) const {
        vector<int> ret;
        while (n > 1) {
            ret.emplace_back(min_factor[n]);
            n /= min_factor[n];
        }
        return ret;
    }

    map<int, int> p_factorization_map(int n) const {
        map<int, int> ret;
        while (n > 1) {
            ++ret[min_factor[n]];
            n /= min_factor[n];
        }
        return ret;
    }

    vector<int> get_divisors(int n) const {
        if (n == 1) {
            return {1};
        }
        vector<int> g = p_factorization(n);
        sort(g.begin(), g.end());
        auto f = rle(g);
        int m = f.size();
        vector<int> ret;

        auto dfs = [&] (auto &&dfs, int idx, int val) -> void {
            auto [k, v] = f[idx];
            if (idx+1 < m) {
                int now = 1;
                for (int i = 0; i <= v; ++i) {
                    dfs(dfs, idx+1, val*now);
                    now *= k;
                }
            } else {
                int now = 1;
                for (int i = 0; i <= v; ++i) {
                    ret.emplace_back(val * now);
                    now *= k;
                }
            }
        };

        dfs(dfs, 0, 1);
        sort(ret.begin(), ret.end());
        ret.erase(unique(ret.begin(), ret.end()), ret.end());
        return ret;
    }
};
}  // namespace titan23
