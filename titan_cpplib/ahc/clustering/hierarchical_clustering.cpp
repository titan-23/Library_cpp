/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/clustering/hierarchical_clustering.cpp
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
using namespace std;
namespace titan23 {
enum class HierarchicalLinkage {
    single,
    complete,
    average,
    ward
};

struct HierarchicalMerge {
    int left;
    int right;
    long double distance;
    int size;
};

class HierarchicalClusteringResult {
public:
    int point_count = 0;
    vector<HierarchicalMerge> merges;

    vector<int> labels(int cluster_count) const {
        check_result();
        if (point_count == 0) {
            if (cluster_count != 0) throw invalid_argument("HierarchicalClusteringResult::labels: cluster_count must be zero for an empty result");
            return {};
        }
        if (cluster_count <= 0 || cluster_count > point_count) throw invalid_argument("HierarchicalClusteringResult::labels: cluster_count must be in [1, point_count]");
        return labels_after_merges(point_count - cluster_count);
    }

    vector<int> labels_at_distance(long double maximum_distance) const {
        check_result();
        if (isnan(maximum_distance)) throw invalid_argument("HierarchicalClusteringResult::labels_at_distance: maximum_distance must not be NaN");
        int merge_count = (int)(upper_bound(merges.begin(), merges.end(), maximum_distance, [](long double value, const HierarchicalMerge& merge) {
            return value < merge.distance;
        }) - merges.begin());
        return labels_after_merges(merge_count);
    }

    vector<vector<int>> clusters(int cluster_count) const {
        vector<int> point_labels = labels(cluster_count);
        vector<vector<int>> result((size_t)cluster_count);
        for (int point = 0; point < point_count; ++point) result[point_labels[point]].push_back(point);
        return result;
    }

private:
    class UnionFind {
    public:
        explicit UnionFind(int n) : parent_((size_t)n, -1) {}

        int root(int x) {
            while (parent_[x] >= 0) {
                if (parent_[parent_[x]] >= 0) parent_[x] = parent_[parent_[x]];
                x = parent_[x];
            }
            return x;
        }

        int unite(int x, int y) {
            x = root(x);
            y = root(y);
            if (x == y) return x;
            if (parent_[x] > parent_[y]) swap(x, y);
            parent_[x] += parent_[y];
            parent_[y] = x;
            return x;
        }

    private:
        vector<int> parent_;
    };

    void check_result() const {
        if (point_count < 0) throw logic_error("HierarchicalClusteringResult: point_count is negative");
        int expected = point_count == 0 ? 0 : point_count - 1;
        if ((int)merges.size() != expected) throw logic_error("HierarchicalClusteringResult: merge count is inconsistent");
    }

    vector<int> labels_after_merges(int merge_count) const {
        if (point_count == 0) return {};
        UnionFind uf(point_count);
        vector<int> representative((size_t)(2 * point_count - 1));
        iota(representative.begin(), representative.begin() + point_count, 0);
        for (int i = 0; i < merge_count; ++i) {
            const auto& merge = merges[i];
            int root = uf.unite(representative[merge.left], representative[merge.right]);
            representative[point_count + i] = root;
        }
        vector<int> root_label((size_t)point_count, -1);
        vector<int> result((size_t)point_count);
        int next_label = 0;
        for (int point = 0; point < point_count; ++point) {
            int root = uf.root(point);
            if (root_label[root] == -1) root_label[root] = next_label++;
            result[point] = root_label[root];
        }
        return result;
    }
};

namespace hierarchical_clustering_internal {
class PackedDistances {
public:
    explicit PackedDistances(int n) : values_(checked_size(n)) {}

    long double get(int a, int b) const {
        if (a == b) return 0;
        if (a > b) swap(a, b);
        return values_[index(a, b)];
    }

    void set(int a, int b, long double value) {
        if (a == b) return;
        if (a > b) swap(a, b);
        values_[index(a, b)] = value;
    }

private:
    vector<long double> values_;

    static size_t checked_size(int n) {
        if (n < 0) throw invalid_argument("hierarchical_clustering: point_count must not be negative");
        size_t size = (size_t)n;
        if (size > 0 && size - 1 > numeric_limits<size_t>::max() / size) throw length_error("hierarchical_clustering: distance table is too large");
        return size * (size - 1) / 2;
    }

