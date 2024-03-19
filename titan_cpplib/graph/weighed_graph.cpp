#include <vector>
#include <queue>
using namespace std;

namespace titan23 {

  template<typename T>
  struct Graph {
    vector<vector<int, T>> G;
    int n;

    Graph(int n) : n(n), G(n) {};

    void add_edge(const int u, const int v, const T w) {
      G[u].emplace_back({v, w});
    }

    template<typename T>
    vector<T> dijkstra(const int start, const T INF) {
      vector<T> dist(n, INF);
      priority_queue<pair<T, int>, vector<pair<T, int>>, greater<pair<T, int>>> hq;
      hq.push({0, start});
      dist[start] = 0;
      while (!hq.empty()) {
        auto [d, v] = hq.top();
        hq.pop();
        if (dist[v] > d) continue;
        for (const auto &[x, c]: G[v]) {
          if (dist[x] > d + c) {
            dist[x] = d + c;
            hq.push({dist[x], x});
          }
        }
      }
      return dist;
    }

    int len() const {
      return (int)G.size();
    }
  };
} // namespace titan23
