#include <iostream>
#include <set>
#include <vector>
#include <algorithm>
#include <map>
#include "titan_cpplib/algorithm/run_length_encoding.cpp"
using namespace std;

// Osa_k
namespace titan23 {

/// @brief 素因数分解, 約数列挙
/// 前計算: <O(nloglogn), O(n)>
/// クエリ: O(logn) 時間
class Osa_k {
private:
    vector<int> min_factor;

public:
    Osa_k() : min_factor(0) {}

    /// @brief <O(nloglogn), O(n)>
    /// @param n 最大値
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

    /// @brief 素因数分解する / O(logn)
    vector<int> p_factorization(int n) const {
        vector<int> ret;
        while (n > 1) {
            ret.emplace_back(min_factor[n]);
            n /= min_factor[n];
        }
        return ret;
    }

    /// @brief 素因数分解する / O(logn)
    map<int, int> p_factorization_map(int n) const {
        map<int, int> ret;
        while (n > 1) {
            ++ret[min_factor[n]];
            n /= min_factor[n];
        }
        return ret;
    }

    /// @brief 約数列挙
    /// O(logn + d(n)logd(n)) / d(n)はnの約数の個数
    vector<int> get_divisors(int n) const {
        if (n == 1) {
            return {1};
        }
        vector<int> g = p_factorization(n);
        sort(g.begin(), g.end());
        vector<pair<int, int>> f = rle(g);
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
