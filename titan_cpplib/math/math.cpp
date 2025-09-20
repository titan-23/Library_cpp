#include <vector>
#include <cassert>
using namespace std;

// pow ----------------
long long pow_mod(long long a, long long b, const long long mod) {
    long long res = 1;
    while (b) {
        if (b & 1) res = res * a % mod;
        a = a * a % mod;
        b >>= 1;
    }
    return res;
}

long long pow(long long a, long long b) {
    long long res = 1;
    while (b) {
        if (b & 1) res *= a;
        a *= a;
        b >>= 1;
    }
    return res;
}
// pow ----------------

// factorial -----------------
long long factorial(long long x) {
    long long res = 1;
    for (long long i = 1ll; i <= x; ++i) {
        res *= i;
    }
    return res;
}
// factorial -----------------

// get_primelist -----------------
vector<int> get_primelist(int MAX) {
    vector<int> is_prime(MAX+1, 1);
    is_prime[0] = 0;
    is_prime[1] = 0;
    for (int i = 2; i < sqrt(MAX)+1; ++i) {
        if (!is_prime[i]) continue;
        for (int j = i+i; j < MAX+1; j+=i) {
            is_prime[j] = 0;
        }
    }
    return is_prime;
}
// get_primelist -----------------

using int128 = __int128_t;

template<class T>
T pow_mod(T a, T b, const T mod) {
    T res = 1;
    a %= mod;
    while (b) {
        if (b & 1) res = res * a % mod;
        a = a * a % mod;
        b >>= 1;
    }
    return res;
}

const vector<long long> A1 = {2, 7, 61};
const vector<long long> A2 = {2, 325, 9375, 28178, 450775, 9780504, 1795265022};

bool is_primell(long long n) {
    if (n <= 1) return false;
    if (n == 2 || n == 7 || n == 61) return true;
    if (n % 2 == 0) return false;
    long long d = n - 1;
    long long s = __builtin_ctzll(d);
    d >>= s;
    const auto &A = n < 4759123141 ? A1 : A2;
    for (const long long &a : A) {
        if (n <= a) return true;
        long long t, x = pow_mod<int128>(a, d, n);
        if (x != 1) {
            for (t = 0; t < s; ++t) {
                if (x == n - 1) break;
                x = (int128)x * x % n;
            }
            if (t == s) return false;
        }
    }
    return true;
}
