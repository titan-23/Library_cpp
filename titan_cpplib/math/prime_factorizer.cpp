#include <vector>
#include <map>
#include <algorithm>
using namespace std;

namespace titan23 {

/// @brief 素因数分解, 約数列挙
/// 前計算: <O(√nloglog√n), O(√n)>
/// クエリ: O(√n / log√n) 時間
class PrimeFactorizer {
private:
    vector<int> primelist;

public:
    /// @brief <O(√nloglog√n), O(√n)>
    /// @param n 最大値
    PrimeFactorizer(long long n) {
        long long max_sqrt = sqrt(n+1) + 1;
        vector<bool> is_prime(max_sqrt + 1, true);
        if (max_sqrt >= 0) is_prime[0] = false;
        if (max_sqrt >= 1) is_prime[1] = false;

        for (long long i = 2; i * i <= max_sqrt; ++i) {
            if (is_prime[i]) {
                for (long long j = i * i; j <= max_sqrt; j += i) {
                    is_prime[j] = false;
                }
            }
        }
        for (long long i = 2; i <= max_sqrt; ++i) {
            if (is_prime[i]) {
                primelist.emplace_back(i);
            }
        }
    }

    /// @brief 素因数分解する / O(√n / log√n)
    vector<long long> p_factorization(long long n) const {
        vector<long long> ret;
        for (long long p : primelist) {
            if (p * p > n) break;
            while (n % p == 0) {
                ret.emplace_back(p);
                n /= p;
            }
        }
        if (n > 1) {
            ret.emplace_back(n);
        }
        return ret;
    }

    /// @brief 素因数分解する / O(√n / log√n)
    map<long long, int> p_factorization_map(long long n) const {
        map<long long, int> ret;
        for (long long p : primelist) {
            if (p * p > n) break;
            while (n % p == 0) {
                ++ret[p];
                n /= p;
            }
        }
        if (n > 1) {
            ++ret[n];
        }
        return ret;
    }

    /// @brief 約数列挙
    /// O(√n/log√n + d(n)logd(n)) / d(n)はnの約数の個数
    vector<long long> get_divisors(long long n) const {
        if (n == 1) {
            return {1};
        }

        auto f = p_factorization_map(n);
        vector<pair<long long, int>> factors(f.begin(), f.end());
        vector<long long> ret;

        auto dfs = [&](auto&& self, int idx, long long val) -> void {
            if (idx == (int)factors.size()) {
                ret.emplace_back(val);
                return;
            }
            long long p = factors[idx].first;
            int count = factors[idx].second;
            long long now = 1;
            for (int i = 0; i <= count; ++i) {
                self(self, idx + 1, val * now);
                now *= p;
            }
        };

        dfs(dfs, 0, 1);
        sort(ret.begin(), ret.end());
        return ret;
    }
};
}  // namespace titan23
