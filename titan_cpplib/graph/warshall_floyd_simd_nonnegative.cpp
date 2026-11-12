/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/warshall_floyd_simd_nonnegative.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <climits>
#include <limits>
#include <type_traits>
#include <vector>
#include "titan_cpplib/others/simd_int.cpp"

using namespace std;

// WarshallFloydSIMDNonnegative
namespace titan23 {

using ll = long long;

/**
 * @brief 非負辺専用の Warshall-Floyd
 * @details AVX-512 (F, VL) > AVX2 > 逐次 の順にコンパイル時に選ぶ。
 *          T は 32 bit または 64 bit の符号付き整数。構築時および `add_edge` で与える辺の重みは `0` 以上。
 *          到達不能な頂点対の距離は INF で、最短距離は INF 未満に収まる必要がある。
 */
template<typename T = long long>
class WarshallFloydSIMDNonnegative {
    static_assert(is_integral_v<T> && is_signed_v<T> && (sizeof(T) == 4 || sizeof(T) == 8),
                  "WarshallFloydSIMDNonnegative: T must be a 32-bit or 64-bit signed integer");
    using Ops = SimdInt<T>;
    using Vec = typename Ops::Vec;

public:
    // 3 値の和が T に収まる大きさ
    static constexpr T INF = numeric_limits<T>::max() / 3;

private:
    int n;
    vector<T> d;

public:
    WarshallFloydSIMDNonnegative() : n(0) {}

    /// @brief 時間 O(|V|^3), 空間 O(|V|^2)
    WarshallFloydSIMDNonnegative(const vector<vector<pair<int, T>>> &G) : n(G.size()) {
        n = (n + Ops::LANES - 1) / Ops::LANES * Ops::LANES;
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
                Vec v_d_ik = Ops::set1(d[i*n+k]);
                for (int j = 0; j < n; j += Ops::LANES) {
                    Vec v_d_ij = Ops::loadu(&d[i*n+j]);
                    Vec v_d_kj = Ops::loadu(&d[k*n+j]);
                    Vec v_sum = Ops::add(v_d_ik, v_d_kj);
                    Vec v_min = Ops::min(v_d_ij, v_sum);
                    Ops::storeu(&d[i*n+j], v_min);
                }
            }
        }
    }

    /// @brief 重み `w` の有向辺 `(s, t)` を追加する / O(|V|^2)
    void add_edge(int s, int t, T w) {
        assert(w >= 0);
        if (w >= d[s*n+t]) return;
        d[s*n+t] = w;
        for (int i = 0; i < n; ++i) {
            if (d[i*n+s] == INF) continue;
            Vec v_isw = Ops::set1(d[i*n+s]+w);
            for (int j = 0; j < n; j += Ops::LANES) {
                Vec v_d_ij = Ops::loadu(&d[i*n+j]);
                Vec v_d_tj = Ops::loadu(&d[t*n+j]);
                Vec v_new_d = Ops::add(v_isw, v_d_tj);
                Vec v_min = Ops::min(v_d_ij, v_new_d);
                Ops::storeu(&d[i*n+j], v_min);
            }
        }
    }

    /// @brief 到達不能なら INF / O(1)
    T get_dist(int s, int t) const {
        return d[s*n+t];
    }
};
}  // namespace titan23
