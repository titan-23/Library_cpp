/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/geometry/test/kd_tree_test.cpp
#include <bits/stdc++.h>
#include "titan_cpplib/geometry/kd_tree.cpp"
using namespace std;
using namespace titan23;

using TestPoint = vector<long double>;

long double squared_distance(const TestPoint& a, const TestPoint& b) {
    long double result = 0;
    for (int axis = 0; axis < (int)a.size(); ++axis) {
        long double difference = a[axis] - b[axis];
        result += difference * difference;
    }
    return result;
}

vector<KdNeighbor> brute_force(const vector<TestPoint>& points, const TestPoint& query, int excluded_index = -1) {
    vector<KdNeighbor> result;
    for (int point = 0; point < (int)points.size(); ++point) {
        if (point != excluded_index) result.push_back({point, squared_distance(points[point], query)});
    }
    sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return pair(a.squared_distance, a.index) < pair(b.squared_distance, b.index);
    });
    return result;
}

void check_neighbors(const vector<KdNeighbor>& actual, const vector<KdNeighbor>& expected) {
    assert(actual.size() == expected.size());
    for (int i = 0; i < (int)actual.size(); ++i) {
        assert(actual[i].index == expected[i].index);
        assert(actual[i].squared_distance == expected[i].squared_distance);
    }
}

void test_random() {
    mt19937 random(23);
    for (int dimension = 1; dimension <= 8; ++dimension) {
        for (int n = 0; n <= 100; ++n) {
            vector<TestPoint> points((size_t)n, TestPoint((size_t)dimension));
            for (auto& point : points) {
                for (auto& coordinate : point) coordinate = (int)(random() % 41) - 20;
            }
            auto tree = make_kd_tree(points, dimension, [](const TestPoint& point, int axis) { return point[axis]; });
            assert(tree.point_count() == n);
            assert(tree.dimension() == dimension);
            for (int trial = 0; trial < 10; ++trial) {
                TestPoint query((size_t)dimension);
                for (auto& coordinate : query) coordinate = (int)(random() % 51) - 25;
                auto expected = brute_force(points, query);
                auto nearest = tree.nearest_neighbor(query);
                if (n == 0) assert(!nearest);
                else check_neighbors(vector<KdNeighbor>{*nearest}, vector<KdNeighbor>{expected[0]});
                for (int count : {0, 1, 2, 5, 20, 200}) {
                    auto actual = tree.k_nearest_neighbors(query, count);
                    auto selected = expected;
                    selected.resize((size_t)min(count, n));
                    check_neighbors(actual, selected);
                }
                long double radius = random() % 30;
                vector<KdNeighbor> within_radius;
                copy_if(expected.begin(), expected.end(), back_inserter(within_radius), [&](const auto& neighbor) {
                    return neighbor.squared_distance <= radius * radius;
                });
                check_neighbors(tree.radius_neighbors(query, radius), within_radius);
                check_neighbors(tree.k_nearest_neighbors(span<const long double>(query), 7), vector<KdNeighbor>(expected.begin(), expected.begin() + min(7, n)));
            }
            for (int point = 0; point < n; ++point) {
                auto expected = brute_force(points, points[point], point);
                auto nearest = tree.nearest_neighbor_of(point);
                if (n == 1) assert(!nearest);
                else check_neighbors(vector<KdNeighbor>{*nearest}, vector<KdNeighbor>{expected[0]});
                auto actual = tree.k_nearest_neighbors_of(point, 10);
                expected.resize((size_t)min(10, n - 1));
                check_neighbors(actual, expected);
            }
        }
    }
}

void test_ties_and_ownership() {
    vector<TestPoint> points = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {0, 1}};
    auto tree = make_kd_tree(move(points), 2, [](const TestPoint& point, int axis) { return point[axis]; });
    TestPoint query = {0, 0};
    auto neighbors = tree.k_nearest_neighbors(query, 5);
    for (int i = 0; i < 5; ++i) {
        assert(neighbors[i].index == i);
        assert(neighbors[i].squared_distance == 1);
        assert(neighbors[i].distance() == 1);
    }
    assert(tree.point(4) == TestPoint({0, 1}));
}

void test_invalid_inputs() {
    auto getter = [](const TestPoint& point, int axis) { return point[axis]; };
    bool failed = false;
    try {
        make_kd_tree(vector<TestPoint>{}, 0, getter);
    } catch (const invalid_argument&) {
        failed = true;
    }
    assert(failed);
    failed = false;
    try {
        make_kd_tree(vector<TestPoint>{{numeric_limits<long double>::infinity()}}, 1, getter);
    } catch (const invalid_argument&) {
        failed = true;
    }
    assert(failed);
    auto tree = make_kd_tree(vector<TestPoint>{{0, 0}, {1, 1}}, 2, getter);
    failed = false;
    try {
        tree.nearest_neighbor(span<const long double>());
    } catch (const invalid_argument&) {
        failed = true;
    }
    assert(failed);
    failed = false;
    try {
        tree.k_nearest_neighbors(TestPoint{0, 0}, -1);
    } catch (const invalid_argument&) {
        failed = true;
    }
    assert(failed);
    failed = false;
    try {
        tree.radius_neighbors(TestPoint{0, 0}, -1);
    } catch (const invalid_argument&) {
        failed = true;
    }
    assert(failed);
    failed = false;
    try {
        tree.nearest_neighbor(TestPoint{0, 0}, 2);
    } catch (const out_of_range&) {
        failed = true;
    }
    assert(failed);
}

int main() {
    test_random();
    test_ties_and_ownership();
    test_invalid_inputs();
    cout << "ok\n";
}
