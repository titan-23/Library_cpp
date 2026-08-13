#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include "titan_cpplib/ahc/clustering/clustering_partition.cpp"
#include "titan_cpplib/ahc/sa/sa.cpp"
using namespace std;
namespace titan23 {
class ClusteringSaProblem {
public:
    template<class Point>
    explicit ClusteringSaProblem(const vector<Point>& points) {
        if (points.empty()) throw invalid_argument("ClusteringSaProblem: points must not be empty");
        int dimension = checked_int(points[0].size(), "ClusteringSaProblem: point dimension must fit in int");
        initialize<true>(points, dimension, [](const Point& point, int axis) { return point[axis]; });
    }

    template<class Point, class Coordinate>
    ClusteringSaProblem(const vector<Point>& points, int dimension, Coordinate coordinate) {
        initialize<false>(points, dimension, move(coordinate));
    }

    ClusteringSaProblem(const ClusteringSaProblem&) = delete;
    ClusteringSaProblem& operator=(const ClusteringSaProblem&) = delete;
    ClusteringSaProblem(ClusteringSaProblem&&) = delete;
    ClusteringSaProblem& operator=(ClusteringSaProblem&&) = delete;

    int point_count() const { return point_count_; }
    int dimension() const { return dimension_; }
    double coordinate(int point, int axis) const {
#ifdef TITAN_DEBUG
        if (point < 0 || point >= point_count_) throw out_of_range("ClusteringSaProblem::coordinate: point is out of range");
        if (axis < 0 || axis >= dimension_) throw out_of_range("ClusteringSaProblem::coordinate: axis is out of range");
#endif
        return coordinates_[(size_t)point * dimension_ + axis];
    }
    const double* point_data(int point) const {
#ifdef TITAN_DEBUG
        if (point < 0 || point >= point_count_) throw out_of_range("ClusteringSaProblem::point_data: point is out of range");
#endif
        return coordinates_.data() + (size_t)point * dimension_;
    }
    double squared_norm(int point) const {
#ifdef TITAN_DEBUG
        if (point < 0 || point >= point_count_) throw out_of_range("ClusteringSaProblem::squared_norm: point is out of range");
#endif
        return squared_norms_[point];
    }
    double squared_distance(int point1, int point2) const {
        const double* a = point_data(point1);
        const double* b = point_data(point2);
        double distance = 0;
        for (int axis = 0; axis < dimension_; ++axis) {
            double difference = a[axis] - b[axis];
            distance += difference * difference;
        }
        if (!isfinite(distance)) throw overflow_error("ClusteringSaProblem: squared distance must be finite");
        return distance;
    }
    double squared_distance_to_center(int point, const double* center) const {
        const double* coordinates = point_data(point);
        double distance = 0;
        for (int axis = 0; axis < dimension_; ++axis) {
            double difference = coordinates[axis] - center[axis];
            distance += difference * difference;
        }
        if (!isfinite(distance)) throw overflow_error("ClusteringSaProblem: squared distance to center must be finite");
        return distance;
    }

private:
    int point_count_ = 0;
    int dimension_ = 0;
    vector<double> coordinates_;
    vector<double> squared_norms_;

    static int checked_int(size_t value, const char* message) {
        if (value > (size_t)numeric_limits<int>::max()) throw invalid_argument(message);
        return value;
    }

    template<bool CheckPointSizes, class Point, class Coordinate>
    void initialize(const vector<Point>& points, int dimension, Coordinate coordinate) {
        if (points.empty()) throw invalid_argument("ClusteringSaProblem: points must not be empty");
        if (dimension <= 0) throw invalid_argument("ClusteringSaProblem: point dimension must be positive");
        point_count_ = checked_int(points.size(), "ClusteringSaProblem: points.size() must fit in int");
        dimension_ = dimension;
        if (points.size() > coordinates_.max_size() / (size_t)dimension) throw length_error("ClusteringSaProblem: coordinate array is too large");
        coordinates_.reserve(points.size() * dimension);
        squared_norms_.reserve(points.size());
        for (const Point& point : points) {
            if constexpr (CheckPointSizes) if (checked_int(point.size(), "ClusteringSaProblem: point dimension must fit in int") != dimension) throw invalid_argument("ClusteringSaProblem: all points must have the same dimension");
            double squared_norm = 0;
            for (int axis = 0; axis < dimension; ++axis) {
                double value = (double)coordinate(point, axis);
                if (!isfinite(value)) throw invalid_argument("ClusteringSaProblem: coordinates must be finite");
                coordinates_.push_back(value);
                squared_norm += value * value;
            }
            if (!isfinite(squared_norm)) throw overflow_error("ClusteringSaProblem: squared point norm must be finite");
            squared_norms_.push_back(squared_norm);
        }
    }
};

struct ClusteringNeighborhoodWeights {
    int relocate = 40;
    int swap = 40;
    int cycle = 10;
    int rebuild = 10;
};

struct ClusteringSaOptions {
    int nearby_cluster_count = 4;
    int cluster_samples = 4;
    int early_cluster_samples = -1;
    int point_samples = 8;
    int early_point_samples = 0;
    int swap_partner_samples = 12;
    int cycle_partner_samples = 8;
    int candidate_refresh_interval = 256;
    int rebuild_point_limit = 128;
    int rebuild_three_point_limit = 36;
    int rebuild_iterations = 4;
    bool refine_rebuild_seeds = false;
    double rebuild_three_probability = 0;
    int rebuild_three_same_state_threshold = -1;
    double score_scale = 0;
    double uniform_selection_probability = 0.05;
    double middle_phase_start = 0.30;
    double late_phase_start = 0.80;
    ClusteringNeighborhoodWeights early_weights{35, 35, 10, 20};
    ClusteringNeighborhoodWeights middle_weights{45, 40, 10, 5};
    ClusteringNeighborhoodWeights late_weights{50, 45, 5, 0};
};

enum ClusteringMoveType {
    clustering_relocate = 0,
    clustering_swap = 1,
    clustering_cycle = 2,
    clustering_rebuild = 3,
    clustering_move_type_count = 4
};

struct ClusteringSaStatistics {
    array<int64_t, clustering_move_type_count> attempts{};
    array<int64_t, clustering_move_type_count> valid_proposals{};
    array<int64_t, clustering_move_type_count> accepted{};
    array<int64_t, clustering_move_type_count> accepted_improvements{};
    array<double, clustering_move_type_count> improvement_sum{};
    int64_t candidate_refreshes = 0;
    int64_t rebuild_same_state = 0;
    int64_t rebuild_three_attempts = 0;
    int64_t rebuild_three_valid_proposals = 0;
    int64_t rebuild_three_accepted = 0;
    int64_t rebuild_three_accepted_improvements = 0;
    double rebuild_three_improvement_sum = 0;
    int64_t rebuild_three_same_state = 0;
};

struct ClusteringSaResult {
    double score = 0;
    double true_score = 0;
    double total_cost = 0;
    double score_scale = 1;
    vector<int> labels;
    vector<vector<double>> centers;
    vector<int> cluster_sizes;
    ClusteringSaStatistics statistics;
    int64_t initial_trials = 1;

    void print(ostream& output = cout) const {
        for (int label : labels) output << label << ' ';
        output << '\n';
    }
};

inline double clustering_cost_from_labels(const ClusteringSaProblem& problem, const vector<int>& labels, const vector<ClusteringSizeRange>& ranges) {
    check_clustering_size_ranges(problem.point_count(), ranges);
    if (ranges.size() > (size_t)numeric_limits<int>::max()) throw invalid_argument("clustering_cost_from_labels: ranges.size() must fit in int");
    int cluster_count = ranges.size();
    ClusteringPartition partition(problem.point_count(), cluster_count, labels);
    int dimension = problem.dimension();
    if ((size_t)cluster_count > vector<double>().max_size() / (size_t)dimension) throw length_error("clustering_cost_from_labels: coordinate sums are too large");
    vector<double> sums((size_t)cluster_count * dimension);
    vector<double> squared_norm_sums(cluster_count);
    for (int point = 0; point < problem.point_count(); ++point) {
        int cluster = partition.label(point);
        double* sum = sums.data() + (size_t)cluster * dimension;
        const double* coordinates = problem.point_data(point);
        for (int axis = 0; axis < dimension; ++axis) sum[axis] += coordinates[axis];
        squared_norm_sums[cluster] += problem.squared_norm(point);
    }
    double total_cost = 0;
    for (int cluster = 0; cluster < cluster_count; ++cluster) {
        int count = partition.cluster_size(cluster);
        if (count < ranges[cluster].lower || count > ranges[cluster].upper) throw invalid_argument("clustering_cost_from_labels: labels violate cluster size ranges");
        const double* sum = sums.data() + (size_t)cluster * dimension;
        double center_term = 0;
        for (int axis = 0; axis < dimension; ++axis) center_term += sum[axis] * sum[axis];
        total_cost += squared_norm_sums[cluster] - center_term / count;
    }
    if (!isfinite(total_cost)) throw overflow_error("clustering_cost_from_labels: cost must be finite");
    return total_cost;
}

class ClusteringSaState {
public:
    using ScoreType = double;

