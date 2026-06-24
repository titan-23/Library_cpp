#pragma GCC target("avx2")
#pragma GCC optimize("O3")
#pragma GCC optimize("unroll-loops")

#include <bits/stdc++.h>
using namespace std;

// #include <atcoder/all>
// using namespace atcoder;

using ll = long long;
#define rep(i, n) for (int i = 0; i < (n); ++i)

const int dy[] = {-1, 0, 0, 1};
const int dx[] = {0, -1, 1, 0};

template <class T, class T1, class T2> bool isrange(T target, T1 low, T2 high) { return low <= target && target < high; }
template <class T, class U> T min(const T &t, const U &u) { return t < u ? t : u; }
template <class T, class U> T max(const T &t, const U &u) { return t < u ? u : t; }
template <class T, class U> bool chmin(T &t, const U &u) { if (t > u) { t = u; return true; } return false; }
template <class T, class U> bool chmax(T &t, const U &u) { if (t < u) { t = u; return true; } return false; }

// #include "titan_cpplib/others/io.cpp"
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ahc/timer.cpp"
titan23::Timer global_timer;


void input() {

}


void solve() {

}

int main(int argc, char *argv[]) {
    global_timer.reset();

    ios::sync_with_stdio(false);
    cin.tie(0);
    cout << fixed << setprecision(3);
    cerr << fixed << setprecision(3);

    input();
    solve();

    return 0;
}
