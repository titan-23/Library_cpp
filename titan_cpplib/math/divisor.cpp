/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/math/divisor.cpp
#pragma once

#include <vector>
#include <algorithm>
using namespace std;

namespace titan23 {

/// @brief N の約数を昇順で返す / O(√N)
vector<long long> get_divisors(long long n) {
    vector<long long> l, r;
    for (long long i = 1; i*i <= n; ++i) {
        if (n % i == 0) {
            l.emplace_back(i);
            if (i != n/i) r.emplace_back(n/i);
        }
    }
    reverse(r.begin(), r.end());
    for (long long a : r) l.emplace_back(a);
    return l;
}

/// @brief N を素因数分解し、素因数と指数の組を昇順で返す / O(√N)
vector<pair<long long, int>> factorization(long long n) {
    if (n == 1) return {};
    vector<pair<long long, int>> ret;
    for (long long i = 2; i*i <= n; ++i) {
        if (n == 1) break;
        if (n % i == 0) {
            int cnt = 0;
            while (n % i == 0) {
                cnt++;
                n /= i;
            }
            ret.emplace_back(i, cnt);
        }
    }
    if (n != 1) {
        ret.emplace_back(n, 1);
    }
    return ret;
}

/// @brief N の約数の個数を返す / O(√N)
int divisors_num(long long n) {
    int cnt = 0;
    for (long long i = 1; i * i <= n; ++i) {
        if (n % i == 0) {
            cnt += (i * i == n) ? 1 : 2;
        }
    }
    return cnt;
}

/// @brief N 以下の各数の約数の個数を返す / O(NlogN)
vector<int> divisors_num_all(int n) {
    vector<int> cnt(n+1, 0);
    for (int i = 1; i <= n; ++i) {
        for (int j = i; j <= n; j += i) {
            cnt[j]++;
        }
    }
    return cnt;
}

/// @brief N 以下の各数の約数の総和を返す / O(NlogN)
vector<long long> divisors_sum_all(int n) {
    vector<long long> S(n+1, 0);
    for (int i = 1; i <= n; ++i) {
        for (int j = i; j <= n; j += i) {
            S[j] += i;
        }
    }
    return S;
}

/// @brief N 以下の各数の素因数の種類数を返す / O(NloglogN)
vector<int> primefactor_num(int n) {
    vector<int> cnt(n+1, 0);
    for (int i = 2; i <= n; ++i) {
        if (cnt[i] >= 1) continue;
        for (int j = i; j <= n; j += i) {
            cnt[j]++;
        }
    }
    return cnt;
}

/// @brief 素数表 primes を使って N を素因数分解する / O(√N / log√N + log N)
/// primes には sqrt(N) 以下の素数をエラトステネスの篩などで昇順にすべて列挙しておく
vector<pair<int, int>> factorization_eratos(int n, const vector<int> &primes) {
    vector<pair<int, int>> res;
    for (int p : primes) {
        if (p * p > n) break;
        if (n % p == 0) {
            int cnt = 0;
            while (n % p == 0) {
                cnt++;
                n /= p;
            }
            res.emplace_back(p, cnt);
        }
    }
    if (n != 1) res.emplace_back(n, 1);
    return res;
}

} // namespace titan23
