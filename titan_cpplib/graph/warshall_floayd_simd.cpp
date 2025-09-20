#include <iostream>
#include <vector>
#include <algorithm>
#include <immintrin.h>

using namespace std;

// WarshallFloydSIMD
namespace titan23 {

class WarshallFloydSIMD {
private:
    int n;
    ll INF = LLONG_MAX / 3;
    vector<ll> d;

public:
    WarshallFloydSIMD() {}

    // 時間 O(|V|^3), 空間 O(|V|^2)
    WarshallFloydSIMD(const vector<vector<pair<int, ll>>> &G) : n(G.size()) {
        n = (n + 7) & ~7;
        d = vector<ll>(n*n, INF);

        for (int v = 0; v < (int)G.size(); ++v) {
            for (const auto &[x, c]: G[v]) {
                d[v*n+x] = min(d[v*n+x], c);
            }
            d[v*n+v] = 0;
        }

        for (int k = 0; k < n; ++k) {
            for (int i = 0; i < n; ++i) {
                if (i == k || d[i*n+k] == INF) continue;
                // for (int j = 0; j < n; ++j) {
                //     d[i*n+j] = min(d[i*n+j], d[i*n+k]+d[k*n+j]);
                // }
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

    // 重みwの辺(s, t)を追加する / O(|V|^2)
    void add_edge(int s, int t, ll w) {
        if (w >= d[s*n+t]) return;
        d[s*n+t] = w;
        for (int i = 0; i < n; ++i) {
            if (d[i*n+s] == INF) continue;
            // for (int j = 0; j < n; ++j) {
            //     d[i*n+j] = min(d[i*n+j], d[i*n+s]+w+d[t*n+j]);
            // }
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

    ll get_inf() const {
        return INF;
    }

    // O(1)
    ll get_dist(int s, int t) const {
        return d[s*n+t];
    }
};
}  // namespace titan23
