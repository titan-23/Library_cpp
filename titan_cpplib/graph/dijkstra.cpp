#include <vector>
#include <queue>
using namespace std;

// dijkstra
namespace titan23 {
/**
 * @brief dijkstra
 * 
 * @tparam T weight type
 * @param G weighted graph
 * @param start start
 * @param INF inf
 * @return vector<T> dist from start
 */
template<typename T>
vector<T> dijkstra(
        const vector<vector<pair<int, T>>> &G,
        const int start,
        const T INF) {
    vector<T> dist(n, INF);
    priority_queue<pair<T, int>, vector<pair<T, int>>, greater<pair<T, int>>> hq;
    hq.emplace(0, start);
    dist[start] = 0;
    while (!hq.empty()) {
        auto [d, v] = hq.top();
        hq.pop();
        if (dist[v] < d) continue;
        for (const auto &[x, c] : G[v]) {
            if (dist[x] > d + c) {
                dist[x] = d + c;
                hq.emplace(dist[x], x);
            }
        }
    }
    return dist;
}
}  // namespace titan23