    struct Param {
        double start_temp;
        double end_temp;

        Param() : start_temp(1), end_temp(0.001) {}
    };

    struct Changed {
        int TYPE_CNT = clustering_move_type_count;
        int type = clustering_relocate;
    } changed;

    using Result = ClusteringSaResult;
    inline static Param param;
    bool is_valid = true;
    ScoreType score = 0;

    ClusteringSaState(const ClusteringSaProblem& problem, vector<int> labels, vector<ClusteringSizeRange> ranges, ClusteringSaOptions options, uint32_t seed)
        : problem_(&problem), ranges_(move(ranges)), options_(move(options)), partition_(problem.point_count(), checked_cluster_count(ranges_), move(labels)), random_(seed) {
        resolve_options();
        check_options();
        check_initial_partition();
        initialize_aggregates();
        initialize_score_scale();
        reserve_workspaces();
        refresh_candidates();
        score = normalized_score(total_cost_);
    }

    void reset_is_valid() { is_valid = true; }
    ScoreType get_score() const { return score; }
    ScoreType get_true_score() const { return total_cost_; }
    const ClusteringPartition& partition() const { return partition_; }
    const vector<double>& cluster_costs() const { return cluster_costs_; }
    double total_cost() const { return total_cost_; }

    void modify(int64_t, ScoreType, double progress) {
        clear_pending();
        current_progress_ = progress;
        if (candidates_dirty_) refresh_candidates();
        int type = select_move_type(progress);
        changed.type = type == -1 ? clustering_relocate : type;
        if (type == -1) {
            is_valid = false;
            return;
        }
        increment_counter(statistics_.attempts[type]);
        bool valid = false;
        bool attempted_rebuild_three = false;
        if (type == clustering_relocate) valid = propose_relocate();
        else if (type == clustering_swap) valid = propose_swap();
        else if (type == clustering_cycle) valid = propose_cycle();
        else {
            bool rebuild_three_due_to_stagnation = options_.rebuild_three_same_state_threshold > 0 && rebuild_two_same_state_streak_ >= options_.rebuild_three_same_state_threshold;
            bool rebuild_three_randomly = options_.rebuild_three_probability > 0 && random_unit() < options_.rebuild_three_probability;
            attempted_rebuild_three = partition_.cluster_count() >= 3 && (rebuild_three_due_to_stagnation || rebuild_three_randomly);
            if (attempted_rebuild_three) {
                if (rebuild_three_due_to_stagnation) rebuild_two_same_state_streak_ = 0;
                increment_counter(statistics_.rebuild_three_attempts);
                valid = propose_rebuild_three();
            } else {
                valid = propose_rebuild();
            }
        }
        if (!valid || !isfinite(pending_delta_) || !isfinite(total_cost_ + pending_delta_)) {
            is_valid = false;
            return;
        }
        increment_counter(statistics_.valid_proposals[type]);
        if (attempted_rebuild_three) increment_counter(statistics_.rebuild_three_valid_proposals);
        score = normalized_score(total_cost_ + pending_delta_);
    }

    void rollback() {
        clear_pending();
    }

    void advance() {
        bool rebuild_three = pending_type_ == clustering_rebuild && pending_rebuild_cluster_count_ == 3;
        if (pending_type_ == clustering_relocate) apply_relocate();
        else if (pending_type_ == clustering_swap) apply_swap();
        else if (pending_type_ == clustering_cycle) apply_cycle();
        else if (pending_type_ == clustering_rebuild) apply_rebuild();
        else throw logic_error("ClusteringSaState::advance: no pending move");
        if (pending_type_ != clustering_rebuild || rebuild_three) {
            rebuild_two_same_state_streak_ = 0;
        }
        increment_counter(statistics_.accepted[pending_type_]);
        if (rebuild_three) increment_counter(statistics_.rebuild_three_accepted);
        if (pending_delta_ < 0) {
            increment_counter(statistics_.accepted_improvements[pending_type_]);
            statistics_.improvement_sum[pending_type_] -= pending_delta_;
            if (rebuild_three) {
                increment_counter(statistics_.rebuild_three_accepted_improvements);
                statistics_.rebuild_three_improvement_sum -= pending_delta_;
            }
        }
        ++accepted_since_refresh_;
        if (accepted_since_refresh_ >= options_.candidate_refresh_interval || pending_type_ == clustering_rebuild) candidates_dirty_ = true;
        score = normalized_score(total_cost_);
        clear_pending();
    }

    Result get_result() const {
        int cluster_count = partition_.cluster_count();
        int dimension = problem_->dimension();
        vector<vector<double>> centers(cluster_count, vector<double>(dimension));
        vector<int> sizes(cluster_count);
        for (int cluster = 0; cluster < cluster_count; ++cluster) {
            sizes[cluster] = partition_.cluster_size(cluster);
            const double* center = center_data(cluster);
            copy(center, center + dimension, centers[cluster].begin());
        }
        return {normalized_score(total_cost_), total_cost_, total_cost_, score_scale_, partition_.labels(), move(centers), move(sizes), statistics_};
    }

    void finalize_result(Result& result) const {
        result.statistics = statistics_;
    }

private:
    struct RelocateMove {
        int point = -1;
        int source = -1;
        int target = -1;
        double source_cost = 0;
        double target_cost = 0;
    } relocate_move_;

    struct SwapMove {
        int point1 = -1;
        int point2 = -1;
        int cluster1 = -1;
        int cluster2 = -1;
        double cluster1_cost = 0;
        double cluster2_cost = 0;
    } swap_move_;

    struct CycleMove {
        int point1 = -1;
        int point2 = -1;
        int point3 = -1;
        int cluster1 = -1;
        int cluster2 = -1;
        int cluster3 = -1;
        double cluster1_cost = 0;
        double cluster2_cost = 0;
        double cluster3_cost = 0;
    } cycle_move_;

    const ClusteringSaProblem* problem_;
    vector<ClusteringSizeRange> ranges_;
    ClusteringSaOptions options_;
    ClusteringPartition partition_;
    Random random_;
    vector<double> coordinate_sums_;
    vector<double> squared_norm_sums_;
    vector<double> cluster_costs_;
    vector<double> centers_;
    double total_cost_ = 0;
    double score_scale_ = 1;
    double current_progress_ = 0;
    int nearby_count_ = 0;
    vector<int> nearby_clusters_;
    vector<int> alternative_cluster_;
    vector<double> assignment_gap_;
    vector<double> point_cost_;
    int accepted_since_refresh_ = 0;
    bool candidates_dirty_ = false;
    bool can_relocate_ = false;
    ClusteringSaStatistics statistics_;
    int pending_type_ = -1;
    double pending_delta_ = 0;
    int rebuild_cluster1_ = -1;
    int rebuild_cluster2_ = -1;
    double rebuild_cluster1_cost_ = 0;
    double rebuild_cluster2_cost_ = 0;
    double rebuild_cluster1_squared_norm_sum_ = 0;
    double rebuild_cluster2_squared_norm_sum_ = 0;
    vector<int> pending_rebuild_points_;
    vector<int> pending_rebuild_labels_;
    vector<double> pending_rebuild_sum1_;
    vector<double> pending_rebuild_sum2_;
    vector<pair<double, int>> neighbor_work_;
    vector<pair<double, int>> point_priority_work_;
    vector<int> rebuild_selected_generation_;
    int rebuild_generation_ = 0;
    vector<pair<double, int>> rebuild_order_;
    vector<double> rebuild_sum1_;
    vector<double> rebuild_sum2_;
    vector<double> rebuild_core_sum1_;
    vector<double> rebuild_core_sum2_;
    vector<double> rebuild_center1_;
    vector<double> rebuild_center2_;
    vector<int> apply_from1_;
    vector<int> apply_from2_;
    int pending_rebuild_cluster_count_ = 0;
    array<int, 3> rebuild_three_clusters_{};
    array<int, 3> rebuild_three_sizes_{};
    array<double, 3> rebuild_three_squared_norm_sums_{};
    array<double, 3> rebuild_three_costs_{};
    vector<double> pending_rebuild_three_sums_;
    vector<double> rebuild_three_sums_;
    vector<double> rebuild_three_core_sums_;
    vector<double> rebuild_three_centers_;
    vector<double> rebuild_three_cost_matrix_;
    vector<double> rebuild_three_dp_current_;
    vector<double> rebuild_three_dp_next_;
    vector<int8_t> rebuild_three_predecessor_;
    int rebuild_two_same_state_streak_ = 0;

    static int checked_cluster_count(const vector<ClusteringSizeRange>& ranges) {
        if (ranges.size() > (size_t)numeric_limits<int>::max()) throw invalid_argument("ClusteringSaState: ranges.size() must fit in int");
        return (int)ranges.size();
    }

