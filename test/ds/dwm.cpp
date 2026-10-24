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
#include "titan_cpplib/ds/dynamic_wavelet_tree.cpp"


void solve() {
    int q; cin >> q;
    vector<int> A = {0};

    titan23::DynamicWaveletTree<int> dwm(q+2, A);
    for (int qdx = 0; qdx < q; ++qdx) {
        int c; cin >> c;
        if (c == 1) {
            int x; cin >> x;
            int idx = dwm.select(0, x);
            int i = qdx+1;
            dwm.insert(idx+1, i);
        } else {
            int x, y; cin >> x >> y;
            ll ans = 0;
            int idx_x = dwm.select(0, x);
            int idx_y = dwm.select(0, y);
            int s = min(idx_x, idx_y);
            int t = max(idx_x, idx_y);
            for (int i = s+1; i < t; ++i) {
                int v = dwm.pop(s+1);
                ans += v;
            }
            cout << ans << "\n";
        }
    }
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
