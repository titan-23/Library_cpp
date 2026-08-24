/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/clustering/test/euclidean_single_linkage_test.cpp
#include <bits/stdc++.h>
#include "titan_cpplib/ahc/clustering/euclidean_single_linkage.cpp"
using namespace std;
using namespace titan23;

vector<vector<long double>> cophenetic_distances(const HierarchicalClusteringResult& result) {
    int n = result.point_count;
    vector<vector<long double>> distances((size_t)n, vector<long double>((size_t)n));
    if (n == 0) return distances;
    vector<vector<int>> members((size_t)(2 * n - 1));
    for (int i = 0; i < n; ++i) members[i] = {i};
    long double last_distance = 0;
    for (int i = 0; i < n - 1; ++i) {
        const auto& merge = result.merges[i];
        assert(last_distance <= merge.distance);
        last_distance = merge.distance;
        for (int a : members[merge.left]) {
            for (int b : members[merge.right]) distances[a][b] = distances[b][a] = merge.distance;
        }
        int node = n + i;
        members[node] = members[merge.left];
        members[node].insert(members[node].end(), members[merge.right].begin(), members[merge.right].end());
        assert((int)members[node].size() == merge.size);
    }
    return distances;
}

long double euclidean_distance(const IntegerPoint& a, const IntegerPoint& b) {
    return hypot((long double)a.x - b.x, (long double)a.y - b.y);
}

void compare_with_general(const vector<IntegerPoint>& points) {
    auto result = euclidean_single_linkage_2d(points);
    auto expected = hierarchical_clustering(points, euclidean_distance, HierarchicalLinkage::single);
    auto actual_distances = cophenetic_distances(result);
    auto expected_distances = cophenetic_distances(expected);
    assert(actual_distances.size() == expected_distances.size());
    for (int i = 0; i < (int)points.size(); ++i) {
        for (int j = 0; j < (int)points.size(); ++j) {
            long double scale = max({(long double)1, actual_distances[i][j], expected_distances[i][j]});
            assert(abs(actual_distances[i][j] - expected_distances[i][j]) <= 1e-15L * scale);
        }
    }
    for (int cluster_count = 1; cluster_count <= (int)points.size(); ++cluster_count) {
        auto labels = result.labels(cluster_count);
        assert((int)*max_element(labels.begin(), labels.end()) + 1 == cluster_count);
    }
}

void test_small_cases() {
    auto empty = euclidean_single_linkage_2d(vector<IntegerPoint>{});
    assert(empty.labels(0).empty());
    compare_with_general({{3, 4}});
    compare_with_general({{0, 0}, {0, 0}, {0, 0}, {10, 0}, {10, 0}, {5, 8}});
    compare_with_general({{0, 0}, {10, 0}, {10, 10}, {0, 10}, {5, 5}});
    compare_with_general({{3, 0}, {0, 0}, {2, 0}, {1, 0}});
}

void test_random() {
    mt19937 random(23);
    for (int n = 2; n <= 80; ++n) {
        for (int trial = 0; trial < 20; ++trial) {
            vector<IntegerPoint> points(n);
            for (auto& point : points) {
                point.x = (int)(random() % 101) - 50;
                point.y = (int)(random() % 101) - 50;
            }
            compare_with_general(points);
        }
    }
}

void test_coordinate_getters() {
    vector<pair<long long, long long>> points = {{0, 0}, {3, 4}, {3, 4}, {20, 0}};
    auto result = euclidean_single_linkage_2d(points, [](const auto& point) { return point.first; }, [](const auto& point) { return point.second; });
    assert(result.merges.size() == 3);
    assert(result.merges[0].distance == 0);
}

int main() {
    test_small_cases();
    test_random();
    test_coordinate_getters();
    cout << "ok\n";
}
