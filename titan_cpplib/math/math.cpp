#include <vector>
#include <cassert>
using namespace std;

i128 isqrt(i128 v) {
    if (v < 0) return 0;
    if (v == 0) return 0;
    i128 x = sqrt((long double)v);
    while ((x-1)*(x-1) >= v) x--;
    while ((x+1)*(x+1) <= v) x++;
    return x;
}

template<typename T>
tuple<bool, T, T> solve_quadratic_equation(T a, T b, T c) {
    T D = b*b - 4*a*c;
    if (D < 0) return {false, T{}, T{}};
    T v = sqrt(D);
    return {true, (-b-v)/2, (-b+v)/2};
}

// pow ----------------
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
