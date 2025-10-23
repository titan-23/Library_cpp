#include <bits/stdc++.h>
using namespace std;

int nCr(int n, int r) {
    if (r > n) return 0;
    if (r*2 > n) {
        r = n - r;
    }
    int res = 1;
    for (int i = 1; i <= r; ++i) {
        res = res*(n-r+i) / i;
    }
    return res;
}

// O(r nCr) ?
vector<vector<int>> combinations(int n, int r) {
    if (r < 0 || r > n) return {};
    int m = nCr(n, r);
    vector<vector<int>> res;
    if (m < 1e7) res.reserve(m);
    vector<int> now(r);
    for (int i = 0; i < r; ++i) {
        now[i] = i;
    }
    while (1) {
        res.emplace_back(now);
        int i = r - 1;
        while (i >= 0 && now[i] == n-r+i) {
            --i;
        }
        if (i < 0) break;
        now[i]++;
        for (int j = i+1; j < r; ++j) {
            now[j] = now[j-1] + 1;
        }
    }
    return res;
}
