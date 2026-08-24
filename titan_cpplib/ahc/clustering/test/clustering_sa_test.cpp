/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/clustering/test/clustering_sa_test.cpp
#include <bits/stdc++.h>
#include "titan_cpplib/ahc/clustering/clustering_sa.cpp"
using namespace std;
using namespace titan23;

using Point = array<double, 3>;

double calculate_cost(const vector<Point>& points, const vector<int>& labels, int cluster_count, vector<double>* cluster_costs = nullptr) {
    int dimension = points[0].size();
    vector<int> counts(cluster_count);
    vector<vector<double>> centers(cluster_count, vector<double>(dimension));
    for (int point = 0; point < (int)points.size(); ++point) {
        ++counts[labels[point]];
        for (int axis = 0; axis < dimension; ++axis) centers[labels[point]][axis] += points[point][axis];
    }
    for (int cluster = 0; cluster < cluster_count; ++cluster) {
        assert(counts[cluster] > 0);
        for (double& value : centers[cluster]) value /= counts[cluster];
    }
    vector<double> costs(cluster_count);
    for (int point = 0; point < (int)points.size(); ++point) for (int axis = 0; axis < dimension; ++axis) {
        double difference = points[point][axis] - centers[labels[point]][axis];
        costs[labels[point]] += difference * difference;
    }
    if (cluster_costs) *cluster_costs = costs;
    return accumulate(costs.begin(), costs.end(), 0.0);
}

void verify_state(const vector<Point>& points, const ClusteringSaState& state, const vector<ClusteringSizeRange>& ranges) {
    const ClusteringPartition& partition = state.partition();
    int point_count = points.size();
    int cluster_count = ranges.size();
    vector<int> seen(point_count);
    for (int cluster = 0; cluster < cluster_count; ++cluster) {
        assert(ranges[cluster].lower <= partition.cluster_size(cluster));
        assert(partition.cluster_size(cluster) <= ranges[cluster].upper);
        for (int position = 0; position < partition.cluster_size(cluster); ++position) {
            int point = partition.members(cluster)[position];
            assert(0 <= point && point < point_count);
            assert(partition.label(point) == cluster);
            assert(partition.position_in_cluster(point) == position);
            ++seen[point];
        }
    }
    for (int count : seen) assert(count == 1);
    vector<double> expected_cluster_costs;
    double expected = calculate_cost(points, partition.labels(), cluster_count, &expected_cluster_costs);
    double tolerance = 1e-8 * max(1.0, abs(expected));
    assert(abs(state.total_cost() - expected) <= tolerance);
    for (int cluster = 0; cluster < cluster_count; ++cluster) assert(abs(state.cluster_costs()[cluster] - expected_cluster_costs[cluster]) <= tolerance);
}

ClusteringNeighborhoodWeights only_move(int type) {
    ClusteringNeighborhoodWeights weights{0, 0, 0, 0};
    if (type == clustering_relocate) weights.relocate = 1;
    else if (type == clustering_swap) weights.swap = 1;
    else if (type == clustering_cycle) weights.cycle = 1;
    else weights.rebuild = 1;
    return weights;
}

