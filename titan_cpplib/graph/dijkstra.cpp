/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/dijkstra.cpp
#pragma once

#include <vector>
#include <queue>
using namespace std;

namespace titan23 {

/// @brief start から各頂点への最短距離を返す / 到達できない頂点は INF / O(n + m log m)
/// @note G[v] は (行き先, 重み) の組を持ち、重みの型は T とする。重みは非負に限る
template<typename T>
vector<T> dijkstra(const vector<vector<pair<int, T>>> &G, const int start, const T INF) {
    const int n = G.size();
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
