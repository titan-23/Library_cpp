/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/clustering/kmeans.cpp
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "titan_cpplib/alg/random.cpp"
#include "titan_cpplib/others/simd_int.cpp"
using namespace std;
namespace titan23 {
/// @brief kmeans 系の関数の設定 / max_iterations は反復の上限、seed は K-means++ の乱数の種
struct KMeansOptions {
    int max_iterations = 100;
    uint32_t seed = 0;
};

/// @brief kmeans 系の関数の結果
template <class Center, class Cost>
struct KMeansResult {
    vector<int> labels;
    vector<Center> centers;
    vector<int> cluster_sizes;
    Cost total_cost;
    int iterations;
    bool converged;
};

namespace kmeans_detail {
inline void check_lloyd_input(size_t point_count, size_t cluster_count, int max_iterations) {
    if (point_count <= 0) throw invalid_argument("kmeans: points must not be empty");
    if (cluster_count <= 0 || cluster_count > point_count) throw invalid_argument("kmeans: cluster_count must be in [1, points.size()]");
    if (point_count > (size_t)numeric_limits<int>::max()) throw invalid_argument("kmeans: points.size() must fit in int");
    if (max_iterations <= 0) throw invalid_argument("kmeans: max_iterations must be positive");
}

inline double random_unit(Random& random) {
    return (double)(random.rand_u64() >> 11) / 9007199254740992.0;
}

// 費用の関数が euclidean_squared = true を持つとき、二乗ユークリッド距離を SIMD で求める
template <class CostFn, class = void>
struct is_euclidean_squared : false_type {};
template <class CostFn>
struct is_euclidean_squared<CostFn, void_t<decltype(CostFn::euclidean_squared)>> : bool_constant<CostFn::euclidean_squared> {};

// 点や中心を軸ごとに並べた表。末尾の詰め物の座標は +inf で、距離も +inf になる
class Columns {
public:
    using S = SimdVec<double>;
    int k = 0, d = 0, stride = 0;
    vector<double> data;

    template <class Item>
    void assign(const vector<Item>& items) {
        k = items.size();
        d = items[0].size();
        stride = (k + S::LANES - 1) / S::LANES * S::LANES;
        data.assign((size_t)d * stride, numeric_limits<double>::infinity());
        for (int j = 0; j < k; ++j) for (int a = 0; a < d; ++a) data[(size_t)a * stride + j] = double(items[j][a]);
    }

