/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/warshall_floyd.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>
#include "titan_cpplib/others/simd_int.cpp"
using namespace std;

namespace titan23 {

/// @brief 負閉路に対応した Warshall-Floyd / INF は到達不能を表す / 重みが -INF 未満の辺は -INF として扱う
/// 負閉路を経由しない頂点対の最短距離は (-INF, INF) に収まること
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

    T add_dist(T a, T b) const {
        if (a >= INF || b >= INF) return INF;
        if (a <= -INF || b <= -INF) return -INF;
        if (b < 0 && a <= -INF-b) return -INF;
        if (b > 0 && a >= INF-b) return INF;
        return a+b;
    }

    T clamp_weight(T w) const { return w < -INF ? -INF : w; }

    // 32/64 bit 以外の T では使わないが、型は定義できるようにしておく
    using Simd = SimdInt<T, sizeof(T) == 8 ? 8 : 4>;
    using Vec = typename Simd::Vec;
    using Mask = typename Simd::Mask;
    static constexpr bool simd_available = is_integral_v<T> && is_signed_v<T> && (sizeof(T) == 8 || sizeof(T) == 4) && Simd::VECTORIZED;

    // add_dist(a, b) を b の各要素について計算する。a <= INF, -INF <= b < INF を仮定する
    Vec add_dist_vec(T a, Vec b) const {
        const Vec v_inf = Simd::set1(INF);
        const Vec v_ninf = Simd::set1(-INF);
        if (a >= INF) return v_inf;
        if (a <= -INF) return v_ninf;
        Vec sum = Simd::min(Simd::max(Simd::add(Simd::set1(a), b), v_ninf), v_inf);
        return Simd::blend(sum, Simd::cmpeq(b, v_ninf), v_ninf);
    }

    // [-INF, INF) の 2 値の和が T に収まれば SIMD で加算できる
    bool use_simd() const {
        if constexpr (simd_available) return INF <= numeric_limits<T>::max() / 2 + 1;
        return false;
    }

    void build() {
        if constexpr (simd_available) {
            if (use_simd()) {
                const Vec v_inf = Simd::set1(INF);
                const Vec v_ninf = Simd::set1(-INF);
                const int full = n / Simd::LANES * Simd::LANES;  // 端数のない部分の長さ
                for (int k = 0; k < n; ++k) {
                    const T *row_k = dist.data() + (size_t)k*n;
                    for (int i = 0; i < n; ++i) {
                        T dik = dist[(size_t)i*n+k];
                        if (dik == INF) continue;
                        T *row_i = dist.data() + (size_t)i*n;
                        // add_dist(dik, d[k][j]) を、dik についての場合分けをループの外に出して計算する
                        const bool dik_ninf = dik <= -INF;
                        const Vec v_dik = Simd::set1(dik);
                        auto relax = [&](const int j, const Mask lane) {
                            Vec v_kj = Simd::load(lane, row_k+j);
                            Vec v_ij = Simd::load(lane, row_i+j);
                            Mask finite = Simd::cmpneq(lane, v_kj, v_inf);
                            Vec v_new = dik_ninf ? v_ninf : Simd::blend(Simd::min(Simd::max(Simd::add(v_dik, v_kj), v_ninf), v_inf), Simd::cmpeq(v_kj, v_ninf), v_ninf);
                            Mask upd = Simd::cmpgt(finite, v_ij, v_new);
                            // n が大きいとメモリ帯域が律速になるので、更新がなければ書き込まない
                            if (upd) Simd::store(row_i+j, upd, v_new);
                        };
                        for (int j = 0; j < full; j += Simd::LANES) relax(j, Simd::ALL);
                        if (full < n) relax(full, Simd::lane_mask(n - full));
                    }
                }
                return;
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
                    }
                }
            }
        }
    }

    /// @brief 負閉路を経由できる頂点対を記録する / O(|V|^3)
    void set_negative_infinity() {
        vector<int> negs;
        for (int v = 0; v < n; ++v) {
            if (dist[v*n+v] < 0) {
                negs.emplace_back(v);
            }
        }
        neg_cycle = !negs.empty();
        if constexpr (simd_available) {
            if (use_simd()) {
                const Vec v_inf = Simd::set1(INF);
                const auto v_one = Simd::int_set1(1);
                for (int k : negs) {
                    const T *row_k = dist.data() + (size_t)k*n;
                    for (int i = 0; i < n; ++i) {
                        if (dist[(size_t)i*n+k] == INF) continue;
                        for (int j = 0; j < n; j += Simd::LANES) {
                            Mask lane = Simd::lane_mask(n - j);
                            Mask reach = Simd::cmpneq(lane, Simd::load(lane, row_k+j), v_inf);
                            Simd::int_store(neg_inf.data() + (size_t)i*n+j, reach, v_one);
                        }
                    }
                }
                return;
            }
        }
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

    // add_edge の更新部分。行 i は s に到達できる
    void add_edge_rows(T w, bool new_neg_cycle) {
        if constexpr (simd_available) {
            if (use_simd()) {
                const Vec v_inf = Simd::set1(INF);
                const auto v_one = Simd::int_set1(1);
                for (int i = 0; i < n; ++i) {
                    if (!to_s_neg_inf[i] && to_s[i] == INF) continue;
                    T *row_i = dist.data() + (size_t)i*n;
                    int *neg_i = neg_inf.data() + (size_t)i*n;
                    bool whole_row = new_neg_cycle || to_s_neg_inf[i];
                    T a = whole_row ? INF : add_dist(to_s[i], w);
                    for (int j = 0; j < n; j += Simd::LANES) {
                        Mask lane = Simd::lane_mask(n - j);
                        Vec v_from_t = Simd::load(lane, from_t.data()+j);
                        Mask from_neg = Simd::int_nonzero(lane, from_t_neg_inf.data()+j);
                        Mask finite = Simd::cmpneq(lane, v_from_t, v_inf);
                        // t から j へ負閉路を経由できるか、この行全体が負閉路を経由する
                        Mask to_neg = whole_row ? (Mask)(from_neg | finite) : from_neg;
                        if (to_neg) Simd::int_store(neg_i+j, to_neg, v_one);
                        if (whole_row) continue;
                        Mask cand = (Mask)(finite & ~from_neg & ~Simd::int_nonzero(lane, neg_i+j));
                        Vec v_new = add_dist_vec(a, v_from_t);
                        Mask upd = Simd::cmpgt(cand, Simd::load(lane, row_i+j), v_new);
                        if (upd) Simd::store(row_i+j, upd, v_new);
                    }
                }
                return;
            }
        }
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
                T new_dist = add_dist(add_dist(to_s[i], w), from_t[j]);
                if (dist[ij] > new_dist) {
                    dist[ij] = new_dist;
                }
            }
        }
    }

