// #pragma GCC target("avx2")
// #pragma GCC optimize("O3")
// #pragma GCC optimize("unroll-loops")

#include <bits/stdc++.h>
#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/hash_policy.hpp>
using namespace std;
using namespace __gnu_pbds;

// #include <atcoder/all>
// using mint = atcoder::modint998244353;

using ll = long long;
#define rep(i, n) for (ll i = 0; i < (ll)(n); ++i)

// const ll dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};
// const ll dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
const ll dy[] = {-1, 0, 0, 1};
const ll dx[] = {0, -1, 1, 0};

template <class T, class T1, class T2> bool isrange(T target, T1 low, T2 high) { return low <= target && target < high; }
template <class T, class U> T min(const T &t, const U &u) { return t < u ? t : u; }
template <class T, class U> T max(const T &t, const U &u) { return t < u ? u : t; }
template <class T, class U> bool chmin(T &t, const U &u) { if (t > u) { t = u; return true; } return false; }
template <class T, class U> bool chmax(T &t, const U &u) { if (t < u) { t = u; return true; } return false; }
template<class K, class V> using hash_map = gp_hash_table<K, V>;
template<class K> using hash_set = gp_hash_table<K, null_type>;

// #include "titan_cpplib/others/io.cpp"
// #include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/alg/zaatsu.cpp"
#include "titan_cpplib/ds/dynamic_wavelet_tree_sum.cpp"
#include "titan_cpplib/ahc/profiler.cpp"

void solve() {
    int n, q; cin >> n >> q;
    vector<ll> A(n);
    rep(i, n) cin >> A[i];
    titan23::Zaatsu<ll> Z(A);
    vector<tuple<int, ll, int, int, ll>> Q(q);
    rep(qdx, q) {
        ll c, x, l, r, k; cin >> c >> x >> l >> r >> k;
        --c;
        --l;
        Q[qdx] = {c, x, l, r, k};
        Z.add(x);
    }
    Z.build();
    vector<int> B(n);
    rep(i, n) B[i] = Z.to_zaatsu(A[i]);
    PROF_START("wm-build");
    titan23::DynamicWaveletTreeSum<int, ll> wm(Z.len()+1, B, A);
    PROF_STOP();

    rep(qdx, q) {
        auto [c, x, l, r, k] = Q[qdx];
        PROF_START("wm.set()");
        wm.set(c, Z.to_zaatsu(x), x);
        PROF_STOP();
        PROF_START("wm.min_count_largest_sum_ge()");
        const int answer = wm.min_count_largest_sum_ge(l, r, k);
        PROF_STOP();
        cout << answer << "\n";
    }

    titan23::profiler.report();
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(0);
    cout << fixed << setprecision(15);
    cerr << fixed << setprecision(15);

    int t = 1;
    // cin >> t;
    for (int i = 0; i < t; ++i) {
        solve();
    }

    return 0;
}
