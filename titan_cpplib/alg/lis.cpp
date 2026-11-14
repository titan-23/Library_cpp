/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/alg/lis.cpp
#pragma once

#include <vector>
#include <utility>
#include <algorithm>
#include <iterator>
using namespace std;

namespace titan23 {

/// @brief 最長増加部分列の長さを返す / strict が false なら広義増加 / O(n log n)
template <typename T>
int LIS(const vector<T>& a, bool strict=true) {
    if (a.empty()) return 0;
    int n = (int)a.size();
    vector<T> dp(n);
    int sz = 0;
    for (const T& x : a) {
        auto it = strict ? lower_bound(dp.begin(), dp.begin() + sz, x) : upper_bound(dp.begin(), dp.begin() + sz, x);
        int idx = (int)(it - dp.begin());
        dp[idx] = x;
        if (idx == sz) {
            sz++;
        }
    }
    return sz;
}

/// @brief 最長増加部分列を {添字, 値} の列で返す / strict が false なら広義増加 / O(n log n)
template <typename T>
vector<pair<int, T>> LIS_vec(const vector<T>& a, bool strict=true) {
    int n = a.size();
    if (n == 0) return {};
    vector<T> dp(n);
    vector<int> pos(n);
    vector<int> prev(n, -1);
    int sz = 0;
    for (int i = 0; i < n; i++) {
        auto it = strict ? lower_bound(dp.begin(), dp.begin()+sz, a[i]) : upper_bound(dp.begin(), dp.begin()+sz, a[i]);
        int idx = it - dp.begin();
        dp[idx] = a[i];
        pos[idx] = i;
        if (idx == sz) {
            sz++;
        }
        if (idx > 0) {
            prev[i] = pos[idx - 1];
        }
    }
    vector<pair<int, T>> res;
    if (sz > 0) {
        int now = pos[sz - 1];
        while (now != -1) {
            res.push_back({now, a[now]});
            now = prev[now];
        }
        reverse(res.begin(), res.end());
    }
    return res;
}
}