    void resolve_options() {
        if (options_.early_cluster_samples == -1) options_.early_cluster_samples = partition_.cluster_count() >= 50 ? 16 : 0;
        if (options_.rebuild_three_same_state_threshold != -1) return;
        bool exact_sizes = true;
        bool free_sizes = true;
        for (auto [lower, upper] : ranges_) {
            exact_sizes &= lower == upper;
            free_sizes &= lower == 1 && upper == problem_->point_count();
        }
        options_.rebuild_three_same_state_threshold = !exact_sizes && !free_sizes ? 3 : 1;
    }

    void check_options() {
        check_clustering_size_ranges(problem_->point_count(), ranges_);
        if (options_.nearby_cluster_count < 0) throw invalid_argument("ClusteringSaState: nearby_cluster_count must be nonnegative");
        if (options_.cluster_samples <= 0 || options_.early_cluster_samples < 0 || options_.point_samples <= 0 || options_.early_point_samples < 0) throw invalid_argument("ClusteringSaState: cluster and point sample counts are invalid");
        if (options_.swap_partner_samples <= 0 || options_.cycle_partner_samples <= 0) throw invalid_argument("ClusteringSaState: partner sample counts must be positive");
        if (options_.candidate_refresh_interval <= 0) throw invalid_argument("ClusteringSaState: candidate_refresh_interval must be positive");
        if (options_.rebuild_point_limit < 2 || options_.rebuild_three_point_limit < 3 || options_.rebuild_iterations <= 0) throw invalid_argument("ClusteringSaState: rebuild limits are too small");
        if (!isfinite(options_.rebuild_three_probability) || options_.rebuild_three_probability < 0 || options_.rebuild_three_probability > 1) throw invalid_argument("ClusteringSaState: rebuild_three_probability must be in [0, 1]");
        if (options_.rebuild_three_same_state_threshold < 0) throw invalid_argument("ClusteringSaState: rebuild_three_same_state_threshold must be -1 or nonnegative");
        if (!isfinite(options_.score_scale) || options_.score_scale < 0) throw invalid_argument("ClusteringSaState: score_scale must be finite and nonnegative");
        if (!isfinite(options_.uniform_selection_probability) || options_.uniform_selection_probability < 0 || options_.uniform_selection_probability > 1) throw invalid_argument("ClusteringSaState: uniform_selection_probability must be in [0, 1]");
        if (!isfinite(options_.middle_phase_start) || !isfinite(options_.late_phase_start) || options_.middle_phase_start < 0 || options_.middle_phase_start > options_.late_phase_start || options_.late_phase_start > 1) throw invalid_argument("ClusteringSaState: phase starts must satisfy 0 <= middle <= late <= 1");
        check_weights(options_.early_weights);
        check_weights(options_.middle_weights);
        check_weights(options_.late_weights);
        for (auto [lower, upper] : ranges_) if (lower < upper) can_relocate_ = true;
    }

    void check_weights(const ClusteringNeighborhoodWeights& weights) const {
        array<int, 4> values = {weights.relocate, weights.swap, weights.cycle, weights.rebuild};
        long long total = 0;
        for (int value : values) {
            if (value < 0) throw invalid_argument("ClusteringSaState: neighborhood weights must be nonnegative");
            total += value;
        }
        if (total > numeric_limits<int>::max()) throw invalid_argument("ClusteringSaState: neighborhood weight sum is too large");
    }

    void check_initial_partition() const {
        for (int cluster = 0; cluster < partition_.cluster_count(); ++cluster) {
            int size = partition_.cluster_size(cluster);
            if (size < ranges_[cluster].lower || size > ranges_[cluster].upper) throw invalid_argument("ClusteringSaState: initial labels violate cluster size ranges");
        }
    }

    void initialize_aggregates() {
        int cluster_count = partition_.cluster_count();
        int dimension = problem_->dimension();
        coordinate_sums_.assign((size_t)cluster_count * dimension, 0);
        squared_norm_sums_.assign(cluster_count, 0);
        cluster_costs_.assign(cluster_count, 0);
        centers_.assign((size_t)cluster_count * dimension, 0);
        for (int point = 0; point < problem_->point_count(); ++point) {
            int cluster = partition_.label(point);
            const double* coordinates = problem_->point_data(point);
            double* sum = sum_data(cluster);
            for (int axis = 0; axis < dimension; ++axis) sum[axis] += coordinates[axis];
            squared_norm_sums_[cluster] += problem_->squared_norm(point);
        }
        total_cost_ = 0;
        for (int cluster = 0; cluster < cluster_count; ++cluster) {
            update_center(cluster);
            cluster_costs_[cluster] = calculate_cluster_cost(cluster);
            total_cost_ += cluster_costs_[cluster];
        }
        if (!isfinite(total_cost_)) throw overflow_error("ClusteringSaState: initial cost must be finite");
    }

    void initialize_score_scale() {
        if (options_.score_scale > 0) score_scale_ = options_.score_scale;
        else score_scale_ = total_cost_ > 0 ? total_cost_ / problem_->point_count() : 1;
        if (!isfinite(score_scale_) || score_scale_ <= 0) throw overflow_error("ClusteringSaState: score scale must be finite and positive");
    }

    double normalized_score(double cost) const {
        return cost / score_scale_;
    }

    void reserve_workspaces() {
        int cluster_count = partition_.cluster_count();
        int dimension = problem_->dimension();
        nearby_count_ = min(options_.nearby_cluster_count, max(0, cluster_count - 1));
        nearby_clusters_.resize((size_t)cluster_count * nearby_count_);
        alternative_cluster_.resize(problem_->point_count(), -1);
        assignment_gap_.resize(problem_->point_count(), numeric_limits<double>::infinity());
        point_cost_.resize(problem_->point_count());
        neighbor_work_.reserve(max(0, cluster_count - 1));
        point_priority_work_.reserve(problem_->point_count());
        rebuild_selected_generation_.resize(problem_->point_count());
        int rebuild_limit = min(options_.rebuild_point_limit, problem_->point_count());
        int rebuild_three_limit = min(options_.rebuild_three_point_limit, problem_->point_count());
        int rebuild_capacity = max(rebuild_limit, rebuild_three_limit);
        pending_rebuild_points_.reserve(rebuild_capacity);
        pending_rebuild_labels_.reserve(rebuild_capacity);
        rebuild_order_.reserve(rebuild_limit);
        rebuild_sum1_.resize(dimension);
        rebuild_sum2_.resize(dimension);
        rebuild_core_sum1_.resize(dimension);
        rebuild_core_sum2_.resize(dimension);
        rebuild_center1_.resize(dimension);
        rebuild_center2_.resize(dimension);
        pending_rebuild_sum1_.resize(dimension);
        pending_rebuild_sum2_.resize(dimension);
        apply_from1_.reserve(rebuild_limit);
        apply_from2_.reserve(rebuild_limit);
        pending_rebuild_three_sums_.resize((size_t)3 * dimension);
        rebuild_three_sums_.resize((size_t)3 * dimension);
        rebuild_three_core_sums_.resize((size_t)3 * dimension);
        rebuild_three_centers_.resize((size_t)3 * dimension);
        rebuild_three_cost_matrix_.reserve((size_t)rebuild_three_limit * 3);
    }

    double* sum_data(int cluster) { return coordinate_sums_.data() + (size_t)cluster * problem_->dimension(); }
    const double* sum_data(int cluster) const { return coordinate_sums_.data() + (size_t)cluster * problem_->dimension(); }
    double* center_data(int cluster) { return centers_.data() + (size_t)cluster * problem_->dimension(); }
    const double* center_data(int cluster) const { return centers_.data() + (size_t)cluster * problem_->dimension(); }

    void update_center(int cluster) {
        int count = partition_.cluster_size(cluster);
        if (count <= 0) throw logic_error("ClusteringSaState: empty clusters are not supported");
        const double* sum = sum_data(cluster);
        double* center = center_data(cluster);
        for (int axis = 0; axis < problem_->dimension(); ++axis) center[axis] = sum[axis] / count;
    }

    double calculate_cost_from_values(const double* sum, double squared_norm_sum, int count) const {
        if (count <= 0) throw logic_error("ClusteringSaState: cluster count must be positive");
        double center_term = 0;
        for (int axis = 0; axis < problem_->dimension(); ++axis) center_term += sum[axis] * sum[axis];
        double cost = squared_norm_sum - center_term / count;
        if (!isfinite(cost)) throw overflow_error("ClusteringSaState: cluster cost must be finite");
        return cost;
    }

    double calculate_cluster_cost(int cluster) const {
        return calculate_cost_from_values(sum_data(cluster), squared_norm_sums_[cluster], partition_.cluster_size(cluster));
    }

