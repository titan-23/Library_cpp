/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/alg/traveling_salesman_problem.cpp
#pragma once

#include <vector>
#include <algorithm>
#include <cassert>
using namespace std;

// traveling_salesman_problem
namespace titan23 {

/// @brief 巡回セールスマン問題 最小重みハミルトン閉路 / O(2^n n^2)
template<typename T>
T traveling_salesman_problem(int n, T INF, const vector<vector<T>> &dist) {
    assert(n > 0);
    vector<vector<T>> dp(1<<n, vector<T>(n, INF));
    dp[1<<0][0] = 0;
    for (int s = 0; s < (1<<n); ++s) {
        if ((s & 1) == 0) continue;
        for (int v = 0; v < n; ++v) if (s>>v&1) {
            if (dp[s][v] == INF) continue;
            for (int u = 0; u < n; ++u) if ((s>>u&1)==0) {
                // v -> u
                dp[s|(1<<u)][u] = min(dp[s|(1<<u)][u], dp[s][v]+dist[v][u]);
            }
        }
    }
    T ans = INF;
    for (int v = 0; v < n; ++v) {
        if (dp[(1<<n)-1][v] == INF) continue;
        ans = min(ans, dp[(1<<n)-1][v] + dist[v][0]);
    }
    return ans;
}
} // namespace titan23
