#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using namespace std;
namespace titan23 {
struct ClusteringBenchmarkRange {
    int lower;
    int upper;
};

struct ClusteringBenchmarkCase {
    string name;
    string source;
    vector<vector<double>> points;
    int cluster_count = 0;
    vector<ClusteringBenchmarkRange> ranges;
    bool has_best_known_cost = false;
    double best_known_cost = 0;
    vector<int> reference_labels;
};

inline void check_clustering_benchmark_case(const ClusteringBenchmarkCase& problem) {
    if (problem.name.empty()) throw invalid_argument("clustering benchmark: name must not be empty");
    if (problem.name.find_first_of("\r\n") != string::npos || problem.source.find_first_of("\r\n") != string::npos) throw invalid_argument("clustering benchmark: name and source must be one line");
    if (problem.points.empty()) throw invalid_argument("clustering benchmark: points must not be empty");
    if (problem.points.size() > (size_t)numeric_limits<int>::max()) throw invalid_argument("clustering benchmark: point count must fit in int");
    if (problem.points[0].empty()) throw invalid_argument("clustering benchmark: dimension must be positive");
    if (problem.points[0].size() > (size_t)numeric_limits<int>::max()) throw invalid_argument("clustering benchmark: dimension must fit in int");
    size_t dimension = problem.points[0].size();
    long double maximum_squared_distance = 0;
    long double total_squared_norm = 0;
    vector<double> minimum(dimension, numeric_limits<double>::infinity());
    vector<double> maximum(dimension, -numeric_limits<double>::infinity());
    vector<long double> absolute_sums(dimension);
    for (const auto& point : problem.points) {
        if (point.size() != dimension) throw invalid_argument("clustering benchmark: all points must have the same dimension");
        long double squared_norm = 0;
        for (int axis = 0; axis < (int)dimension; ++axis) {
            double coordinate = point[axis];
            if (!isfinite(coordinate)) throw invalid_argument("clustering benchmark: coordinates must be finite");
            squared_norm += (long double)coordinate * coordinate;
            absolute_sums[axis] += abs((long double)coordinate);
            minimum[axis] = min(minimum[axis], coordinate);
            maximum[axis] = max(maximum[axis], coordinate);
        }
        if (squared_norm > numeric_limits<double>::max()) throw invalid_argument("clustering benchmark: squared point norm must fit in double");
        total_squared_norm += squared_norm;
    }
    if (total_squared_norm > numeric_limits<double>::max()) throw invalid_argument("clustering benchmark: total squared point norm must fit in double");
    long double maximum_center_term = 0;
    for (long double value : absolute_sums) maximum_center_term += value * value;
    if (maximum_center_term > numeric_limits<double>::max()) throw invalid_argument("clustering benchmark: squared coordinate sums must fit in double");
    for (int axis = 0; axis < (int)dimension; ++axis) {
        long double difference = (long double)maximum[axis] - minimum[axis];
        maximum_squared_distance += difference * difference;
    }
    if (maximum_squared_distance > numeric_limits<double>::max()) throw invalid_argument("clustering benchmark: squared distance must fit in double");
    if (problem.cluster_count <= 0 || problem.cluster_count > (int)problem.points.size()) throw invalid_argument("clustering benchmark: cluster count must be in [1, point count]");
    if ((int)problem.ranges.size() != problem.cluster_count) throw invalid_argument("clustering benchmark: ranges.size() must equal cluster count");
    long long lower_sum = 0, upper_sum = 0;
    for (auto [lower, upper] : problem.ranges) {
        if (lower <= 0 || lower > upper || upper > (int)problem.points.size()) throw invalid_argument("clustering benchmark: each range must satisfy 1 <= lower <= upper <= point count");
        lower_sum += lower;
        upper_sum += upper;
    }
    if (lower_sum > (long long)problem.points.size() || upper_sum < (long long)problem.points.size()) throw invalid_argument("clustering benchmark: size ranges are infeasible");
    if (problem.has_best_known_cost && (!isfinite(problem.best_known_cost) || problem.best_known_cost < 0)) throw invalid_argument("clustering benchmark: best known cost must be finite and nonnegative");
    if (!problem.reference_labels.empty()) {
        if (problem.reference_labels.size() != problem.points.size()) throw invalid_argument("clustering benchmark: reference label count must equal point count");
        for (int label : problem.reference_labels) {
            if (label < 0) throw invalid_argument("clustering benchmark: reference labels must be nonnegative");
        }
    }
}

inline ClusteringBenchmarkCase read_clustering_benchmark_case(const string& path) {
    ifstream input(path);
    if (!input) throw runtime_error("clustering benchmark: could not open " + path);
    string format;
    input >> format;
    if (format != "titan_clustering_benchmark_v1") throw invalid_argument("clustering benchmark: unsupported file format in " + path);
    ClusteringBenchmarkCase problem;
    int point_count = 0, dimension = 0;
    string key;
    input >> key >> quoted(problem.name);
    if (key != "name") throw invalid_argument("clustering benchmark: expected name in " + path);
    input >> key >> quoted(problem.source);
    if (key != "source") throw invalid_argument("clustering benchmark: expected source in " + path);
    input >> key >> point_count;
    if (key != "points") throw invalid_argument("clustering benchmark: expected points in " + path);
    input >> key >> dimension;
    if (key != "dimension") throw invalid_argument("clustering benchmark: expected dimension in " + path);
    input >> key >> problem.cluster_count;
    if (key != "clusters") throw invalid_argument("clustering benchmark: expected clusters in " + path);
    if (point_count <= 0 || dimension <= 0) throw invalid_argument("clustering benchmark: point count and dimension must be positive in " + path);
    if (problem.cluster_count <= 0 || problem.cluster_count > point_count) throw invalid_argument("clustering benchmark: cluster count must be in [1, point count] in " + path);
    string best_known;
    input >> key >> best_known;
    if (key != "best_known_cost") throw invalid_argument("clustering benchmark: expected best_known_cost in " + path);
    if (best_known != "none") {
        size_t parsed = 0;
        problem.best_known_cost = stod(best_known, &parsed);
        if (parsed != best_known.size()) throw invalid_argument("clustering benchmark: invalid best known cost in " + path);
        problem.has_best_known_cost = true;
    }
    input >> key;
    if (key != "ranges") throw invalid_argument("clustering benchmark: expected ranges in " + path);
    problem.ranges.resize(problem.cluster_count);
    for (auto& range : problem.ranges) input >> range.lower >> range.upper;
    input >> key;
    if (key != "reference_labels") throw invalid_argument("clustering benchmark: expected reference_labels in " + path);
    string reference;
    input >> reference;
    if (reference == "present") {
        problem.reference_labels.resize(point_count);
        for (int& label : problem.reference_labels) input >> label;
    } else if (reference != "none") {
        throw invalid_argument("clustering benchmark: reference_labels must be present or none in " + path);
    }
    input >> key;
    if (key != "data") throw invalid_argument("clustering benchmark: expected data in " + path);
    problem.points.assign(point_count, vector<double>(dimension));
    for (auto& point : problem.points) for (double& coordinate : point) input >> coordinate;
    if (!input) throw invalid_argument("clustering benchmark: incomplete file " + path);
    string extra;
    if (input >> extra) throw invalid_argument("clustering benchmark: unexpected data after the last point in " + path);
    check_clustering_benchmark_case(problem);
    return problem;
}

inline void write_clustering_benchmark_case(const string& path, const ClusteringBenchmarkCase& problem) {
    check_clustering_benchmark_case(problem);
    ofstream output(path);
    if (!output) throw runtime_error("clustering benchmark: could not write " + path);
    output << "titan_clustering_benchmark_v1\n";
    output << "name " << quoted(problem.name) << '\n';
    output << "source " << quoted(problem.source) << '\n';
    output << "points " << problem.points.size() << '\n';
    output << "dimension " << problem.points[0].size() << '\n';
    output << "clusters " << problem.cluster_count << '\n';
    output << "best_known_cost ";
    if (problem.has_best_known_cost) output << setprecision(17) << problem.best_known_cost << '\n';
    else output << "none\n";
    output << "ranges";
    for (auto [lower, upper] : problem.ranges) output << ' ' << lower << ' ' << upper;
    output << '\n';
    output << "reference_labels ";
    if (problem.reference_labels.empty()) {
        output << "none\n";
    } else {
        output << "present";
        for (int label : problem.reference_labels) output << ' ' << label;
        output << '\n';
    }
    output << "data\n" << setprecision(17);
    for (const auto& point : problem.points) {
        for (int axis = 0; axis < (int)point.size(); ++axis) {
            if (axis) output << ' ';
            output << point[axis];
        }
        output << '\n';
    }
}

inline bool clustering_benchmark_has_size_constraints(const ClusteringBenchmarkCase& problem) {
    int point_count = problem.points.size();
    for (auto [lower, upper] : problem.ranges) if (lower != 1 || upper != point_count) return true;
    return false;
}

inline bool clustering_benchmark_has_exact_sizes(const ClusteringBenchmarkCase& problem) {
    for (auto [lower, upper] : problem.ranges) if (lower != upper) return false;
    return true;
}

inline vector<int> clustering_benchmark_cluster_sizes(const vector<int>& labels, int cluster_count) {
    if (cluster_count <= 0) return {};
    vector<int> sizes(cluster_count);
    for (int label : labels) {
        if (label < 0 || label >= cluster_count) return {};
        ++sizes[label];
    }
    return sizes;
}

inline bool clustering_benchmark_valid_labels(const ClusteringBenchmarkCase& problem, const vector<int>& labels) {
    if (labels.size() != problem.points.size()) return false;
    vector<int> sizes = clustering_benchmark_cluster_sizes(labels, problem.cluster_count);
    if (sizes.empty()) return false;
    for (int cluster = 0; cluster < problem.cluster_count; ++cluster) {
        if (sizes[cluster] < problem.ranges[cluster].lower || sizes[cluster] > problem.ranges[cluster].upper) return false;
    }
    return true;
}

inline double clustering_benchmark_cost(const ClusteringBenchmarkCase& problem, const vector<int>& labels) {
    if (!clustering_benchmark_valid_labels(problem, labels)) return numeric_limits<double>::infinity();
    int dimension = problem.points[0].size();
    vector<vector<long double>> centers(problem.cluster_count, vector<long double>(dimension));
    vector<int> sizes(problem.cluster_count);
    for (int point = 0; point < (int)problem.points.size(); ++point) {
        int cluster = labels[point];
        ++sizes[cluster];
        for (int axis = 0; axis < dimension; ++axis) centers[cluster][axis] += problem.points[point][axis];
    }
    for (int cluster = 0; cluster < problem.cluster_count; ++cluster) {
        for (long double& value : centers[cluster]) value /= sizes[cluster];
    }
    long double cost = 0;
    for (int point = 0; point < (int)problem.points.size(); ++point) {
        int cluster = labels[point];
        for (int axis = 0; axis < dimension; ++axis) {
            long double difference = (long double)problem.points[point][axis] - centers[cluster][axis];
            cost += difference * difference;
        }
    }
    if (cost > numeric_limits<double>::max()) return numeric_limits<double>::infinity();
    return (double)cost;
}

inline double clustering_benchmark_adjusted_rand_index(const vector<int>& labels, const vector<int>& reference_labels) {
    if (labels.empty() || labels.size() != reference_labels.size()) return numeric_limits<double>::quiet_NaN();
    for (int label : labels) if (label < 0) return numeric_limits<double>::quiet_NaN();
    for (int label : reference_labels) if (label < 0) return numeric_limits<double>::quiet_NaN();
    vector<pair<int, int>> joint;
    joint.reserve(labels.size());
    vector<int> sorted_labels = labels, sorted_reference = reference_labels;
    for (int i = 0; i < (int)labels.size(); ++i) joint.emplace_back(labels[i], reference_labels[i]);
    sort(joint.begin(), joint.end());
    sort(sorted_labels.begin(), sorted_labels.end());
    sort(sorted_reference.begin(), sorted_reference.end());
    auto pairs = [](long long count) -> long double { return (long double)count * (count - 1) / 2; };
    long double same_both = 0, same_labels = 0, same_reference = 0;
    for (int begin = 0; begin < (int)joint.size();) {
        int end = begin + 1;
        while (end < (int)joint.size() && joint[end] == joint[begin]) ++end;
        same_both += pairs(end - begin);
        begin = end;
    }
    for (int begin = 0; begin < (int)sorted_labels.size();) {
        int end = begin + 1;
        while (end < (int)sorted_labels.size() && sorted_labels[end] == sorted_labels[begin]) ++end;
        same_labels += pairs(end - begin);
        begin = end;
    }
    for (int begin = 0; begin < (int)sorted_reference.size();) {
        int end = begin + 1;
        while (end < (int)sorted_reference.size() && sorted_reference[end] == sorted_reference[begin]) ++end;
        same_reference += pairs(end - begin);
        begin = end;
    }
    long double all_pairs = pairs(labels.size());
    if (all_pairs == 0) return 1;
    long double expected = same_labels * same_reference / all_pairs;
    long double maximum = (same_labels + same_reference) / 2;
    if (maximum == expected) return same_both == expected ? 1 : 0;
    return (double)((same_both - expected) / (maximum - expected));
}
}