    double calculate_changed_cost(int cluster, int count_change, int remove1, int remove2, int add1, int add2) const {
        int count = partition_.cluster_size(cluster) + count_change;
        if (count <= 0) throw logic_error("ClusteringSaState: changed cluster count must be positive");
        double squared_norm_sum = squared_norm_sums_[cluster];
        if (remove1 != -1) squared_norm_sum -= problem_->squared_norm(remove1);
        if (remove2 != -1) squared_norm_sum -= problem_->squared_norm(remove2);
        if (add1 != -1) squared_norm_sum += problem_->squared_norm(add1);
        if (add2 != -1) squared_norm_sum += problem_->squared_norm(add2);
        const double* old_sum = sum_data(cluster);
        double center_term = 0;
        for (int axis = 0; axis < problem_->dimension(); ++axis) {
            double value = old_sum[axis];
            if (remove1 != -1) value -= problem_->coordinate(remove1, axis);
            if (remove2 != -1) value -= problem_->coordinate(remove2, axis);
            if (add1 != -1) value += problem_->coordinate(add1, axis);
            if (add2 != -1) value += problem_->coordinate(add2, axis);
            center_term += value * value;
        }
        double cost = squared_norm_sum - center_term / count;
        if (!isfinite(cost)) throw overflow_error("ClusteringSaState: changed cluster cost must be finite");
        return cost;
    }

    double squared_center_distance(int cluster1, int cluster2) const {
        const double* center1 = center_data(cluster1);
        const double* center2 = center_data(cluster2);
        double distance = 0;
        for (int axis = 0; axis < problem_->dimension(); ++axis) {
            double difference = center1[axis] - center2[axis];
            distance += difference * difference;
        }
        if (!isfinite(distance)) throw overflow_error("ClusteringSaState: squared center distance must be finite");
        return distance;
    }

    void refresh_candidates() {
        int point_count = problem_->point_count();
        int cluster_count = partition_.cluster_count();
        if (nearby_count_ > 0) for (int cluster = 0; cluster < cluster_count; ++cluster) {
            neighbor_work_.clear();
            for (int other = 0; other < cluster_count; ++other) if (other != cluster) neighbor_work_.emplace_back(squared_center_distance(cluster, other), other);
            auto less_neighbor = [](const pair<double, int>& a, const pair<double, int>& b) {
                if (a.first < b.first) return true;
                if (b.first < a.first) return false;
                return a.second < b.second;
            };
            if (nearby_count_ < cluster_count - 1) nth_element(neighbor_work_.begin(), neighbor_work_.begin() + nearby_count_, neighbor_work_.end(), less_neighbor);
            sort(neighbor_work_.begin(), neighbor_work_.begin() + nearby_count_, less_neighbor);
            for (int index = 0; index < nearby_count_; ++index) nearby_clusters_[(size_t)cluster * nearby_count_ + index] = neighbor_work_[index].second;
        }
        neighbor_work_.clear();
        for (int point = 0; point < point_count; ++point) {
            int current = partition_.label(point);
            double current_cost = problem_->squared_distance_to_center(point, center_data(current));
            int alternative = -1;
            double alternative_cost = numeric_limits<double>::infinity();
            for (int index = 0; index < nearby_count_; ++index) {
                int cluster = nearby_clusters_[(size_t)current * nearby_count_ + index];
                double cost = problem_->squared_distance_to_center(point, center_data(cluster));
                if (cost < alternative_cost || (cost == alternative_cost && cluster < alternative)) {
                    alternative = cluster;
                    alternative_cost = cost;
                }
            }
            point_cost_[point] = current_cost;
            alternative_cluster_[point] = alternative;
            assignment_gap_[point] = alternative == -1 ? numeric_limits<double>::infinity() : alternative_cost - current_cost;
        }
        accepted_since_refresh_ = 0;
        candidates_dirty_ = false;
        increment_counter(statistics_.candidate_refreshes);
    }

    double random_unit() {
        return (double)(random_.rand_u64() >> 11) / 9007199254740992.0;
    }

    bool use_uniform_selection() {
        return random_unit() < options_.uniform_selection_probability;
    }

    const ClusteringNeighborhoodWeights& phase_weights(double progress) const {
        if (progress < options_.middle_phase_start) return options_.early_weights;
        if (progress < options_.late_phase_start) return options_.middle_weights;
        return options_.late_weights;
    }

    int select_move_type(double progress) {
        const ClusteringNeighborhoodWeights& weights = phase_weights(progress);
        array<int, 4> values = {weights.relocate, weights.swap, weights.cycle, weights.rebuild};
        int cluster_count = partition_.cluster_count();
        if (cluster_count < 2) values[clustering_relocate] = 0;
        if (!can_relocate_) values[clustering_relocate] = 0;
        if (cluster_count < 2) values[clustering_swap] = values[clustering_rebuild] = 0;
        if (cluster_count < 3) values[clustering_cycle] = 0;
        int total = accumulate(values.begin(), values.end(), 0);
        if (total <= 0) return -1;
        int target = random_.randrange(total);
        for (int type = 0; type < clustering_move_type_count; ++type) {
            if (target < values[type]) return type;
            target -= values[type];
        }
        throw logic_error("ClusteringSaState: failed to select a move type");
    }

    int select_source_cluster(bool require_removal_space, bool uniform) {
        int cluster_count = partition_.cluster_count();
        auto feasible = [&](int cluster) {
            return !require_removal_space || partition_.cluster_size(cluster) > ranges_[cluster].lower;
        };
        if (uniform) {
            int selected = -1;
            int feasible_count = 0;
            for (int cluster = 0; cluster < cluster_count; ++cluster) if (feasible(cluster)) {
                ++feasible_count;
                if (random_.randrange(feasible_count) == 0) selected = cluster;
            }
            return selected;
        } else {
            int configured_samples = current_progress_ < options_.middle_phase_start && options_.early_cluster_samples > 0 ? options_.early_cluster_samples : options_.cluster_samples;
            int samples = min(configured_samples, cluster_count);
            int best = -1;
            double best_value = -numeric_limits<double>::infinity();
            for (int attempt = 0; attempt < samples; ++attempt) {
                int cluster = samples == cluster_count ? attempt : random_.randrange(cluster_count);
                if (!feasible(cluster)) continue;
                double value = cluster_costs_[cluster] / partition_.cluster_size(cluster);
                if (best == -1 || best_value < value) {
                    best = cluster;
                    best_value = value;
                }
            }
            if (best != -1) return best;
        }
        for (int cluster = 0; cluster < cluster_count; ++cluster) if (feasible(cluster)) return cluster;
        return -1;
    }

    int select_point(int cluster, int target_cluster, bool uniform) {
        const vector<int>& members = partition_.members(cluster);
        if (members.empty()) return -1;
        if (uniform) return members[random_.randrange(members.size())];
        bool prefer_high_cost = target_cluster == -1 && random_.randint(1) == 0;
        int best = -1;
        double best_value = prefer_high_cost ? -numeric_limits<double>::infinity() : numeric_limits<double>::infinity();
        int configured_samples = current_progress_ < options_.middle_phase_start && options_.early_point_samples > 0 ? options_.early_point_samples : options_.point_samples;
        int samples = min(configured_samples, (int)members.size());
        for (int attempt = 0; attempt < samples; ++attempt) {
            int point = samples == (int)members.size() ? members[attempt] : members[random_.randrange(members.size())];
            double value;
            if (target_cluster != -1) value = problem_->squared_distance_to_center(point, center_data(target_cluster)) - problem_->squared_distance_to_center(point, center_data(cluster));
            else value = prefer_high_cost ? point_cost_[point] : assignment_gap_[point];
            if ((prefer_high_cost && best_value < value) || (!prefer_high_cost && value < best_value) || best == -1) {
                best = point;
                best_value = value;
            }
        }
        return best;
    }

    int select_target_cluster(int source, int point, int forbidden, bool require_addition_space, bool uniform) {
        int cluster_count = partition_.cluster_count();
        auto feasible = [&](int cluster) {
            if (cluster == source || cluster == forbidden) return false;
            return !require_addition_space || partition_.cluster_size(cluster) < ranges_[cluster].upper;
        };
        if (uniform) {
            int selected = -1;
            int feasible_count = 0;
            for (int cluster = 0; cluster < cluster_count; ++cluster) if (feasible(cluster)) {
                ++feasible_count;
                if (random_.randrange(feasible_count) == 0) selected = cluster;
            }
            return selected;
        }
        int alternative = alternative_cluster_[point];
        if (alternative != -1 && feasible(alternative)) return alternative;
        if (nearby_count_ > 0) {
            int begin = random_.randrange(nearby_count_);
            for (int step = 0; step < nearby_count_; ++step) {
                int index = (begin + step) % nearby_count_;
                int cluster = nearby_clusters_[(size_t)source * nearby_count_ + index];
                if (feasible(cluster)) return cluster;
            }
        }
        int begin = random_.randrange(cluster_count);
        for (int step = 0; step < cluster_count; ++step) {
            int cluster = (begin + step) % cluster_count;
            if (feasible(cluster)) return cluster;
        }
        return -1;
    }

    bool propose_relocate() {
        bool uniform = use_uniform_selection();
        int source = select_source_cluster(true, uniform);
        if (source == -1) return false;
        int point = select_point(source, -1, uniform);
        if (point == -1) return false;
        int target = select_target_cluster(source, point, -1, true, uniform);
        if (target == -1) return false;
        double source_cost = calculate_changed_cost(source, -1, point, -1, -1, -1);
        double target_cost = calculate_changed_cost(target, 1, -1, -1, point, -1);
        relocate_move_ = {point, source, target, source_cost, target_cost};
        pending_type_ = clustering_relocate;
        pending_delta_ = source_cost + target_cost - cluster_costs_[source] - cluster_costs_[target];
        return isfinite(pending_delta_);
    }

