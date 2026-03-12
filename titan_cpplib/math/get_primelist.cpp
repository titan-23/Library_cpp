#include <vector>
#include <cmath>
#include <algorithm>
#include <cassert>
using namespace std;

namespace titan23 {

/// @brief エラトステネスの篩
/// 時間計算量: O(NloglogN)
vector<int> get_primelist(int n) {
    if (n < 2) return {};
    vector<bool> is_prime(n + 1, true);
    is_prime[0] = false;
    is_prime[1] = false;
    vector<int> primelist;
    for (int i = 2; i * i <= n; ++i) {
        if (is_prime[i]) {
            for (int j = i * i; j <= n; j += i) {
                is_prime[j] = false;
            }
        }
    }
    for (int i = 2; i <= n; ++i) {
        if (is_prime[i]) {
            primelist.emplace_back(i);
        }
    }
    return primelist;
}

/// @brief 区間 [l, r) の素数を列挙
/// 時間計算量: O(√r + (r - l) loglog r)
/// 空間計算量: O(√r + (r - l))
vector<long long> get_primelist_range(long long l, long long r) {
    assert(l <= r);
    if (r <= 2) return {};
    long long s = sqrt(r) + 1;
    vector<bool> is_prime_small(s + 1, true);
    if (s >= 0) is_prime_small[0] = false;
    if (s >= 1) is_prime_small[1] = false;
    for (long long i = 2; i * i <= s; ++i) {
        if (is_prime_small[i]) {
            for (long long j = i * i; j <= s; j += i) {
                is_prime_small[j] = false;
            }
        }
    }
    vector<long long> small_primes;
    for (long long i = 2; i <= s; ++i) {
        if (is_prime_small[i]) {
            small_primes.emplace_back(i);
        }
    }
    vector<bool> is_prime(r - l, true);
    if (l == 0 && r > 0) is_prime[0] = false;
    if (l <= 1 && r > 1) is_prime[1 - l] = false;
    for (long long p : small_primes) {
        long long start = max(2LL, (l + p - 1) / p) * p;
        for (long long j = start; j < r; j += p) {
            is_prime[j - l] = false;
        }
    }
    vector<long long> primelist;
    for (long long i = 0; i < r - l; ++i) {
        if (is_prime[i]) {
            primelist.emplace_back(i + l);
        }
    }
    return primelist;
}
}  // namespace titan23
