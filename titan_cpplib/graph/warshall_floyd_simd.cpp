/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/warshall_floyd_simd.cpp
#pragma once

#include <cassert>
#include <iostream>
#include <vector>
#include <algorithm>
#include <climits>
#include <limits>
#include <type_traits>
#include "titan_cpplib/others/simd_int.cpp"

using namespace std;

namespace titan23 {

using ll = long long;

/**
 * @brief SIMD を用いた Warshall-Floyd
 * @details AVX-512 (F, VL) > AVX2 > 逐次 の順にコンパイル時に選ぶ。
 *          T は 32 bit または 64 bit の符号付き整数。距離は [-INF, INF] に収め、INF は到達不能を表す。
 *          負閉路を経由しない頂点対の最短距離は (-INF, INF) に収まる必要がある。
 */
template<typename T = long long>
class WarshallFloydSIMD {
    static_assert(is_integral_v<T> && is_signed_v<T> && (sizeof(T) == 4 || sizeof(T) == 8),
                  "WarshallFloydSIMD: T must be a 32-bit or 64-bit signed integer");
    using Ops = SimdInt<T>;
    using Vec = typename Ops::Vec;

public:
    // 3 値の和が T に収まる大きさ
    static constexpr T INF = numeric_limits<T>::max() / 3;

private:
    int n;
    vector<T> d;
    vector<int> neg_inf;
    vector<T> to_s;
    vector<T> from_t;
    vector<int> to_s_neg_inf;
    vector<int> from_t_neg_inf;
    bool neg_cycle;

public:
    WarshallFloydSIMD() : n(0), neg_cycle(false) {}

    /// @brief 時間 O(|V|^3), 空間 O(|V|^2)
    WarshallFloydSIMD(const vector<vector<pair<int, T>>> &G) :
        n(G.size()), neg_cycle(false)
    {
        n = (n + Ops::LANES - 1) / Ops::LANES * Ops::LANES;
        d.assign(n*n, INF);
        neg_inf.assign(n*n, 0);
        to_s.resize(n);
        from_t.resize(n);
        to_s_neg_inf.resize(n);
        from_t_neg_inf.resize(n);

        for (int v = 0; v < (int)G.size(); ++v) {
            d[v*n+v] = 0;
            for (const auto &[x, c]: G[v]) {
                T cost = c < -INF ? -INF : c;
                d[v*n+x] = min(d[v*n+x], cost);
            }
        }

        const Vec v_inf = Ops::set1(INF);
        const Vec v_ninf = Ops::set1(-INF);
        for (int k = 0; k < n; ++k) {
            for (int i = 0; i < n; ++i) {
                if (i == k || d[i*n+k] == INF) continue;
                Vec v_d_ik = Ops::set1(d[i*n+k]);
                for (int j = 0; j < n; j += Ops::LANES) {
                    Vec v_d_ij = Ops::loadu(&d[i*n+j]);
                    Vec v_d_kj = Ops::loadu(&d[k*n+j]);
                    auto finite = Ops::cmpneq(Ops::ALL, v_d_kj, v_inf);
                    Vec v_sum = Ops::add(v_d_ik, v_d_kj);
                    v_sum = Ops::max(v_sum, v_ninf);
                    Vec v_min = Ops::min(v_d_ij, v_sum);
                    v_min = Ops::blend(v_d_ij, finite, v_min);
                    Ops::storeu(&d[i*n+j], v_min);
                }
            }
        }

        vector<int> negs;
        for (int v = 0; v < n; ++v) {
            if (d[v*n+v] < 0) {
                negs.emplace_back(v);
            }
        }
        neg_cycle = !negs.empty();
        for (int k : negs) {
            for (int i = 0; i < n; ++i) {
                if (d[i*n+k] == INF) continue;
                for (int j = 0; j < n; ++j) {
                    if (d[k*n+j] != INF) {
                        neg_inf[i*n+j] = 1;
                    }
                }
            }
        }
    }

    /// @brief 重み `w` の有向辺 `(s, t)` を追加する / O(|V|^2)
    void add_edge(int s, int t, T w) {
        if (w < -INF) w = -INF;
        int st = s*n+t;
        if (neg_inf[st] || w >= d[st]) return;

        bool from_t_has_neg_inf = false;
        for (int i = 0; i < n; ++i) {
            to_s[i] = d[i*n+s];
            to_s_neg_inf[i] = neg_inf[i*n+s];
            from_t[i] = d[t*n+i];
            from_t_neg_inf[i] = neg_inf[t*n+i];
            if (from_t_neg_inf[i]) {
                from_t[i] = INF;
                from_t_has_neg_inf = true;
            }
        }

        int ts = t*n+s;
        bool new_neg_cycle = neg_inf[ts] || (d[ts] != INF && d[ts] + w < 0);
        if (new_neg_cycle) neg_cycle = true;

        const Vec v_inf = Ops::set1(INF);
        const Vec v_ninf = Ops::set1(-INF);
        for (int i = 0; i < n; ++i) {
            if (!to_s_neg_inf[i] && to_s[i] == INF) continue;

            if (new_neg_cycle || to_s_neg_inf[i]) {
                for (int j = 0; j < n; ++j) {
                    if (from_t_neg_inf[j] || from_t[j] != INF) {
                        neg_inf[i*n+j] = 1;
                    }
                }
                continue;
            }

            if (from_t_has_neg_inf) {
                for (int j = 0; j < n; ++j) {
                    if (from_t_neg_inf[j]) {
                        neg_inf[i*n+j] = 1;
                    }
                }
            }

            Vec v_isw = Ops::set1(to_s[i]+w);
            for (int j = 0; j < n; j += Ops::LANES) {
                Vec v_d_ij = Ops::loadu(&d[i*n+j]);
                Vec v_from_t = Ops::loadu(&from_t[j]);
                auto finite = Ops::cmpneq(Ops::ALL, v_from_t, v_inf);
                Vec v_new_d = Ops::add(v_isw, v_from_t);
                v_new_d = Ops::max(v_new_d, v_ninf);
                Vec v_min = Ops::min(v_d_ij, v_new_d);
                v_min = Ops::blend(v_d_ij, finite, v_min);
                Ops::storeu(&d[i*n+j], v_min);
            }
        }
    }

    /// @brief `s` から `t` に到達できるか / O(1)
    bool reachable(int s, int t) const {
        int idx = s*n+t;
        return neg_inf[idx] || d[idx] != INF;
    }

    /// @brief `s` から `t` への経路上に負閉路があるか / O(1)
    bool is_neg_inf(int s, int t) const {
        return neg_inf[s*n+t];
    }

    /// @brief 負閉路が存在するか / O(1)
    bool has_neg_cycle() const {
        return neg_cycle;
    }

    /// @brief `is_neg_inf(s, t) == false` のときのみ有効 / O(1)
    T get_dist(int s, int t) const {
        assert(!is_neg_inf(s, t));
        return d[s*n+t];
    }
};
}  // namespace titan23
