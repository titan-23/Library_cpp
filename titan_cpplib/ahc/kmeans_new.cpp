#pragma once

#include <vector>
#include <cassert>
#include <algorithm>
#include <type_traits>
#include <limits>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <utility>
#include "titan_cpplib/algorithm/random.cpp"
#include <atcoder/mincostflow>

// Kmeans (refactored)
namespace titan23 {

// Template-Functor 形式: DistFn / MeanFn を型として受けることで
// std::function を介さず直接インライン化できる(ラムダ可)。
//
// 使い方:
//   auto km = titan23::make_kmeans<Point>(
//       /*k=*/5, /*max_iter=*/100,
//       [](const Point& a, const Point& b){ return ...; },
//       [](const std::vector<Point>& v){ return ...; },
//       /*seed=*/0, /*verbose=*/false);
//   auto [labels, centers] = km.fit(X);
//
// `dist` は最小化対象の距離関数。Hamerly 法を使う場合は三角不等式を満たす必要がある。
template <class ElmType, class DistFn, class MeanFn>
class KmeansNew {
public:
    using DistType = std::invoke_result_t<DistFn, const ElmType&, const ElmType&>;
    using SizeRange = std::pair<int, int>;

    // 整数型なら 1.0、浮動小数なら 1e6 を既定値に
    static constexpr double default_cost_scale() {
        if constexpr (std::is_integral_v<DistType>) return 1.0;
        else return 1e6;
    }

private:
    int k_;
    int max_iter_;
    DistFn dist_;
    MeanFn mean_;
    titan23::Random krnd_;
    bool verbose_;

    // ---------- k-means++ 初期化 ----------
    std::vector<ElmType> kmeans_pp(const std::vector<ElmType>& X) {
        int n = (int)X.size();
        assert(n > 0);
        assert(k_ <= n);

        std::vector<ElmType> centers;
        centers.reserve(k_);
        std::vector<bool> picked(n, false);

        int first = krnd_.randrange(n);
        centers.emplace_back(X[first]);
        picked[first] = true;

        // 各点について「これまで選ばれた中心への最小距離の二乗」をインクリメンタルに保持
        std::vector<double> min_d_sq(n, std::numeric_limits<double>::infinity());

        while ((int)centers.size() < k_) {
            const ElmType& last = centers.back();
            for (int i = 0; i < n; ++i) {
                if (picked[i]) continue;
                DistType d = dist_(X[i], last);
                double d_sq = (double)d * (double)d;
                if (d_sq < min_d_sq[i]) min_d_sq[i] = d_sq;
            }

            double total = 0.0;
            for (int i = 0; i < n; ++i) {
                if (picked[i]) continue;
                if (std::isfinite(min_d_sq[i])) total += min_d_sq[i];
            }

            int chosen = -1;
            if (total <= 0.0) {
                for (int i = 0; i < n; ++i) {
                    if (!picked[i]) { chosen = i; break; }
                }
            } else {
                double r = krnd_.random() * total;
                double acc = 0.0;
                for (int i = 0; i < n; ++i) {
                    if (picked[i]) continue;
                    acc += min_d_sq[i];
                    if (acc >= r) { chosen = i; break; }
                }
                if (chosen < 0) {
                    for (int i = n - 1; i >= 0; --i) {
                        if (!picked[i]) { chosen = i; break; }
                    }
                }
            }
            assert(chosen >= 0);
            centers.emplace_back(X[chosen]);
            picked[chosen] = true;
        }
        return centers;
    }

