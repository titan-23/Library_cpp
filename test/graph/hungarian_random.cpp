// min_cost_assignment と Hungarian を部分集合 DP の総当たりと比べる
// g++ -std=c++20 -O2 -I. test/graph/hungarian_random.cpp -o /tmp/hungarian_random && /tmp/hungarian_random
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>
#include "titan_cpplib/graph/hungarian.cpp"
using namespace std;

// 行数 n、列数 m。小さい方の次元を全て割り当てる最小費用 (部分集合 DP)
template <class T>
T brute(const vector<vector<T>> &a) {
    int n = a.size(), m = a[0].size();
    bool rows_small = n <= m;
    int small = rows_small ? n : m, big = rows_small ? m : n;
    auto c = [&](int s, int b) { return rows_small ? a[s][b] : a[b][s]; };
    const T INF = numeric_limits<T>::max() / 4;
    vector<T> dp(1 << big, INF);
    dp[0] = 0;
    for (int mask = 0; mask < (1 << big); ++mask) {
        if (dp[mask] == INF) continue;
        int k = __builtin_popcount(mask);
        if (k >= small) continue;
        for (int b = 0; b < big; ++b) if (!(mask >> b & 1)) dp[mask | 1 << b] = min(dp[mask | 1 << b], dp[mask] + c(k, b));
    }
    T best = INF;
    for (int mask = 0; mask < (1 << big); ++mask) if (__builtin_popcount(mask) == small) best = min(best, dp[mask]);
    return best;
}

template <class T>
void validate(const vector<vector<T>> &a, const pair<T, vector<int>> &res) {
    int n = a.size(), m = a[0].size();
    auto &[c, al] = res;
    assert((int)al.size() == n);
    vector<int> seen(m);
    T sum = 0;
    int cnt = 0;
    for (int i = 0; i < n; ++i) {
        if (al[i] == -1) continue;
        assert(0 <= al[i] && al[i] < m);
        assert(!seen[al[i]]);
        seen[al[i]] = 1;
        sum += a[i][al[i]];
        ++cnt;
    }
    assert(cnt == min(n, m));
    assert(sum == c);
}

int main() {
    mt19937_64 rng(42);
    long long checked = 0;
    for (int trial = 0; trial < 200000; ++trial) {
        int n = 1 + rng() % 9, m = 1 + rng() % 9;
        int range = vector<int>{1, 2, 3, 10, 1000, 1000000000}[rng() % 6];
        bool neg = rng() % 3 == 0;
        vector<vector<long long>> a(n, vector<long long>(m));
        for (auto &r : a) for (auto &x : r) x = (long long)(rng() % range) - (neg ? range / 2 : 0);
        auto res = titan23::min_cost_assignment(a);
        validate(a, res);
        assert(res.first == brute(a));
        vector<vector<int>> b(n, vector<int>(m));
        for (int i = 0; i < n; ++i) for (int j = 0; j < m; ++j) b[i][j] = (int)(a[i][j] % 1000000);
        auto rb = titan23::min_cost_assignment(b);
        validate(b, rb);
        assert(rb.first == brute(b));
        {
            // 符号なしと long double も逐次の経路で正しく解く
            long long mn = 0;
            for (auto &r : a) for (auto x : r) mn = min(mn, x);
            vector<vector<unsigned long long>> ua(n, vector<unsigned long long>(m));
            vector<vector<long double>> la(n, vector<long double>(m));
            for (int i = 0; i < n; ++i) for (int j = 0; j < m; ++j) ua[i][j] = (unsigned long long)(a[i][j] - mn), la[i][j] = (long double)a[i][j];
            auto ru = titan23::min_cost_assignment(ua);
            validate(ua, ru);
            assert((long long)ru.first + mn * min(n, m) == res.first);
            auto rl = titan23::min_cost_assignment(la);
            validate(la, rl);
            assert(rl.first == (long double)res.first);
        }
        if (n >= m) {
            titan23::Hungarian<long long> h(a);
            auto rh = h.solve();
            validate(a, rh);
            assert(rh.first == res.first);
        }
        ++checked;
    }
    // 浮動小数点: 整数値の double は正確に比べられる。一般の値は許容誤差で比べる
    for (int trial = 0; trial < 50000; ++trial) {
        int n = 1 + rng() % 9, m = 1 + rng() % 9;
        vector<vector<double>> a(n, vector<double>(m));
        vector<vector<long double>> al(n, vector<long double>(m));
        bool integral = trial % 2 == 0;
        uniform_real_distribution<double> d(-5, 5);
        for (int i = 0; i < n; ++i) for (int j = 0; j < m; ++j) {
            a[i][j] = integral ? double(rng() % 20) : d(rng);
            al[i][j] = a[i][j];
        }
        auto res = titan23::min_cost_assignment(a);
        auto &[c, alloc] = res;
        vector<int> seen(m);
        long double sum = 0;
        int cnt = 0;
        for (int i = 0; i < n; ++i) if (alloc[i] != -1) { assert(!seen[alloc[i]]); seen[alloc[i]] = 1; sum += a[i][alloc[i]]; ++cnt; }
        assert(cnt == min(n, m));
        long double best = brute(al);
        if (integral) assert(c == (double)best);
        else assert(fabsl(sum - best) <= 1e-9L);
        vector<vector<float>> f(n, vector<float>(m));
        for (int i = 0; i < n; ++i) for (int j = 0; j < m; ++j) f[i][j] = (float)a[i][j];
        auto rf = titan23::min_cost_assignment(f);
        long double fs = 0;
        for (int i = 0; i < n; ++i) if (rf.second[i] != -1) fs += f[i][rf.second[i]];
        vector<vector<long double>> fl(n, vector<long double>(m));
        for (int i = 0; i < n; ++i) for (int j = 0; j < m; ++j) fl[i][j] = f[i][j];
        assert(fabsl(fs - brute(fl)) <= 1e-4L);
        ++checked;
    }
    cout << "ok " << checked << '\n';
}
