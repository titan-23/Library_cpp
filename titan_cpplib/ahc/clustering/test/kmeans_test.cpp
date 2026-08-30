/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/clustering/test/kmeans_test.cpp
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <random>
#include <span>
#include <vector>
#include "titan_cpplib/ahc/clustering/kmeans_hamerly.cpp"
using namespace std;
using namespace titan23;
namespace {
using Point = array<double, 3>;

struct SquaredDistance {
    double operator()(const Point& a, const Point& b) const {
        double result = 0;
        for (int axis = 0; axis < 3; ++axis) {
            double difference = a[axis] - b[axis];
            result += difference * difference;
        }
        return result;
    }
};

struct EuclideanDistance {
    double operator()(const Point& a, const Point& b) const {
        return sqrt(SquaredDistance{}(a, b));
    }
};

struct MeanCenter {
    Point operator()(const vector<Point>& points, span<const int> members) const {
        Point center{};
        for (int point : members) for (int axis = 0; axis < 3; ++axis) center[axis] += points[point][axis];
        for (double& value : center) value /= members.size();
        return center;
    }
};

template <class Result>
void verify_result(const vector<Point>& points, int cluster_count, const Result& result) {
    assert((int)result.labels.size() == (int)points.size());
    assert((int)result.centers.size() == cluster_count);
    assert((int)result.cluster_sizes.size() == cluster_count);
    vector<int> counts(cluster_count);
    double total_cost = 0;
    for (int point = 0; point < (int)points.size(); ++point) {
        assert(0 <= result.labels[point] && result.labels[point] < cluster_count);
        ++counts[result.labels[point]];
        total_cost += SquaredDistance{}(points[point], result.centers[result.labels[point]]);
    }
    assert(counts == result.cluster_sizes);
    for (int count : counts) assert(count > 0);
    assert(abs(total_cost - result.total_cost) <= 1e-10 * max(1.0, total_cost));
}

void compare_implementations(const vector<Point>& points, int cluster_count, uint32_t seed, int max_iterations = 100) {
    KMeansOptions options{max_iterations, seed};
    auto lloyd = kmeans(points, cluster_count, SquaredDistance{}, MeanCenter{}, options);
    auto hamerly = kmeans_hamerly(points, cluster_count, SquaredDistance{}, EuclideanDistance{}, MeanCenter{}, options);
    verify_result(points, cluster_count, lloyd);
    verify_result(points, cluster_count, hamerly);
    assert(lloyd.labels == hamerly.labels);
    assert(lloyd.cluster_sizes == hamerly.cluster_sizes);
    assert(lloyd.centers == hamerly.centers);
    assert(lloyd.total_cost == hamerly.total_cost);
    assert(lloyd.iterations == hamerly.iterations);
    assert(lloyd.converged == hamerly.converged);
}

void test_random_points() {
    mt19937_64 random(23);
    for (int trial = 0; trial < 30; ++trial) {
        int point_count = 20 + random() % 80;
        int cluster_count = 1 + random() % min(point_count, 12);
        vector<Point> points(point_count);
        for (Point& point : points) for (double& coordinate : point) {
            coordinate = (int)(random() % 41) - 20 + (random() % 1000) * 0.000001;
        }
        compare_implementations(points, cluster_count, (uint32_t)random());
        compare_implementations(points, cluster_count, (uint32_t)random(), 1);
    }
}

void test_empty_cluster_repair() {
    vector<Point> identical(12, {3, -2, 5});
    compare_implementations(identical, 12, 7);
    vector<Point> points = {
        {0, 0, 0}, {0, 0, 0}, {1, 0, 0}, {1, 0, 0},
        {10, 0, 0}, {10, 0, 0}, {11, 0, 0}, {11, 0, 0}
    };
    vector<Point> initial_centers = {{0, 0, 0}, {0, 0, 0}, {10, 0, 0}, {10, 0, 0}};
    auto lloyd = kmeans_from_centers(points, initial_centers, SquaredDistance{}, MeanCenter{});
    auto hamerly = kmeans_hamerly_from_centers(points, initial_centers, SquaredDistance{}, EuclideanDistance{},
                                               MeanCenter{});
    verify_result(points, 4, lloyd);
    verify_result(points, 4, hamerly);
    assert(lloyd.labels == hamerly.labels);
    assert(lloyd.cluster_sizes == hamerly.cluster_sizes);
    assert(lloyd.total_cost == hamerly.total_cost);
}
}

int main() {
    test_random_points();
    test_empty_cluster_repair();
}
