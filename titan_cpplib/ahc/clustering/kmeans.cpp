#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include "titan_cpplib/alg/random.cpp"
using namespace std;
namespace titan23 {
struct KMeansOptions {
    int max_iterations = 100;
    uint32_t seed = 0;
};

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
        build_members(labels, counts, offsets, members, cursor);
        calculate_centers<Point, Center>(points, cluster_count, offsets, members, new_centers, center_of);
        centers.swap(new_centers);
        ++iterations;
    }
    Cost total_cost = calculate_total_cost(points, labels, centers, cost);
    return KMeansResult<Center, Cost>{move(labels), move(centers), move(counts), move(total_cost), iterations, converged};
}

template <class Point, class Center, class CostFn, class CenterFn>
vector<Center> initialize_kmeans_pp(const vector<Point>& points, int cluster_count, CostFn& cost, CenterFn& center_of, uint32_t seed) {
    using RawCost = remove_cvref_t<invoke_result_t<CostFn&, const Point&, const Center&>>;
    using Weight = conditional_t<is_integral_v<RawCost>, long double, common_type_t<RawCost, double>>;
    check_lloyd_input(points.size(), cluster_count, 1);
    int n = points.size();
    Random random(seed);
    vector<unsigned char> selected(n, false);
    vector<Weight> minimum_cost(n, numeric_limits<Weight>::infinity());
    vector<Center> centers;
    centers.reserve(cluster_count);
    int chosen = random.randrange(n);
    selected[chosen] = true;
    centers.emplace_back(center_of(points, span<const int>(&chosen, 1)));
    while ((int)centers.size() < cluster_count) {
        const Center& newest = centers.back();
        Weight maximum = 0, total = 0;
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
            for (int i = 0; i < n; ++i) {
                if (selected[i]) continue;
                sum += minimum_cost[i] / maximum;
                if (target < sum) {
                    chosen = i;
                    break;
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
}

template <class Point, class Center, class CostFn, class CenterFn>
auto kmeans_from_centers(const vector<Point>& points, const vector<Center>& initial_centers, CostFn cost, CenterFn center_of, int max_iterations = 100) {
    return kmeans_detail::run_lloyd(points, initial_centers, cost, center_of, max_iterations);
}

template <class Point, class CostFn, class CenterFn>
auto kmeans(const vector<Point>& points, int cluster_count, CostFn cost, CenterFn center_of, KMeansOptions options = {}) {
    using Center = remove_cvref_t<invoke_result_t<CenterFn&, const vector<Point>&, span<const int>>>;
    kmeans_detail::check_lloyd_input(points.size(), cluster_count, options.max_iterations);
    vector<Center> centers = kmeans_detail::initialize_kmeans_pp<Point, Center>(points, cluster_count, cost, center_of, options.seed);
    return kmeans_detail::run_lloyd(points, move(centers), cost, center_of, options.max_iterations);
}

template <class Point, class CostFn, class CenterFn>
auto kmeans_best_of(const vector<Point>& points, int cluster_count, int trials, CostFn cost, CenterFn center_of, KMeansOptions options = {}) {
    if (trials <= 0) throw invalid_argument("kmeans_best_of: trials must be positive");
    KMeansOptions current = options;
    current.seed = options.seed;
    auto best = kmeans(points, cluster_count, cost, center_of, current);
    for (int trial = 1; trial < trials; ++trial) {
        current.seed = options.seed + uint32_t(trial);
        auto result = kmeans(points, cluster_count, cost, center_of, current);
        if (result.total_cost < best.total_cost) best = move(result);
    }
    return best;
}
}
