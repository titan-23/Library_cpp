#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "titan_cpplib/ahc/clustering/benchmark/clustering_benchmark.cpp"
#include "titan_cpplib/ahc/clustering/clustering_sa.cpp"
#include "titan_cpplib/ahc/clustering/kmeans_balanced.cpp"
#include "titan_cpplib/ahc/clustering/kmeans_hamerly.cpp"
using namespace std;
using namespace titan23;
namespace {
using Point = vector<double>;
using Clock = chrono::steady_clock;
constexpr long double acl_cost_limit = (long double)8000000000000000000.0L + 1000;

struct BenchmarkOptions {
    string output_path;
    string experiment_tag;
    vector<string> case_paths;
    set<string> methods;
    int runs = 30;
    uint32_t seed = 23;
    double time_limit_ms = 1000;
    int max_iterations = 100;
    long long balanced_edge_limit = 50000;
};

struct Solution {
    vector<int> labels;
    double reported_cost = numeric_limits<double>::quiet_NaN();
    int iterations = 0;
    int64_t trials = 1;
    bool converged = false;
    double flow_cost_scale = numeric_limits<double>::quiet_NaN();
    ClusteringSaStatistics statistics;
    double initial_cost = numeric_limits<double>::quiet_NaN();
};

struct BenchmarkRow {
    string case_path;
    string case_name;
    string source;
    string experiment_tag;
    string compiler;
    string method;
    string limit_kind;
    string status = "ok";
    string message;
    int run = 0;
    uint32_t seed = 0;
    int point_count = 0;
    int dimension = 0;
    int cluster_count = 0;
    int max_iterations = 0;
    long long balanced_edge_limit = 0;
    string size_condition;
    double time_limit_ms = numeric_limits<double>::quiet_NaN();
    double elapsed_ms = numeric_limits<double>::quiet_NaN();
    double initialization_ms = numeric_limits<double>::quiet_NaN();
    double search_ms = numeric_limits<double>::quiet_NaN();
    double reported_cost = numeric_limits<double>::quiet_NaN();
    double recalculated_cost = numeric_limits<double>::quiet_NaN();
    double initial_cost = numeric_limits<double>::quiet_NaN();
    double improvement_from_initial_percent = numeric_limits<double>::quiet_NaN();
    double cost_per_point = numeric_limits<double>::quiet_NaN();
    double best_known_cost = numeric_limits<double>::quiet_NaN();
    double difference_from_best_percent = numeric_limits<double>::quiet_NaN();
    double adjusted_rand_index = numeric_limits<double>::quiet_NaN();
    double flow_cost_scale = numeric_limits<double>::quiet_NaN();
    int valid = 0;
    int iterations = 0;
    int64_t trials = 0;
    int converged = 0;
    uint64_t label_hash = 0;
    ClusteringSaStatistics statistics;
};

double elapsed_ms(Clock::time_point start) {
    return chrono::duration<double, milli>(Clock::now() - start).count();
}

double squared_distance(const Point& a, const Point& b) {
    double result = 0;
    for (int axis = 0; axis < (int)a.size(); ++axis) {
        double difference = a[axis] - b[axis];
        result += difference * difference;
    }
    return result;
}

struct SquaredDistance {
    double operator()(const Point& a, const Point& b) const {
        return squared_distance(a, b);
    }
};

struct EuclideanDistance {
    double operator()(const Point& a, const Point& b) const {
        return sqrt(squared_distance(a, b));
    }
};

struct MeanCenter {
    Point operator()(const vector<Point>& points, span<const int> members) const {
        Point center(points[0].size());
        for (int point : members) for (int axis = 0; axis < (int)center.size(); ++axis) center[axis] += points[point][axis];
        for (double& value : center) value /= members.size();
        return center;
    }
};

string size_condition(const ClusteringBenchmarkCase& problem) {
    if (!clustering_benchmark_has_size_constraints(problem)) return "free";
    if (clustering_benchmark_has_exact_sizes(problem)) return "exact";
    return "range";
}

vector<ClusterSizeRange> balanced_ranges(const ClusteringBenchmarkCase& problem) {
    vector<ClusterSizeRange> ranges;
    ranges.reserve(problem.ranges.size());
    for (auto [lower, upper] : problem.ranges) ranges.push_back({lower, upper});
    return ranges;
}

vector<ClusteringSizeRange> sa_ranges(const ClusteringBenchmarkCase& problem) {
    vector<ClusteringSizeRange> ranges;
    ranges.reserve(problem.ranges.size());
    for (auto [lower, upper] : problem.ranges) ranges.push_back({lower, upper});
    return ranges;
}

double choose_flow_cost_scale(const ClusteringBenchmarkCase& problem) {
    int dimension = problem.points[0].size();
    vector<double> minimum(dimension, numeric_limits<double>::infinity());
    vector<double> maximum(dimension, -numeric_limits<double>::infinity());
    for (const auto& point : problem.points) for (int axis = 0; axis < dimension; ++axis) {
        minimum[axis] = min(minimum[axis], point[axis]);
        maximum[axis] = max(maximum[axis], point[axis]);
    }
    long double maximum_squared_distance = 0;
    for (int axis = 0; axis < dimension; ++axis) {
        long double difference = (long double)maximum[axis] - minimum[axis];
        maximum_squared_distance += difference * difference;
    }
    if (maximum_squared_distance == 0) return 1;
    long double node_count = (long double)problem.points.size() + problem.cluster_count + 3;
    long double scale = min((long double)1000000, acl_cost_limit / (node_count * maximum_squared_distance * 2));
    if (!(scale > 0) || scale > numeric_limits<double>::max()) throw overflow_error("benchmark: could not choose a flow cost scale");
    return (double)scale;
}

uint64_t hash_labels(const vector<int>& labels) {
    uint64_t hash = 1469598103934665603ULL;
    for (int label : labels) {
        uint32_t value = label;
        for (int byte = 0; byte < 4; ++byte) {
            hash ^= (value >> (byte * 8)) & 255;
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

vector<int> target_cluster_sizes(const ClusteringBenchmarkCase& problem, const vector<int>& labels) {
    vector<int> current = clustering_benchmark_cluster_sizes(labels, problem.cluster_count);
    if ((int)current.size() != problem.cluster_count) throw invalid_argument("benchmark: labels contain an invalid cluster number");
    vector<int> target(problem.cluster_count);
    int sum = 0;
    for (int cluster = 0; cluster < problem.cluster_count; ++cluster) {
        target[cluster] = clamp(current[cluster], problem.ranges[cluster].lower, problem.ranges[cluster].upper);
        sum += target[cluster];
    }
    while (sum < (int)problem.points.size()) {
        int selected = -1;
        for (int cluster = 0; cluster < problem.cluster_count; ++cluster) {
            if (target[cluster] == problem.ranges[cluster].upper) continue;
            if (selected == -1 || problem.ranges[selected].upper - target[selected] < problem.ranges[cluster].upper - target[cluster]) selected = cluster;
        }
        if (selected == -1) throw logic_error("benchmark: could not construct target cluster sizes");
        int add = min((int)problem.points.size() - sum, problem.ranges[selected].upper - target[selected]);
        target[selected] += add;
        sum += add;
    }
    while (sum > (int)problem.points.size()) {
        int selected = -1;
        for (int cluster = 0; cluster < problem.cluster_count; ++cluster) {
            if (target[cluster] == problem.ranges[cluster].lower) continue;
            if (selected == -1 || target[selected] - problem.ranges[selected].lower < target[cluster] - problem.ranges[cluster].lower) selected = cluster;
        }
        if (selected == -1) throw logic_error("benchmark: could not construct target cluster sizes");
        int remove = min(sum - (int)problem.points.size(), target[selected] - problem.ranges[selected].lower);
        target[selected] -= remove;
        sum -= remove;
    }
    return target;
}

vector<int> repair_labels_to_ranges(const ClusteringBenchmarkCase& problem, const vector<int>& labels, const vector<Point>& centers) {
    if (clustering_benchmark_valid_labels(problem, labels)) return labels;
    if (labels.size() != problem.points.size()) throw invalid_argument("benchmark: label count does not match point count");
    if ((int)centers.size() != problem.cluster_count) throw invalid_argument("benchmark: center count does not match cluster count");
    for (const Point& center : centers) if (center.size() != problem.points[0].size()) throw invalid_argument("benchmark: center dimension does not match point dimension");
    vector<int> target = target_cluster_sizes(problem, labels);
    vector<vector<int>> members(problem.cluster_count);
    for (int point = 0; point < (int)labels.size(); ++point) members[labels[point]].push_back(point);
    vector<int> repaired(labels.size(), -1), unassigned;
    for (int cluster = 0; cluster < problem.cluster_count; ++cluster) {
        auto& points = members[cluster];
        sort(points.begin(), points.end(), [&](int a, int b) {
            double cost_a = squared_distance(problem.points[a], centers[cluster]);
            double cost_b = squared_distance(problem.points[b], centers[cluster]);
            if (cost_a != cost_b) return cost_a < cost_b;
            return a < b;
        });
        int keep = min((int)points.size(), target[cluster]);
        for (int i = 0; i < keep; ++i) repaired[points[i]] = cluster;
        for (int i = keep; i < (int)points.size(); ++i) unassigned.push_back(points[i]);
    }
    vector<int> counts(problem.cluster_count);
    for (int label : repaired) if (label >= 0) ++counts[label];
    vector<int> order(problem.cluster_count);
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b) {
        int deficit_a = target[a] - counts[a];
        int deficit_b = target[b] - counts[b];
        if (deficit_a != deficit_b) return deficit_a < deficit_b;
        return a < b;
    });
    for (int cluster : order) {
        int need = target[cluster] - counts[cluster];
        if (need <= 0) continue;
        auto less = [&](int a, int b) {
            double cost_a = squared_distance(problem.points[a], centers[cluster]);
            double cost_b = squared_distance(problem.points[b], centers[cluster]);
            if (cost_a != cost_b) return cost_a < cost_b;
            return a < b;
        };
        if (need < (int)unassigned.size()) nth_element(unassigned.begin(), unassigned.begin() + need, unassigned.end(), less);
        sort(unassigned.begin(), unassigned.begin() + need, less);
        for (int i = 0; i < need; ++i) repaired[unassigned[i]] = cluster;
        unassigned.erase(unassigned.begin(), unassigned.begin() + need);
        counts[cluster] += need;
    }
    if (!unassigned.empty() || !clustering_benchmark_valid_labels(problem, repaired)) throw logic_error("benchmark: label repair failed");
    return repaired;
}

Solution run_kmeans(const ClusteringBenchmarkCase& problem, uint32_t seed, int max_iterations, bool hamerly) {
    KMeansOptions options{max_iterations, seed};
    if (hamerly) {
        auto result = kmeans_hamerly(problem.points, problem.cluster_count, SquaredDistance{}, EuclideanDistance{}, MeanCenter{}, options);
        return {move(result.labels), result.total_cost, result.iterations, 1, result.converged};
    }
    auto result = kmeans(problem.points, problem.cluster_count, SquaredDistance{}, MeanCenter{}, options);
    return {move(result.labels), result.total_cost, result.iterations, 1, result.converged};
}

Solution run_repeated_hamerly(const ClusteringBenchmarkCase& problem, uint32_t seed, int max_iterations, double time_limit_ms) {
    Clock::time_point start = Clock::now();
    Solution best;
    best.reported_cost = numeric_limits<double>::infinity();
    int64_t trials = 0;
    do {
        Solution result = run_kmeans(problem, seed + (uint32_t)trials, max_iterations, true);
        ++trials;
        if (result.reported_cost < best.reported_cost) best = move(result);
    } while (elapsed_ms(start) < time_limit_ms);
    best.trials = trials;
    return best;
}

Solution run_balanced(const ClusteringBenchmarkCase& problem, uint32_t seed, int max_iterations, double flow_cost_scale) {
    auto flow_cost = [flow_cost_scale](const Point& point, const Point& center) -> long long {
        long double value = (long double)squared_distance(point, center) * flow_cost_scale;
        if (value < 0 || value > numeric_limits<long long>::max()) throw overflow_error("benchmark: flow cost does not fit in long long");
        return llround(value);
    };
    KMeansOptions options{max_iterations, seed};
    auto result = kmeans_balanced(problem.points, balanced_ranges(problem), SquaredDistance{}, flow_cost, MeanCenter{}, options);
    Solution solution{move(result.labels), result.total_cost, result.iterations, 1, result.converged};
    solution.flow_cost_scale = flow_cost_scale;
    return solution;
}

Solution run_repeated_balanced(const ClusteringBenchmarkCase& problem, uint32_t seed, int max_iterations, double time_limit_ms) {
    Clock::time_point start = Clock::now();
    double flow_cost_scale = choose_flow_cost_scale(problem);
    Solution best;
    best.reported_cost = numeric_limits<double>::infinity();
    int64_t trials = 0;
    do {
        Solution result = run_balanced(problem, seed + (uint32_t)trials, max_iterations, flow_cost_scale);
        ++trials;
        if (result.reported_cost < best.reported_cost) best = move(result);
    } while (elapsed_ms(start) < time_limit_ms);
    best.trials = trials;
    return best;
}

void set_move_weight(ClusteringNeighborhoodWeights& weights, int type, int value) {
    if (type == clustering_relocate) weights.relocate = value;
    else if (type == clustering_swap) weights.swap = value;
    else if (type == clustering_cycle) weights.cycle = value;
    else if (type == clustering_rebuild) weights.rebuild = value;
}

ClusteringSaOptions sa_options_for_method(const string& method) {
    ClusteringSaOptions options;
    if (method == "sa_cluster_samples_8") {
        options.cluster_samples = 8;
        options.early_cluster_samples = 8;
    } else if (method == "sa_cluster_samples_16") {
        options.cluster_samples = 16;
        options.early_cluster_samples = 16;
    } else if (method == "sa_early_cluster_samples_8") {
        options.early_cluster_samples = 8;
    } else if (method == "sa_early_cluster_samples_16") {
        options.early_cluster_samples = 16;
    } else if (method == "sa_no_early_cluster_boost") {
        options.early_cluster_samples = 0;
    } else if (method == "sa_point_samples_16") {
        options.point_samples = 16;
    } else if (method == "sa_point_samples_32") {
        options.point_samples = 32;
    } else if (method == "sa_early_point_samples_16") {
        options.early_point_samples = 16;
    } else if (method == "sa_early_rebuild_40") {
        options.early_weights.rebuild = 40;
    } else if (method == "sa_early_rebuild_60") {
        options.early_weights.rebuild = 60;
    } else if (method == "sa_refined_rebuild_seeds") {
        options.refine_rebuild_seeds = true;
    } else if (method == "sa_rebuild_iterations_2") {
        options.rebuild_iterations = 2;
    } else if (method == "sa_rebuild_iterations_6") {
        options.rebuild_iterations = 6;
    } else if (method == "sa_no_rebuild_three") {
        options.rebuild_three_probability = 0;
        options.rebuild_three_same_state_threshold = 0;
    } else if (method == "sa_rebuild_three_24") {
        options.rebuild_three_point_limit = 24;
    } else if (method == "sa_rebuild_three_48") {
        options.rebuild_three_point_limit = 48;
    }
    int disabled = -1;
    if (method == "sa_no_relocate") disabled = clustering_relocate;
    else if (method == "sa_no_swap") disabled = clustering_swap;
    else if (method == "sa_no_cycle") disabled = clustering_cycle;
    else if (method == "sa_no_rebuild") disabled = clustering_rebuild;
    if (disabled != -1) {
        set_move_weight(options.early_weights, disabled, 0);
        set_move_weight(options.middle_weights, disabled, 0);
        set_move_weight(options.late_weights, disabled, 0);
    }
    return options;
}

Solution run_sa(const ClusteringBenchmarkCase& problem, uint32_t seed, int max_iterations, double total_time_limit_ms, const string& method, bool balanced_start, double& initialization_ms, double& search_ms) {
    Clock::time_point total_start = Clock::now();
    double flow_cost_scale = balanced_start ? choose_flow_cost_scale(problem) : numeric_limits<double>::quiet_NaN();
    ClusteringSaProblem sa_problem(problem.points);
    Solution initial;
    if (balanced_start) {
        initial = run_balanced(problem, seed, max_iterations, flow_cost_scale);
    } else {
        double initialization_fraction = method == "sa_multi_initial_10" ? 0.10 : method == "sa_multi_initial_20" ? 0.20 : method == "sa_multi_initial_30" ? 0.30 : method == "sa_multi_initial_40" ? 0.40 : method == "sa_multi_initial_70" ? 0.70 : method == "sa_multi_initial_80" ? 0.80 : method == "sa_multi_initial_90" ? 0.90 : 0;
        double initialization_limit = total_time_limit_ms * initialization_fraction;
        int64_t trials = 0;
        do {
            KMeansOptions options{max_iterations, seed + (uint32_t)trials};
            auto result = kmeans_hamerly(problem.points, problem.cluster_count, SquaredDistance{}, EuclideanDistance{}, MeanCenter{}, options);
            Solution candidate;
            candidate.labels = repair_labels_to_ranges(problem, result.labels, result.centers);
            candidate.reported_cost = clustering_benchmark_cost(problem, candidate.labels);
            candidate.iterations = result.iterations;
            candidate.converged = result.converged;
            ++trials;
            if (initial.labels.empty() || candidate.reported_cost < initial.reported_cost) initial = move(candidate);
        } while (initialization_fraction > 0 && elapsed_ms(total_start) < initialization_limit);
        initial.trials = trials;
    }
    if (!isfinite(initial.reported_cost)) initial.reported_cost = clustering_benchmark_cost(problem, initial.labels);
    initial.initial_cost = initial.reported_cost;
    initialization_ms = elapsed_ms(total_start);
    double remaining = max(0.0, total_time_limit_ms - initialization_ms);
    if (remaining == 0) {
        search_ms = 0;
        return initial;
    }
    Clock::time_point search_start = Clock::now();
    auto result = clustering_sa_from_labels(sa_problem, initial.labels, sa_ranges(problem), remaining, sa_options_for_method(method), seed, false);
    search_ms = elapsed_ms(search_start);
    Solution solution;
    solution.labels = move(result.labels);
    solution.reported_cost = result.total_cost;
    solution.initial_cost = initial.reported_cost;
    solution.iterations = initial.iterations;
    solution.converged = initial.converged;
    solution.trials = initial.trials;
    solution.statistics = result.statistics;
    solution.flow_cost_scale = balanced_start ? flow_cost_scale : numeric_limits<double>::quiet_NaN();
    return solution;
}

BenchmarkRow make_base_row(const string& case_path, const ClusteringBenchmarkCase& problem, const string& method, int run, uint32_t seed, const BenchmarkOptions& options) {
    BenchmarkRow row;
    row.case_path = case_path;
    row.case_name = problem.name;
    row.source = problem.source;
    row.experiment_tag = options.experiment_tag;
#ifdef __VERSION__
    row.compiler = __VERSION__;
#else
    row.compiler = "unknown";
#endif
    row.method = method;
    row.run = run;
    row.seed = seed;
    row.point_count = problem.points.size();
    row.dimension = problem.points[0].size();
    row.cluster_count = problem.cluster_count;
    row.max_iterations = options.max_iterations;
    row.balanced_edge_limit = options.balanced_edge_limit;
    row.size_condition = size_condition(problem);
    if (problem.has_best_known_cost) row.best_known_cost = problem.best_known_cost;
    return row;
}

void fill_solution_fields(BenchmarkRow& row, const ClusteringBenchmarkCase& problem, const Solution& solution) {
    row.reported_cost = solution.reported_cost;
    row.valid = clustering_benchmark_valid_labels(problem, solution.labels);
    row.recalculated_cost = clustering_benchmark_cost(problem, solution.labels);
    row.initial_cost = solution.initial_cost;
    if (isfinite(row.initial_cost) && row.initial_cost > 0 && isfinite(row.recalculated_cost)) row.improvement_from_initial_percent = (1 - row.recalculated_cost / row.initial_cost) * 100;
    else if (row.initial_cost == 0 && row.recalculated_cost == 0) row.improvement_from_initial_percent = 0;
    if (isfinite(row.recalculated_cost)) row.cost_per_point = row.recalculated_cost / problem.points.size();
    if (problem.has_best_known_cost && problem.best_known_cost > 0 && isfinite(row.recalculated_cost)) row.difference_from_best_percent = (row.recalculated_cost / problem.best_known_cost - 1) * 100;
    else if (problem.has_best_known_cost && problem.best_known_cost == 0 && row.recalculated_cost == 0) row.difference_from_best_percent = 0;
    if (row.valid && !problem.reference_labels.empty()) row.adjusted_rand_index = clustering_benchmark_adjusted_rand_index(solution.labels, problem.reference_labels);
    row.flow_cost_scale = solution.flow_cost_scale;
    row.iterations = solution.iterations;
    row.trials = solution.trials;
    row.converged = solution.converged;
    row.label_hash = hash_labels(solution.labels);
    row.statistics = solution.statistics;
}

string csv_text(const string& value) {
    string result = "\"";
    for (char character : value) {
        if (character == '"') result += '"';
        result += character;
    }
    result += '"';
    return result;
}

void write_double(ostream& output, double value) {
    if (isfinite(value)) output << setprecision(17) << value;
}

void write_header(ostream& output) {
    output << "case_path,case_name,source,experiment_tag,compiler,method,limit_kind,status,message,run,seed,point_count,dimension,cluster_count,size_condition,max_iterations,balanced_edge_limit,time_limit_ms,elapsed_ms,initialization_ms,search_ms,reported_cost,recalculated_cost,initial_cost,improvement_from_initial_percent,cost_per_point,best_known_cost,difference_from_best_percent,adjusted_rand_index,flow_cost_scale,valid,iterations,trials,converged,label_hash";
    for (string name : {"relocate", "swap", "cycle", "rebuild"}) output << ",attempts_" << name << ",valid_" << name << ",accepted_" << name << ",improvements_" << name << ",improvement_sum_" << name;
    output << ",candidate_refreshes,rebuild_same_state,rebuild_three_attempts,rebuild_three_valid,rebuild_three_accepted,rebuild_three_improvements,rebuild_three_improvement_sum,rebuild_three_same_state\n";
}

void write_row(ostream& output, const BenchmarkRow& row) {
    output << csv_text(row.case_path) << ',' << csv_text(row.case_name) << ',' << csv_text(row.source) << ',' << csv_text(row.experiment_tag) << ',' << csv_text(row.compiler) << ',' << csv_text(row.method) << ',' << csv_text(row.limit_kind) << ',' << csv_text(row.status) << ',' << csv_text(row.message) << ',';
    output << row.run << ',' << row.seed << ',' << row.point_count << ',' << row.dimension << ',' << row.cluster_count << ',' << csv_text(row.size_condition) << ',' << row.max_iterations << ',' << row.balanced_edge_limit << ',';
    write_double(output, row.time_limit_ms); output << ',';
    write_double(output, row.elapsed_ms); output << ',';
    write_double(output, row.initialization_ms); output << ',';
    write_double(output, row.search_ms); output << ',';
    write_double(output, row.reported_cost); output << ',';
    write_double(output, row.recalculated_cost); output << ',';
    write_double(output, row.initial_cost); output << ',';
    write_double(output, row.improvement_from_initial_percent); output << ',';
    write_double(output, row.cost_per_point); output << ',';
    write_double(output, row.best_known_cost); output << ',';
    write_double(output, row.difference_from_best_percent); output << ',';
    write_double(output, row.adjusted_rand_index); output << ',';
    write_double(output, row.flow_cost_scale); output << ',';
    output << row.valid << ',' << row.iterations << ',' << row.trials << ',' << row.converged << ',' << row.label_hash;
    for (int type = 0; type < clustering_move_type_count; ++type) {
        output << ',' << row.statistics.attempts[type] << ',' << row.statistics.valid_proposals[type] << ',' << row.statistics.accepted[type] << ',' << row.statistics.accepted_improvements[type] << ',';
        write_double(output, row.statistics.improvement_sum[type]);
    }
    output << ',' << row.statistics.candidate_refreshes << ',' << row.statistics.rebuild_same_state;
    output << ',' << row.statistics.rebuild_three_attempts << ',' << row.statistics.rebuild_three_valid_proposals << ',' << row.statistics.rebuild_three_accepted << ',' << row.statistics.rebuild_three_accepted_improvements << ',';
    write_double(output, row.statistics.rebuild_three_improvement_sum);
    output << ',' << row.statistics.rebuild_three_same_state << '\n';
    output.flush();
}

set<string> default_methods() {
    return {
        "kmeans",
        "hamerly",
        "hamerly_repeated",
        "hamerly_repaired",
        "balanced",
        "balanced_repeated",
        "sa",
        "sa_balanced",
        "sa_no_relocate",
        "sa_no_swap",
        "sa_no_cycle",
        "sa_no_rebuild",
        "sa_no_rebuild_three"
    };
}

set<string> known_methods() {
    set<string> result = default_methods();
    for (string method : {"sa_multi_initial_10", "sa_multi_initial_20", "sa_multi_initial_30", "sa_multi_initial_40", "sa_multi_initial_70", "sa_multi_initial_80", "sa_multi_initial_90"}) result.insert(move(method));
    result.insert("sa_cluster_samples_8");
    result.insert("sa_cluster_samples_16");
    result.insert("sa_early_cluster_samples_8");
    result.insert("sa_early_cluster_samples_16");
    result.insert("sa_no_early_cluster_boost");
    result.insert("sa_point_samples_16");
    result.insert("sa_point_samples_32");
    result.insert("sa_early_point_samples_16");
    result.insert("sa_early_rebuild_40");
    result.insert("sa_early_rebuild_60");
    result.insert("sa_refined_rebuild_seeds");
    result.insert("sa_rebuild_iterations_2");
    result.insert("sa_rebuild_iterations_6");
    result.insert("sa_rebuild_three_24");
    result.insert("sa_rebuild_three_48");
    return result;
}

set<string> parse_methods(const string& value) {
    set<string> result;
    stringstream input(value);
    string method;
    while (getline(input, method, ',')) if (!method.empty()) result.insert(method);
    if (result.empty()) throw invalid_argument("--methods must not be empty");
    set<string> known = known_methods();
    for (const string& name : result) if (!known.count(name)) throw invalid_argument("unknown method: " + name);
    return result;
}

long long parse_integer(const string& text, const string& option) {
    size_t parsed = 0;
    long long value = stoll(text, &parsed);
    if (parsed != text.size()) throw invalid_argument(option + " must be an integer");
    return value;
}

double parse_real(const string& text, const string& option) {
    size_t parsed = 0;
    double value = stod(text, &parsed);
    if (parsed != text.size()) throw invalid_argument(option + " must be a number");
    return value;
}

BenchmarkOptions parse_arguments(int argc, char** argv) {
    BenchmarkOptions options;
    options.methods = default_methods();
    for (int i = 1; i < argc; ++i) {
        string argument = argv[i];
        auto value = [&]() -> string {
            if (++i == argc) throw invalid_argument("missing value after " + argument);
            return argv[i];
        };
        if (argument == "--output") options.output_path = value();
        else if (argument == "--tag") options.experiment_tag = value();
        else if (argument == "--runs") {
            long long parsed = parse_integer(value(), argument);
            if (parsed > numeric_limits<int>::max()) throw invalid_argument("--runs must fit in int");
            options.runs = (int)parsed;
        }
        else if (argument == "--seed") {
            long long parsed = parse_integer(value(), argument);
            if (parsed < 0 || (unsigned long long)parsed > numeric_limits<uint32_t>::max()) throw invalid_argument("--seed must be in the uint32_t range");
            options.seed = (uint32_t)parsed;
        }
        else if (argument == "--time-ms") options.time_limit_ms = parse_real(value(), argument);
        else if (argument == "--max-iterations") {
            long long parsed = parse_integer(value(), argument);
            if (parsed > numeric_limits<int>::max()) throw invalid_argument("--max-iterations must fit in int");
            options.max_iterations = (int)parsed;
        }
        else if (argument == "--balanced-edge-limit") {
            options.balanced_edge_limit = parse_integer(value(), argument);
        }
        else if (argument == "--methods") options.methods = parse_methods(value());
        else if (!argument.empty() && argument[0] == '-') throw invalid_argument("unknown option: " + argument);
        else options.case_paths.push_back(argument);
    }
    if (options.output_path.empty()) throw invalid_argument("--output is required");
    if (options.case_paths.empty()) throw invalid_argument("at least one case file is required");
    if (options.runs <= 0) throw invalid_argument("--runs must be positive");
    if (!isfinite(options.time_limit_ms) || options.time_limit_ms <= 0) throw invalid_argument("--time-ms must be finite and positive");
    if (options.max_iterations <= 0) throw invalid_argument("--max-iterations must be positive");
    if (options.balanced_edge_limit < 0) throw invalid_argument("--balanced-edge-limit must be nonnegative");
    return options;
}

bool is_sa_method(const string& method) {
    return method == "sa" || method == "sa_balanced" || method == "sa_no_relocate" || method == "sa_no_swap" || method == "sa_no_cycle" || method == "sa_no_rebuild" || method == "sa_no_rebuild_three" || method == "sa_multi_initial_10" || method == "sa_multi_initial_20" || method == "sa_multi_initial_30" || method == "sa_multi_initial_40" || method == "sa_multi_initial_70" || method == "sa_multi_initial_80" || method == "sa_multi_initial_90" || method == "sa_cluster_samples_8" || method == "sa_cluster_samples_16" || method == "sa_early_cluster_samples_8" || method == "sa_early_cluster_samples_16" || method == "sa_no_early_cluster_boost" || method == "sa_point_samples_16" || method == "sa_point_samples_32" || method == "sa_early_point_samples_16" || method == "sa_early_rebuild_40" || method == "sa_early_rebuild_60" || method == "sa_refined_rebuild_seeds" || method == "sa_rebuild_iterations_2" || method == "sa_rebuild_iterations_6" || method == "sa_rebuild_three_24" || method == "sa_rebuild_three_48";
}

void run_method(ostream& output, const BenchmarkOptions& options, const string& case_path, const ClusteringBenchmarkCase& problem, const string& method, int run) {
    uint32_t seed = options.seed + (uint32_t)run * 1000003U;
    BenchmarkRow row = make_base_row(case_path, problem, method, run, seed, options);
    bool constrained = clustering_benchmark_has_size_constraints(problem);
    bool balanced_allowed = (long long)problem.points.size() * problem.cluster_count <= options.balanced_edge_limit;
    bool needs_balanced = method == "balanced" || method == "balanced_repeated" || method == "sa_balanced";
    bool unconstrained_only = method == "kmeans" || method == "hamerly" || method == "hamerly_repeated";
    if (unconstrained_only && constrained) {
        row.status = "skipped";
        row.message = "method does not enforce cluster size constraints";
        write_row(output, row);
        return;
    }
    if (method == "hamerly_repaired" && !constrained) {
        row.status = "skipped";
        row.message = "label repair is only needed for constrained cases";
        write_row(output, row);
        return;
    }
    if (needs_balanced && !constrained) {
        row.status = "skipped";
        row.message = "balanced method is only measured on constrained cases";
        write_row(output, row);
        return;
    }
    if (needs_balanced && !balanced_allowed) {
        row.status = "skipped";
        row.message = "point_count * cluster_count exceeds --balanced-edge-limit";
        write_row(output, row);
        return;
    }
    Clock::time_point start = Clock::now();
    try {
        Solution solution;
        if (method == "kmeans") {
            row.limit_kind = "completion";
            solution = run_kmeans(problem, seed, options.max_iterations, false);
        } else if (method == "hamerly") {
            row.limit_kind = "completion";
            solution = run_kmeans(problem, seed, options.max_iterations, true);
        } else if (method == "hamerly_repeated") {
            row.limit_kind = "total_time";
            row.time_limit_ms = options.time_limit_ms;
            solution = run_repeated_hamerly(problem, seed, options.max_iterations, options.time_limit_ms);
        } else if (method == "hamerly_repaired") {
            row.limit_kind = "completion";
            KMeansOptions kmeans_options{options.max_iterations, seed};
            auto initial = kmeans_hamerly(problem.points, problem.cluster_count, SquaredDistance{}, EuclideanDistance{}, MeanCenter{}, kmeans_options);
            solution.labels = repair_labels_to_ranges(problem, initial.labels, initial.centers);
            solution.reported_cost = clustering_benchmark_cost(problem, solution.labels);
            solution.iterations = initial.iterations;
            solution.converged = initial.converged;
        } else if (method == "balanced") {
            row.limit_kind = "completion";
            solution = run_balanced(problem, seed, options.max_iterations, choose_flow_cost_scale(problem));
        } else if (method == "balanced_repeated") {
            row.limit_kind = "total_time";
            row.time_limit_ms = options.time_limit_ms;
            solution = run_repeated_balanced(problem, seed, options.max_iterations, options.time_limit_ms);
        } else if (is_sa_method(method)) {
            row.limit_kind = "total_time";
            row.time_limit_ms = options.time_limit_ms;
            bool balanced_start = method == "sa_balanced";
            solution = run_sa(problem, seed, options.max_iterations, options.time_limit_ms, method, balanced_start, row.initialization_ms, row.search_ms);
        } else {
            throw logic_error("unhandled method: " + method);
        }
        row.elapsed_ms = elapsed_ms(start);
        fill_solution_fields(row, problem, solution);
        if (!row.valid) {
            row.status = "error";
            row.message = "returned labels violate the benchmark conditions";
        } else if (!isfinite(row.reported_cost) || row.reported_cost < 0 || !isfinite(row.recalculated_cost) || row.recalculated_cost < 0) {
            row.status = "error";
            row.message = "returned cost must be finite and nonnegative";
        }
    } catch (const exception& exception) {
        row.elapsed_ms = elapsed_ms(start);
        row.status = "error";
        row.message = exception.what();
    }
    write_row(output, row);
}
}

int main(int argc, char** argv) {
    try {
        BenchmarkOptions options = parse_arguments(argc, argv);
        filesystem::path output_path = filesystem::absolute(options.output_path).lexically_normal();
        for (const string& case_path : options.case_paths) {
            filesystem::path input_path = filesystem::absolute(case_path).lexically_normal();
            bool same_existing_file = filesystem::exists(input_path) && filesystem::exists(output_path) && filesystem::equivalent(input_path, output_path);
            if (input_path == output_path || same_existing_file) throw invalid_argument("output path must differ from every case path");
        }
        ofstream output(options.output_path);
        if (!output) throw runtime_error("could not open output file: " + options.output_path);
        write_header(output);
        for (const string& case_path : options.case_paths) {
            ClusteringBenchmarkCase problem = read_clustering_benchmark_case(case_path);
            vector<string> methods(options.methods.begin(), options.methods.end());
            for (int run = 0; run < options.runs; ++run) {
                if (run) rotate(methods.begin(), methods.begin() + 1, methods.end());
                for (const string& method : methods) run_method(output, options, case_path, problem, method, run);
            }
        }
    } catch (const exception& exception) {
        cerr << exception.what() << '\n';
        return 1;
    }
}
