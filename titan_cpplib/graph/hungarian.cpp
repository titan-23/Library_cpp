/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/hungarian.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>
#include "titan_cpplib/others/simd_int.cpp"
using namespace std;

namespace titan23 {

namespace hungarian_detail {

template<class T>
using Simd = SimdVec<T>;

template<class T>
int padded(const int m) {
    const int lanes = Simd<T>::LANES;
    return (m + lanes - 1) / lanes * lanes;
}

// 左の頂点 n 個を右の頂点 m 個 (n <= m) へ割り当てる最小費用の割当を返す
// cost は左の頂点ごとに stride 個ずつ並べ、先頭の m 個を使う。stride は padded<T>(m)
template<class T>
vector<int> solve(const T *cost, const int n, const int m, const int stride) {
    using S = Simd<T>;
    using Idx = typename S::Idx;
    const T INF = numeric_limits<T>::has_infinity ? numeric_limits<T>::infinity() : numeric_limits<T>::max();
    // u, v はポテンシャル。key[j] は始点から右の頂点 j までの距離に、走査中の距離の累積を足した値
    vector<T> u(n), v(stride, T(0)), key(stride, INF);
    vector<Idx> way(stride, -1), active(stride, 0);
    fill(active.begin(), active.begin() + m, Idx(-1));
    vector<int> owner(m, -1), match(n, -1), free_cols;
    vector<pair<int, T>> visited;
    visited.reserve(m);
    for (int l = 0; l < n; ++l) {
        const T *row = cost + (size_t)l * stride;
        int best = 0;
        for (int r = 1; r < m; ++r) if (row[r] < row[best]) best = r;
        u[l] = row[best];
        for (int r = best; r < m; ++r) {
            if (row[r] == row[best] && owner[r] == -1) {
                owner[r] = l, match[l] = r;
                break;
            }
        }
    }
    for (int r = 0; r < m; ++r) if (owner[r] == -1) free_cols.push_back(r);
    for (int s = 0; s < n; ++s) {
        if (match[s] != -1) continue;
        T dist = 0;
        int i0 = s, j1;
        Idx j0 = -1;
        while (true) {
            const T *row = cost + (size_t)i0 * stride;
            const T offset = dist - u[i0];
            T best;
            if constexpr (S::VECTORIZED) {
                const auto off = S::set1(offset);
                const auto from = S::iset1(j0), step = S::iset1(S::LANES);
                auto best_val = S::set1(INF);
                auto best_idx = S::iset1(-1), idx = S::iota();
                for (int j = 0; j < stride; j += S::LANES) {
                    const auto c = S::add(S::sub(S::loadu(row + j), S::loadu(v.data() + j)), off);
                    auto k = S::loadu(key.data() + j);
                    const auto upd = S::mask_and(S::lt(c, k), S::load_mask(active.data() + j));
                    k = S::select(upd, c, k);
                    S::storeu(key.data() + j, k);
                    S::istoreu(way.data() + j, S::iselect(upd, from, S::iloadu(way.data() + j)));
                    const auto better = S::lt(k, best_val);
                    best_val = S::select(better, k, best_val);
                    best_idx = S::iselect(better, idx, best_idx);
                    idx = S::iadd(idx, step);
                }
                const auto [b, bj] = S::argmin(best_val, best_idx);
                best = b, j1 = (int)bj;
            } else {
                best = INF, j1 = -1;
                for (int j = 0; j < m; ++j) {
                    if (!active[j]) continue;
                    const T c = row[j] - v[j] + offset;
                    if (c < key[j]) key[j] = c, way[j] = j0;
                    if (key[j] < best) best = key[j], j1 = j;
                }
            }
            dist = best;
            // 同じ距離に空いている列があればそちらで終える。空いている列が多いときは探す手間の方が大きい
            if (owner[j1] != -1 && (int)free_cols.size() * 4 <= m) {
                for (const int j : free_cols) {
                    if (key[j] == best) {
                        j1 = j;
                        break;
                    }
                }
            }
            if (owner[j1] == -1) break;
            visited.emplace_back(j1, best);
            key[j1] = INF;
            active[j1] = 0;
            j0 = j1;
            i0 = owner[j1];
        }
        u[s] += dist;
        for (const auto &[j, kj] : visited) {
            const T d = dist - kj;
            v[j] -= d;
            u[owner[j]] += d;
            active[j] = -1;
        }
        visited.clear();
        fill(key.begin(), key.begin() + m, INF);
        free_cols.erase(find(free_cols.begin(), free_cols.end(), j1));
        for (int j = j1;;) {
            const Idx jp = way[j];
            const int l = jp < 0 ? s : owner[jp];
            owner[j] = l;
            match[l] = j;
            if (jp < 0) break;
            j = (int)jp;
        }
    }
    return match;
}

}  // namespace hungarian_detail

/// @brief ハンガリアン法による最小費用割当 / 費用は有限で、途中の加減算が T に収まること
template<typename T>
class Hungarian {
private:
    int U, V, stride;
    bool transposed;
    vector<T> cost;

public:
    /// @brief U 行 V 列の費用行列を写して持つ / U >= V であること / O(UV)
    Hungarian(const vector<vector<T>> &cost_) : U((int)cost_.size()), V(U ? (int)cost_[0].size() : 0) {
        assert(U >= V);
        assert(all_of(cost_.begin(), cost_.end(), [&](const vector<T> &row) { return (int)row.size() == V; }));
        transposed = U > V;
        stride = hungarian_detail::padded<T>(transposed ? U : V);
        cost.assign((size_t)(transposed ? V : U) * stride, T(0));
        for (int i = 0; i < U; ++i) {
            for (int j = 0; j < V; ++j) {
                if (transposed) cost[(size_t)j * stride + i] = cost_[i][j];
                else cost[(size_t)i * stride + j] = cost_[i][j];
            }
        }
    }