    bool evaluate_swap(int point1, int point2, SwapMove& move, double& delta) const {
        int cluster1 = partition_.label(point1);
        int cluster2 = partition_.label(point2);
        if (cluster1 == cluster2) return false;
        double cluster1_cost = calculate_changed_cost(cluster1, 0, point1, -1, point2, -1);
        double cluster2_cost = calculate_changed_cost(cluster2, 0, point2, -1, point1, -1);
        delta = cluster1_cost + cluster2_cost - cluster_costs_[cluster1] - cluster_costs_[cluster2];
        move = {point1, point2, cluster1, cluster2, cluster1_cost, cluster2_cost};
        return isfinite(delta);
    }

    bool propose_swap() {
        bool uniform = use_uniform_selection();
        int cluster1 = select_source_cluster(false, uniform);
        if (cluster1 == -1) return false;
        int point1 = select_point(cluster1, -1, uniform);
        if (point1 == -1) return false;
        int cluster2 = select_target_cluster(cluster1, point1, -1, false, uniform);
        if (cluster2 == -1) return false;
        if (uniform) {
            int point2 = select_point(cluster2, cluster1, true);
            if (point2 == -1) return false;
            if (!evaluate_swap(point1, point2, swap_move_, pending_delta_)) return false;
        } else {
            const vector<int>& members = partition_.members(cluster2);
            if (members.empty()) return false;
            bool found = false;
            SwapMove best_move;
            double best_delta = numeric_limits<double>::infinity();
            int samples = min(options_.swap_partner_samples, (int)members.size());
            for (int attempt = 0; attempt < samples; ++attempt) {
                int point2 = samples == (int)members.size() ? members[attempt] : members[random_.randrange(members.size())];
                SwapMove move;
                double delta;
                if (!evaluate_swap(point1, point2, move, delta)) continue;
                if (!found || delta < best_delta) {
                    found = true;
                    best_move = move;
                    best_delta = delta;
                }
            }
            if (!found) return false;
            swap_move_ = best_move;
            pending_delta_ = best_delta;
        }
        pending_type_ = clustering_swap;
        return true;
    }

    bool evaluate_cycle(int point1, int point2, int point3, CycleMove& move, double& delta) const {
        int cluster1 = partition_.label(point1);
        int cluster2 = partition_.label(point2);
        int cluster3 = partition_.label(point3);
        if (cluster1 == cluster2 || cluster2 == cluster3 || cluster3 == cluster1) return false;
        double cluster1_cost = calculate_changed_cost(cluster1, 0, point1, -1, point3, -1);
        double cluster2_cost = calculate_changed_cost(cluster2, 0, point2, -1, point1, -1);
        double cluster3_cost = calculate_changed_cost(cluster3, 0, point3, -1, point2, -1);
        delta = cluster1_cost + cluster2_cost + cluster3_cost - cluster_costs_[cluster1] - cluster_costs_[cluster2] - cluster_costs_[cluster3];
        move = {point1, point2, point3, cluster1, cluster2, cluster3, cluster1_cost, cluster2_cost, cluster3_cost};
        return isfinite(delta);
    }

    bool propose_cycle() {
        bool uniform = use_uniform_selection();
        int cluster1 = select_source_cluster(false, uniform);
        if (cluster1 == -1) return false;
        int point1 = select_point(cluster1, -1, uniform);
        if (point1 == -1) return false;
        int cluster2 = select_target_cluster(cluster1, point1, -1, false, uniform);
        if (cluster2 == -1) return false;
        int point2 = select_point(cluster2, -1, uniform);
        if (point2 == -1) return false;
        int cluster3 = select_target_cluster(cluster2, point2, cluster1, false, uniform);
        if (cluster3 == -1) return false;
        const vector<int>& members = partition_.members(cluster3);
        if (members.empty()) return false;
        bool found = false;
        CycleMove best_move;
        double best_delta = numeric_limits<double>::infinity();
        int samples = uniform ? 1 : min(options_.cycle_partner_samples, (int)members.size());
        for (int attempt = 0; attempt < samples; ++attempt) {
            int point3 = samples == (int)members.size() ? members[attempt] : members[random_.randrange(members.size())];
            CycleMove move;
            double delta;
            if (!evaluate_cycle(point1, point2, point3, move, delta)) continue;
            if (!found || delta < best_delta) {
                found = true;
                best_move = move;
                best_delta = delta;
            }
        }
        if (!found) return false;
        cycle_move_ = best_move;
        pending_delta_ = best_delta;
        pending_type_ = clustering_cycle;
        return true;
    }

    void select_rebuild_points(int cluster, int other1, int other2, int take) {
        const vector<int>& members = partition_.members(cluster);
        if (take >= (int)members.size()) {
            pending_rebuild_points_.insert(pending_rebuild_points_.end(), members.begin(), members.end());
            return;
        }
        if (rebuild_generation_ == numeric_limits<int>::max()) {
            fill(rebuild_selected_generation_.begin(), rebuild_selected_generation_.end(), 0);
            rebuild_generation_ = 0;
        }
        ++rebuild_generation_;
        auto less_point = [](const pair<double, int>& a, const pair<double, int>& b) {
            if (a.first < b.first) return true;
            if (b.first < a.first) return false;
            return a.second < b.second;
        };
        auto append_best = [&](int count) {
            if (count <= 0) return;
            if (count < (int)point_priority_work_.size()) nth_element(point_priority_work_.begin(), point_priority_work_.begin() + count, point_priority_work_.end(), less_point);
            sort(point_priority_work_.begin(), point_priority_work_.begin() + count, less_point);
            for (int index = 0; index < count; ++index) {
                int point = point_priority_work_[index].second;
                pending_rebuild_points_.push_back(point);
                rebuild_selected_generation_[point] = rebuild_generation_;
            }
        };
        int boundary_count = (take + 1) / 2;
        point_priority_work_.clear();
        for (int point : members) {
            double own_cost = problem_->squared_distance_to_center(point, center_data(cluster));
            double other_cost = problem_->squared_distance_to_center(point, center_data(other1));
            if (other2 != -1) other_cost = min(other_cost, problem_->squared_distance_to_center(point, center_data(other2)));
            point_priority_work_.emplace_back(other_cost - own_cost, point);
        }
        append_best(boundary_count);
        int distant_count = take - boundary_count;
        point_priority_work_.clear();
        for (int point : members) if (rebuild_selected_generation_[point] != rebuild_generation_) {
            double own_cost = problem_->squared_distance_to_center(point, center_data(cluster));
            point_priority_work_.emplace_back(-own_cost, point);
        }
        append_best(distant_count);
        point_priority_work_.clear();
    }

    void copy_point_to_center(int point, vector<double>& center) {
        const double* coordinates = problem_->point_data(point);
        copy(coordinates, coordinates + problem_->dimension(), center.begin());
    }

    array<int, 3> calculate_rebuild_three_takes(const array<int, 3>& sizes, int budget) const {
        array<int, 3> takes{1, 1, 1};
        int total_size = sizes[0] + sizes[1] + sizes[2];
        for (int used = 3; used < budget; ++used) {
            int best = -1;
            long double best_gap = -numeric_limits<long double>::infinity();
            for (int index = 0; index < 3; ++index) if (takes[index] < sizes[index]) {
                long double desired = (long double)budget * sizes[index] / total_size;
                long double gap = desired - takes[index];
                if (best == -1 || best_gap < gap) {
                    best = index;
                    best_gap = gap;
                }
            }
            if (best == -1) throw logic_error("ClusteringSaState: failed to distribute the three-cluster rebuild budget");
            ++takes[best];
        }
        return takes;
    }

