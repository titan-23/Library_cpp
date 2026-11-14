/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/clustering/test/kmeans_balanced_test.cpp
// AtCoder Library を include パスに含めてコンパイルする
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>
#include "titan_cpplib/ahc/clustering/kmeans_balanced.cpp"
using namespace std;
using namespace titan23;
namespace {
using Point = array<double, 2>;

double squared_distance(const Point& a, const Point& b) {
    double dx = a[0] - b[0];
    double dy = a[1] - b[1];
    return dx * dx + dy * dy;
}

long long flow_cost(const Point& a, const Point& b) {
    return llround(squared_distance(a, b) * 1000);
}

Point mean(const vector<Point>& points, span<const int> members) {
    Point center{};
    for (int point : members) {
        center[0] += points[point][0];
        center[1] += points[point][1];
    }
    center[0] /= members.size();
    center[1] /= members.size();
    return center;
}

template <class Result>
void verify_result(const vector<Point>& points, const vector<ClusterSizeRange>& ranges, const Result& result) {
    int n = points.size(), k = ranges.size();
    assert((int)result.labels.size() == n);
    assert((int)result.centers.size() == k);
    vector<int> counts(k);
    vector<vector<int>> members(k);
    double total_cost = 0;
    long long total_flow_cost = 0;
    for (int point = 0; point < n; ++point) {
        int label = result.labels[point];
        assert(0 <= label && label < k);
        ++counts[label];
        members[label].push_back(point);
        total_cost += squared_distance(points[point], result.centers[label]);
        total_flow_cost += flow_cost(points[point], result.centers[label]);
    }
    assert(counts == result.cluster_sizes);
    for (int cluster = 0; cluster < k; ++cluster) {
        assert(ranges[cluster].lower <= counts[cluster] && counts[cluster] <= ranges[cluster].upper);
        if (counts[cluster] == 0) continue;
        Point center = mean(points, members[cluster]);
        assert(abs(center[0] - result.centers[cluster][0]) < 1e-9 && abs(center[1] - result.centers[cluster][1]) < 1e-9);
    }
    assert(abs(total_cost - result.total_cost) <= 1e-9 * max(1.0, total_cost));
    assert(total_flow_cost == result.total_flow_cost);
    if (!result.converged) return;
    // 収束した割り当ては自身の中心で flow_cost が最小なので、2 点の交換や制約を満たす 1 点の移動で良くならない
    for (int a = 0; a < n; ++a) for (int b = 0; b < n; ++b) {
        int la = result.labels[a], lb = result.labels[b];
        if (la == lb) continue;
        long long before = flow_cost(points[a], result.centers[la]) + flow_cost(points[b], result.centers[lb]);
        long long after = flow_cost(points[a], result.centers[lb]) + flow_cost(points[b], result.centers[la]);
        assert(before <= after);
    }
    for (int a = 0; a < n; ++a) for (int target = 0; target < k; ++target) {
        int source = result.labels[a];
        if (source == target || counts[source] == ranges[source].lower || counts[target] == ranges[target].upper) continue;
        assert(flow_cost(points[a], result.centers[source]) <= flow_cost(points[a], result.centers[target]));
    }
}

void test_random_inputs() {
    mt19937_64 random(23);
    for (int trial = 0; trial < 40; ++trial) {
        int n = 12 + random() % 40;
        int k = 2 + random() % 5;
        vector<Point> points(n);
        for (Point& point : points) point = {double(random() % 2001) / 100 - 10, double(random() % 2001) / 100 - 10};
        vector<int> sizes(k, n / k);
        for (int i = 0; i < n % k; ++i) ++sizes[i];
        KMeansOptions options{30, (uint32_t)random()};
        auto exact = kmeans_balanced_exact(points, sizes, squared_distance, flow_cost, mean, options);
        vector<ClusterSizeRange> exact_ranges;
        for (int size : sizes) exact_ranges.push_back({size, size});
        verify_result(points, exact_ranges, exact);
        auto same = kmeans_balanced(points, exact_ranges, squared_distance, flow_cost, mean, options);
        assert(same.labels == exact.labels && same.total_cost == exact.total_cost);

        vector<ClusterSizeRange> ranges;
        for (int cluster = 0; cluster < k; ++cluster) ranges.push_back({cluster == 0 ? 0 : 1, n / k + 3});
        auto ranged = kmeans_balanced(points, ranges, squared_distance, flow_cost, mean, options);
        verify_result(points, ranges, ranged);

        auto best = kmeans_balanced_best_of(points, ranges, 3, squared_distance, flow_cost, mean, options);
        verify_result(points, ranges, best);
        double minimum = numeric_limits<double>::infinity();
        for (int t = 0; t < 3; ++t) {
            KMeansOptions current = options;
            current.seed = options.seed + t;
            minimum = min(minimum, kmeans_balanced(points, ranges, squared_distance, flow_cost, mean, current).total_cost);
        }
        assert(best.total_cost == minimum);
    }
}

void test_from_centers_and_empty_cluster() {
    vector<Point> points = {{0, 0}, {1, 0}, {0, 1}, {10, 10}, {11, 10}, {10, 11}};
    vector<Point> centers = {{0, 0}, {10, 10}, {100, 100}};
    vector<ClusterSizeRange> ranges = {{0, 6}, {0, 6}, {0, 6}};
    auto result = kmeans_balanced_from_centers(points, centers, ranges, squared_distance, flow_cost, mean, 10);
    verify_result(points, ranges, result);
    assert(result.cluster_sizes[2] == 0);
    assert(result.centers[2] == centers[2]);
    assert(result.converged);
}

void test_invalid_inputs() {
    vector<Point> points = {{0, 0}, {1, 0}, {2, 0}};
    auto throws = [](auto f) {
        try {
            f();
        } catch (const invalid_argument&) {
            return true;
        }
        return false;
    };
    assert(throws([&] { kmeans_balanced(points, vector<ClusterSizeRange>{{0, 1}, {0, 1}}, squared_distance, flow_cost, mean); }));
    assert(throws([&] { kmeans_balanced(points, vector<ClusterSizeRange>{{2, 1}, {0, 3}}, squared_distance, flow_cost, mean); }));
    assert(throws([&] { kmeans_balanced_exact(points, {1, 1}, squared_distance, flow_cost, mean); }));
    assert(throws([&] { kmeans_balanced_best_of(points, vector<ClusterSizeRange>{{0, 3}, {0, 3}}, 0, squared_distance, flow_cost, mean); }));
}
}

int main() {
    test_random_inputs();
    test_from_centers_and_empty_cluster();
    test_invalid_inputs();
}