public:
    WarshallFloyd() : n(0), INF(), neg_cycle(false) {}

    /// @brief 隣接リスト G と INF から全点対の最短距離を求める / 辺の重みは INF 未満 / 時間 O(|V|^3)、空間 O(|V|^2)
    WarshallFloyd(const vector<vector<pair<int, T>>> &G, const T INF) :
        n(G.size()), INF(INF), dist(n*n, INF), neg_inf(n*n, 0),
        to_s(n), from_t(n), to_s_neg_inf(n), from_t_neg_inf(n), neg_cycle(false)
    {
        for (int v = 0; v < n; ++v) {
            dist[v*n+v] = 0;
            for (const auto &[x, c]: G[v]) {
                assert(c < INF);
                dist[v*n+x] = min(dist[v*n+x], clamp_weight(c));
            }
        }
        build();
        set_negative_infinity();
    }

    /// @brief 重み w の有向辺 (s, t) を追加して距離を更新する / O(|V|^2)
    void add_edge(int s, int t, T w) {
        w = clamp_weight(w);
        int st = s*n+t;
        if (neg_inf[st] || w >= dist[st]) return;

        for (int i = 0; i < n; ++i) {
            to_s[i] = dist[i*n+s];
            to_s_neg_inf[i] = neg_inf[i*n+s];
            from_t[i] = dist[t*n+i];
            from_t_neg_inf[i] = neg_inf[t*n+i];
        }

        int ts = t*n+s;
        bool new_neg_cycle = neg_inf[ts] || (dist[ts] != INF && add_dist(dist[ts], w) < 0);
        if (new_neg_cycle) neg_cycle = true;
        add_edge_rows(w, new_neg_cycle);
    }

    /// @brief s から t に到達できるかを返す / O(1)
    bool reachable(int s, int t) const {
        int idx = s*n+t;
        return neg_inf[idx] || dist[idx] != INF;
    }

    /// @brief s から t への経路上に負閉路があるかを返す / O(1)
    bool is_neg_inf(int s, int t) const {
        return neg_inf[s*n+t];
    }

    /// @brief 負閉路が存在するかを返す / O(1)
    bool has_neg_cycle() const {
        return neg_cycle;
    }

    /// @brief s から t への最短距離を返す / is_neg_inf(s, t) == false のときに限る / 到達不能なら INF / O(1)
    T get_dist(int s, int t) const {
        assert(!is_neg_inf(s, t));
        return dist[s*n+t];
    }
};
}  // namespace titan23
