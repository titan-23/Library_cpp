#include <vector>
#include <algorithm>
using namespace std;

namespace titan23 {

// 約数全列挙 / O(√N)
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

// Nを1回素因数分解する / O(√N)
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

// Nの約数の個数を求める / O(√N)
int divisors_num(long long n) {
    int cnt = 0;
    for (long long i = 1; i <= n; ++i) {
        if (n % i == 0) {
            cnt++;
            if (i != n/i) cnt++;
        }
    }
    return cnt;
}

// N以下のそれぞれの数の約数の個数を求める / O(NlogN)
vector<int> divisors_num_all(int n) {
    vector<int> cnt(n+1, 0);
    for (int i = 1; i <= n; ++i) {
        for (int j = i; j <= n; j += i) {
            cnt[j]++;
        }
    }
    return cnt;
}

// N以下のそれぞれの数の約数の総和を求める / O(NlogN)
vector<long long> divisors_sum_all(int n) {
    vector<long long> S(n+1, 0);
    for (int i = 1; i <= n; ++i) {
        for (int j = i; j <= n; j += i) {
            S[j] += i;
        }
    }
    return S;
}

// N以下のそれぞれの数の素因数の種類数を求める / O(NloglogN)
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

// エラトステネスの篩(N以下の素数を返す) / O(NloglogN)
vector<int> get_primelist(int n) {
    vector<int> p(n+1, 1);
    p[0] = 0;
    p[1] = 0;
    vector<int> res;
    for (long long i = 2; i <= n; ++i) {
        if (!p[i]) continue;
        res.emplace_back(i);
        for (int j = i+i; j <= n; j += i) {
            p[j] = 0;
        }
    }
    return res;
}

// N以下の素数の個数を求める / O(NloglogN)
int get_prime_count(int n) {
    int ret = 0;
    for (int i = 2; i <= n; ++i) {
        bool ok = false;
        for (int j = 2; j*j <= n; ++j) {
            if (i % j == 0) {
                ok = true;
                break;
            }
        }
        if (!ok) ret++;
    }
    return ret;
}

// 事前にエラトステネスとかでsart(N)以下の素数を全列挙しておく
// O(sqrt(N)log(log(sqrt(N)) + log(N))
vector<int> factorization_eratos(int n, const vector<int> &primes) {
    vector<int> res;
    for (int p : primes) {
        if (p * p > n) break;
        if (n % p == 0) {
            n /= p;
            while (n % p == 0) {
                n /= p;
            }
            res.emplace_back(p);
        }
    }
    if (n != 1) res.emplace_back(n);
    return res;
}

} // namespace titan23
