#pragma once

#include <algorithm>
#include <cassert>
#include <climits>
#include <immintrin.h>
#include <vector>

using namespace std;

// WarshallFloydSIMDNonnegative
namespace titan23 {

using ll = long long;

/**
 * @brief 非負辺専用の Warshall-Floyd
 * @details 構築時および `add_edge` で与える辺の重みは `0` 以上。
 */
class WarshallFloydSIMDNonnegative {
private:
    int n;
    ll INF = LLONG_MAX / 3;
    vector<ll> d;

public:
    WarshallFloydSIMDNonnegative() : n(0) {}

    /// @brief 時間 O(|V|^3), 空間 O(|V|^2)
    WarshallFloydSIMDNonnegative(const vector<vector<pair<int, ll>>> &G) : n(G.size()) {
        n = (n + 7) & ~7;
        d.assign(n*n, INF);

        for (int v = 0; v < (int)G.size(); ++v) {
            d[v*n+v] = 0;
            for (const auto &[x, c]: G[v]) {
                assert(c >= 0);
                d[v*n+x] = min(d[v*n+x], c);
            }
        }

        for (int k = 0; k < n; ++k) {
            for (int i = 0; i < n; ++i) {
                if (i == k || d[i*n+k] == INF) continue;
                __m512i v_d_ik = _mm512_set1_epi64(d[i*n+k]);
                for (int j = 0; j < n; j += 8) {
                    __m512i v_d_ij = _mm512_loadu_si512((__m512i*)&d[i*n+j]);
                    __m512i v_d_kj = _mm512_loadu_si512((__m512i*)&d[k*n+j]);
                    __m512i v_sum = _mm512_add_epi64(v_d_ik, v_d_kj);
                    __m512i v_min = _mm512_min_epi64(v_d_ij, v_sum);
                    _mm512_storeu_si512((__m512i*)&d[i*n+j], v_min);
                }
            }
        }
    }

    /// @brief 重み `w` の有向辺 `(s, t)` を追加する / O(|V|^2)
    void add_edge(int s, int t, ll w) {
        assert(w >= 0);
        if (w >= d[s*n+t]) return;
        d[s*n+t] = w;
        for (int i = 0; i < n; ++i) {
            if (d[i*n+s] == INF) continue;
            __m512i v_isw = _mm512_set1_epi64(d[i*n+s]+w);
            for (int j = 0; j < n; j += 8) {
                __m512i v_d_ij = _mm512_loadu_si512((__m512i*)&d[i*n+j]);
                __m512i v_d_tj = _mm512_loadu_si512((__m512i*)&d[t*n+j]);
                __m512i v_new_d = _mm512_add_epi64(v_isw, v_d_tj);
                __m512i v_min = _mm512_min_epi64(v_d_ij, v_new_d);
                _mm512_storeu_si512((__m512i*)&d[i*n+j], v_min);
            }
        }
    }

    /// @brief O(1)
    ll get_dist(int s, int t) const {
        return d[s*n+t];
    }
};
}  // namespace titan23
