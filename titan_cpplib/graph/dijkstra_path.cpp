#include <vector>
#include <queue>
#include <algorithm>
using namespace std;

// dijkstra_path
namespace titan23 {
  template<typename T>
  struct dijkstra_path {
    int n;
    T INF;
    vector<int> prev;
    vector<T> dist;

    dijkstra_path() {}
    dijkstra_path(vector<vector<pair<int, T>>> &G, int s, T INF) :
        n(G.size()), INF(INF), prev(n, -1), dist(n, INF) {
      dist[s] = 0;
      priority_queue<pair<T, int>, vector<pair<T, int>>, greater<pair<T, int>>> hq;
      hq.emplace(0, s);
      while (!hq.empty()) {
        auto [d, v] = hq.top();
        hq.pop();
        if (dist[v] < d) continue;
        for (const auto &[x, c]: G[v]) {
          if (dist[x] > d + c) {
            dist[x] = d + c;
            prev[x] = v;
            hq.emplace(d+c, x);
          }
        }
      }
    }

    T get_dist(int t) const { return dist[t]; }

    vector<int> get_path(int t) const {
      vector<int> path;
      if (dist[t] == INF) { return path; }
      while (prev[t] != -1) {
        path.emplace_back(t);
        t = prev[t];
      }
      path.emplace_back(t);
      reverse(path.begin(), path.end());
      return path;
    }
  };
}  // namespace titan23
