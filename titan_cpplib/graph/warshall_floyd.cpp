#pragma once

#include <cassert>
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
    vector<int> neg_inf;
    vector<T> to_s;
    vector<T> from_t;
    vector<int> to_s_neg_inf;
    vector<int> from_t_neg_inf;
    bool neg_cycle;

public:
    WarshallFloyd() : n(0), INF(), neg_cycle(false) {}

    /// @brief 時間 O(|V|^3), 空間 O(|V|^2)
    WarshallFloyd(const vector<vector<pair<int, T>>> &G, const T INF) :
        n(G.size()), INF(INF), dist(n*n, INF), neg_inf(n*n, 0),
        to_s(n), from_t(n), to_s_neg_inf(n), from_t_neg_inf(n), neg_cycle(false)
    {
        for (int v = 0; v < n; ++v) {
            dist[v*n+v] = 0;
            for (const auto &[x, c]: G[v]) {
                dist[v*n+x] = min(dist[v*n+x], c);
            }
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

        vector<int> negs;
        for (int v = 0; v < n; ++v) {
            if (dist[v*n+v] < 0) {
                negs.emplace_back(v);
            }
        }
        neg_cycle = !negs.empty();
        for (int k : negs) {
            for (int i = 0; i < n; ++i) {
                if (dist[i*n+k] == INF) continue;
                for (int j = 0; j < n; ++j) {
                    if (dist[k*n+j] != INF) {
                        neg_inf[i*n+j] = 1;
                    }
                }
            }
        }
    }

    /// @brief 重みwの辺(s, t)を追加する / O(|V|^2)
    void add_edge(int s, int t, T w) {
        int st = s*n+t;
        if (neg_inf[st] || w >= dist[st]) return;

        for (int i = 0; i < n; ++i) {
            to_s[i] = dist[i*n+s];
            to_s_neg_inf[i] = neg_inf[i*n+s];
            from_t[i] = dist[t*n+i];
            from_t_neg_inf[i] = neg_inf[t*n+i];
        }

        int ts = t*n+s;
        bool new_neg_cycle = neg_inf[ts] || (dist[ts] != INF && dist[ts] + w < 0);
        if (new_neg_cycle) neg_cycle = true;

        for (int i = 0; i < n; ++i) {
            if (!to_s_neg_inf[i] && to_s[i] == INF) continue;
            for (int j = 0; j < n; ++j) {
                if (!from_t_neg_inf[j] && from_t[j] == INF) continue;
                int ij = i*n+j;
                if (new_neg_cycle ||
                    to_s_neg_inf[i] || from_t_neg_inf[j]) {
                    neg_inf[ij] = 1;
                    continue;
                }
                if (neg_inf[ij]) continue;
                T new_dist = to_s[i] + w + from_t[j];
                if (dist[ij] == INF || dist[ij] > new_dist) {
                    dist[ij] = new_dist;
                }
            }
        }
    }

    /// @brief s から t に到達できるか / O(1)
    bool reachable(int s, int t) const {
        int idx = s*n+t;
        return neg_inf[idx] || dist[idx] != INF;
    }

    /// @brief s から t への経路上に負閉路があるか / O(1)
    bool is_neg_inf(int s, int t) const {
        return neg_inf[s*n+t];
    }

    /// @brief 負閉路が存在するか / O(1)
    bool has_neg_cycle() const {
        return neg_cycle;
    }

    /// @brief is_neg_inf(s, t) == false のときのみ有効 / O(1)
    T get_dist(int s, int t) const {
        assert(!is_neg_inf(s, t));
        return dist[s*n+t];
    }
};
}  // namespace titan23