    static size_t index(int a, int b) {
        return (size_t)b * (b - 1) / 2 + a;
    }
};

struct RawMerge {
    int left;
    int right;
    int size;
    int minimum_point;
    long double distance;
};

inline long double updated_distance(HierarchicalLinkage linkage, long double distance_ac, long double distance_bc,
                                    long double distance_ab, int size_a, int size_b, int size_c) {
    if (linkage == HierarchicalLinkage::single) return min(distance_ac, distance_bc);
    if (linkage == HierarchicalLinkage::complete) return max(distance_ac, distance_bc);
    if (linkage == HierarchicalLinkage::average) return (size_a * distance_ac + size_b * distance_bc) / (size_a + size_b);
    long double numerator = (size_a + size_c) * distance_ac + (size_b + size_c) * distance_bc - size_c * distance_ab;
    long double result = numerator / (size_a + size_b + size_c);
    return max((long double)0, result);
}

inline HierarchicalClusteringResult reorder_merges(int point_count, const vector<RawMerge>& raw_merges) {
    HierarchicalClusteringResult result;
    result.point_count = point_count;
    if (point_count <= 1) return result;
    vector<int> order(raw_merges.size());
    iota(order.begin(), order.end(), 0);
    stable_sort(order.begin(), order.end(), [&](int a, int b) {
        if (raw_merges[a].distance != raw_merges[b].distance) return raw_merges[a].distance < raw_merges[b].distance;
        return a < b;
    });
    vector<int> new_id((size_t)(2 * point_count - 1), -1);
    vector<int> minimum_point((size_t)(2 * point_count - 1));
    iota(new_id.begin(), new_id.begin() + point_count, 0);
    iota(minimum_point.begin(), minimum_point.begin() + point_count, 0);
    result.merges.reserve(raw_merges.size());
    for (int raw_index : order) {
        const auto& raw = raw_merges[raw_index];
        int left = new_id[raw.left];
        int right = new_id[raw.right];
        if (left == -1 || right == -1) throw invalid_argument("hierarchical_clustering: linkage produced an inversion; Ward linkage requires Euclidean distances");
        if (minimum_point[left] > minimum_point[right]) swap(left, right);
        int id = point_count + (int)result.merges.size();
        result.merges.push_back({left, right, raw.distance, raw.size});
        new_id[point_count + raw_index] = id;
        minimum_point[id] = min(minimum_point[left], minimum_point[right]);
    }
    return result;
}
}

template <class PointDistance>
HierarchicalClusteringResult hierarchical_clustering_by_index(
    int point_count, PointDistance point_distance, HierarchicalLinkage linkage = HierarchicalLinkage::average) {
    using DistanceResult = remove_cv_t<remove_reference_t<invoke_result_t<PointDistance&, int, int>>>;
    static_assert(is_convertible_v<DistanceResult, long double>, "point_distance must return a value convertible to long double");
    if (point_count < 0) throw invalid_argument("hierarchical_clustering: point_count must not be negative");
    HierarchicalClusteringResult result;
    result.point_count = point_count;
    if (point_count <= 1) return result;
    hierarchical_clustering_internal::PackedDistances distances(point_count);
    vector<int> active_clusters((size_t)point_count);
    vector<int> node((size_t)point_count);
    vector<int> cluster_size((size_t)point_count, 1);
    iota(active_clusters.begin(), active_clusters.end(), 0);
    iota(node.begin(), node.end(), 0);
    for (int b = 1; b < point_count; ++b) {
        for (int a = 0; a < b; ++a) {
            long double distance = (long double)point_distance(a, b);
            if (!isfinite(distance) || distance < 0) throw invalid_argument("hierarchical_clustering: every distance must be finite and nonnegative");
            if (linkage == HierarchicalLinkage::ward) distance *= distance;
            if (!isfinite(distance)) throw overflow_error("hierarchical_clustering: squared Ward distance overflowed");
            distances.set(a, b, distance);
        }
    }
    vector<hierarchical_clustering_internal::RawMerge> raw_merges;
    raw_merges.reserve((size_t)point_count - 1);
    vector<int> chain;
    chain.reserve((size_t)point_count);
    while ((int)active_clusters.size() > 1) {
        // Each merge keeps the smaller slot, so cluster 0 remains active.
        if (chain.empty()) chain.push_back(0);
        int a = chain.back();
        int nearest = -1;
        long double nearest_distance = numeric_limits<long double>::infinity();
        for (int b : active_clusters) {
            if (b == a) continue;
            long double distance = distances.get(a, b);
            if (nearest == -1 || distance < nearest_distance || (distance == nearest_distance && node[b] < node[nearest])) {
                nearest = b;
                nearest_distance = distance;
            }
        }
        if (chain.size() < 2 || nearest != chain[chain.size() - 2]) {
            chain.push_back(nearest);
            continue;
        }
        int b = nearest;
        chain.pop_back();
        chain.pop_back();
        if (!chain.empty()) chain.pop_back();
        int keep = min(a, b);
        int remove = max(a, b);
        int size_a = cluster_size[a];
        int size_b = cluster_size[b];
        int new_node = point_count + (int)raw_merges.size();
        int left = node[a];
        int right = node[b];
        int minimum_point = min(left < point_count ? left : raw_merges[left - point_count].minimum_point,
                                right < point_count ? right : raw_merges[right - point_count].minimum_point);
        long double merge_distance = nearest_distance;
        if (linkage == HierarchicalLinkage::ward) merge_distance = sqrt(max((long double)0, nearest_distance));
        raw_merges.push_back({left, right, size_a + size_b, minimum_point, merge_distance});
        for (int c : active_clusters) {
            if (c == a || c == b) continue;
            long double updated = hierarchical_clustering_internal::updated_distance(
                linkage, distances.get(a, c), distances.get(b, c), nearest_distance, size_a, size_b, cluster_size[c]);
            distances.set(keep, c, updated);
        }
        auto remove_position = lower_bound(active_clusters.begin(), active_clusters.end(), remove);
        if (remove_position == active_clusters.end() || *remove_position != remove) {
            throw logic_error("hierarchical_clustering: active cluster was not found");
        }
        active_clusters.erase(remove_position);
        node[keep] = new_node;
        cluster_size[keep] = size_a + size_b;
    }
    return hierarchical_clustering_internal::reorder_merges(point_count, raw_merges);
}

template <class Point, class PointDistance>
HierarchicalClusteringResult hierarchical_clustering(const vector<Point>& points, PointDistance point_distance, HierarchicalLinkage linkage = HierarchicalLinkage::average) {
    if (points.size() > (size_t)numeric_limits<int>::max()) throw invalid_argument("hierarchical_clustering: points.size() must fit in int");
    return hierarchical_clustering_by_index((int)points.size(), [&](int a, int b) {
        return point_distance(points[a], points[b]);
    }, linkage);
}
}
