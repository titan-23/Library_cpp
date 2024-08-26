#include <vector>
using namespace std;

// warshall_floyd_path_e1
namespace titan23 {
  template<typename T>
  struct warshall_floyd_path_e1 {
   private:
    int n;
    T INF;
    vector<int> nxt;
    vector<T> dist;

   public:
    warshall_floyd_path_e1() {}

    // 時間 O(|V|^3), 空間 O(|V|^2)
    warshall_floyd_path_e1(const vector<vector<int>> &G, const T INF) :
        n(G.size()), INF(INF), nxt(n*n), dist(n*n, INF) {
      for (int v = 0; v < n; ++v) {
        for (int x = 0; x < n; ++x) {
          nxt[v*n+x] = x;
        }
        for (const int x : G[v]) {
          dist[v*n+x] = 1;
        }
        dist[v*n+v] = 0;
      }
      for (int k = 0; k < n; ++k) {
        for (int i = 0; i < n; ++i) {
          T dik = dist[i*n+k];
          if (dik == INF) continue;
          for (int j = 0; j < n; ++j) {
            if (dist[k*n+j] == INF) continue;
            if (dist[i*n+j] > dik + dist[k*n+j]) {
              dist[i*n+j] = dik + dist[k*n+j];
              nxt[i*n+j] = nxt[i*n+k];
            }
          }
        }
      }
    }

    // O(1)
    T get_dist(const int s, const int t) const {
      return dist[s*n+t];
    }

    // O(|path|)
    vector<int> get_path(int s, int t) const {
      vector<int> path;
      if (dist[s*n+t] == INF) { return path; }
      for (; s != t; s = nxt[s*n+t]) {
        path.emplace_back(s);
      }
      path.emplace_back(t);
      return path;
    }
  };
}  // namespace titan23
