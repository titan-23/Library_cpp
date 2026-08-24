/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/clustering/clustering_partition.cpp
#pragma once
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>
using namespace std;
namespace titan23 {
struct ClusteringSizeRange {
    int lower;
    int upper;
};

inline vector<ClusteringSizeRange> make_clustering_size_ranges(int cluster_count, int point_count) {
    if (cluster_count <= 0) throw invalid_argument("make_clustering_size_ranges: cluster_count must be positive");
    if (point_count < cluster_count) throw invalid_argument("make_clustering_size_ranges: point_count must be at least cluster_count");
    return vector<ClusteringSizeRange>(cluster_count, {1, point_count});
}

inline vector<ClusteringSizeRange> make_exact_clustering_size_ranges(const vector<int>& sizes) {
    if (sizes.empty()) throw invalid_argument("make_exact_clustering_size_ranges: sizes must not be empty");
    if (sizes.size() > (size_t)numeric_limits<int>::max()) throw invalid_argument("make_exact_clustering_size_ranges: sizes.size() must fit in int");
    vector<ClusteringSizeRange> ranges;
    ranges.reserve(sizes.size());
    for (int size : sizes) {
        if (size <= 0) throw invalid_argument("make_exact_clustering_size_ranges: each size must be positive");
        ranges.push_back({size, size});
    }
    return ranges;
}

inline void check_clustering_size_ranges(int point_count, const vector<ClusteringSizeRange>& ranges) {
    if (point_count <= 0) throw invalid_argument("clustering: point_count must be positive");
    if (ranges.empty()) throw invalid_argument("clustering: at least one cluster is required");
    if (ranges.size() > (size_t)numeric_limits<int>::max()) throw invalid_argument("clustering: ranges.size() must fit in int");
    long long lower_sum = 0;
    long long upper_sum = 0;
    for (auto [lower, upper] : ranges) {
        if (lower <= 0 || lower > upper) throw invalid_argument("clustering: each size range must satisfy 1 <= lower <= upper");
        if (upper > point_count) throw invalid_argument("clustering: cluster upper bound must not exceed point_count");
        lower_sum += lower;
        upper_sum += upper;
    }
    if (lower_sum > point_count || upper_sum < point_count) throw invalid_argument("clustering: size ranges are infeasible");
}

class ClusteringPartition {
public:
    ClusteringPartition(int point_count, int cluster_count, vector<int> labels)
        : labels_(move(labels)), members_(checked_cluster_size(point_count, cluster_count)), position_in_cluster_((size_t)point_count) {
        if (labels_.size() != (size_t)point_count) throw invalid_argument("ClusteringPartition: labels.size() must equal point_count");
        for (int point = 0; point < point_count; ++point) {
            int cluster = labels_[point];
            if (cluster < 0 || cluster >= cluster_count) throw invalid_argument("ClusteringPartition: label is out of range");
            position_in_cluster_[point] = (int)members_[cluster].size();
            members_[cluster].push_back(point);
        }
    }

    int point_count() const { return (int)labels_.size(); }
    int cluster_count() const { return (int)members_.size(); }
    int label(int point) const {
#ifdef TITAN_DEBUG
        check_point(point);
#endif
        return labels_[point];
    }
    int cluster_size(int cluster) const {
#ifdef TITAN_DEBUG
        check_cluster(cluster);
#endif
        return (int)members_[cluster].size();
    }
    int position_in_cluster(int point) const {
#ifdef TITAN_DEBUG
        check_point(point);
#endif
        return position_in_cluster_[point];
    }
    const vector<int>& labels() const { return labels_; }
    const vector<int>& members(int cluster) const {
#ifdef TITAN_DEBUG
        check_cluster(cluster);
#endif
        return members_[cluster];
    }

    void move_point(int point, int target_cluster) {
#ifdef TITAN_DEBUG
        check_point(point);
        check_cluster(target_cluster);
#endif
        int source_cluster = labels_[point];
        if (source_cluster == target_cluster) return;
        members_[target_cluster].push_back(point);
        int source_position = position_in_cluster_[point];
        int moved_point = members_[source_cluster].back();
        members_[source_cluster][source_position] = moved_point;
        position_in_cluster_[moved_point] = source_position;
        members_[source_cluster].pop_back();
        labels_[point] = target_cluster;
        position_in_cluster_[point] = (int)members_[target_cluster].size() - 1;
    }

    void swap_points(int point1, int point2) {
#ifdef TITAN_DEBUG
        check_point(point1);
        check_point(point2);
#endif
        int cluster1 = labels_[point1];
        int cluster2 = labels_[point2];
        if (cluster1 == cluster2) return;
        int position1 = position_in_cluster_[point1];
        int position2 = position_in_cluster_[point2];
        members_[cluster1][position1] = point2;
        members_[cluster2][position2] = point1;
        labels_[point1] = cluster2;
        labels_[point2] = cluster1;
        position_in_cluster_[point1] = position2;
        position_in_cluster_[point2] = position1;
    }

    void cycle_points(int point1, int point2, int point3) {
#ifdef TITAN_DEBUG
        check_point(point1);
        check_point(point2);
        check_point(point3);
#endif
        int cluster1 = labels_[point1];
        int cluster2 = labels_[point2];
        int cluster3 = labels_[point3];
        if (cluster1 == cluster2 || cluster2 == cluster3 || cluster3 == cluster1) throw invalid_argument("ClusteringPartition::cycle_points: points must belong to distinct clusters");
        int position1 = position_in_cluster_[point1];
        int position2 = position_in_cluster_[point2];
        int position3 = position_in_cluster_[point3];
        members_[cluster1][position1] = point3;
        members_[cluster2][position2] = point1;
        members_[cluster3][position3] = point2;
        labels_[point1] = cluster2;
        labels_[point2] = cluster3;
        labels_[point3] = cluster1;
        position_in_cluster_[point1] = position2;
        position_in_cluster_[point2] = position3;
        position_in_cluster_[point3] = position1;
    }

private:
    vector<int> labels_;
    vector<vector<int>> members_;
    vector<int> position_in_cluster_;

    static size_t checked_cluster_size(int point_count, int cluster_count) {
        if (point_count <= 0) throw invalid_argument("ClusteringPartition: point_count must be positive");
        if (cluster_count <= 0) throw invalid_argument("ClusteringPartition: cluster_count must be positive");
        return (size_t)cluster_count;
    }

    void check_point(int point) const {
        if (point < 0 || point >= point_count()) throw out_of_range("ClusteringPartition: point is out of range");
    }

    void check_cluster(int cluster) const {
        if (cluster < 0 || cluster >= cluster_count()) throw out_of_range("ClusteringPartition: cluster is out of range");
    }
};
}
