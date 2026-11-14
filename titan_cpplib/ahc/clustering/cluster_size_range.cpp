/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/clustering/cluster_size_range.cpp
#pragma once
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>
using namespace std;
namespace titan23 {
/// @brief 1 クラスタの点数の範囲 [lower, upper] / 両端を含む
struct ClusterSizeRange {
    int lower;
    int upper;
};
/// @brief ClusterSizeRange の別名
using ClusteringSizeRange = ClusterSizeRange;

/// @brief 個数自由の範囲を返す / 各クラスタは空でなければよく、範囲は [1, point_count]
inline vector<ClusterSizeRange> make_clustering_size_ranges(int cluster_count, int point_count) {
    if (cluster_count <= 0) throw invalid_argument("make_clustering_size_ranges: cluster_count must be positive");
    if (point_count < cluster_count) throw invalid_argument("make_clustering_size_ranges: point_count must be at least cluster_count");
    return vector<ClusterSizeRange>(cluster_count, {1, point_count});
}

/// @brief 個数固定の範囲を返す / クラスタ i の点数は sizes[i]
inline vector<ClusterSizeRange> make_exact_clustering_size_ranges(const vector<int>& sizes) {
    if (sizes.empty()) throw invalid_argument("make_exact_clustering_size_ranges: sizes must not be empty");
    if (sizes.size() > (size_t)numeric_limits<int>::max()) throw invalid_argument("make_exact_clustering_size_ranges: sizes.size() must fit in int");
    vector<ClusterSizeRange> ranges;
    ranges.reserve(sizes.size());
    for (int size : sizes) {
        if (size <= 0) throw invalid_argument("make_exact_clustering_size_ranges: each size must be positive");
        ranges.push_back({size, size});
    }
    return ranges;
}

/// @brief ranges が clustering_sa の条件を満たすか検査する / 満たさなければ invalid_argument を投げる
/// 条件は 1 <= lower <= upper <= point_count と、下限の和 <= point_count <= 上限の和
inline void check_clustering_size_ranges(int point_count, const vector<ClusterSizeRange>& ranges) {
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
}