    // ---------- 空クラスタ対策: 最大クラスタの最遠点を新中心にする ----------
    void handle_empty_clusters(
        const std::vector<ElmType>& X,
        std::vector<int>& labels,
        std::vector<ElmType>& new_centers,
        std::vector<std::vector<ElmType>>& grouped,
        const std::vector<ElmType>& old_centers)
    {
        int n = (int)X.size();
        for (int j = 0; j < k_; ++j) {
            if (!grouped[j].empty()) continue;
            int big = -1;
            for (int b = 0; b < k_; ++b) {
                if (b == j) continue;
                if ((int)grouped[b].size() <= 1) continue;
                if (big < 0 || grouped[b].size() > grouped[big].size()) big = b;
            }
            if (big < 0) {
                new_centers[j] = old_centers[j];
                continue;
            }
            DistType max_d = std::numeric_limits<DistType>::lowest();
            int best_i = -1;
            for (int i = 0; i < n; ++i) {
                if (labels[i] != big) continue;
                DistType d = dist_(X[i], new_centers[big]);
                if (best_i < 0 || d > max_d) { max_d = d; best_i = i; }
            }
            if (best_i < 0) {
                new_centers[j] = old_centers[j];
                continue;
            }
            new_centers[j] = X[best_i];
            labels[best_i] = j;
            grouped[j].clear();
            grouped[j].emplace_back(X[best_i]);
            grouped[big].clear();
            for (int i = 0; i < n; ++i) {
                if (labels[i] == big) grouped[big].emplace_back(X[i]);
            }
            if (!grouped[big].empty()) new_centers[big] = mean_(grouped[big]);
        }
    }

    DistType compute_inertia_internal(
        const std::vector<ElmType>& X,
        const std::vector<int>& labels,
        const std::vector<ElmType>& centers)
    {
        DistType total = DistType(0);
        int n = (int)X.size();
        for (int i = 0; i < n; ++i) {
            if (labels[i] < 0) continue;
            total = total + dist_(X[i], centers[labels[i]]);
        }
        return total;
    }

    void recompute_centers(
        const std::vector<ElmType>& X,
        std::vector<int>& labels,
        std::vector<std::vector<ElmType>>& grouped,
        std::vector<ElmType>& new_centers,
        const std::vector<ElmType>& old_centers,
        bool& empty_occurred)
    {
        int n = (int)X.size();
        for (auto& v : grouped) v.clear();
        for (int i = 0; i < n; ++i) grouped[labels[i]].emplace_back(X[i]);
        empty_occurred = false;
        for (int j = 0; j < k_; ++j) {
            if (!grouped[j].empty()) {
                new_centers[j] = mean_(grouped[j]);
            } else {
                new_centers[j] = old_centers[j];
                empty_occurred = true;
            }
        }
        if (empty_occurred) handle_empty_clusters(X, labels, new_centers, grouped, old_centers);
    }