    bool assign_rebuild_three(const array<int, 3>& takes) {
        int selected_count = pending_rebuild_points_.size();
        size_t width2 = (size_t)takes[1] + 1;
        size_t width1 = (size_t)takes[0] + 1;
        if (width1 > rebuild_three_dp_current_.max_size() / width2) throw length_error("ClusteringSaState: three-cluster rebuild workspace is too large");
        size_t state_count = width1 * width2;
        if ((size_t)selected_count + 1 > rebuild_three_predecessor_.max_size() / state_count) throw length_error("ClusteringSaState: three-cluster rebuild history is too large");
        rebuild_three_cost_matrix_.resize((size_t)selected_count * 3);
        for (int point_index = 0; point_index < selected_count; ++point_index) {
            int point = pending_rebuild_points_[point_index];
            for (int cluster_index = 0; cluster_index < 3; ++cluster_index) {
                const double* center = rebuild_three_centers_.data() + (size_t)cluster_index * problem_->dimension();
                rebuild_three_cost_matrix_[(size_t)point_index * 3 + cluster_index] = problem_->squared_distance_to_center(point, center);
            }
        }
        double infinity = numeric_limits<double>::infinity();
        rebuild_three_dp_current_.assign(state_count, infinity);
        rebuild_three_dp_next_.resize(state_count);
        rebuild_three_predecessor_.assign(((size_t)selected_count + 1) * state_count, -1);
        rebuild_three_dp_current_[0] = 0;
        for (int point_index = 0; point_index < selected_count; ++point_index) {
            fill(rebuild_three_dp_next_.begin(), rebuild_three_dp_next_.end(), infinity);
            int maximum_first = min(takes[0], point_index);
            for (int first_count = 0; first_count <= maximum_first; ++first_count) {
                int minimum_second = max(0, point_index - first_count - takes[2]);
                int maximum_second = min(takes[1], point_index - first_count);
                for (int second_count = minimum_second; second_count <= maximum_second; ++second_count) {
                    size_t source = (size_t)first_count * width2 + second_count;
                    double base_cost = rebuild_three_dp_current_[source];
                    if (!isfinite(base_cost)) continue;
                    int third_count = point_index - first_count - second_count;
                    auto relax = [&](int cluster_index, int next_first, int next_second) {
                        size_t destination = (size_t)next_first * width2 + next_second;
                        double candidate = base_cost + rebuild_three_cost_matrix_[(size_t)point_index * 3 + cluster_index];
                        if (candidate < rebuild_three_dp_next_[destination]) {
                            rebuild_three_dp_next_[destination] = candidate;
                            rebuild_three_predecessor_[((size_t)point_index + 1) * state_count + destination] = (int8_t)cluster_index;
                        }
                    };
                    if (first_count < takes[0]) relax(0, first_count + 1, second_count);
                    if (second_count < takes[1]) relax(1, first_count, second_count + 1);
                    if (third_count < takes[2]) relax(2, first_count, second_count);
                }
            }
            rebuild_three_dp_current_.swap(rebuild_three_dp_next_);
        }
        int first_count = takes[0];
        int second_count = takes[1];
        size_t final_state = (size_t)first_count * width2 + second_count;
        if (!isfinite(rebuild_three_dp_current_[final_state])) return false;
        pending_rebuild_labels_.resize(selected_count);
        for (int point_index = selected_count; point_index > 0; --point_index) {
            size_t state = (size_t)first_count * width2 + second_count;
            int cluster_index = rebuild_three_predecessor_[(size_t)point_index * state_count + state];
            if (cluster_index < 0 || cluster_index >= 3) throw logic_error("ClusteringSaState: failed to restore a three-cluster assignment");
            pending_rebuild_labels_[point_index - 1] = rebuild_three_clusters_[cluster_index];
            if (cluster_index == 0) --first_count;
            else if (cluster_index == 1) --second_count;
        }
        if (first_count != 0 || second_count != 0) throw logic_error("ClusteringSaState: invalid three-cluster assignment counts");
        return true;
    }

    bool propose_rebuild_three() {
        bool uniform = use_uniform_selection();
        int cluster1 = select_source_cluster(false, uniform);
        if (cluster1 == -1) return false;
        int point1 = select_point(cluster1, -1, uniform);
        if (point1 == -1) return false;
        int cluster2 = select_target_cluster(cluster1, point1, -1, false, uniform);
        if (cluster2 == -1) return false;
        int point2 = select_point(cluster2, -1, uniform);
        if (point2 == -1) return false;
        int cluster3 = select_target_cluster(cluster2, point2, cluster1, false, uniform);
        if (cluster3 == -1) return false;
        rebuild_three_clusters_ = {cluster1, cluster2, cluster3};
        rebuild_three_sizes_ = {partition_.cluster_size(cluster1), partition_.cluster_size(cluster2), partition_.cluster_size(cluster3)};
        int total_size = rebuild_three_sizes_[0] + rebuild_three_sizes_[1] + rebuild_three_sizes_[2];
        int budget = min(options_.rebuild_three_point_limit, total_size);
        if (budget < 3) return false;
        array<int, 3> takes = calculate_rebuild_three_takes(rebuild_three_sizes_, budget);
        pending_rebuild_points_.clear();
        pending_rebuild_labels_.clear();
        select_rebuild_points(cluster1, cluster2, cluster3, takes[0]);
        select_rebuild_points(cluster2, cluster1, cluster3, takes[1]);
        select_rebuild_points(cluster3, cluster1, cluster2, takes[2]);
        int selected_count = pending_rebuild_points_.size();
        if (selected_count != budget) throw logic_error("ClusteringSaState: selected an invalid number of points for a three-cluster rebuild");
        array<int, 3> seed_indices{-1, -1, -1};
        seed_indices[0] = random_.randrange(selected_count);
        double farthest_distance = -1;
        for (int index = 0; index < selected_count; ++index) if (index != seed_indices[0]) {
            double distance = problem_->squared_distance(pending_rebuild_points_[seed_indices[0]], pending_rebuild_points_[index]);
            if (seed_indices[1] == -1 || farthest_distance < distance) {
                seed_indices[1] = index;
                farthest_distance = distance;
            }
        }
        if (options_.refine_rebuild_seeds && seed_indices[1] != -1) {
            seed_indices[0] = seed_indices[1];
            seed_indices[1] = -1;
            farthest_distance = -1;
            for (int index = 0; index < selected_count; ++index) if (index != seed_indices[0]) {
                double distance = problem_->squared_distance(pending_rebuild_points_[seed_indices[0]], pending_rebuild_points_[index]);
                if (seed_indices[1] == -1 || farthest_distance < distance) {
                    seed_indices[1] = index;
                    farthest_distance = distance;
                }
            }
        }
        double largest_nearest_distance = -1;
        for (int index = 0; index < selected_count; ++index) if (index != seed_indices[0] && index != seed_indices[1]) {
            double distance1 = problem_->squared_distance(pending_rebuild_points_[seed_indices[0]], pending_rebuild_points_[index]);
            double distance2 = problem_->squared_distance(pending_rebuild_points_[seed_indices[1]], pending_rebuild_points_[index]);
            double nearest_distance = min(distance1, distance2);
            if (seed_indices[2] == -1 || largest_nearest_distance < nearest_distance) {
                seed_indices[2] = index;
                largest_nearest_distance = nearest_distance;
            }
        }
        if (seed_indices[1] == -1 || seed_indices[2] == -1) return false;
        int dimension = problem_->dimension();
        for (int cluster_index = 0; cluster_index < 3; ++cluster_index) {
            const double* coordinates = problem_->point_data(pending_rebuild_points_[seed_indices[cluster_index]]);
            copy(coordinates, coordinates + dimension, rebuild_three_centers_.data() + (size_t)cluster_index * dimension);
            copy(sum_data(rebuild_three_clusters_[cluster_index]), sum_data(rebuild_three_clusters_[cluster_index]) + dimension, rebuild_three_core_sums_.data() + (size_t)cluster_index * dimension);
        }
        array<double, 3> core_squared_norm_sums = {
            squared_norm_sums_[cluster1], squared_norm_sums_[cluster2], squared_norm_sums_[cluster3]
        };
        for (int point : pending_rebuild_points_) {
            int old_cluster = partition_.label(point);
            int cluster_index = old_cluster == cluster1 ? 0 : old_cluster == cluster2 ? 1 : old_cluster == cluster3 ? 2 : -1;
            if (cluster_index == -1) throw logic_error("ClusteringSaState: selected a point outside the rebuilt clusters");
            double* core_sum = rebuild_three_core_sums_.data() + (size_t)cluster_index * dimension;
            const double* coordinates = problem_->point_data(point);
            for (int axis = 0; axis < dimension; ++axis) core_sum[axis] -= coordinates[axis];
            core_squared_norm_sums[cluster_index] -= problem_->squared_norm(point);
        }
        array<double, 3> final_squared_norm_sums{};
        for (int iteration = 0; iteration < options_.rebuild_iterations; ++iteration) {
            if (!assign_rebuild_three(takes)) return false;
            rebuild_three_sums_ = rebuild_three_core_sums_;
            final_squared_norm_sums = core_squared_norm_sums;
            for (int index = 0; index < selected_count; ++index) {
                int point = pending_rebuild_points_[index];
                int label = pending_rebuild_labels_[index];
                int cluster_index = label == cluster1 ? 0 : label == cluster2 ? 1 : 2;
                double* sum = rebuild_three_sums_.data() + (size_t)cluster_index * dimension;
                const double* coordinates = problem_->point_data(point);
                for (int axis = 0; axis < dimension; ++axis) sum[axis] += coordinates[axis];
                final_squared_norm_sums[cluster_index] += problem_->squared_norm(point);
            }
            for (int cluster_index = 0; cluster_index < 3; ++cluster_index) {
                const double* sum = rebuild_three_sums_.data() + (size_t)cluster_index * dimension;
                double* center = rebuild_three_centers_.data() + (size_t)cluster_index * dimension;
                for (int axis = 0; axis < dimension; ++axis) center[axis] = sum[axis] / rebuild_three_sizes_[cluster_index];
            }
        }
        bool changed_labels = false;
        for (int index = 0; index < selected_count; ++index) if (partition_.label(pending_rebuild_points_[index]) != pending_rebuild_labels_[index]) {
            changed_labels = true;
            break;
        }
        if (!changed_labels) {
            increment_counter(statistics_.rebuild_same_state);
            increment_counter(statistics_.rebuild_three_same_state);
            pending_rebuild_points_.clear();
            pending_rebuild_labels_.clear();
            return false;
        }
        pending_rebuild_three_sums_ = rebuild_three_sums_;
        rebuild_three_squared_norm_sums_ = final_squared_norm_sums;
        pending_delta_ = 0;
        for (int cluster_index = 0; cluster_index < 3; ++cluster_index) {
            const double* sum = pending_rebuild_three_sums_.data() + (size_t)cluster_index * dimension;
            rebuild_three_costs_[cluster_index] = calculate_cost_from_values(sum, rebuild_three_squared_norm_sums_[cluster_index], rebuild_three_sizes_[cluster_index]);
            pending_delta_ += rebuild_three_costs_[cluster_index] - cluster_costs_[rebuild_three_clusters_[cluster_index]];
        }
        pending_rebuild_cluster_count_ = 3;
        pending_type_ = clustering_rebuild;
        return isfinite(pending_delta_);
    }

