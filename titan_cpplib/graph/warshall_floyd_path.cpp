/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/warshall_floyd_path.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <utility>
#include <vector>
using namespace std;

namespace titan23 {

/**
 * @brief 経路復元付き Warshall-Floyd
 */
template<typename T>
class WarshallFloydPath {
private:
    int n;
    T INF;
    vector<int> nxt;
    vector<T> dist;
    vector<int> neg_inf;
    vector<T> to_s;
    vector<T> from_t;
    vector<int> next_to_s;
    vector<int> to_s_neg_inf;
    vector<int> from_t_neg_inf;
    bool negative_cycle;

    T add_dist(T a, T b) const {
        if (a >= INF || b >= INF) return INF;
        if (a <= -INF || b <= -INF) return -INF;
        if (b < 0 && a <= -INF-b) return -INF;
        if (b > 0 && a >= INF-b) return INF;
        return a+b;
    }

    /// @brief 負閉路を経由できる頂点対を `NEG_INF` にする / O(|V|^3)
    void set_negative_infinity() {
        vector<int> negative_vertices;
        for (int v = 0; v < n; ++v) {
            if (dist[v*n+v] < 0) {
                negative_vertices.emplace_back(v);
            }
        }
        negative_cycle = !negative_vertices.empty();
        for (int k : negative_vertices) {
            for (int i = 0; i < n; ++i) {
                if (dist[i*n+k] == INF) continue;
                for (int j = 0; j < n; ++j) {
                    if (dist[k*n+j] != INF) {
                        neg_inf[i*n+j] = 1;
                        nxt[i*n+j] = -1;
                    }
                }
            }
        }
    }

public:
    WarshallFloydPath() : n(0), INF(), negative_cycle(false) {}

    /// @brief 時間 O(|V|^3), 空間 O(|V|^2)
    WarshallFloydPath(const vector<vector<pair<int, T>>> &G, const T INF) :
        n(G.size()), INF(INF), nxt(n*n, -1), dist(n*n, INF),
        neg_inf(n*n, 0), to_s(n), from_t(n), next_to_s(n),
        to_s_neg_inf(n), from_t_neg_inf(n), negative_cycle(false)
    {
        for (int v = 0; v < n; ++v) {
            dist[v*n+v] = 0;
            nxt[v*n+v] = v;
            for (const auto &[x, c]: G[v]) {
                if (dist[v*n+x] > c) {
                    dist[v*n+x] = c;
                    nxt[v*n+x] = x;
                }
            }
        }
        for (int k = 0; k < n; ++k) {
            for (int i = 0; i < n; ++i) {
                T dik = dist[i*n+k];
                if (dik == INF) continue;
                for (int j = 0; j < n; ++j) {
                    if (dist[k*n+j] == INF) continue;
                    T new_dist = add_dist(dik, dist[k*n+j]);
                    if (dist[i*n+j] > new_dist) {
                        dist[i*n+j] = new_dist;
                        nxt[i*n+j] = nxt[i*n+k];
                    }
                }
            }
        }
        set_negative_infinity();
    }

    /// @brief 重み `w` の有向辺 `(s, t)` を追加する / O(|V|^2)
    void add_edge(int s, int t, T w) {
        int st = s*n+t;
        if (neg_inf[st] || w >= dist[st]) return;

        for (int i = 0; i < n; ++i) {
            to_s[i] = dist[i*n+s];
            next_to_s[i] = nxt[i*n+s];
            to_s_neg_inf[i] = neg_inf[i*n+s];
            from_t[i] = dist[t*n+i];
            from_t_neg_inf[i] = neg_inf[t*n+i];
        }

        int ts = t*n+s;
        bool new_negative_cycle = neg_inf[ts] || (dist[ts] != INF && add_dist(dist[ts], w) < 0);
        if (new_negative_cycle) negative_cycle = true;

        for (int i = 0; i < n; ++i) {
            if (!to_s_neg_inf[i] && to_s[i] == INF) continue;
            for (int j = 0; j < n; ++j) {
                if (!from_t_neg_inf[j] && from_t[j] == INF) continue;
                int ij = i*n+j;
                if (new_negative_cycle ||
                    to_s_neg_inf[i] || from_t_neg_inf[j]) {
                    neg_inf[ij] = 1;
                    nxt[ij] = -1;
                    continue;
                }
                if (neg_inf[ij]) continue;
                T new_dist = add_dist(add_dist(to_s[i], w), from_t[j]);
                if (dist[ij] == INF || dist[ij] > new_dist) {
                    dist[ij] = new_dist;
                    nxt[ij] = i == s ? t : next_to_s[i];
                }
            }
        }
    }

    /// @brief `s` から `t` に到達できるか / O(1)
    bool reachable(int s, int t) const {
        int idx = s*n+t;
        return neg_inf[idx] || dist[idx] != INF;
    }

    /// @brief `s` から `t` への経路上に負閉路があるか / O(1)
    bool is_neg_inf(int s, int t) const {
        return neg_inf[s*n+t];
    }

    /// @brief 負閉路が存在するか / O(1)
    bool has_negative_cycle() const {
        return negative_cycle;
    }

    /// @brief `is_neg_inf(s, t) == false` のときのみ有効 / O(1)
    T get_dist(int s, int t) const {
        assert(!is_neg_inf(s, t));
        return dist[s*n+t];
    }

    /// @brief 到達不能または `NEG_INF` の場合は空配列を返す / O(|path|)
    vector<int> get_path(int s, int t) const {
        vector<int> path;
        int idx = s*n+t;
        if (neg_inf[idx] || dist[idx] == INF) { return path; }
        for (; s != t; s = nxt[s*n+t]) {
            path.emplace_back(s);
        }
        path.emplace_back(t);
        return path;
    }
};
}  // namespace titan23