    // ---------- 標準/ハマリィ共用の本体 ----------
    template <bool UseBounds>
    std::pair<std::vector<int>, std::vector<ElmType>> fit_impl(
        const std::vector<ElmType>& X, std::vector<ElmType> centers)
    {
        int n = (int)X.size();
        assert(k_ <= n);
        assert((int)centers.size() == k_);

        std::vector<int> labels(n, -1);
        std::vector<std::vector<ElmType>> grouped(k_);
        std::vector<ElmType> new_centers(k_);

        if constexpr (UseBounds) {
            // Hamerly:
            //   u[i] : 上界 ≥ dist(X[i], centers[labels[i]])
            //   l[i] : 下界 ≤ dist(X[i], 2 番目に近いクラスタ中心)
            //   s[j] : (1/2) * min_{j' != j} dist(c_j, c_{j'})
            // 三角不等式により u[i] <= max(s[a], l[i]) ならその点は再評価不要。
            std::vector<DistType> u(n), l(n);
            std::vector<DistType> s(k_);
            std::vector<DistType> drifts(k_);

            for (int i = 0; i < n; ++i) {
                DistType best_d = dist_(X[i], centers[0]);
                int best_j = 0;
                DistType second_d = std::numeric_limits<DistType>::max();
                for (int j = 1; j < k_; ++j) {
                    DistType d = dist_(X[i], centers[j]);
                    if (d < best_d) { second_d = best_d; best_d = d; best_j = j; }
                    else if (d < second_d) { second_d = d; }
                }
                labels[i] = best_j;
                u[i] = best_d;
                l[i] = second_d;
            }

            for (int iter = 0; iter < max_iter_; ++iter) {
                if (k_ >= 2) {
                    for (int j = 0; j < k_; ++j) {
                        DistType min_d = std::numeric_limits<DistType>::max();
                        for (int jp = 0; jp < k_; ++jp) {
                            if (jp == j) continue;
                            DistType d = dist_(centers[j], centers[jp]);
                            if (d < min_d) min_d = d;
                        }
                        s[j] = min_d / DistType(2);
                    }
                } else {
                    s[0] = std::numeric_limits<DistType>::max();
                }

                bool changed = false;
                for (int i = 0; i < n; ++i) {
                    int a = labels[i];
                    DistType m = std::max(s[a], l[i]);
                    if (!(u[i] > m)) continue;
                    u[i] = dist_(X[i], centers[a]);
                    if (!(u[i] > m)) continue;

                    DistType best_d = u[i];
                    int best_j = a;
                    DistType second_d = std::numeric_limits<DistType>::max();
                    for (int j = 0; j < k_; ++j) {
                        if (j == a) continue;
                        DistType d = dist_(X[i], centers[j]);
                        if (d < best_d) { second_d = best_d; best_d = d; best_j = j; }
                        else if (d < second_d) { second_d = d; }
                    }
                    if (best_j != a) { labels[i] = best_j; changed = true; }
                    u[i] = best_d;
                    l[i] = second_d;
                }

                // iter > 0 ガードが必要: 初期割当はループ外で済んでいるため、
                // iter==0 で changed==false でもまだ平均更新が一度も走っていない。
                if (!changed && iter > 0) {
                    if (verbose_) std::cerr << "[Kmeans] hamerly converged at iter=" << iter << "\n";
                    break;
                }

                bool empty_occurred = false;
                recompute_centers(X, labels, grouped, new_centers, centers, empty_occurred);

                // ドリフト量と最大/二番手最大
                DistType max_drift = DistType(0);
                int max_drift_idx = 0;
                DistType second_max_drift = DistType(0);
                bool first = true;
                for (int j = 0; j < k_; ++j) {
                    drifts[j] = dist_(centers[j], new_centers[j]);
                    if (first) {
                        max_drift = drifts[j];
                        max_drift_idx = j;
                        first = false;
                    } else if (drifts[j] > max_drift) {
                        second_max_drift = max_drift;
                        max_drift = drifts[j];
                        max_drift_idx = j;
                    } else if (drifts[j] > second_max_drift) {
                        second_max_drift = drifts[j];
                    }
                }

                if (empty_occurred) {
                    // 空クラスタ補填でラベルが手動変更されたためバウンドを無効化
                    std::fill(u.begin(), u.end(), std::numeric_limits<DistType>::max());
                    std::fill(l.begin(), l.end(), DistType(0));
                } else {
                    for (int i = 0; i < n; ++i) {
                        int a = labels[i];
                        u[i] = u[i] + drifts[a];
                        DistType dec = (a == max_drift_idx) ? second_max_drift : max_drift;
                        l[i] = (l[i] > dec) ? (l[i] - dec) : DistType(0);
                    }
                }

                centers = std::move(new_centers);
                new_centers.assign(k_, ElmType{});

                if (verbose_) {
                    std::cerr << "[Kmeans] hamerly iter=" << iter << "\n";
                }
            }
            return {labels, centers};
        } else {
            for (int iter = 0; iter < max_iter_; ++iter) {
                bool changed = false;
                for (int i = 0; i < n; ++i) {
                    DistType best_d = dist_(X[i], centers[0]);
                    int best_j = 0;
                    for (int j = 1; j < k_; ++j) {
                        DistType d = dist_(X[i], centers[j]);
                        if (d < best_d) { best_d = d; best_j = j; }
                    }
                    if (labels[i] != best_j) { labels[i] = best_j; changed = true; }
                }

                if (!changed) {
                    if (verbose_) std::cerr << "[Kmeans] converged at iter=" << iter << "\n";
                    break;
                }

                bool empty_occurred = false;
                recompute_centers(X, labels, grouped, new_centers, centers, empty_occurred);
                centers = std::move(new_centers);
                new_centers.assign(k_, ElmType{});

                if (verbose_) {
                    DistType ine = compute_inertia_internal(X, labels, centers);
                    std::cerr << "[Kmeans] iter=" << iter << " inertia=" << ine << "\n";
                }
            }
            return {labels, centers};
        }
    }