int exercise_move(const vector<Point>& points, const vector<int>& initial_labels, const vector<ClusteringSizeRange>& ranges, int type, int iterations, double rebuild_three_probability = 0, ClusteringSaStatistics* statistics = nullptr, int rebuild_three_same_state_threshold = 0) {
    ClusteringSaProblem problem(points);
    ClusteringSaOptions options;
    options.nearby_cluster_count = 3;
    options.cluster_samples = 8;
    options.point_samples = 8;
    options.swap_partner_samples = 12;
    options.cycle_partner_samples = 12;
    options.candidate_refresh_interval = 7;
    options.rebuild_point_limit = 16;
    options.rebuild_three_point_limit = 15;
    options.rebuild_iterations = 4;
    options.rebuild_three_probability = rebuild_three_probability;
    options.rebuild_three_same_state_threshold = rebuild_three_same_state_threshold;
    options.uniform_selection_probability = 0.15;
    options.early_weights = only_move(type);
    options.middle_weights = only_move(type);
    options.late_weights = only_move(type);
    ClusteringSaState state(problem, initial_labels, ranges, options, 1234567 + type);
    verify_state(points, state, ranges);
    int accepted = 0;
    for (int iteration = 0; iteration < iterations; ++iteration) {
        double old_score = state.get_score();
        double old_cost = state.total_cost();
        vector<int> old_labels;
        if (iteration % 11 == 0) old_labels = state.partition().labels();
        state.reset_is_valid();
        state.modify(iteration, numeric_limits<double>::infinity(), (double)iteration / iterations);
        if (state.is_valid) {
            if (iteration % 11 == 0) {
                state.rollback();
                state.score = old_score;
                assert(state.partition().labels() == old_labels);
                assert(state.total_cost() == old_cost);
            } else {
                state.advance();
                ++accepted;
                verify_state(points, state, ranges);
            }
        } else {
            state.rollback();
            state.score = old_score;
        }
    }
    if (statistics) *statistics = state.get_result().statistics;
    return accepted;
}

