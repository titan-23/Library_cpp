/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/geometry/test/kd_tree_test.cpp
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
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

template <class Exception, class Function>
void expect_exception(Function function) {
    bool failed = false;
    try {
        function();
    } catch (const Exception&) {
        failed = true;
    }
    assert(failed);
}

void test_random() {
    mt19937 random(23);
    for (int dimension = 1; dimension <= 8; ++dimension) {
        for (int n = 0; n <= 100; ++n) {
            vector<TestPoint> points(n, TestPoint(dimension));
            for (auto& point : points) {
                for (auto& coordinate : point) coordinate = (int)(random() % 41) - 20;
            }
            auto tree = make_kd_tree(points, dimension, [](const TestPoint& point, int axis) { return point[axis]; });
            assert(tree.point_count() == n);
            assert(tree.dimension() == dimension);
            vector<KdNeighbor> knn_buffer;
            vector<KdNeighbor> radius_buffer;
            knn_buffer.reserve(200);
            radius_buffer.reserve(n);
            auto knn_capacity = knn_buffer.capacity();
            auto radius_capacity = radius_buffer.capacity();
            for (int trial = 0; trial < 10; ++trial) {
                TestPoint query(dimension);
                for (auto& coordinate : query) coordinate = (int)(random() % 51) - 25;
                auto expected = brute_force(points, query);
                auto nearest = tree.nearest_neighbor(query);
                if (n == 0) assert(!nearest);
                else check_neighbors(vector<KdNeighbor>{*nearest}, vector<KdNeighbor>{expected[0]});
                for (int count : {0, 1, 2, 5, 20, 200}) {
                    auto actual = tree.k_nearest_neighbors(query, count);
                    auto selected = expected;
                    selected.resize(min(count, n));
                    check_neighbors(actual, selected);
                    tree.k_nearest_neighbors(query, count, knn_buffer);
                    check_neighbors(knn_buffer, selected);
                    assert(knn_buffer.capacity() == knn_capacity);
                }
                long double radius = random() % 30;
                vector<KdNeighbor> within_radius;
                copy_if(expected.begin(), expected.end(), back_inserter(within_radius), [&](const auto& neighbor) {
                    return neighbor.squared_distance <= radius * radius;
                });
                check_neighbors(tree.radius_neighbors(query, radius), within_radius);
                tree.radius_neighbors(span<const long double>(query), radius, radius_buffer);
                check_neighbors(radius_buffer, within_radius);
                assert(radius_buffer.capacity() == radius_capacity);
                vector<KdNeighbor> nearest_seven(expected.begin(), expected.begin() + min(7, n));
                check_neighbors(tree.k_nearest_neighbors(span<const long double>(query), 7), nearest_seven);
            }
            for (int point = 0; point < n; ++point) {
                auto expected = brute_force(points, points[point], point);
                auto nearest = tree.nearest_neighbor_of(point);
                if (n == 1) assert(!nearest);
                else check_neighbors(vector<KdNeighbor>{*nearest}, vector<KdNeighbor>{expected[0]});
                auto actual = tree.k_nearest_neighbors_of(point, 10);
                auto nearest_ten = expected;
                nearest_ten.resize(min(10, n - 1));
                check_neighbors(actual, nearest_ten);
                tree.k_nearest_neighbors_of(point, 10, knn_buffer);
                check_neighbors(knn_buffer, nearest_ten);

                long double radius = 10;
                vector<KdNeighbor> within_radius;
                copy_if(expected.begin(), expected.end(), back_inserter(within_radius), [&](const auto& neighbor) {
                    return neighbor.squared_distance <= radius * radius;
                });
                tree.radius_neighbors_of(point, radius, radius_buffer);
                check_neighbors(radius_buffer, within_radius);
            }
        }
    }
}

