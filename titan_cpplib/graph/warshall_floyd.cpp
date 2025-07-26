#include <vector>
using namespace std;

// WarshallFloyd
namespace titan23 {

template<typename T>
class WarshallFloyd {
private:
    int n;
    T INF;
    vector<T> dist;

public:
    WarshallFloyd() {}

    // 時間 O(|V|^3), 空間 O(|V|^2)
    WarshallFloyd(const vector<vector<pair<int, T>>> &G, const T INF) :
            n(G.size()), INF(INF), dist(n*n, INF) {
        for (int v = 0; v < n; ++v) {
            for (const auto &[x, c]: G[v]) {
                dist[v*n+x] = min(dist[v*n+x], c);
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
                    }
                }
            }
        }
    }

    // 重みwの辺(s, t)を追加する / O(|V|^2)
    void update(int s, int t, T w) {
        if (w >= dist[s*n+t]) return;
        dist[s*n+t] = w;
        for (int i = 0; i < n; ++i) {
            if (dist[i*n+s] == INF) continue;
            for (int j = 0; j < n; ++j) {
                if (dist[t*n+j] == INF) continue;
                T new_dist = dist[i*n+s] + dist[t*n+j] + w;
                if (dist[i*n+j] > new_dist) {
                    dist[i*n+j] = new_dist;
                }
            }
        }
    }

    // O(1)
    T get_dist(int s, int t) const {
        return dist[s*n+t];
    }
};
}  // namespace titan23