    // ---------- 区間制約付き MCF (下限付きフロー変換) ----------
    //
    // ノード割り当て:
    //   points    : 0 ..  n-1
    //   clusters  : n ..  n+K-1
    //   S = n+K, T = n+K+1, SS = n+K+2, TT = n+K+3
    //
    // 元の制約:
    //   S → i        : cap [0,1], cost 0
    //   i → j        : cap [0,1], cost dist(i,j)*scale
    //   j → T        : cap [lo_j, hi_j], cost 0
    //   T → S (循環) : cap [n,n], cost 0  (各点が一度だけ割当られることを強制)
    //
    // 下限付きフロー → 標準フローへの変換:
    //   各下限 lo を取り除いた残余グラフに対し、各ノードの「過剰」を SS/TT で吸収。
    //   excess(S)  = +n          → SS → S, cap n
    //   excess(T)  = sum(lo) - n → 負なら T → TT, cap n - sum(lo)
    //   excess(j)  = -lo_j       → j  → TT, cap lo_j
    //   target = n
    std::pair<std::vector<int>, std::vector<ElmType>> fit_flow_impl(
        const std::vector<ElmType>& X,
        std::vector<ElmType> centers,
        const std::vector<SizeRange>& size_ranges,
        double cost_scale)
    {
        int n = (int)X.size();
        int K = k_;
        assert(K <= n);
        assert((int)centers.size() == K);
        assert((int)size_ranges.size() == K);

        long long sum_lo = 0, sum_hi = 0;
        for (int j = 0; j < K; ++j) {
            int lo = size_ranges[j].first;
            int hi = size_ranges[j].second;
            assert(0 <= lo && lo <= hi);
            sum_lo += lo;
            sum_hi += hi;
        }
        assert(sum_lo <= (long long)n);
        assert((long long)n <= sum_hi);

        const int S = n + K, T = n + K + 1, SS = n + K + 2, TT = n + K + 3;
        const int total_nodes = n + K + 4;

        std::vector<int> labels(n, -1);
        std::vector<std::vector<ElmType>> grouped(K);
        std::vector<ElmType> new_centers(K);

        const long long T_diff = (long long)n - sum_lo; // ≥ 0
        const long long target = (long long)n;

        for (int iter = 0; iter < max_iter_; ++iter) {
            atcoder::mcf_graph<int, long long> graph(total_nodes);

            for (int i = 0; i < n; ++i) graph.add_edge(S, i, 1, 0);

            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < K; ++j) {
                    DistType d = dist_(X[i], centers[j]);
                    long long cost = (long long)std::llround((double)d * cost_scale);
                    graph.add_edge(i, n + j, 1, cost);
                }
            }

            for (int j = 0; j < K; ++j) {
                int free_cap = size_ranges[j].second - size_ranges[j].first;
                if (free_cap > 0) graph.add_edge(n + j, T, free_cap, 0);
            }

            graph.add_edge(SS, S, n, 0);
            if (T_diff > 0) graph.add_edge(T, TT, (int)T_diff, 0);
            for (int j = 0; j < K; ++j) {
                int lo = size_ranges[j].first;
                if (lo > 0) graph.add_edge(n + j, TT, lo, 0);
            }

            auto result = graph.flow(SS, TT, (int)target);
            assert((long long)result.first == target);

            bool changed = false;
            for (const auto& e : graph.edges()) {
                if (e.from < n && e.to >= n && e.to < n + K && e.flow == 1) {
                    int c_id = e.to - n;
                    if (labels[e.from] != c_id) {
                        labels[e.from] = c_id;
                        changed = true;
                    }
                }
            }

            if (!changed) {
                if (verbose_) std::cerr << "[Kmeans] flow converged at iter=" << iter << "\n";
                break;
            }

            bool empty_occurred = false;
            recompute_centers(X, labels, grouped, new_centers, centers, empty_occurred);
            centers = std::move(new_centers);
            new_centers.assign(K, ElmType{});

            if (verbose_) {
                DistType ine = compute_inertia_internal(X, labels, centers);
                std::cerr << "[Kmeans] flow iter=" << iter << " inertia=" << ine << "\n";
            }
        }

        return {labels, centers};
    }