void test_ties_and_ownership() {
    vector<TestPoint> points = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {0, 1}};
    auto getter = [](const TestPoint& point, int axis) { return point[axis]; };
    auto copied_tree = make_kd_tree(points, 2, getter);
    points[0][0] = -100;
    assert(copied_tree.point(0) == TestPoint({-1, 0}));
    points[0][0] = -1;

    auto tree = make_kd_tree(move(points), 2, getter);
    TestPoint query = {0, 0};
    auto neighbors = tree.k_nearest_neighbors(query, 5);
    for (int i = 0; i < 5; ++i) {
        assert(neighbors[i].index == i);
        assert(neighbors[i].squared_distance == 1);
        assert(neighbors[i].distance() == 1);
    }
    assert(tree.point(4) == TestPoint({0, 1}));
}

void test_move() {
    auto getter = [](const TestPoint& point, int axis) { return point[axis]; };
    auto tree = make_kd_tree(vector<TestPoint>{{0, 0}, {2, 3}}, 2, getter);
    using Tree = decltype(tree);
    auto copy = tree;
    auto copy_assigned = make_kd_tree(vector<TestPoint>{{9, 9}}, 2, getter);
    copy_assigned = copy;
    copy_assigned = copy_assigned;
    assert(copy_assigned.points() == tree.points());

    Tree moved(move(tree));
    assert(moved.point_count() == 2);
    assert(tree.point_count() == 0);
    assert(!tree.nearest_neighbor(TestPoint{0, 0}));

    auto assigned = make_kd_tree(vector<TestPoint>{{5, 5}}, 2, getter);
    assigned = move(moved);
    assert(assigned.point_count() == 2);
    assert(moved.point_count() == 0);
    assert(!moved.nearest_neighbor(TestPoint{0, 0}));

    using Function = function<long double(const TestPoint&, int)>;
    auto function_tree = KdTree<TestPoint, Function>(vector<TestPoint>{{0, 0}}, 2, Function(getter));
    auto function_moved = move(function_tree);
    assert(function_moved.point_count() == 1);
    assert(function_tree.dimension() == 0);
    assert(!function_tree.nearest_neighbor(TestPoint{0, 0}));
    auto function_assigned = KdTree<TestPoint, Function>(vector<TestPoint>{{9, 9}}, 2, Function(getter));
    function_assigned = function_moved;
    assert(function_assigned.points() == function_moved.points());
}

void test_scalar_and_output_buffers() {
    using Point = array<double, 2>;
    vector<Point> points = {{{0, 0}}, {{3, 4}}, {{1, 1}}, {{-1, 1}}};
    auto getter = [](const Point& point, int axis) { return point[axis]; };

    auto default_tree = make_kd_tree(points, 2, getter);
    static_assert(is_same_v<typename decltype(default_tree)::Scalar, long double>);
    static_assert(is_same_v<typename decltype(default_tree)::Neighbor, KdNeighbor>);
    static_assert(is_same_v<decltype(default_tree.nearest_neighbor(points[0])), optional<KdNeighbor>>);

    auto tree = make_kd_tree_as<double>(points, 2, getter);
    static_assert(is_same_v<typename decltype(tree)::Scalar, double>);
    static_assert(is_same_v<typename decltype(tree)::Neighbor, KdNeighborT<double>>);

    array<double, 2> query = {0, 0};
    auto nearest = tree.nearest_neighbor(span<const double>(query));
    assert(nearest && nearest->index == 0 && nearest->squared_distance == 0);

    vector<KdNeighborT<double>> output;
    output.reserve(points.size());
    auto capacity = output.capacity();
    const auto* storage = output.data();

    tree.k_nearest_neighbors(span<const double>(query), 3, output);
    assert(output.size() == 3);
    assert(output[0].index == 0 && output[0].squared_distance == 0);
    assert(output[1].index == 2 && output[1].squared_distance == 2);
    assert(output[2].index == 3 && output[2].squared_distance == 2);
    assert(output.capacity() == capacity && output.data() == storage);

    tree.k_nearest_neighbors(span<const double>(query), 2, output, 0);
    assert(output.size() == 2);
    assert(output[0].index == 2 && output[0].squared_distance == 2);
    assert(output[1].index == 3 && output[1].squared_distance == 2);

    tree.k_nearest_neighbors_of(0, 2, output);
    assert(output.size() == 2);
    assert(output[0].index == 2 && output[0].squared_distance == 2);
    assert(output[1].index == 3 && output[1].squared_distance == 2);
    assert(output.capacity() == capacity && output.data() == storage);

    tree.radius_neighbors(span<const double>(query), 2.0, output, 0);
    assert(output.size() == 2 && output[0].index == 2 && output[1].index == 3);

    tree.radius_neighbors(points[0], 2.0, output, -1, false);
    sort(output.begin(), output.end(), [](const auto& neighbor1, const auto& neighbor2) {
        return pair(neighbor1.squared_distance, neighbor1.index) <
               pair(neighbor2.squared_distance, neighbor2.index);
    });
    assert(output.size() == 3);
    assert(output[0].index == 0 && output[1].index == 2 && output[2].index == 3);
    assert(output.capacity() == capacity && output.data() == storage);

    tree.radius_neighbors_of(0, 2.0, output);
    assert(output.size() == 2 && output[0].index == 2 && output[1].index == 3);
}