    // 要素 j, ..., j + LANES - 1 との二乗距離。軸の順に足し、逐次の計算と同じ値になる
    typename S::Vec squared(const double* point, int j) const {
        auto acc = S::set1(0);
        for (int a = 0; a < d; ++a) {
            const auto diff = S::sub(S::set1(point[a]), S::loadu(data.data() + (size_t)a * stride + j));
            acc = S::add(acc, S::mul_rounded(diff, diff));
        }
        return acc;
    }
};

template <class Point>
void load_point(const Point& point, int d, vector<double>& out) {
    out.resize(d);
    for (int a = 0; a < d; ++a) out[a] = double(point[a]);
}

// クラスタ j の点の番号を members[offsets[j] .. offsets[j + 1]) に番号の昇順で並べる
inline void build_members(const vector<int>& labels, const vector<int>& counts, vector<int>& offsets, vector<int>& members, vector<int>& cursor) {
    int n = labels.size(), cluster_count = counts.size();
    offsets.resize(cluster_count + 1);
    offsets[0] = 0;
    for (int j = 0; j < cluster_count; ++j) offsets[j + 1] = offsets[j] + counts[j];
    cursor.resize(cluster_count);
    copy(offsets.begin(), offsets.end() - 1, cursor.begin());
    members.resize(n);
    for (int i = 0; i < n; ++i) members[cursor[labels[i]]++] = i;
}

// 空のクラスタに、2 点以上のクラスタの点のうち費用が最大の点を移す
template <class Cost>
bool repair_empty_clusters(vector<int>& labels, vector<int>& counts, const vector<Cost>& assigned_cost) {
    bool repaired = false;
    int cluster_count = counts.size();
    for (int empty = 0; empty < cluster_count; ++empty) {
        if (counts[empty] != 0) continue;
        int selected = -1;
        for (int i = 0; i < (int)labels.size(); ++i) {
            if (counts[labels[i]] < 2) continue;
            if (selected == -1 || assigned_cost[selected] < assigned_cost[i]) selected = i;
        }
        if (selected == -1) throw logic_error("kmeans: empty cluster cannot be repaired");
        int old = labels[selected];
        labels[selected] = empty;
        --counts[old];
        ++counts[empty];
        repaired = true;
    }
    return repaired;
}

template <class Point, class Center, class CostFn>
auto calculate_total_cost(const vector<Point>& points, const vector<int>& labels, const vector<Center>& centers, CostFn& cost) {
    using Cost = remove_cvref_t<invoke_result_t<CostFn&, const Point&, const Center&>>;
    Cost total = Cost(0);
    for (int i = 0; i < (int)points.size(); ++i) total += cost(points[i], centers[labels[i]]);
    return total;
}

template <class Point, class Center, class CenterFn>
void calculate_centers(const vector<Point>& points, int cluster_count, const vector<int>& offsets, const vector<int>& members, vector<Center>& centers, CenterFn& center_of) {
    centers.clear();
    centers.reserve(cluster_count);
    for (int j = 0; j < cluster_count; ++j) {
        span<const int> cluster_members(members.data() + offsets[j], offsets[j + 1] - offsets[j]);
        centers.emplace_back(center_of(points, cluster_members));
    }
}

// 中心の関数が mean_center = true を持つとき、点の番号の順に 1 回だけ読んで平均を求める
template <class CenterFn, class = void>
struct is_mean_center : false_type {};
template <class CenterFn>
struct is_mean_center<CenterFn, void_t<decltype(CenterFn::mean_center)>> : bool_constant<CenterFn::mean_center> {};

// 各クラスタの和を点の番号の昇順に足すので、members を番号順に足す平均と同じ値になる
template <class Point>
void calculate_mean_centers(const vector<Point>& points, const vector<int>& labels, const vector<int>& counts, vector<vector<double>>& centers) {
    const size_t d = points[0].size();
    centers.resize(counts.size());
    for (auto& center : centers) center.assign(d, 0.0);
    for (size_t i = 0; i < points.size(); ++i) {
        double* center = centers[labels[i]].data();
        for (size_t axis = 0; axis < d; ++axis) center[axis] += double(points[i][axis]);
    }
    for (size_t j = 0; j < centers.size(); ++j) {
        for (double& value : centers[j]) value /= (size_t)counts[j];
    }
}

// 所属から中心を作り直す
template <class Point, class Center, class CenterFn>
void update_centers(const vector<Point>& points, const vector<int>& labels, const vector<int>& counts, vector<int>& offsets, vector<int>& members, vector<int>& cursor, vector<Center>& centers, CenterFn& center_of) {
    if constexpr (is_mean_center<CenterFn>::value && is_same_v<Center, vector<double>>) {
        calculate_mean_centers(points, labels, counts, centers);
    } else {
        build_members(labels, counts, offsets, members, cursor);
        calculate_centers<Point, Center>(points, counts.size(), offsets, members, centers, center_of);
    }
}

// 各点を費用が最小の中心に割り当てる。同じ費用なら前のラベル、次に番号の小さい中心を選ぶ
template <class Point, class Center, class CostFn>
void assign_nearest(const vector<Point>& points, const vector<Center>& centers, const vector<int>& old_labels, vector<int>& new_labels, vector<int>& counts, vector<remove_cvref_t<invoke_result_t<CostFn&, const Point&, const Center&>>>& assigned_cost, CostFn& cost) {
    using Cost = remove_cvref_t<invoke_result_t<CostFn&, const Point&, const Center&>>;
    int n = points.size(), cluster_count = centers.size();
    new_labels.resize(n);
    counts.assign(cluster_count, 0);
    assigned_cost.clear();
    assigned_cost.reserve(n);
    for (int i = 0; i < n; ++i) {
        int initial = old_labels[i] == -1 ? 0 : old_labels[i];
        int best = initial;
        Cost best_cost = cost(points[i], centers[best]);
        for (int j = 0; j < cluster_count; ++j) {
            if (j == initial) continue;
            Cost candidate = cost(points[i], centers[j]);
            if (candidate < best_cost) {
                best = j;
                best_cost = move(candidate);
            }
        }
        new_labels[i] = best;
        ++counts[best];
        assigned_cost.emplace_back(move(best_cost));
    }
}

template <class Point, class Center, class CostFn, class CenterFn>
auto run_lloyd(const vector<Point>& points, vector<Center> centers, CostFn& cost, CenterFn& center_of, int max_iterations) {
    using Cost = remove_cvref_t<invoke_result_t<CostFn&, const Point&, const Center&>>;
    check_lloyd_input(points.size(), centers.size(), max_iterations);
    int n = points.size(), cluster_count = centers.size();
    vector<int> labels(n, -1), new_labels(n), counts, offsets, members, cursor;
    vector<Cost> assigned_cost;
    vector<Center> new_centers;
    new_centers.reserve(cluster_count);
    int iterations = 0;
    bool converged = false;
    while (iterations < max_iterations) {
        assign_nearest(points, centers, labels, new_labels, counts, assigned_cost, cost);
        bool repaired = repair_empty_clusters(new_labels, counts, assigned_cost);
        bool changed = new_labels != labels;
        labels.swap(new_labels);
        if (!changed && !repaired) {
            converged = true;
            break;
        }
        update_centers<Point, Center>(points, labels, counts, offsets, members, cursor, new_centers, center_of);
        centers.swap(new_centers);
        ++iterations;
    }
    Cost total_cost = calculate_total_cost(points, labels, centers, cost);
    return KMeansResult<Center, Cost>{move(labels), move(centers), move(counts), move(total_cost), iterations, converged};
}

// K-means++ で初期の中心を選ぶ
template <class Point, class Center, class CostFn, class CenterFn>
vector<Center> initialize_kmeans_pp(const vector<Point>& points, int cluster_count, CostFn& cost, CenterFn& center_of, uint32_t seed) {
    using RawCost = remove_cvref_t<invoke_result_t<CostFn&, const Point&, const Center&>>;
    using Weight = conditional_t<is_integral_v<RawCost>, long double, common_type_t<RawCost, double>>;
    check_lloyd_input(points.size(), cluster_count, 1);
    // 二乗ユークリッド距離なら、距離と商を SIMD でまとめて求める。和は逐次と同じ順に足す
    constexpr bool EUCLIDEAN = is_euclidean_squared<CostFn>::value && is_same_v<Center, vector<double>> && is_same_v<Weight, double>;
    using S = Columns::S;
    constexpr int BLOCK = 256;
    int n = points.size();
    Random random(seed);
    vector<unsigned char> selected(n, false);
    vector<Weight> minimum_cost(n, numeric_limits<Weight>::infinity()), quotient;
    Columns columns;
    if constexpr (EUCLIDEAN) {
        columns.assign(points);
        minimum_cost.resize(columns.stride, numeric_limits<Weight>::infinity());
        quotient.resize(columns.stride);
    }
    // quotient[i] = minimum_cost[i] / by (from <= i < to)
    auto divide = [&](int from, int to, Weight by) {
        if constexpr (EUCLIDEAN) {
            int i = from;
            for (; i + S::LANES <= to; i += S::LANES) S::storeu(&quotient[i], S::div(S::loadu(&minimum_cost[i]), S::set1(by)));
            for (; i < to; ++i) quotient[i] = minimum_cost[i] / by;
        }
    };
    vector<Center> centers;
    centers.reserve(cluster_count);
    int chosen = random.randrange(n);
    selected[chosen] = true;
    centers.emplace_back(center_of(points, span<const int>(&chosen, 1)));
    while ((int)centers.size() < cluster_count) {
        const Center& newest = centers.back();
        // total は未選択の点の minimum_cost / maximum の和。maximum が増えたら比で縮め直す
        Weight maximum = 0, total = 0;
        if constexpr (EUCLIDEAN) {
            // 選んだ点の値も更新するが、以後は読まない
            for (int j = 0; j < columns.stride; j += S::LANES) {
                const auto value = columns.squared(newest.data(), j);
                const auto current = S::loadu(&minimum_cost[j]);
                S::storeu(&minimum_cost[j], S::select(S::lt(value, current), value, current));
            }
            for (int begin = 0; begin < n; begin += BLOCK) {
                const int end = min(n, begin + BLOCK);
                if (maximum > 0) divide(begin, end, maximum);
                for (int i = begin; i < end; ++i) {
                    if (selected[i]) continue;
                    if (maximum < minimum_cost[i]) {
                        total = maximum == 0 ? 1 : total * (maximum / minimum_cost[i]) + 1;
                        maximum = minimum_cost[i];
                        divide(i + 1, end, maximum);
                    } else if (maximum > 0) {
                        total += quotient[i];
                    }
                }
            }
        } else {
            for (int i = 0; i < n; ++i) {
                if (selected[i]) continue;
                Weight value = (Weight)cost(points[i], newest);
                if (value < minimum_cost[i]) minimum_cost[i] = value;
                if (maximum < minimum_cost[i]) {
                    total = maximum == 0 ? 1 : total * (maximum / minimum_cost[i]) + 1;
                    maximum = minimum_cost[i];
                } else if (maximum > 0) {
                    total += minimum_cost[i] / maximum;
                }
            }
        }
        if (maximum == 0) {
            int rank = random.randrange(n - (int)centers.size());
            chosen = -1;
            for (int i = 0; i < n; ++i) {
                if (selected[i]) continue;
                if (rank-- == 0) {
                    chosen = i;
                    break;
                }
            }
        } else {
            Weight target = (Weight)random_unit(random) * total;
            Weight sum = 0;
            chosen = -1;
            if constexpr (EUCLIDEAN) {
                for (int begin = 0; begin < n && chosen == -1; begin += BLOCK) {
                    const int end = min(n, begin + BLOCK);
                    divide(begin, end, maximum);
                    for (int i = begin; i < end; ++i) {
                        if (selected[i]) continue;
                        sum += quotient[i];
                        if (target < sum) {
                            chosen = i;
                            break;
                        }
                    }
                }
            } else {
                for (int i = 0; i < n; ++i) {
                    if (selected[i]) continue;
                    sum += minimum_cost[i] / maximum;
                    if (target < sum) {
                        chosen = i;
                        break;
                    }
                }
            }
            if (chosen == -1) for (int i = n - 1; i >= 0; --i) if (!selected[i] && minimum_cost[i] > 0) {
                chosen = i;
                break;
            }
        }
        if (chosen == -1) throw logic_error("kmeans: initial center selection failed");
        selected[chosen] = true;
        centers.emplace_back(center_of(points, span<const int>(&chosen, 1)));
    }
    return centers;
}

// 種を options.seed, options.seed + 1, ... として run を trials 回呼び、total_cost が最小の最初の結果を返す
template <class Options, class RunFn>
auto run_best_of(const char* name, int trials, const Options& options, RunFn run) {
    if (trials <= 0) throw invalid_argument(string(name) + ": trials must be positive");
    auto best = run(options);
    for (int trial = 1; trial < trials; ++trial) {
        Options current = options;
        current.seed = options.seed + uint32_t(trial);
        auto result = run(current);
        if (result.total_cost < best.total_cost) best = move(result);
    }
    return best;
}
}

/// @brief 中心 initial_centers から Lloyd 法で k-means を行う / 1 反復で cost を nk 回呼ぶ
/// cost(point, center) は割り当ての費用、center_of(points, members) は members の中心を返す
template <class Point, class Center, class CostFn, class CenterFn>
auto kmeans_from_centers(const vector<Point>& points, const vector<Center>& initial_centers, CostFn cost, CenterFn center_of, int max_iterations = 100) {
    return kmeans_detail::run_lloyd(points, initial_centers, cost, center_of, max_iterations);
}

/// @brief K-means++ で初期化し、Lloyd 法で k-means を行う / 1 反復で cost を nk 回呼ぶ
template <class Point, class CostFn, class CenterFn>
auto kmeans(const vector<Point>& points, int cluster_count, CostFn cost, CenterFn center_of, KMeansOptions options = {}) {
    using Center = remove_cvref_t<invoke_result_t<CenterFn&, const vector<Point>&, span<const int>>>;
    kmeans_detail::check_lloyd_input(points.size(), cluster_count, options.max_iterations);
    vector<Center> centers = kmeans_detail::initialize_kmeans_pp<Point, Center>(points, cluster_count, cost, center_of, options.seed);
    return kmeans_detail::run_lloyd(points, move(centers), cost, center_of, options.max_iterations);
}

/// @brief 種を変えて kmeans を trials 回行い、total_cost が最小の結果を返す
template <class Point, class CostFn, class CenterFn>
auto kmeans_best_of(const vector<Point>& points, int cluster_count, int trials, CostFn cost, CenterFn center_of, KMeansOptions options = {}) {
    return kmeans_detail::run_best_of("kmeans_best_of", trials, options, [&](const KMeansOptions& current) {
        return kmeans(points, cluster_count, cost, center_of, current);
    });
}
}
