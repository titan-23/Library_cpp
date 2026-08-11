#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include "titan_cpplib/ahc/kmeans/kmeans.cpp"
using namespace std;
namespace titan23 {
namespace kmeans_detail {
template <class Distance, class Value>
Distance metric_value(Value value) {
    return (Distance)value;
}

template <class Distance>
Distance upper_after_move(Distance value, Distance movement) {
    if (movement == 0) return value;
    Distance result = value + movement;
    return nextafter(result, numeric_limits<Distance>::infinity());
}

template <class Distance>
Distance lower_after_move(Distance value, Distance movement) {
    if (movement == 0) return value;
    if (value <= movement) return 0;
    return nextafter(value - movement, Distance(0));
}

template <class Distance, class Point, class Center, class MetricFn>
void assign_all_metric(const vector<Point>& points, const vector<Center>& centers, const vector<int>& old_labels, vector<int>& new_labels, vector<int>& counts, vector<Distance>& upper, vector<Distance>& lower, MetricFn& metric) {
    int n = points.size(), cluster_count = centers.size();
    new_labels.resize(n);
    counts.assign(cluster_count, 0);
    upper.resize(n);
    lower.resize(n);
    for (int i = 0; i < n; ++i) {
        int initial = old_labels[i] == -1 ? 0 : old_labels[i];
        int best = initial;
        Distance best_distance = metric_value<Distance>(metric(points[i], centers[initial]));
        Distance second_distance = numeric_limits<Distance>::infinity();
        for (int j = 0; j < cluster_count; ++j) {
            if (j == initial) continue;
            Distance candidate = metric_value<Distance>(metric(points[i], centers[j]));
            if (candidate < best_distance) {
                second_distance = best_distance;
                best_distance = candidate;
                best = j;
            } else if (candidate < second_distance) {
                second_distance = candidate;
            }
        }
        new_labels[i] = best;
        ++counts[best];
        upper[i] = best_distance;
        lower[i] = second_distance;
    }
}

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
        if (value > 0 && value < numeric_limits<Distance>::infinity()) value = nextafter(value, Distance(0));
    }
}

template <class Distance, class Point, class Center, class MetricFn>
void assign_hamerly(const vector<Point>& points, const vector<Center>& centers, const vector<int>& old_labels, vector<int>& new_labels, vector<int>& counts, vector<Distance>& upper, vector<Distance>& lower, vector<Distance>& separation, MetricFn& metric) {
    int n = points.size(), cluster_count = centers.size();
    calculate_separation<Distance>(centers, separation, metric);
    new_labels = old_labels;
    for (int i = 0; i < n; ++i) {
        int assigned = old_labels[i];
        Distance boundary = max(lower[i], separation[assigned]);
        if (upper[i] <= boundary) continue;
        upper[i] = metric_value<Distance>(metric(points[i], centers[assigned]));
        if (upper[i] <= boundary) continue;
        int best = assigned;
        Distance best_distance = upper[i];
        Distance second_distance = numeric_limits<Distance>::infinity();
        for (int j = 0; j < cluster_count; ++j) {
            if (j == assigned) continue;
            Distance candidate = metric_value<Distance>(metric(points[i], centers[j]));
            if (candidate < best_distance) {
                second_distance = best_distance;
                best_distance = candidate;
                best = j;
            } else if (candidate < second_distance) {
                second_distance = candidate;
            }
        }
        new_labels[i] = best;
        upper[i] = best_distance;
        lower[i] = second_distance;
    }
    counts.assign(cluster_count, 0);
    for (int label : new_labels) ++counts[label];
}

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
    vector<int> labels(n, -1), new_labels(n), counts, offsets, members, cursor;
    vector<Distance> upper, lower, separation, movement;
    vector<Cost> assigned_cost;
    vector<Center> new_centers;
    new_centers.reserve(cluster_count);
    int iterations = 0;
    bool converged = false, first = true;
    while (iterations < max_iterations) {
        if (first) assign_all_metric<Distance>(points, centers, labels, new_labels, counts, upper, lower, metric);
        else assign_hamerly<Distance>(points, centers, labels, new_labels, counts, upper, lower, separation, metric);
        bool has_empty = find(counts.begin(), counts.end(), 0) != counts.end();
        bool repaired = false;
        if (has_empty) {
            assigned_cost.clear();
            assigned_cost.reserve(n);
            for (int i = 0; i < n; ++i) assigned_cost.emplace_back(cost(points[i], centers[new_labels[i]]));
            repaired = repair_empty_clusters(new_labels, counts, assigned_cost);
        }
        bool changed = new_labels != labels;
        labels.swap(new_labels);
        if (!first && !changed && !repaired) {
            converged = true;
            break;
        }
        build_members(labels, counts, offsets, members, cursor);
        calculate_centers<Point, Center>(points, cluster_count, offsets, members, new_centers, center_of);
        update_hamerly_bounds<Distance>(points, labels, centers, new_centers, upper, lower, movement, repaired, metric);
        centers.swap(new_centers);
        ++iterations;
        first = false;
    }
    Cost total_cost = calculate_total_cost(points, labels, centers, cost);
    return KMeansResult<Center, Cost>{move(labels), move(centers), move(counts), move(total_cost), iterations, converged};
}
}

template <class Point, class Center, class CostFn, class MetricFn, class CenterFn>
auto kmeans_hamerly_from_centers(const vector<Point>& points, const vector<Center>& initial_centers, CostFn cost, MetricFn metric, CenterFn center_of, int max_iterations = 100) {
    return kmeans_detail::run_hamerly(points, initial_centers, cost, metric, center_of, max_iterations);
}

template <class Point, class CostFn, class MetricFn, class CenterFn>
auto kmeans_hamerly(const vector<Point>& points, int cluster_count, CostFn cost, MetricFn metric, CenterFn center_of, KMeansOptions options = {}) {
    using Center = remove_cvref_t<invoke_result_t<CenterFn&, const vector<Point>&, span<const int>>>;
    kmeans_detail::check_lloyd_input(points.size(), cluster_count, options.max_iterations);
    vector<Center> centers = kmeans_detail::initialize_kmeans_pp<Point, Center>(points, cluster_count, cost, center_of, options.seed);
    return kmeans_detail::run_hamerly(points, move(centers), cost, metric, center_of, options.max_iterations);
}

template <class Point, class CostFn, class MetricFn, class CenterFn>
auto kmeans_hamerly_best_of(const vector<Point>& points, int cluster_count, int trials, CostFn cost, MetricFn metric, CenterFn center_of, KMeansOptions options = {}) {
    if (trials <= 0) throw invalid_argument("kmeans_hamerly_best_of: trials must be positive");
    KMeansOptions current = options;
    current.seed = options.seed;
    auto best = kmeans_hamerly(points, cluster_count, cost, metric, center_of, current);
    for (int trial = 1; trial < trials; ++trial) {
        current.seed = options.seed + uint32_t(trial);
        auto result = kmeans_hamerly(points, cluster_count, cost, metric, center_of, current);
        if (result.total_cost < best.total_cost) best = move(result);
    }
    return best;
}
}