public:
    KmeansNew(int k,
           int max_iter,
           DistFn dist,
           MeanFn mean,
           std::uint32_t seed = 0,
           bool verbose = false)
        : k_(k),
          max_iter_(max_iter),
          dist_(std::move(dist)),
          mean_(std::move(mean)),
          krnd_(seed),
          verbose_(verbose) {}

    void set_seed(std::uint32_t seed) { krnd_.set_seed(seed); }
    void set_verbose(bool v) { verbose_ = v; }

    // ---------- 標準 k-means ----------
    std::pair<std::vector<int>, std::vector<ElmType>> fit(const std::vector<ElmType>& X) {
        return fit_impl<false>(X, kmeans_pp(X));
    }

    std::pair<std::vector<int>, std::vector<ElmType>> fit(
        const std::vector<ElmType>& X, const std::vector<ElmType>& init_centers)
    {
        assert((int)init_centers.size() == k_);
        return fit_impl<false>(X, init_centers);
    }

    // ---------- Hamerly 法 (dist が三角不等式を満たすこと) ----------
    std::pair<std::vector<int>, std::vector<ElmType>> fit_hamerly(const std::vector<ElmType>& X) {
        return fit_impl<true>(X, kmeans_pp(X));
    }

    std::pair<std::vector<int>, std::vector<ElmType>> fit_hamerly(
        const std::vector<ElmType>& X, const std::vector<ElmType>& init_centers)
    {
        assert((int)init_centers.size() == k_);
        return fit_impl<true>(X, init_centers);
    }

    // ---------- マルチスタート ----------
    template <bool UseBounds = false>
    std::pair<std::vector<int>, std::vector<ElmType>> fit_best(
        const std::vector<ElmType>& X, int n_init)
    {
        assert(n_init >= 1);
        std::vector<int> best_labels;
        std::vector<ElmType> best_centers;
        DistType best_ine = DistType(0);
        for (int t = 0; t < n_init; ++t) {
            std::pair<std::vector<int>, std::vector<ElmType>> result;
            if constexpr (UseBounds) result = fit_hamerly(X);
            else result = fit(X);
            DistType ine = compute_inertia_internal(X, result.first, result.second);
            if (t == 0 || ine < best_ine) {
                best_ine = ine;
                best_labels = std::move(result.first);
                best_centers = std::move(result.second);
            }
        }
        return {best_labels, best_centers};
    }

    // ---------- 等しいクラスタサイズ制約 ----------
    std::pair<std::vector<int>, std::vector<ElmType>> fit_flow(
        const std::vector<ElmType>& X,
        const std::vector<ElmType>& init_centers,
        const std::vector<int>& target_sizes,
        double cost_scale = default_cost_scale())
    {
        assert((int)target_sizes.size() == k_);
        std::vector<SizeRange> ranges(k_);
        for (int j = 0; j < k_; ++j) ranges[j] = {target_sizes[j], target_sizes[j]};
        return fit_flow_impl(X, init_centers, ranges, cost_scale);
    }

    std::pair<std::vector<int>, std::vector<ElmType>> fit_flow(
        const std::vector<ElmType>& X,
        const std::vector<int>& target_sizes,
        double cost_scale = default_cost_scale())
    {
        return fit_flow(X, kmeans_pp(X), target_sizes, cost_scale);
    }

    // ---------- 区間クラスタサイズ制約 [lo_j, hi_j] ----------
    std::pair<std::vector<int>, std::vector<ElmType>> fit_flow_range(
        const std::vector<ElmType>& X,
        const std::vector<ElmType>& init_centers,
        const std::vector<SizeRange>& size_ranges,
        double cost_scale = default_cost_scale())
    {
        return fit_flow_impl(X, init_centers, size_ranges, cost_scale);
    }

    std::pair<std::vector<int>, std::vector<ElmType>> fit_flow_range(
        const std::vector<ElmType>& X,
        const std::vector<SizeRange>& size_ranges,
        double cost_scale = default_cost_scale())
    {
        return fit_flow_range(X, kmeans_pp(X), size_ranges, cost_scale);
    }

    // ---------- inertia (∑ dist(X[i], c_{labels[i]})) ----------
    DistType inertia(
        const std::vector<ElmType>& X,
        const std::vector<int>& labels,
        const std::vector<ElmType>& centers)
    {
        return compute_inertia_internal(X, labels, centers);
    }
};

template <class ElmType, class DistFn, class MeanFn>
auto make_kmeans(int k,
                 int max_iter,
                 DistFn dist,
                 MeanFn mean,
                 std::uint32_t seed = 0,
                 bool verbose = false)
{
    return KmeansNew<ElmType, DistFn, MeanFn>(
        k, max_iter, std::move(dist), std::move(mean), seed, verbose);
}

} // namespace titan23
