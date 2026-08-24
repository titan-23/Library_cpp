/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/warshall_floyd_simd.cpp
#pragma once

#include <cassert>
#include <iostream>
#include <vector>
#include <algorithm>
#include <climits>
#include <immintrin.h>

using namespace std;

namespace titan23 {

using ll = long long;

/**
 * @brief AVX-512 を用いた Warshall-Floyd
 */
class WarshallFloydSIMD {
private:
    int n;
    ll INF = LLONG_MAX / 3;
    vector<ll> d;
    vector<int> neg_inf;
    vector<ll> to_s;
    vector<ll> from_t;
    vector<int> to_s_neg_inf;
    vector<int> from_t_neg_inf;
    bool neg_cycle;

public:
    WarshallFloydSIMD() : n(0), neg_cycle(false) {}

    /// @brief 時間 O(|V|^3), 空間 O(|V|^2)
    WarshallFloydSIMD(const vector<vector<pair<int, ll>>> &G) :
        n(G.size()), neg_cycle(false)
    {
        n = (n + 7) & ~7;
        d.assign(n*n, INF);
        neg_inf.assign(n*n, 0);
        to_s.resize(n);
        from_t.resize(n);
        to_s_neg_inf.resize(n);
        from_t_neg_inf.resize(n);

        for (int v = 0; v < (int)G.size(); ++v) {
            d[v*n+v] = 0;
            for (const auto &[x, c]: G[v]) {
                ll cost = c < -INF ? -INF : c;
                d[v*n+x] = min(d[v*n+x], cost);
            }
        }

        __m512i v_inf = _mm512_set1_epi64(INF);
        __m512i v_ninf = _mm512_set1_epi64(-INF);
        for (int k = 0; k < n; ++k) {
            for (int i = 0; i < n; ++i) {
                if (i == k || d[i*n+k] == INF) continue;
                __m512i v_d_ik = _mm512_set1_epi64(d[i*n+k]);
                for (int j = 0; j < n; j += 8) {
                    __m512i v_d_ij = _mm512_loadu_si512((__m512i*)&d[i*n+j]);
                    __m512i v_d_kj = _mm512_loadu_si512((__m512i*)&d[k*n+j]);
                    __mmask8 finite = _mm512_cmpneq_epi64_mask(v_d_kj, v_inf);
                    __m512i v_sum = _mm512_add_epi64(v_d_ik, v_d_kj);
                    v_sum = _mm512_max_epi64(v_sum, v_ninf);
                    __m512i v_min = _mm512_min_epi64(v_d_ij, v_sum);
                    v_min = _mm512_mask_mov_epi64(v_d_ij, finite, v_min);
                    _mm512_storeu_si512((__m512i*)&d[i*n+j], v_min);
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
    void add_edge(int s, int t, ll w) {
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

        __m512i v_inf = _mm512_set1_epi64(INF);
        __m512i v_ninf = _mm512_set1_epi64(-INF);
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

            __m512i v_isw = _mm512_set1_epi64(to_s[i]+w);
            for (int j = 0; j < n; j += 8) {
                __m512i v_d_ij = _mm512_loadu_si512((__m512i*)&d[i*n+j]);
                __m512i v_from_t = _mm512_loadu_si512((__m512i*)&from_t[j]);
                __mmask8 finite = _mm512_cmpneq_epi64_mask(v_from_t, v_inf);
                __m512i v_new_d = _mm512_add_epi64(v_isw, v_from_t);
                v_new_d = _mm512_max_epi64(v_new_d, v_ninf);
                __m512i v_min = _mm512_min_epi64(v_d_ij, v_new_d);
                v_min = _mm512_mask_mov_epi64(v_d_ij, finite, v_min);
                _mm512_storeu_si512((__m512i*)&d[i*n+j], v_min);
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
    ll get_dist(int s, int t) const {
        assert(!is_neg_inf(s, t));
        return d[s*n+t];
    }
};
}  // namespace titan23
