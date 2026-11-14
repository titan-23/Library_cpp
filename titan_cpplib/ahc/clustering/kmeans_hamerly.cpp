/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/clustering/kmeans_hamerly.cpp
#pragma once
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include "titan_cpplib/ahc/clustering/kmeans.cpp"
using namespace std;
namespace titan23 {
namespace kmeans_detail {
template <class Distance, class Value>
Distance metric_value(Value value) {
    return (Distance)value;
}

// 距離関数が euclidean_metric = true を持つとき、ユークリッド距離を SIMD で求める
template <class MetricFn, class = void>
struct is_euclidean_metric : false_type {};
template <class MetricFn>
struct is_euclidean_metric<MetricFn, void_t<decltype(MetricFn::euclidean_metric)>> : bool_constant<MetricFn::euclidean_metric> {};

// 二乗距離が最小の中心と二乗距離を返す。同じなら番号が最小の中心
inline pair<int, double> nearest_squared(const double* point, const Columns& centers) {
    using S = Columns::S;
    auto best = S::set1(numeric_limits<double>::infinity());
    auto best_index = S::iset1(-1), index = S::iota();
    const auto step = S::iset1(S::LANES);
    for (int j = 0; j < centers.stride; j += S::LANES) {
        const auto value = centers.squared(point, j);
        const auto better = S::lt(value, best);
        best = S::select(better, value, best);
        best_index = S::iselect(better, index, best_index);
        index = S::iadd(index, step);
    }
    const auto [value, j] = S::argmin(best, best_index);
    // どの中心とも距離が +inf のときは、逐次の探索と同じく番号 0 を返す
    if (j < 0) return {0, numeric_limits<double>::infinity()};
    return {(int)j, value};
}

// nearest_two を距離 sqrt(二乗距離) で呼んだときと同じ結果を返す
inline void nearest_two_euclidean(const double* point, const Columns& centers, int start, double start_distance,
                                  int& best, double& best_distance, double& second_distance) {
    using S = Columns::S;
    constexpr int L = S::LANES;
    const double inf = numeric_limits<double>::infinity();
    // 要素ごとに、受け持つ中心のうち距離が最小と 2 番目のものを持つ。second_sq は 2 番目の二乗距離
    auto first = S::set1(inf), second = S::set1(inf), first_sq = S::set1(inf), second_sq = S::set1(inf);
    auto first_index = S::iset1(-1), index = S::iota();
    const auto step = S::iset1(L), start_index = S::iset1(start);
    const auto infinity = S::set1(inf);
    for (int j = 0; j < centers.stride; j += L) {
        auto sq = S::select(S::ieq(index, start_index), infinity, centers.squared(point, j));
        // 二乗距離が 2 番目以上なら、平方根も 2 番目以上なので何も変わらない
        if (S::any(S::lt(sq, second_sq))) {
            const auto x = S::sqrt(sq);
            const auto m1 = S::lt(x, first), m2 = S::lt(x, second);
            second = S::select(m1, first, S::select(m2, x, second));
            second_sq = S::select(m1, first_sq, S::select(m2, sq, second_sq));
            first = S::select(m1, x, first);
            first_sq = S::select(m1, sq, first_sq);
            first_index = S::iselect(m1, index, first_index);
        }
        index = S::iadd(index, step);
    }
    alignas(32) double f[L], s[L];
    alignas(32) typename S::Idx fi[L];
    S::storeu(f, first), S::storeu(s, second), S::istoreu(fi, first_index);
    // start、番号の小さい順の各要素の最小、各要素の 2 番目、の順に逐次の nearest_two と同じ規則で併合する
    int order[L];
    for (int i = 0; i < L; ++i) order[i] = i;
    sort(order, order + L, [&](int a, int b) { return fi[a] < fi[b]; });
    best = start;
    best_distance = start_distance;
    second_distance = inf;
    for (int i : order) {
        if (fi[i] < 0) continue;
        if (f[i] < best_distance) {
            second_distance = best_distance;
            best_distance = f[i];
            best = (int)fi[i];
        } else if (f[i] < second_distance) {
            second_distance = f[i];
        }
    }
    for (int i = 0; i < L; ++i) if (s[i] < second_distance) second_distance = s[i];
}

// nextafter(x, +inf) と同じ値を返す。double はビット列の加減で求める
template <class Distance>
Distance next_up(Distance x) {
    if constexpr (is_same_v<Distance, double>) {
        if (!(x < numeric_limits<double>::infinity())) return x;
        if (x == 0) return numeric_limits<double>::denorm_min();
        const uint64_t bits = bit_cast<uint64_t>(x);
        return bit_cast<double>(x > 0 ? bits + 1 : bits - 1);
    } else {
        return nextafter(x, numeric_limits<Distance>::infinity());
    }
}

// x > 0 について nextafter(x, 0) と同じ値を返す
template <class Distance>
Distance next_down_positive(Distance x) {
    if constexpr (is_same_v<Distance, double>) return bit_cast<double>(bit_cast<uint64_t>(x) - 1);
    else return nextafter(x, Distance(0));
}

// 中心が movement だけ動いた後の上界と下界。丸めを考えて 1 ulp 広げる
template <class Distance>
Distance upper_after_move(Distance value, Distance movement) {
    if (movement == 0) return value;
    return next_up(value + movement);
}

template <class Distance>
Distance lower_after_move(Distance value, Distance movement) {
    if (movement == 0) return value;
    if (value <= movement) return 0;
    return next_down_positive(value - movement);
}

// centers[start] との距離を既知として、最も近い中心と 2 番目の距離を求める。同じ距離なら start、次に番号の小さい中心を選ぶ
template <class Distance, class Point, class Center, class MetricFn>
void nearest_two(const Point& point, const vector<Center>& centers, int start, Distance start_distance,
                 int& best, Distance& best_distance, Distance& second_distance, MetricFn& metric) {
    best = start;
    best_distance = start_distance;
    second_distance = numeric_limits<Distance>::infinity();
    for (int j = 0; j < (int)centers.size(); ++j) {
        if (j == start) continue;
        Distance candidate = metric_value<Distance>(metric(point, centers[j]));
        if (candidate < best_distance) {
            second_distance = best_distance;
            best_distance = candidate;
            best = j;
        } else if (candidate < second_distance) {
            second_distance = candidate;
        }
    }
}

// 距離がユークリッド距離なら SIMD で、そうでなければ nearest_two で、最近傍と 2 番目の中心を探す
template <class Distance, class Point, class Center, class MetricFn>
class NearestTwo {
    static constexpr bool EUCLIDEAN = is_euclidean_metric<MetricFn>::value && is_same_v<Distance, double>;
    const vector<Center>& centers;
    MetricFn& metric;
    Columns columns;
    vector<double> point_buffer;

public:
    NearestTwo(const vector<Center>& centers, MetricFn& metric) : centers(centers), metric(metric) {
        if constexpr (EUCLIDEAN) columns.assign(centers);
    }