    bool propose_rebuild() {
        bool uniform = use_uniform_selection();
        int cluster1 = select_source_cluster(false, uniform);
        if (cluster1 == -1) return false;
        int point = select_point(cluster1, -1, uniform);
        if (point == -1) return false;
        int cluster2 = select_target_cluster(cluster1, point, -1, false, uniform);
        if (cluster2 == -1) return false;
        int size1 = partition_.cluster_size(cluster1);
        int size2 = partition_.cluster_size(cluster2);
        int total_size = size1 + size2;
        int budget = min(options_.rebuild_point_limit, total_size);
        if (budget < 2) return false;
        int take1;
        int take2;
        if (budget == total_size) {
            take1 = size1;
            take2 = size2;
        } else {
            take1 = max(1, min(size1, (int)((long long)budget * size1 / total_size)));
            take2 = budget - take1;
            if (take2 <= 0) {
                take2 = 1;
                --take1;
            }
            if (take2 > size2) {
                take2 = size2;
                take1 = budget - take2;
            }
            if (take1 <= 0 || take1 > size1 || take2 <= 0 || take2 > size2) return false;
        }
        pending_rebuild_points_.clear();
        pending_rebuild_labels_.clear();
        select_rebuild_points(cluster1, cluster2, -1, take1);
        select_rebuild_points(cluster2, cluster1, -1, take2);
        int selected_count = pending_rebuild_points_.size();
        if (selected_count < 2) return false;
        int first_index = random_.randrange(selected_count);
        int first_point = pending_rebuild_points_[first_index];
        int second_point = -1;
        double farthest_distance = -1;
        for (int candidate : pending_rebuild_points_) {
            if (candidate == first_point) continue;
            double distance = problem_->squared_distance(first_point, candidate);
            if (second_point == -1 || farthest_distance < distance) {
                second_point = candidate;
                farthest_distance = distance;
            }
        }
        if (options_.refine_rebuild_seeds && second_point != -1) {
            first_point = second_point;
            second_point = -1;
            farthest_distance = -1;
            for (int candidate : pending_rebuild_points_) {
                if (candidate == first_point) continue;
                double distance = problem_->squared_distance(first_point, candidate);
                if (second_point == -1 || farthest_distance < distance) {
                    second_point = candidate;
                    farthest_distance = distance;
                }
            }
        }
        if (second_point == -1) return false;
        copy_point_to_center(first_point, rebuild_center1_);
        copy_point_to_center(second_point, rebuild_center2_);
        copy(sum_data(cluster1), sum_data(cluster1) + problem_->dimension(), rebuild_sum1_.begin());
        copy(sum_data(cluster2), sum_data(cluster2) + problem_->dimension(), rebuild_sum2_.begin());
        double core_squared_norm1 = squared_norm_sums_[cluster1];
        double core_squared_norm2 = squared_norm_sums_[cluster2];
        for (int selected : pending_rebuild_points_) {
            int old_cluster = partition_.label(selected);
            vector<double>& sum = old_cluster == cluster1 ? rebuild_sum1_ : rebuild_sum2_;
            for (int axis = 0; axis < problem_->dimension(); ++axis) sum[axis] -= problem_->coordinate(selected, axis);
            if (old_cluster == cluster1) {
                core_squared_norm1 -= problem_->squared_norm(selected);
            } else {
                core_squared_norm2 -= problem_->squared_norm(selected);
            }
        }
        rebuild_core_sum1_ = rebuild_sum1_;
        rebuild_core_sum2_ = rebuild_sum2_;
        pending_rebuild_labels_.resize(selected_count);
        rebuild_order_.resize(selected_count);
        double final_squared_norm1 = 0;
        double final_squared_norm2 = 0;
        for (int iteration = 0; iteration < options_.rebuild_iterations; ++iteration) {
            for (int index = 0; index < selected_count; ++index) {
                int selected = pending_rebuild_points_[index];
                double difference = problem_->squared_distance_to_center(selected, rebuild_center1_.data()) - problem_->squared_distance_to_center(selected, rebuild_center2_.data());
                rebuild_order_[index] = {difference, index};
            }
            auto less_assignment = [&](const pair<double, int>& a, const pair<double, int>& b) {
                if (a.first < b.first) return true;
                if (b.first < a.first) return false;
                return pending_rebuild_points_[a.second] < pending_rebuild_points_[b.second];
            };
            nth_element(rebuild_order_.begin(), rebuild_order_.begin() + take1, rebuild_order_.end(), less_assignment);
            fill(pending_rebuild_labels_.begin(), pending_rebuild_labels_.end(), cluster2);
            for (int rank = 0; rank < take1; ++rank) pending_rebuild_labels_[rebuild_order_[rank].second] = cluster1;
            rebuild_sum1_ = rebuild_core_sum1_;
            rebuild_sum2_ = rebuild_core_sum2_;
            final_squared_norm1 = core_squared_norm1;
            final_squared_norm2 = core_squared_norm2;
            for (int index = 0; index < selected_count; ++index) {
                int selected = pending_rebuild_points_[index];
                vector<double>& sum = pending_rebuild_labels_[index] == cluster1 ? rebuild_sum1_ : rebuild_sum2_;
                for (int axis = 0; axis < problem_->dimension(); ++axis) sum[axis] += problem_->coordinate(selected, axis);
                if (pending_rebuild_labels_[index] == cluster1) final_squared_norm1 += problem_->squared_norm(selected);
                else final_squared_norm2 += problem_->squared_norm(selected);
            }
            for (int axis = 0; axis < problem_->dimension(); ++axis) {
                rebuild_center1_[axis] = rebuild_sum1_[axis] / size1;
                rebuild_center2_[axis] = rebuild_sum2_[axis] / size2;
            }
        }
        bool changed_labels = false;
        for (int index = 0; index < selected_count; ++index) if (partition_.label(pending_rebuild_points_[index]) != pending_rebuild_labels_[index]) {
            changed_labels = true;
            break;
        }
        if (!changed_labels) {
            increment_counter(statistics_.rebuild_same_state);
            if (rebuild_two_same_state_streak_ < numeric_limits<int>::max()) ++rebuild_two_same_state_streak_;
            pending_rebuild_points_.clear();
            pending_rebuild_labels_.clear();
            rebuild_order_.clear();
            return false;
        }
        rebuild_cluster1_ = cluster1;
        rebuild_cluster2_ = cluster2;
        rebuild_two_same_state_streak_ = 0;
        rebuild_cluster1_squared_norm_sum_ = final_squared_norm1;
        rebuild_cluster2_squared_norm_sum_ = final_squared_norm2;
        pending_rebuild_sum1_ = rebuild_sum1_;
        pending_rebuild_sum2_ = rebuild_sum2_;
        rebuild_cluster1_cost_ = calculate_cost_from_values(pending_rebuild_sum1_.data(), final_squared_norm1, size1);
        rebuild_cluster2_cost_ = calculate_cost_from_values(pending_rebuild_sum2_.data(), final_squared_norm2, size2);
        pending_delta_ = rebuild_cluster1_cost_ + rebuild_cluster2_cost_ - cluster_costs_[cluster1] - cluster_costs_[cluster2];
        pending_rebuild_cluster_count_ = 2;
        pending_type_ = clustering_rebuild;
        rebuild_order_.clear();
        return isfinite(pending_delta_);
    }

    void apply_point_delta_to_sum(int cluster, int point, double sign) {
        double* sum = sum_data(cluster);
        const double* coordinates = problem_->point_data(point);
        for (int axis = 0; axis < problem_->dimension(); ++axis) sum[axis] += sign * coordinates[axis];
        squared_norm_sums_[cluster] += sign * problem_->squared_norm(point);
    }

    void apply_relocate() {
        partition_.move_point(relocate_move_.point, relocate_move_.target);
        apply_point_delta_to_sum(relocate_move_.source, relocate_move_.point, -1);
        apply_point_delta_to_sum(relocate_move_.target, relocate_move_.point, 1);
        total_cost_ += pending_delta_;
        cluster_costs_[relocate_move_.source] = relocate_move_.source_cost;
        cluster_costs_[relocate_move_.target] = relocate_move_.target_cost;
        update_center(relocate_move_.source);
        update_center(relocate_move_.target);
    }

