/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/clustering/test/emeans_test.cpp
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>
#include "titan_cpplib/ahc/clustering/emeans.cpp"
using namespace std;
using namespace titan23;
namespace {
template <class Point>
void verify(const vector<Point>& points, int k, const KMeansResult<vector<double>, double>& result) {
    assert(result.labels.size() == points.size());
    assert(result.centers.size() == size_t(k));
    vector<int> sizes(k);
    vector<vector<long double>> sums(k, vector<long double>(points[0].size()));
    long double total = 0;
    for (size_t i = 0; i < points.size(); ++i) {
        int label = result.labels[i];
        assert(0 <= label && label < k);
        ++sizes[label];
        long double assigned = 0;
        for (size_t axis = 0; axis < points[i].size(); ++axis) {
            sums[label][axis] += points[i][axis];
            long double delta = (long double)points[i][axis] - result.centers[label][axis];
            assigned += delta * delta;
        }
        total += assigned;
        if (result.converged) for (const auto& center : result.centers) {
            long double distance = 0;
            for (size_t axis = 0; axis < points[i].size(); ++axis) {
                long double delta = (long double)points[i][axis] - center[axis];
                distance += delta * delta;
            }
            assert(assigned <= distance + 1e-9L * max(1.L, assigned));
        }
    }
    assert(sizes == result.cluster_sizes);
    assert(abs(total - result.total_cost) <= 1e-9L * max(1.L, total));
    for (int j = 0; j < k; ++j) {
        assert(sizes[j] > 0);
        for (size_t axis = 0; axis < points[0].size(); ++axis) {
            long double mean = sums[j][axis] / sizes[j];
            assert(abs(mean - result.centers[j][axis]) <= 1e-10L * max(1.L, abs(mean)));
        }
    }
}

void test_random() {
    mt19937 random(23);
    for (int trial = 0; trial < 60; ++trial) {
        int n = 1 + random() % 90, d = 1 + random() % 18, k = 1 + random() % n;
        vector<vector<double>> points(n, vector<double>(d));
        for (auto& point : points) for (double& value : point) value = int(random() % 31) - 15;
        EMeansOptions options;
        options.seed = random();
        options.epochs = 1 + random() % 4;
        options.batch_size = trial % 3 == 0 ? 0 : 1 + random() % (2 * n);
        options.alpha0 = trial % 3 == 0 ? 1 : 0;
        options.revival_threshold = trial % 2 == 0 ? -1 : 0.9;
        options.kmeans_pp = trial % 2 == 0;
        options.polish_iterations = trial % 3 == 0 ? 0 : 100;
        auto result = emeans(points, k, options);
        verify(points, k, result);
        auto repeat = emeans(points, k, options);
        assert(result.labels == repeat.labels && result.centers == repeat.centers);
        assert(result.total_cost == repeat.total_cost && result.iterations == repeat.iterations);
        bool polished = options.polish_iterations > 0;
        options.polish_iterations = 0;
        auto unpolished = emeans(points, k, options);
        verify(points, k, unpolished);
        if (polished) assert(result.total_cost <= unpolished.total_cost + 1e-8 * max(1.0, unpolished.total_cost));
    }
}

void test_degenerate() {
    vector<array<int, 2>> identical(12, {3, -2});
    for (int k : {1, 5, 12}) for (bool pp : {false, true}) for (int polish : {0, 100}) {
        EMeansOptions options;
        options.kmeans_pp = pp;
        options.polish_iterations = polish;
        auto result = emeans(identical, k, options);
        verify(identical, k, result);
        assert(result.total_cost == 0);
    }
    vector<array<double, 2>> points = {{0, 0}, {0, 0}, {1, 0}, {10, 0}, {10, 0}, {11, 0}};
    vector<array<int, 2>> centers = {{0, 0}, {0, 0}, {10, 0}, {10, 0}};
    auto result = emeans_from_centers(points, centers);
    verify(points, 4, result);
    auto singleton = emeans(points, 1);
    verify(points, 1, singleton);
    auto per_point = emeans(points, points.size());
    verify(points, points.size(), per_point);
    assert(per_point.total_cost == 0);
}

void test_warmup_and_trials() {
    vector<array<double, 2>> points = {{0, 0}, {1, 0}, {10, 0}, {11, 0}};
    EMeansOptions options;
    options.warmup_iterations = 10;
    auto warmed = emeans(points, 2, options);
    verify(points, 2, warmed);
    assert(warmed.converged && warmed.iterations < options.warmup_iterations);
    options.warmup_iterations = 1;
    verify(points, 2, emeans(points, 2, options));
    options.seed = numeric_limits<uint32_t>::max() - 1;
    auto best = emeans_best_of(points, 2, 5, options);
    double expected = numeric_limits<double>::infinity();
    for (int t = 0; t < 5; ++t) {
        auto current = options;
        current.seed += uint32_t(t);
        expected = min(expected, emeans(points, 2, current).total_cost);
    }
    assert(best.total_cost == expected);
}

void test_full_batch_limit() {
    vector<array<double, 2>> points = {{0, 0}, {1, 2}, {3, -1}, {10, 0}, {11, 1}, {14, -2}};
    vector<vector<double>> centers = {{0, 0}, {10, 0}};
    EMeansOptions options;
    options.epochs = 1;
    options.batch_size = points.size();
    options.alpha0 = 1;
    options.polish_iterations = 0;
    auto ema = emeans_from_centers(points, centers, options);
    auto lloyd = kmeans_from_centers(points, centers, emeans_detail::SquaredDistance{}, emeans_detail::Mean{}, 1);
    assert(ema.labels == lloyd.labels && ema.centers == lloyd.centers && ema.total_cost == lloyd.total_cost);
}

template <class Function>
void invalid(Function function) {
    bool caught = false;
    try { function(); } catch (const invalid_argument&) { caught = true; }
    assert(caught);
}

void test_invalid() {
    vector<vector<double>> points = {{0, 0}, {1, 1}};
    invalid([&] { emeans(vector<vector<double>>{}, 1); });
    invalid([&] { emeans(points, 0); });
    invalid([&] { emeans(points, -1); });
    invalid([&] { emeans(points, 3); });
    invalid([&] { emeans(vector<vector<double>>{{}}, 1); });
    invalid([&] { emeans(vector<vector<double>>{{0}, {0, 1}}, 1); });
    invalid([&] { emeans(vector<vector<double>>{{numeric_limits<double>::infinity()}}, 1); });
    invalid([&] { emeans_from_centers(points, vector<vector<double>>{{0}}); });
    invalid([&] { emeans_from_centers(points, vector<vector<double>>{{0, nan("")}}); });
    invalid([&] { emeans_best_of(points, 1, 0); });
    vector<EMeansOptions> bad(9);
    bad[0].epochs = 0;
    bad[1].batch_size = -1;
    bad[2].alpha0 = -0.1;
    bad[3].alpha0 = nan("");
    bad[4].alpha0 = 1.01;
    bad[5].revival_threshold = -0.5;
    bad[6].warmup_iterations = -1;
    bad[7].polish_iterations = -1;
    bad[8].epochs = numeric_limits<int>::max();
    for (auto options : bad) invalid([&] { emeans(points, 1, options); });
}
}

int main() {
    test_random();
    test_degenerate();
    test_warmup_and_trials();
    test_full_batch_limit();
    test_invalid();
}