    void operator()(const Point& point, int start, Distance start_distance, int& best, Distance& best_distance, Distance& second_distance) {
        if constexpr (EUCLIDEAN) {
            load_point(point, columns.d, point_buffer);
            nearest_two_euclidean(point_buffer.data(), columns, start, start_distance, best, best_distance, second_distance);
        } else {
            nearest_two(point, centers, start, start_distance, best, best_distance, second_distance, metric);
        }
    }
};

template <class Distance, class Point, class Center, class MetricFn>
void assign_all_metric(const vector<Point>& points, const vector<Center>& centers, vector<int>& labels,
                       vector<int>& counts, vector<Distance>& upper, vector<Distance>& lower, MetricFn& metric) {
    int n = points.size(), cluster_count = centers.size();
    counts.assign(cluster_count, 0);
    upper.resize(n);
    lower.resize(n);
    NearestTwo<Distance, Point, Center, MetricFn> nearest(centers, metric);
    for (int i = 0; i < n; ++i) {
        int initial = labels[i] == -1 ? 0 : labels[i];
        int best;
        Distance best_distance, second_distance;
        nearest(points[i], initial, metric_value<Distance>(metric(points[i], centers[initial])), best, best_distance, second_distance);
        labels[i] = best;
        ++counts[best];
        upper[i] = best_distance;
        lower[i] = second_distance;
    }
}

// separation[j] は中心 j から他の中心までの最小の距離の半分を 1 ulp 小さくした値
template <class Distance, class Center, class MetricFn>
void calculate_separation(const vector<Center>& centers, vector<Distance>& separation, MetricFn& metric) {
    int cluster_count = centers.size();
    separation.assign(cluster_count, numeric_limits<Distance>::infinity());
    for (int i = 0; i < cluster_count; ++i) for (int j = i + 1; j < cluster_count; ++j) {
        Distance distance = metric_value<Distance>(metric(centers[i], centers[j]));
        if (distance < separation[i]) separation[i] = distance;
        if (distance < separation[j]) separation[j] = distance;
    }
    for (Distance& value : separation) {
        value *= Distance(0.5);
        if (value > 0 && value < numeric_limits<Distance>::infinity()) value = next_down_positive(value);
    }
}

// 上界が max(下界, separation) 以下の点は、最近傍の探索を省く
template <class Distance, class Point, class Center, class MetricFn>
bool assign_hamerly(const vector<Point>& points, const vector<Center>& centers, vector<int>& labels,
                    vector<int>& counts, vector<Distance>& upper, vector<Distance>& lower,
                    vector<Distance>& separation, MetricFn& metric) {
    int n = points.size();
    calculate_separation<Distance>(centers, separation, metric);
    NearestTwo<Distance, Point, Center, MetricFn> nearest(centers, metric);
    bool changed = false;
    for (int i = 0; i < n; ++i) {
        int assigned = labels[i];
        Distance boundary = max(lower[i], separation[assigned]);
        if (upper[i] <= boundary) continue;
        upper[i] = metric_value<Distance>(metric(points[i], centers[assigned]));
        if (upper[i] <= boundary) continue;
        int best;
        Distance best_distance, second_distance;
        nearest(points[i], assigned, upper[i], best, best_distance, second_distance);
        upper[i] = best_distance;
        lower[i] = second_distance;
        if (best != assigned) {
            labels[i] = best;
            --counts[assigned];
            ++counts[best];
            changed = true;
        }
    }
    return changed;
}

// 中心の移動量で上界と下界を更新する。空のクラスタを修復したときは上界を求め直し、下界を 0 にする
template <class Distance, class Point, class Center, class MetricFn>
void update_hamerly_bounds(const vector<Point>& points, const vector<int>& labels, const vector<Center>& old_centers, const vector<Center>& new_centers, vector<Distance>& upper, vector<Distance>& lower, vector<Distance>& movement, bool repaired, MetricFn& metric) {
    int cluster_count = old_centers.size();
    if (repaired) {
        for (int i = 0; i < (int)points.size(); ++i) {
            upper[i] = metric_value<Distance>(metric(points[i], new_centers[labels[i]]));
            lower[i] = 0;
        }
        return;
    }
    movement.resize(cluster_count);
    for (int j = 0; j < cluster_count; ++j) movement[j] = metric_value<Distance>(metric(old_centers[j], new_centers[j]));
    Distance largest = 0, second_largest = 0;
    int largest_index = -1;
    for (int j = 0; j < cluster_count; ++j) {
        if (largest < movement[j]) {
            second_largest = largest;
            largest = movement[j];
            largest_index = j;
        } else if (second_largest < movement[j]) {
            second_largest = movement[j];
        }
    }
    for (int i = 0; i < (int)points.size(); ++i) {
        int assigned = labels[i];
        upper[i] = upper_after_move(upper[i], movement[assigned]);
        Distance other_move = assigned == largest_index ? second_largest : largest;
        lower[i] = lower_after_move(lower[i], other_move);
    }
}

template <class Point, class Center, class CostFn, class MetricFn, class CenterFn>
auto run_hamerly(const vector<Point>& points, vector<Center> centers, CostFn& cost, MetricFn& metric, CenterFn& center_of, int max_iterations) {
    using Cost = remove_cvref_t<invoke_result_t<CostFn&, const Point&, const Center&>>;
    using PointDistance = remove_cvref_t<invoke_result_t<MetricFn&, const Point&, const Center&>>;
    using CenterDistance = remove_cvref_t<invoke_result_t<MetricFn&, const Center&, const Center&>>;
    using RawDistance = common_type_t<PointDistance, CenterDistance>;
    using Distance = conditional_t<is_integral_v<RawDistance>, long double, common_type_t<RawDistance, double>>;
    check_lloyd_input(points.size(), centers.size(), max_iterations);
    int n = points.size(), cluster_count = centers.size();
    vector<int> labels(n, -1), counts, offsets, members, cursor;
    vector<Distance> upper, lower, separation, movement;
    vector<Cost> assigned_cost;
    vector<Center> new_centers;
    new_centers.reserve(cluster_count);
    int iterations = 0;
    bool converged = false, first = true;
    while (iterations < max_iterations) {
        bool changed = true;
        if (first) assign_all_metric<Distance>(points, centers, labels, counts, upper, lower, metric);
        else {
            changed = assign_hamerly<Distance>(points, centers, labels, counts, upper, lower, separation, metric);
        }
        bool has_empty = find(counts.begin(), counts.end(), 0) != counts.end();
        bool repaired = false;
        if (has_empty) {
            assigned_cost.clear();
            assigned_cost.reserve(n);
            for (int i = 0; i < n; ++i) assigned_cost.emplace_back(cost(points[i], centers[labels[i]]));
            repaired = repair_empty_clusters(labels, counts, assigned_cost);
        }
        if (!first && !changed && !repaired) {
            converged = true;
            break;
        }
        update_centers<Point, Center>(points, labels, counts, offsets, members, cursor, new_centers, center_of);
        update_hamerly_bounds<Distance>(points, labels, centers, new_centers, upper, lower, movement, repaired, metric);
        centers.swap(new_centers);
        ++iterations;
        first = false;
    }
    Cost total_cost = calculate_total_cost(points, labels, centers, cost);
    return KMeansResult<Center, Cost>{move(labels), move(centers), move(counts), move(total_cost), iterations, converged};
}
}

/// @brief 中心 initial_centers から Hamerly 法で k-means を行う
/// 割り当ては metric で決め、cost は total_cost と空のクラスタの修復に使う。metric は三角不等式を満たすこと
template <class Point, class Center, class CostFn, class MetricFn, class CenterFn>
auto kmeans_hamerly_from_centers(const vector<Point>& points, const vector<Center>& initial_centers, CostFn cost, MetricFn metric, CenterFn center_of, int max_iterations = 100) {
    return kmeans_detail::run_hamerly(points, initial_centers, cost, metric, center_of, max_iterations);
}

/// @brief K-means++ で初期化し、Hamerly 法で k-means を行う
template <class Point, class CostFn, class MetricFn, class CenterFn>
auto kmeans_hamerly(const vector<Point>& points, int cluster_count, CostFn cost, MetricFn metric, CenterFn center_of, KMeansOptions options = {}) {
    using Center = remove_cvref_t<invoke_result_t<CenterFn&, const vector<Point>&, span<const int>>>;
    kmeans_detail::check_lloyd_input(points.size(), cluster_count, options.max_iterations);
    vector<Center> centers = kmeans_detail::initialize_kmeans_pp<Point, Center>(points, cluster_count, cost, center_of, options.seed);
    return kmeans_detail::run_hamerly(points, move(centers), cost, metric, center_of, options.max_iterations);
}

/// @brief 種を変えて kmeans_hamerly を trials 回行い、total_cost が最小の結果を返す
template <class Point, class CostFn, class MetricFn, class CenterFn>
auto kmeans_hamerly_best_of(const vector<Point>& points, int cluster_count, int trials, CostFn cost, MetricFn metric, CenterFn center_of, KMeansOptions options = {}) {
    return kmeans_detail::run_best_of("kmeans_hamerly_best_of", trials, options, [&](const KMeansOptions& current) {
        return kmeans_hamerly(points, cluster_count, cost, metric, center_of, current);
    });
}
}