    /// @brief 全 V 列を相異なる行に割り当てる最小費用と、各行が担当する列を返す / 担当のない行は -1 / O(V^2 U)
    pair<T, vector<int>> solve() const {
        vector<int> alloc(U, -1);
        T total = 0;
        if (V == 0) return {total, alloc};
        if (transposed) {
            const vector<int> rows = hungarian_detail::solve(cost.data(), V, U, stride);
            for (int j = 0; j < V; ++j) {
                alloc[rows[j]] = j;
                total += cost[(size_t)j * stride + rows[j]];
            }
        } else {
            alloc = hungarian_detail::solve(cost.data(), U, V, stride);
            for (int i = 0; i < U; ++i) total += cost[(size_t)i * stride + alloc[i]];
        }
        return {total, alloc};
    }
};

/// @brief 小さい方の次元を全て割り当てる最小費用と、各行が担当する列を返す / 担当のない行は -1 / n 行 m 列で O(min(n, m)^2 max(n, m))
template<typename T>
pair<T, vector<int>> min_cost_assignment(const vector<vector<T>> &cost) {
    const int n = (int)cost.size();
    const int m = n ? (int)cost[0].size() : 0;
    if (n >= m) return Hungarian<T>(cost).solve();
    assert(all_of(cost.begin(), cost.end(), [&](const vector<T> &row) { return (int)row.size() == m; }));
    const int stride = hungarian_detail::padded<T>(m);
    vector<T> flat((size_t)n * stride, T(0));
    for (int i = 0; i < n; ++i) copy(cost[i].begin(), cost[i].end(), flat.begin() + (size_t)i * stride);
    const vector<int> assign = hungarian_detail::solve(flat.data(), n, m, stride);
    T total = 0;
    for (int i = 0; i < n; ++i) total += cost[i][assign[i]];
    return {total, assign};
}

}  // namespace titan23