    void apply_swap() {
        partition_.swap_points(swap_move_.point1, swap_move_.point2);
        apply_point_delta_to_sum(swap_move_.cluster1, swap_move_.point1, -1);
        apply_point_delta_to_sum(swap_move_.cluster1, swap_move_.point2, 1);
        apply_point_delta_to_sum(swap_move_.cluster2, swap_move_.point2, -1);
        apply_point_delta_to_sum(swap_move_.cluster2, swap_move_.point1, 1);
        total_cost_ += pending_delta_;
        cluster_costs_[swap_move_.cluster1] = swap_move_.cluster1_cost;
        cluster_costs_[swap_move_.cluster2] = swap_move_.cluster2_cost;
        update_center(swap_move_.cluster1);
        update_center(swap_move_.cluster2);
    }

    void apply_cycle() {
        partition_.cycle_points(cycle_move_.point1, cycle_move_.point2, cycle_move_.point3);
        apply_point_delta_to_sum(cycle_move_.cluster1, cycle_move_.point1, -1);
        apply_point_delta_to_sum(cycle_move_.cluster1, cycle_move_.point3, 1);
        apply_point_delta_to_sum(cycle_move_.cluster2, cycle_move_.point2, -1);
        apply_point_delta_to_sum(cycle_move_.cluster2, cycle_move_.point1, 1);
        apply_point_delta_to_sum(cycle_move_.cluster3, cycle_move_.point3, -1);
        apply_point_delta_to_sum(cycle_move_.cluster3, cycle_move_.point2, 1);
        total_cost_ += pending_delta_;
        cluster_costs_[cycle_move_.cluster1] = cycle_move_.cluster1_cost;
        cluster_costs_[cycle_move_.cluster2] = cycle_move_.cluster2_cost;
        cluster_costs_[cycle_move_.cluster3] = cycle_move_.cluster3_cost;
        update_center(cycle_move_.cluster1);
        update_center(cycle_move_.cluster2);
        update_center(cycle_move_.cluster3);
    }

    void apply_rebuild_two() {
        apply_from1_.clear();
        apply_from2_.clear();
        for (int index = 0; index < (int)pending_rebuild_points_.size(); ++index) {
            int point = pending_rebuild_points_[index];
            int old_cluster = partition_.label(point);
            int new_cluster = pending_rebuild_labels_[index];
            if (old_cluster == rebuild_cluster1_ && new_cluster == rebuild_cluster2_) apply_from1_.push_back(point);
            else if (old_cluster == rebuild_cluster2_ && new_cluster == rebuild_cluster1_) apply_from2_.push_back(point);
            else if (old_cluster != new_cluster) throw logic_error("ClusteringSaState::apply_rebuild: invalid rebuilt label");
        }
        if (apply_from1_.size() != apply_from2_.size()) throw logic_error("ClusteringSaState::apply_rebuild: rebuilt cluster sizes changed");
        for (int index = 0; index < (int)apply_from1_.size(); ++index) partition_.swap_points(apply_from1_[index], apply_from2_[index]);
        copy(pending_rebuild_sum1_.begin(), pending_rebuild_sum1_.end(), sum_data(rebuild_cluster1_));
        copy(pending_rebuild_sum2_.begin(), pending_rebuild_sum2_.end(), sum_data(rebuild_cluster2_));
        squared_norm_sums_[rebuild_cluster1_] = rebuild_cluster1_squared_norm_sum_;
        squared_norm_sums_[rebuild_cluster2_] = rebuild_cluster2_squared_norm_sum_;
        cluster_costs_[rebuild_cluster1_] = rebuild_cluster1_cost_;
        cluster_costs_[rebuild_cluster2_] = rebuild_cluster2_cost_;
        total_cost_ += pending_delta_;
        update_center(rebuild_cluster1_);
        update_center(rebuild_cluster2_);
        apply_from1_.clear();
        apply_from2_.clear();
    }

    void apply_rebuild_three() {
        array<int, 3> old_sizes = {
            partition_.cluster_size(rebuild_three_clusters_[0]),
            partition_.cluster_size(rebuild_three_clusters_[1]),
            partition_.cluster_size(rebuild_three_clusters_[2])
        };
        for (int index = 0; index < (int)pending_rebuild_points_.size(); ++index) {
            int point = pending_rebuild_points_[index];
            int new_cluster = pending_rebuild_labels_[index];
            if (new_cluster != rebuild_three_clusters_[0] && new_cluster != rebuild_three_clusters_[1] && new_cluster != rebuild_three_clusters_[2]) throw logic_error("ClusteringSaState::apply_rebuild_three: invalid rebuilt label");
            partition_.move_point(point, new_cluster);
        }
        for (int cluster_index = 0; cluster_index < 3; ++cluster_index) {
            int cluster = rebuild_three_clusters_[cluster_index];
            if (partition_.cluster_size(cluster) != old_sizes[cluster_index]) throw logic_error("ClusteringSaState::apply_rebuild_three: rebuilt cluster sizes changed");
            const double* sum = pending_rebuild_three_sums_.data() + (size_t)cluster_index * problem_->dimension();
            copy(sum, sum + problem_->dimension(), sum_data(cluster));
            squared_norm_sums_[cluster] = rebuild_three_squared_norm_sums_[cluster_index];
            cluster_costs_[cluster] = rebuild_three_costs_[cluster_index];
            update_center(cluster);
        }
        total_cost_ += pending_delta_;
    }

    void apply_rebuild() {
        if (pending_rebuild_cluster_count_ == 2) apply_rebuild_two();
        else if (pending_rebuild_cluster_count_ == 3) apply_rebuild_three();
        else throw logic_error("ClusteringSaState::apply_rebuild: invalid cluster count");
    }

    void clear_pending() {
        pending_type_ = -1;
        pending_delta_ = 0;
        pending_rebuild_cluster_count_ = 0;
        pending_rebuild_points_.clear();
        pending_rebuild_labels_.clear();
    }

    static void increment_counter(int64_t& counter) {
        if (counter == numeric_limits<int64_t>::max()) throw overflow_error("ClusteringSaState: statistics counter overflow");
        ++counter;
    }
};

inline ClusteringSaResult clustering_sa_from_labels(const ClusteringSaProblem& problem, const vector<int>& initial_labels, const vector<ClusteringSizeRange>& ranges, double time_limit_ms, ClusteringSaOptions options = {}, uint32_t seed = 23, bool verbose = false) {
    Timer timer;
    ClusteringSaState state(problem, initial_labels, ranges, move(options), seed);
    double effective_time_limit = ranges.size() == 1 && isfinite(time_limit_ms) && time_limit_ms >= 0 ? 0 : time_limit_ms;
    double remaining_time = max(0.0, effective_time_limit - timer.elapsed());
    ClusteringSaResult result = sa::sa_run<ClusteringSaState>(remaining_time, state, verbose);
    state.finalize_result(result);
    return result;
}

template<class InitialLabelFactory>
inline ClusteringSaResult clustering_sa_from_label_factory(const ClusteringSaProblem& problem, const vector<ClusteringSizeRange>& ranges, double time_limit_ms, double initialization_time_ratio, InitialLabelFactory initial_label_factory, ClusteringSaOptions options = {}, uint32_t seed = 23, bool verbose = false) {
    using Labels = remove_cvref_t<invoke_result_t<InitialLabelFactory&, uint32_t>>;
    static_assert(is_same_v<Labels, vector<int>>, "clustering_sa_from_label_factory: initial_label_factory must return vector<int>");
    if (!isfinite(time_limit_ms) || time_limit_ms < 0) throw invalid_argument("clustering_sa_from_label_factory: time_limit_ms must be finite and nonnegative");
    if (!isfinite(initialization_time_ratio) || initialization_time_ratio < 0 || initialization_time_ratio > 1) throw invalid_argument("clustering_sa_from_label_factory: initialization_time_ratio must be in [0, 1]");
    Timer timer;
    double initialization_time_limit = time_limit_ms * initialization_time_ratio;
    vector<int> best_labels;
    double best_cost = numeric_limits<double>::infinity();
    int64_t trials = 0;
    do {
        vector<int> labels = initial_label_factory(seed + (uint32_t)trials);
        double cost = clustering_cost_from_labels(problem, labels, ranges);
        if (cost < best_cost) {
            best_cost = cost;
            best_labels = move(labels);
        }
        if (trials == numeric_limits<int64_t>::max()) throw overflow_error("clustering_sa_from_label_factory: trial counter overflow");
        ++trials;
    } while (timer.elapsed() < initialization_time_limit);
    double remaining_time = max(0.0, time_limit_ms - timer.elapsed());
    ClusteringSaResult result = clustering_sa_from_labels(problem, best_labels, ranges, remaining_time, move(options), seed, verbose);
    result.initial_trials = trials;
    return result;
}
}