void test_overflow() {
    using Point = vector<double>;
    auto getter = [](const Point& point, int axis) { return point[axis]; };
    double maximum = numeric_limits<double>::max();
    bool failed = false;
    try {
        make_kd_tree_as<double>(vector<Point>{{-maximum}, {maximum}}, 1, getter);
    } catch (const overflow_error&) {
        failed = true;
    }
    assert(failed);

    auto tree = make_kd_tree_as<double>(vector<Point>{{0}}, 1, getter);
    failed = false;
    try {
        tree.nearest_neighbor(Point{maximum});
    } catch (const overflow_error&) {
        failed = true;
    }
    assert(failed);

    failed = false;
    try {
        tree.radius_neighbors(Point{0}, maximum);
    } catch (const overflow_error&) {
        failed = true;
    }
    assert(failed);

    using WidePoint = vector<long double>;
    auto wide_getter = [](const WidePoint& point, int axis) { return point[axis]; };
    if (numeric_limits<long double>::max() > (long double)numeric_limits<double>::max()) {
        failed = false;
        try {
            make_kd_tree_as<double>(vector<WidePoint>{{numeric_limits<long double>::max()}}, 1, wide_getter);
        } catch (const invalid_argument&) {
            failed = true;
        }
        assert(failed);
    }
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

    long double infinity = numeric_limits<long double>::infinity();
    long double nan = numeric_limits<long double>::quiet_NaN();
    expect_exception<invalid_argument>([&] { tree.nearest_neighbor(TestPoint{nan, 0}); });
    expect_exception<invalid_argument>([&] {
        array<long double, 2> query = {0, infinity};
        tree.nearest_neighbor(span<const long double>(query));
    });
    expect_exception<invalid_argument>([&] { tree.radius_neighbors(TestPoint{0, 0}, infinity); });
    expect_exception<invalid_argument>([&] { tree.radius_neighbors(TestPoint{0, 0}, nan); });
    expect_exception<out_of_range>([&] { tree.nearest_neighbor(TestPoint{0, 0}, -2); });
    expect_exception<out_of_range>([&] { tree.point(-1); });
    expect_exception<out_of_range>([&] { tree.point(tree.point_count()); });
    expect_exception<out_of_range>([&] { tree.nearest_neighbor_of(-1); });
    expect_exception<out_of_range>([&] { tree.radius_neighbors_of(tree.point_count(), 1); });

    vector<KdNeighbor> output = {{23, 42}};
    expect_exception<invalid_argument>([&] {
        tree.k_nearest_neighbors(TestPoint{0, 0}, -1, output);
    });
    assert(output.size() == 1 && output[0].index == 23 && output[0].squared_distance == 42);
    expect_exception<invalid_argument>([&] {
        tree.radius_neighbors(TestPoint{0, 0}, nan, output);
    });
    assert(output.size() == 1 && output[0].index == 23 && output[0].squared_distance == 42);
}

int main() {
    test_random();
    test_ties_and_ownership();
    test_move();
    test_scalar_and_output_buffers();
    test_overflow();
    test_invalid_inputs();
    cout << "ok\n";
}
