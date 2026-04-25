// #pragma GCC target("avx2")
// #pragma GCC optimize("O3")
// #pragma GCC optimize("unroll-loops")

#include <bits/stdc++.h>
using namespace std;

// #include <atcoder/all>
// using namespace atcoder;

using ll = long long;
#define rep(i, n) for (ll i = 0; i < (ll)(n); ++i)

const ll dy[] = {-1, 0, 0, 1};
const ll dx[] = {0, -1, 1, 0};

template <class T, class T1, class T2> bool isrange(T target, T1 low, T2 high) { return low <= target && target < high; }
template <class T, class U> T min(const T &t, const U &u) { return t < u ? t : u; }
template <class T, class U> T max(const T &t, const U &u) { return t < u ? u : t; }
template <class T, class U> bool chmin(T &t, const U &u) { if (t > u) { t = u; return true; } return false; }
template <class T, class U> bool chmax(T &t, const U &u) { if (t < u) { t = u; return true; } return false; }

// #include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ahc/kmeans.cpp"

double calc_dist(const pair<double, double>& a, const pair<double, double>& b) {
    double dx = a.first - b.first;
    double dy = a.second - b.second;
    return dx * dx + dy * dy;
}

pair<double, double> calc_mean(const vector<pair<double, double>>& pts) {
    if (pts.empty()) return {0.0, 0.0};
    double sum_x = 0.0, sum_y = 0.0;
    for (const auto& p : pts) {
        sum_x += p.first;
        sum_y += p.second;
    }
    return {sum_x / pts.size(), sum_y / pts.size()};
}

void solve() {
    int n, k; cin >> n >> k;
    vector<int> A(k);
    rep(i, k) cin >> A[i];
    vector<pair<double, double>> points(n);
    rep(i, n) cin >> points[i].first >> points[i].second;

    int max_iter = 1;
    titan23::Kmeans<double, pair<double, double>, calc_dist, calc_mean> kmeans(k, max_iter);
    auto result = kmeans.fit_flow(points, A, 1e6);
    const vector<int>& labels = result.first;
    rep(i, n) {
        cout << labels[i] << (i == n-1 ? "" : " ");
    }
    cout << "\n";
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(0);
    cout << fixed << setprecision(3);
    cerr << fixed << setprecision(3);

    solve();

    return 0;
}
