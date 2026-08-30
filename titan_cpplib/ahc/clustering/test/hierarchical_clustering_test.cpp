/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/clustering/test/hierarchical_clustering_test.cpp
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>
#include <utility>
#include <vector>
#include "titan_cpplib/ahc/clustering/hierarchical_clustering.cpp"
using namespace std;
using namespace titan23;

struct TestPoint {
    long double x;
    long double y;
};

long double point_distance(const TestPoint& a, const TestPoint& b) {
    return hypot(a.x - b.x, a.y - b.y);
}

vector<vector<long double>> cophenetic_distances(const HierarchicalClusteringResult& result) {
    int n = result.point_count;
    vector<vector<long double>> distances((size_t)n, vector<long double>((size_t)n));
    if (n == 0) return distances;
    vector<vector<int>> members((size_t)(2 * n - 1));
    for (int i = 0; i < n; ++i) members[i] = {i};
    for (int i = 0; i < n - 1; ++i) {
        const auto& merge = result.merges[i];
        for (int a : members[merge.left]) {
            for (int b : members[merge.right]) distances[a][b] = distances[b][a] = merge.distance;
        }
        int node = n + i;
        members[node] = members[merge.left];
        members[node].insert(members[node].end(), members[merge.right].begin(), members[merge.right].end());
    }
    return distances;
}

HierarchicalClusteringResult naive_hierarchical_clustering(const vector<TestPoint>& points, HierarchicalLinkage linkage) {
    int n = (int)points.size();
    HierarchicalClusteringResult result;
    result.point_count = n;
    if (n <= 1) return result;
    int node_count = 2 * n - 1;
    vector<vector<long double>> distances((size_t)node_count, vector<long double>((size_t)node_count));
    vector<int> sizes((size_t)node_count, 1);
    vector<char> active((size_t)node_count);
    fill(active.begin(), active.begin() + n, true);
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            long double value = point_distance(points[i], points[j]);
            if (linkage == HierarchicalLinkage::ward) value *= value;
            distances[i][j] = distances[j][i] = value;
        }
    }
    for (int step = 0; step < n - 1; ++step) {
        int a = -1;
        int b = -1;
        for (int i = 0; i < n + step; ++i) {
            if (!active[i]) continue;
            for (int j = i + 1; j < n + step; ++j) {
                if (!active[j]) continue;
                if (a == -1 || pair(distances[i][j], pair(i, j)) < pair(distances[a][b], pair(a, b))) {
                    a = i;
                    b = j;
                }
            }
        }
        int node = n + step;
        long double merge_distance = linkage == HierarchicalLinkage::ward ? sqrt(max((long double)0, distances[a][b])) : distances[a][b];
        result.merges.push_back({a, b, merge_distance, sizes[a] + sizes[b]});
        for (int c = 0; c < node; ++c) {
            if (!active[c] || c == a || c == b) continue;
            long double updated;
            if (linkage == HierarchicalLinkage::single) updated = min(distances[a][c], distances[b][c]);
            else if (linkage == HierarchicalLinkage::complete) updated = max(distances[a][c], distances[b][c]);
            else if (linkage == HierarchicalLinkage::average) updated = (sizes[a] * distances[a][c] + sizes[b] * distances[b][c]) / (sizes[a] + sizes[b]);
            else updated = max((long double)0, ((sizes[a] + sizes[c]) * distances[a][c] + (sizes[b] + sizes[c]) * distances[b][c] - sizes[c] * distances[a][b]) / (sizes[a] + sizes[b] + sizes[c]));
            distances[node][c] = distances[c][node] = updated;
        }
        active[a] = false;
        active[b] = false;
        active[node] = true;
        sizes[node] = sizes[a] + sizes[b];
    }
    return result;
}

