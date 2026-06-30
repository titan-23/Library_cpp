#include <vector>
using namespace std;

// traveling_salesman_problem
namespace titan23 {

template<typename T>
T traveling_salesman_problem(int n, T INF, const vector<vector<T>> &dist) {
    assert(n > 0);
    vector<vector<T>> dp(1<<n, vector<T>(n, INF));
    dp[0][0] = 0;
    for (int s = 0; s < (1<<n); ++s) {
        for (int v = 0; v < n; ++v) if ((s==0) || (s>>v&1)) {
            for (int u = 0; u < n; ++u) if ((s>>u&1)==0) {
                // v -> u
                dp[s|(1<<u)][u] = min(dp[s|(1<<u)][u], dp[s][v]+dist[v][u]);
            }
        }
    }
    T ans = INF;
    for (int v = 0; v < n; ++v) {
        ans = min(ans, dp[(1<<n)-1][v] + dist[v][0]);
    }
    return ans;
}
} // namespace titan23
