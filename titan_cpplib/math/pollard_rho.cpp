#include <iostream>
#include <vector>
#include <map>
#include <stack>
#include "titan_cpplib/math/math.cpp"
#include "titan_cpplib/math/is_primell.cpp"
using namespace std;

namespace titan23 {

class PollardRho {
private:
    using ll = __int128_t;
    // long long;
    vector<ll> L = {2, 325, 9375, 28178, 450775, 9780504, 1795265022};
    vector<ll> P200 = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199};

    inline int bit_length(ll x) const {
        return x ? 64 - __builtin_clzll(x) : 0;
    }

    ll pollard_rho(ll n) {
        if (n & 1 == 0) return 2;
        if (n % 3 == 0) return 3;
        int s = bit_length((n - 1) & (1 - n)) - 1;
        ll d = n >> s;
        for (ll a : L) {
            ll p = pow_mod<ll>(a, d, n);
            if (p == 1 || p == n-1 || a%n == 0) continue;
            bool upd = false;
            for (int i = 0; i < s; ++i) {
                ll prev = p;
                p = (p * p) % n;
                if (p == 1) return gcd(prev-1, n);
                if (p == n-1) {
                    upd = true;
                    break;
                }
            }
            if (!upd) {
                for (ll i = 2; i < n; ++i) {
                    ll x = i;
                    ll y = (i * i + 1) % n;
                    ll f = gcd(abs(x - y), n);
                    while (f == 1) {
                        ll x = (x * x + 1) % n;
                        ll y = (y * y + 1) % n;
                        y = (y * y + 1) % n;
                        ll f = gcd(abs(x - y), n);
                    }
                    if (f != n) return f;
                }
            }
        }
        return n;
    }

public:
    map<ll, int> factorize(ll n) {
        map<ll, int> res;
        // for (ll p : P200) {
        //     if (n % p == 0) {
        //         int cnt = 0;
        //         while (n % p == 0) {
        //             cnt++;
        //             n /= p;
        //         }
        //         res[p] = cnt;
        //         if (n == 1) return res;
        //     }
        // }
        stack<ll> todo; todo.emplace(n);
        while (!todo.empty()) {
            ll v = todo.top(); todo.pop();
            if (v <= 1) continue;
            ll f = pollard_rho(v);
            if (is_primell(f)) {
                int cnt = 0;
                while (v % f == 0) {
                    cnt++;
                    v /= f;
                }
                res[f] += cnt;
                todo.emplace(v);
            } else if (is_primell(v/f)) {
                f = v / f;
                int cnt = 0;
                while (v % f == 0) {
                    cnt++;
                    v /= f;
                }
                res[f] += cnt;
                todo.emplace(v);
            } else {
                todo.emplace(f);
                todo.emplace(v / f);
            }
        }
        return res;
    }
};
} // namespace titan23
