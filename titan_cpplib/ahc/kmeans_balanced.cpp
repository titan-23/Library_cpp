#pragma once
#include <atcoder/mincostflow>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include "titan_cpplib/ahc/kmeans.cpp"
using namespace std;
namespace titan23 {
struct ClusterSizeRange {
    int lower;
    int upper;
};

template <class Center, class Cost>
struct KMeansBalancedResult {
    vector<int> labels;
    vector<Center> centers;
    vector<int> cluster_sizes;
    Cost total_cost;
    long long total_flow_cost;
    int iterations;
    bool converged;
};

namespace kmeans_detail {
inline void check_size_ranges(size_t point_count, size_t cluster_count, const vector<ClusterSizeRange>& ranges, int max_iterations) {
    if (point_count <= 0) throw invalid_argument("kmeans_balanced: points must not be empty");
    if (cluster_count <= 0) throw invalid_argument("kmeans_balanced: at least one cluster is required");
    if (ranges.size() != cluster_count) throw invalid_argument("kmeans_balanced: ranges.size() must equal the number of centers");
    if ((__int128)point_count + cluster_count + 3 > numeric_limits<int>::max()) throw invalid_argument("kmeans_balanced: n+k+3 must fit in int");
    if (max_iterations <= 0) throw invalid_argument("kmeans_balanced: max_iterations must be positive");
    long long lower_sum = 0, upper_sum = 0;
    for (auto [lower, upper] : ranges) {
        if (lower < 0 || lower > upper) throw invalid_argument("kmeans_balanced: each range must satisfy 0 <= lower <= upper");
        lower_sum += lower;
        upper_sum += upper;
    }
    if (lower_sum > (long long)point_count || upper_sum < (long long)point_count) throw invalid_argument("kmeans_balanced: size ranges are infeasible");
}

inline vector<ClusterSizeRange> make_exact_ranges(const vector<int>& sizes) {
    vector<ClusterSizeRange> ranges;
    ranges.reserve(sizes.size());
    for (int size : sizes) ranges.push_back({size, size});
    return ranges;
}

template <class Point, class Center, class FlowCostFn>
long long evaluate_flow_cost(const Point& point, const Center& center, FlowCostFn& flow_cost) {
    using FlowCost = remove_cvref_t<invoke_result_t<FlowCostFn&, const Point&, const Center&>>;
    static_assert(is_integral_v<FlowCost> && is_signed_v<FlowCost> && !is_same_v<FlowCost, bool> && numeric_limits<FlowCost>::digits <= numeric_limits<long long>::digits, "flow_cost must return a signed integer type no wider than long long");
    return (long long)flow_cost(point, center);
}

template <class Point, class Center, class FlowCostFn>
long long calculate_total_flow_cost(const vector<Point>& points, const vector<int>& labels, const vector<Center>& centers, FlowCostFn& flow_cost) {
    __int128 total = 0;
    for (int i = 0; i < (int)points.size(); ++i) {
        long long value = evaluate_flow_cost(points[i], centers[labels[i]], flow_cost);
        if (value < 0) throw domain_error("kmeans_balanced: flow_cost must be nonnegative");
        total += value;
        if (total > numeric_limits<long long>::max()) throw overflow_error("kmeans_balanced: total flow cost does not fit in long long");
    }
    return (long long)total;
}

template <class Point, class Center, class FlowCostFn>
void assign_balanced(const vector<Point>& points, const vector<Center>& centers, const vector<ClusterSizeRange>& ranges, vector<int>& labels, vector<int>& counts, FlowCostFn& flow_cost) {
    int n = points.size(), cluster_count = centers.size();
    int source = n + cluster_count;
    int extra = source + 1;
    int sink = source + 2;
    int node_count = sink + 1;
    constexpr __int128 acl_cost_limit = (__int128)8000000000000000000LL + 1000;
    atcoder::mcf_graph<int, long long> graph(node_count);
    vector<int> assignment_edges((size_t)n * cluster_count);
    for (int i = 0; i < n; ++i) graph.add_edge(source, i, 1, 0);
    for (int i = 0; i < n; ++i) for (int j = 0; j < cluster_count; ++j) {
        long long value = evaluate_flow_cost(points[i], centers[j], flow_cost);
        if (value < 0) throw domain_error("kmeans_balanced: flow_cost must be nonnegative");
        if ((__int128)value * node_count > acl_cost_limit) throw overflow_error("kmeans_balanced: flow cost is too large");
        assignment_edges[(size_t)i * cluster_count + j] = graph.add_edge(i, n + j, 1, value);
    }
    int lower_sum = 0;
    for (int j = 0; j < cluster_count; ++j) {
        lower_sum += ranges[j].lower;
        graph.add_edge(n + j, sink, ranges[j].lower, 0);
        graph.add_edge(n + j, extra, ranges[j].upper - ranges[j].lower, 0);
    }
    graph.add_edge(extra, sink, n - lower_sum, 0);
    auto result = graph.flow(source, sink, n);
    if (result.first != n) throw runtime_error("kmeans_balanced: no feasible assignment");
    labels.assign(n, -1);
    counts.assign(cluster_count, 0);
    for (int i = 0; i < n; ++i) for (int j = 0; j < cluster_count; ++j) {
        if (graph.get_edge(assignment_edges[(size_t)i * cluster_count + j]).flow == 0) continue;
        labels[i] = j;
        ++counts[j];
        break;
    }
    for (int label : labels) if (label == -1) throw logic_error("kmeans_balanced: a point was not assigned");
}

template <class Point, class Center, class CenterFn>
void calculate_balanced_centers(const vector<Point>& points, const vector<Center>& old_centers, const vector<int>& counts, const vector<int>& offsets, const vector<int>& members, vector<Center>& centers, CenterFn& center_of) {
    int cluster_count = old_centers.size();
    centers.clear();
    centers.reserve(cluster_count);
    for (int j = 0; j < cluster_count; ++j) {
        if (counts[j] == 0) centers.emplace_back(old_centers[j]);
        else centers.emplace_back(center_of(points, span<const int>(members.data() + offsets[j], counts[j])));
    }
}

template <class Point, class Center, class CostFn, class FlowCostFn, class CenterFn>
auto run_balanced(const vector<Point>& points, vector<Center> centers, const vector<ClusterSizeRange>& ranges, CostFn& cost, FlowCostFn& flow_cost, CenterFn& center_of, int max_iterations) {
    using Cost = remove_cvref_t<invoke_result_t<CostFn&, const Point&, const Center&>>;
    check_size_ranges(points.size(), centers.size(), ranges, max_iterations);
    int n = points.size(), cluster_count = centers.size();
    vector<int> labels(n, -1), new_labels, counts, offsets, members, cursor;
    vector<int> best_labels, best_counts;
    vector<Center> best_centers;
    vector<Center> new_centers;
    new_centers.reserve(cluster_count);
    Cost best_cost = Cost(0);
    long long best_flow_cost = 0;
    bool has_best = false, best_converged = false;
    int iterations = 0;
    while (iterations < max_iterations) {
        assign_balanced(points, centers, ranges, new_labels, counts, flow_cost);
        if (new_labels == labels) {
            Cost current_cost = calculate_total_cost(points, labels, centers, cost);
            long long current_flow_cost = calculate_total_flow_cost(points, labels, centers, flow_cost);
            if (!has_best || !(best_cost < current_cost)) {
                best_labels = labels;
                best_centers = centers;
                best_counts = counts;
                best_cost = move(current_cost);
                best_flow_cost = current_flow_cost;
                best_converged = true;
                has_best = true;
            }
            break;
        }
        labels.swap(new_labels);
        build_members(labels, counts, offsets, members, cursor);
        calculate_balanced_centers(points, centers, counts, offsets, members, new_centers, center_of);
        centers.swap(new_centers);
        ++iterations;
        Cost current_cost = calculate_total_cost(points, labels, centers, cost);
        if (!has_best || current_cost < best_cost) {
            best_labels = labels;
            best_centers = centers;
            best_counts = counts;
            best_cost = move(current_cost);
            best_flow_cost = calculate_total_flow_cost(points, labels, centers, flow_cost);
            best_converged = false;
            has_best = true;
        }
    }
    if (!has_best) throw logic_error("kmeans_balanced: no result was produced");
    return KMeansBalancedResult<Center, Cost>{move(best_labels), move(best_centers), move(best_counts), move(best_cost), best_flow_cost, iterations, best_converged};
}
}

template <class Point, class Center, class CostFn, class FlowCostFn, class CenterFn>
auto kmeans_balanced_from_centers(const vector<Point>& points, const vector<Center>& initial_centers, const vector<ClusterSizeRange>& ranges, CostFn cost, FlowCostFn flow_cost, CenterFn center_of, int max_iterations = 100) {
    return kmeans_detail::run_balanced(points, initial_centers, ranges, cost, flow_cost, center_of, max_iterations);
}

template <class Point, class CostFn, class FlowCostFn, class CenterFn>
auto kmeans_balanced(const vector<Point>& points, const vector<ClusterSizeRange>& ranges, CostFn cost, FlowCostFn flow_cost, CenterFn center_of, KMeansOptions options = {}) {
    using Center = remove_cvref_t<invoke_result_t<CenterFn&, const vector<Point>&, span<const int>>>;
    kmeans_detail::check_size_ranges(points.size(), ranges.size(), ranges, options.max_iterations);
    int cluster_count = ranges.size();
    vector<Center> centers = kmeans_detail::initialize_kmeans_pp<Point, Center>(points, cluster_count, cost, center_of, options.seed);
    return kmeans_detail::run_balanced(points, move(centers), ranges, cost, flow_cost, center_of, options.max_iterations);
}

template <class Point, class CostFn, class FlowCostFn, class CenterFn>
auto kmeans_balanced_best_of(const vector<Point>& points, const vector<ClusterSizeRange>& ranges, int trials, CostFn cost, FlowCostFn flow_cost, CenterFn center_of, KMeansOptions options = {}) {
    if (trials <= 0) throw invalid_argument("kmeans_balanced_best_of: trials must be positive");
    KMeansOptions current = options;
    current.seed = options.seed;
    auto best = kmeans_balanced(points, ranges, cost, flow_cost, center_of, current);
    for (int trial = 1; trial < trials; ++trial) {
        current.seed = options.seed + uint32_t(trial);
        auto result = kmeans_balanced(points, ranges, cost, flow_cost, center_of, current);
        if (result.total_cost < best.total_cost) best = move(result);
    }
    return best;
}

template <class Point, class Center, class CostFn, class FlowCostFn, class CenterFn>
auto kmeans_balanced_exact_from_centers(const vector<Point>& points, const vector<Center>& initial_centers, const vector<int>& sizes, CostFn cost, FlowCostFn flow_cost, CenterFn center_of, int max_iterations = 100) {
    return kmeans_balanced_from_centers(points, initial_centers, kmeans_detail::make_exact_ranges(sizes), move(cost), move(flow_cost), move(center_of), max_iterations);
}

template <class Point, class CostFn, class FlowCostFn, class CenterFn>
auto kmeans_balanced_exact(const vector<Point>& points, const vector<int>& sizes, CostFn cost, FlowCostFn flow_cost, CenterFn center_of, KMeansOptions options = {}) {
    return kmeans_balanced(points, kmeans_detail::make_exact_ranges(sizes), move(cost), move(flow_cost), move(center_of), options);
}

template <class Point, class CostFn, class FlowCostFn, class CenterFn>
auto kmeans_balanced_exact_best_of(const vector<Point>& points, const vector<int>& sizes, int trials, CostFn cost, FlowCostFn flow_cost, CenterFn center_of, KMeansOptions options = {}) {
    return kmeans_balanced_best_of(points, kmeans_detail::make_exact_ranges(sizes), trials, move(cost), move(flow_cost), move(center_of), options);
}
}