void test_against_naive() {
    mt19937_64 random(23);
    for (int n = 2; n <= 30; ++n) {
        for (int trial = 0; trial < 20; ++trial) {
            vector<TestPoint> points(n);
            for (int i = 0; i < n; ++i) {
                points[i] = {(long double)(random() % 1000000) + i * 0.000001L,
                             (long double)(random() % 1000000) + i * i * 0.0000001L};
            }
            for (auto linkage : {HierarchicalLinkage::single, HierarchicalLinkage::complete, HierarchicalLinkage::average, HierarchicalLinkage::ward}) {
                auto result = hierarchical_clustering(points, point_distance, linkage);
                auto expected = naive_hierarchical_clustering(points, linkage);
                auto distances = cophenetic_distances(result);
                auto expected_distances = cophenetic_distances(expected);
                for (int i = 0; i < n; ++i) {
                    for (int j = 0; j < n; ++j) {
                        long double scale = max({(long double)1, abs(distances[i][j]), abs(expected_distances[i][j])});
                        if (abs(distances[i][j] - expected_distances[i][j]) > 1e-12L * scale) {
                            cerr << "mismatch n=" << n << " trial=" << trial << " linkage=" << (int)linkage
                                 << " i=" << i << " j=" << j << " actual=" << (double)distances[i][j]
                                 << " expected=" << (double)expected_distances[i][j] << '\n';
                            for (int p = 0; p < n; ++p) cerr << p << ": " << (double)points[p].x << ' ' << (double)points[p].y << '\n';
                            cerr << "actual merges\n";
                            for (auto merge : result.merges) cerr << merge.left << ' ' << merge.right << ' ' << (double)merge.distance << '\n';
                            cerr << "expected merges\n";
                            for (auto merge : expected.merges) cerr << merge.left << ' ' << merge.right << ' ' << (double)merge.distance << '\n';
                            abort();
                        }
                    }
                }
                for (int cluster_count = 1; cluster_count <= n; ++cluster_count) {
                    auto labels = result.labels(cluster_count);
                    assert((int)*max_element(labels.begin(), labels.end()) + 1 == cluster_count);
                }
            }
        }
    }
}

void test_boundaries() {
    vector<TestPoint> empty;
    auto empty_result = hierarchical_clustering(empty, point_distance);
    assert(empty_result.labels(0).empty());
    vector<TestPoint> one = {{3, 4}};
    auto one_result = hierarchical_clustering(one, point_distance);
    assert(one_result.labels(1) == vector<int>{0});
    vector<TestPoint> points = {{0, 0}, {1, 0}, {10, 0}, {11, 0}};
    auto result = hierarchical_clustering(points, point_distance, HierarchicalLinkage::single);
    auto labels = result.labels(2);
    assert(labels[0] == labels[1]);
    assert(labels[2] == labels[3]);
    assert(labels[0] != labels[2]);
    assert(result.labels_at_distance(1).size() == 4);
    bool failed = false;
    try {
        hierarchical_clustering_by_index(2, [](int, int) { return -1; });
    } catch (const invalid_argument&) {
        failed = true;
    }
    assert(failed);
    failed = false;
    try {
        result.labels(0);
    } catch (const invalid_argument&) {
        failed = true;
    }
    assert(failed);
}

void test_equal_distances() {
    vector<TestPoint> points(20, {1, 1});
    int point_count = points.size();
    for (auto linkage : {HierarchicalLinkage::single, HierarchicalLinkage::complete, HierarchicalLinkage::average,
                         HierarchicalLinkage::ward}) {
        auto result = hierarchical_clustering(points, point_distance, linkage);
        assert((int)result.merges.size() == point_count - 1);
        vector<int> sizes(2 * point_count - 1, 1);
        for (int index = 0; index < (int)result.merges.size(); ++index) {
            const auto& merge = result.merges[index];
            assert(0 <= merge.left && merge.left < point_count + index);
            assert(0 <= merge.right && merge.right < point_count + index);
            assert(merge.distance == 0);
            assert(merge.size == sizes[merge.left] + sizes[merge.right]);
            sizes[point_count + index] = merge.size;
        }
        assert(sizes.back() == point_count);
    }
}

int main() {
    test_against_naive();
    test_boundaries();
    test_equal_distances();
    cout << "ok\n";
}