int main() {
    vector<Point> points;
    for (int group = 0; group < 4; ++group) for (int index = 0; index < 8; ++index) {
        double x = (group & 1) * 20 + index * 0.31;
        double y = (group >> 1) * 20 + (index % 3) * 0.47;
        double z = group * 2 + index * 0.13;
        points.push_back({x, y, z});
    }
    vector<int> labels(points.size());
    for (int point = 0; point < (int)points.size(); ++point) labels[point] = point % 4;
    vector<ClusteringSizeRange> free_ranges(4, {2, 14});
    vector<ClusteringSizeRange> exact_ranges = make_exact_clustering_size_ranges({8, 8, 8, 8});
    assert(exercise_move(points, labels, free_ranges, clustering_relocate, 500) > 0);
    assert(exercise_move(points, labels, exact_ranges, clustering_swap, 500) > 0);
    assert(exercise_move(points, labels, exact_ranges, clustering_cycle, 500) > 0);
    assert(exercise_move(points, labels, exact_ranges, clustering_rebuild, 200) > 0);
    ClusteringSaStatistics rebuild_three_statistics;
    assert(exercise_move(points, labels, exact_ranges, clustering_rebuild, 200, 1, &rebuild_three_statistics) > 0);
    assert(rebuild_three_statistics.rebuild_three_attempts > 0);
    assert(rebuild_three_statistics.rebuild_three_valid_proposals > 0);
    assert(rebuild_three_statistics.rebuild_three_accepted > 0);
    assert(rebuild_three_statistics.rebuild_three_accepted <= rebuild_three_statistics.rebuild_three_valid_proposals);
    ClusteringSaStatistics stagnation_statistics;
    assert(exercise_move(points, labels, exact_ranges, clustering_rebuild, 400, 0, &stagnation_statistics, 1) > 0);
    assert(stagnation_statistics.rebuild_three_attempts > 0);

    vector<pair<double, double>> pair_points = {{0, 0}, {1, 0}, {10, 0}, {11, 0}};
    ClusteringSaProblem pair_problem(pair_points, 2, [](const auto& point, int axis) { return axis == 0 ? point.first : point.second; });
    vector<int> pair_labels = {0, 0, 1, 1};
    auto result = clustering_sa_from_labels(pair_problem, pair_labels, make_exact_clustering_size_ranges({2, 2}), 0, {}, 23, false);
    assert(result.labels == pair_labels);
    assert(result.cluster_sizes == vector<int>({2, 2}));
    assert(abs(result.total_cost - 1) < 1e-12);
    assert(abs(clustering_cost_from_labels(pair_problem, pair_labels, make_exact_clustering_size_ranges({2, 2})) - 1) < 1e-12);
    int factory_calls = 0;
    auto factory_result = clustering_sa_from_label_factory(
        pair_problem,
        make_exact_clustering_size_ranges({2, 2}),
        0,
        0.5,
        [&](uint32_t) {
            ++factory_calls;
            return pair_labels;
        },
        {},
        23,
        false
    );
    assert(factory_calls == 1);
    assert(factory_result.initial_trials == 1);
    assert(factory_result.labels == pair_labels);
    assert(abs(factory_result.total_cost - 1) < 1e-12);

    ClusteringSaProblem problem(points);
    double initial_cost = calculate_cost(points, labels, 4);
    auto sa_result = clustering_sa_from_labels(problem, labels, exact_ranges, 5, {}, 987654321, false);
    assert(sa_result.total_cost <= initial_cost + 1e-8 * max(1.0, initial_cost));
    assert(sa_result.cluster_sizes == vector<int>({8, 8, 8, 8}));
    assert(abs(sa_result.score * sa_result.score_scale - sa_result.total_cost) <= 1e-8 * max(1.0, sa_result.total_cost));

    vector<array<double, 1>> one_cluster_points = {{{3}}, {{3}}, {{3}}};
    ClusteringSaProblem one_cluster_problem(one_cluster_points);
    auto one_cluster_result = clustering_sa_from_labels(one_cluster_problem, {0, 0, 0}, make_clustering_size_ranges(1, 3), 5, {}, 1, false);
    assert(one_cluster_result.total_cost == 0);
    assert(one_cluster_result.labels == vector<int>({0, 0, 0}));

    bool rejected_invalid_size = false;
    try {
        ClusteringPartition invalid_partition(-1, 1, {});
    } catch (const invalid_argument&) {
        rejected_invalid_size = true;
    }
    assert(rejected_invalid_size);

    bool rejected_invalid_rebuild_three_probability = false;
    try {
        ClusteringSaOptions options;
        options.rebuild_three_probability = -0.1;
        ClusteringSaState invalid_state(problem, labels, exact_ranges, options, 1);
    } catch (const invalid_argument&) {
        rejected_invalid_rebuild_three_probability = true;
    }
    assert(rejected_invalid_rebuild_three_probability);

    bool rejected_invalid_rebuild_three_threshold = false;
    try {
        ClusteringSaOptions options;
        options.rebuild_three_same_state_threshold = -2;
        ClusteringSaState invalid_state(problem, labels, exact_ranges, options, 1);
    } catch (const invalid_argument&) {
        rejected_invalid_rebuild_three_threshold = true;
    }
    assert(rejected_invalid_rebuild_three_threshold);

    bool rejected_invalid_early_cluster_samples = false;
    try {
        ClusteringSaOptions options;
        options.early_cluster_samples = -2;
        ClusteringSaState invalid_state(problem, labels, exact_ranges, options, 1);
    } catch (const invalid_argument&) {
        rejected_invalid_early_cluster_samples = true;
    }
    assert(rejected_invalid_early_cluster_samples);

    bool rejected_invalid_early_point_samples = false;
    try {
        ClusteringSaOptions options;
        options.early_point_samples = -1;
        ClusteringSaState invalid_state(problem, labels, exact_ranges, options, 1);
    } catch (const invalid_argument&) {
        rejected_invalid_early_point_samples = true;
    }
    assert(rejected_invalid_early_point_samples);

    bool rejected_invalid_initialization_ratio = false;
    try {
        auto invalid_result = clustering_sa_from_label_factory(
            pair_problem,
            make_exact_clustering_size_ranges({2, 2}),
            0,
            1.1,
            [&](uint32_t) { return pair_labels; }
        );
    } catch (const invalid_argument&) {
        rejected_invalid_initialization_ratio = true;
    }
    assert(rejected_invalid_initialization_ratio);
    cout << "ok\n";
}
