/// https://github.com/titan-23/Library_cpp/blob/main/test/geometry/kd_tree_2d_test.cpp
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include "titan_cpplib/geometry/kd_tree_2d.cpp"
using namespace std;
using namespace titan23;

using Scalar = double;
using Point = array<Scalar, 2>;
using Neighbor = KdNeighborT<Scalar>;

bool neighbor_less(const Neighbor& a, const Neighbor& b) {
    if (a.squared_distance != b.squared_distance) {
        return a.squared_distance < b.squared_distance;
    }
    return a.index < b.index;
}

vector<Neighbor> brute_force(const vector<Point>& points, const Point& query, int excluded_index = -1) {
    vector<Neighbor> result;
    for (int i = 0; i < (int)points.size(); ++i) {
        if (i == excluded_index) continue;
        Scalar dx = points[i][0] - query[0];
        Scalar dy = points[i][1] - query[1];
        result.push_back({i, dx * dx + dy * dy});
    }
    sort(result.begin(), result.end(), neighbor_less);
    return result;
}

void check_neighbors(const vector<Neighbor>& actual, const vector<Neighbor>& expected) {
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
    for (int n = 0; n <= 120; ++n) {
        vector<Point> points(n);
        for (Point& p : points) {
            p[0] = (int)(random() % 41) - 20;
            p[1] = (int)(random() % 41) - 20;
        }
        auto tree = make_kd_tree_2d(points);
        assert(tree.point_count() == n);
        assert(tree.points() == points);
        for (int trial = 0; trial < 8; ++trial) {
            Point query = {
                (Scalar)((int)(random() % 201) - 100),
                (Scalar)((int)(random() % 201) - 100),
            };
            int excluded = n == 0 || trial % 2 == 0 ? -1 : (int)(random() % n);
            auto expected = brute_force(points, query, excluded);

            auto nearest = tree.nearest_neighbor(query, excluded);
            auto nearest_xy = tree.nearest_neighbor(query[0], query[1], excluded);
            if (expected.empty()) {
                assert(!nearest);
                assert(!nearest_xy);
            } else {
                check_neighbors({*nearest}, {expected[0]});
                check_neighbors({*nearest_xy}, {expected[0]});
            }

            for (int count : {0, 1, 2, 7, 30, 200}) {
                auto selected = expected;
                selected.resize(min(count, (int)selected.size()));
                check_neighbors(tree.k_nearest_neighbors(query, count, excluded), selected);
                vector<Neighbor> out = {{-2, -1}};
                tree.k_nearest_neighbors(query[0], query[1], count, out, excluded);
                check_neighbors(out, selected);
            }

            Scalar radius = (int)(random() % 40);
            auto within = expected;
            Scalar squared_radius = radius * radius;
            within.erase(remove_if(within.begin(), within.end(), [&](const Neighbor& neighbor) {
                return neighbor.squared_distance > squared_radius;
            }), within.end());
            check_neighbors(tree.radius_neighbors(query, radius, excluded), within);
            auto unsorted = tree.radius_neighbors(query, radius, excluded, false);
            sort(unsorted.begin(), unsorted.end(), neighbor_less);
            check_neighbors(unsorted, within);
        }

        for (int point = 0; point < min(n, 12); ++point) {
            auto expected = brute_force(points, points[point], point);
            auto nearest = tree.nearest_neighbor_of(point);
            if (expected.empty()) assert(!nearest);
            else check_neighbors({*nearest}, {expected[0]});

            auto selected = expected;
            selected.resize(min(9, (int)selected.size()));
            check_neighbors(tree.k_nearest_neighbors_of(point, 9), selected);
            vector<Neighbor> out;
            tree.k_nearest_neighbors_of(point, 9, out);
            check_neighbors(out, selected);

            Scalar radius = 5;
            auto within = expected;
            within.erase(remove_if(within.begin(), within.end(), [](const Neighbor& neighbor) {
                return neighbor.squared_distance > 25;
            }), within.end());
            check_neighbors(tree.radius_neighbors_of(point, radius), within);
            tree.radius_neighbors_of(point, radius, out);
            check_neighbors(out, within);
        }
    }
}

void test_duplicates_and_ties() {
    vector<Point> points = {
        {0, 0}, {0, 0}, {-1, 0}, {1, 0}, {0, -1}, {0, 1}, {0, 0},
    };
    auto tree = make_kd_tree_2d(points);
    auto zero = tree.radius_neighbors(Point{0, 0}, 0);
    check_neighbors(zero, {{0, 0}, {1, 0}, {6, 0}});
    auto nearest = tree.nearest_neighbor_of(0);
    assert(nearest && nearest->index == 1 && nearest->squared_distance == 0);
    auto neighbors = tree.k_nearest_neighbors(Point{0, 0}, 7);
    check_neighbors(neighbors, brute_force(points, {0, 0}));
    assert(neighbors[3].index == 2);
    assert(neighbors[3].distance() == 1);

    vector<Point> equal_points(256, {3, -4});
    auto equal_tree = make_kd_tree_2d(equal_points);
    auto outside = equal_tree.k_nearest_neighbors(100.0, -4.0, 256);
    assert(outside.size() == equal_points.size());
    for (int i = 0; i < (int)outside.size(); ++i) {
        assert(outside[i].index == i);
        assert(outside[i].squared_distance == 97.0 * 97.0);
    }

    auto boundary_tree = make_kd_tree_2d(vector<Point>{{3, 4}, {6, 8}});
    check_neighbors(boundary_tree.radius_neighbors(0, 0, 5), {{0, 25}});
    Scalar below = nextafter(5.0, 0.0);
    assert(boundary_tree.radius_neighbors(0, 0, below).empty());
}

void test_outside_and_collinear() {
    vector<Point> points;
    for (int i = 0; i < 511; ++i) points.push_back({(Scalar)(2 * i), 0});
    auto tree = make_kd_tree_2d(points);
    vector<Point> queries = {
        {-1e9, 3}, {1e9, 3}, {500, -1e9}, {500, 1e9}, {-1e9, -1e9}, {1e9, 1e9},
    };
    for (const Point& query : queries) {
        auto expected = brute_force(points, query);
        check_neighbors({*tree.nearest_neighbor(query)}, {expected[0]});
        expected.resize(40);
        check_neighbors(tree.k_nearest_neighbors(query, 40), expected);
    }

    Point boundary_query = {-50, 0};
    auto expected = brute_force(points, boundary_query);
    expected.erase(remove_if(expected.begin(), expected.end(), [](const Neighbor& neighbor) {
        return neighbor.squared_distance > 60.0 * 60.0;
    }), expected.end());
    check_neighbors(tree.radius_neighbors(boundary_query, 60), expected);
}

void test_fractional_and_large_coordinates() {
    mt19937 random(91);
    for (int n = 1; n <= 80; ++n) {
        vector<Point> points(n);
        for (Point& p : points) {
            p[0] = ((int)(random() % 2001) - 1000) / 7.0;
            p[1] = ((int)(random() % 2001) - 1000) / 11.0;
        }
        auto tree = make_kd_tree_2d(points);
        for (int trial = 0; trial < 5; ++trial) {
            Point query = {
                ((int)(random() % 8001) - 4000) / 13.0,
                ((int)(random() % 8001) - 4000) / 17.0,
            };
            auto expected = brute_force(points, query);
            check_neighbors({*tree.nearest_neighbor(query)}, {expected[0]});
            int count = (int)(random() % 20);
            expected.resize(min(count, n));
            check_neighbors(tree.k_nearest_neighbors(query, count), expected);
        }
    }

    Scalar base = 1e100;
    Scalar step1 = nextafter(base, numeric_limits<Scalar>::infinity());
    Scalar step2 = nextafter(step1, numeric_limits<Scalar>::infinity());
    vector<Point> points = {{base, base}, {step1, base}, {step2, step1}, {base, step2}};
    auto tree = make_kd_tree_2d(points);
    vector<Point> queries = {
        {nextafter(base, -numeric_limits<Scalar>::infinity()), base},
        {nextafter(step2, numeric_limits<Scalar>::infinity()), step2},
    };
    for (const Point& query : queries) {
        check_neighbors(tree.k_nearest_neighbors(query, 4), brute_force(points, query));
    }
}

void test_output_reuse() {
    vector<Point> points;
    for (int i = 0; i < 100; ++i) points.push_back({(Scalar)(i % 10), (Scalar)(i / 10)});
    auto tree = make_kd_tree_2d(points);
    vector<Neighbor> out;
    out.reserve(256);
    Neighbor* storage = out.data();
    tree.k_nearest_neighbors(4.5, 4.5, 80, out);
    assert(out.data() == storage);
    auto expected = brute_force(points, {4.5, 4.5});
    expected.resize(80);
    check_neighbors(out, expected);

    out.push_back({-1, -1});
    tree.radius_neighbors(Point{4.5, 4.5}, 100, out);
    assert(out.data() == storage);
    check_neighbors(out, brute_force(points, {4.5, 4.5}));

    tree.radius_neighbors(4.5, 4.5, 2, out, -1, false);
    assert(out.data() == storage);
    sort(out.begin(), out.end(), neighbor_less);
    expected = brute_force(points, {4.5, 4.5});
    expected.erase(remove_if(expected.begin(), expected.end(), [](const Neighbor& neighbor) {
        return neighbor.squared_distance > 4;
    }), expected.end());
    check_neighbors(out, expected);
}

void test_conversion_factory() {
    struct CustomPoint {
        int x;
        int y;
        int payload;
    };
    vector<CustomPoint> points = {{1, 2, 10}, {-3, 4, 20}, {5, -6, 30}};
    auto getter = [](const CustomPoint& point, int axis) {
        return axis == 0 ? point.x : point.y;
    };
    auto tree = make_kd_tree_2d(points, getter);
    static_assert(is_same_v<decltype(tree), KdTree2D<double>>);
    assert(tree.points() == vector<Point>({{1, 2}, {-3, 4}, {5, -6}}));
    assert(tree.nearest_neighbor(Point{-2, 4})->index == 1);

    auto precise = make_kd_tree_2d_as<long double>(points, getter);
    static_assert(is_same_v<decltype(precise), KdTree2D<long double>>);
    assert(precise.nearest_neighbor(-2.0L, 4.0L)->index == 1);

    struct WidePoint {
        long double x;
        long double y;
    };
    if (numeric_limits<long double>::max() > (long double)numeric_limits<double>::max()) {
        vector<WidePoint> wide = {{numeric_limits<long double>::max(), 0}};
        expect_exception<invalid_argument>([&] {
            make_kd_tree_2d(wide, [](const WidePoint& p, int axis) { return axis == 0 ? p.x : p.y; });
        });
    }
}

void test_move() {
    auto tree = make_kd_tree_2d(vector<Point>{{0, 0}, {2, 3}});
    KdTree2D<> copy_assigned(vector<Point>{{9, 9}});
    copy_assigned = tree;
    copy_assigned = copy_assigned;
    assert(copy_assigned.points() == tree.points());

    KdTree2D<> moved(move(tree));
    assert(moved.point_count() == 2);
    assert(tree.point_count() == 0);
    assert(!tree.nearest_neighbor(0, 0));

    KdTree2D<> assigned;
    assigned = move(moved);
    assert(assigned.point_count() == 2);
    assert(moved.point_count() == 0);
    assert(!moved.nearest_neighbor(0, 0));
}

void test_invalid_inputs() {
    Scalar inf = numeric_limits<Scalar>::infinity();
    Scalar nan = numeric_limits<Scalar>::quiet_NaN();
    expect_exception<invalid_argument>([&] {
        KdTree2D<> tree(vector<Point>{{inf, 0}});
    });
    expect_exception<invalid_argument>([&] {
        KdTree2D<> tree(vector<Point>{{0, nan}});
    });

    auto tree = make_kd_tree_2d(vector<Point>{{0, 0}, {1, 1}});
    expect_exception<invalid_argument>([&] { tree.nearest_neighbor(nan, 0); });
    expect_exception<invalid_argument>([&] { tree.nearest_neighbor(0, inf); });
    expect_exception<out_of_range>([&] { tree.nearest_neighbor(0, 0, -2); });
    expect_exception<out_of_range>([&] { tree.nearest_neighbor(0, 0, 2); });
    expect_exception<out_of_range>([&] { tree.point(-1); });
    expect_exception<out_of_range>([&] { tree.point(2); });
    expect_exception<out_of_range>([&] { tree.nearest_neighbor_of(2); });
    expect_exception<invalid_argument>([&] { tree.k_nearest_neighbors(0, 0, -1); });
    expect_exception<invalid_argument>([&] { tree.radius_neighbors(0, 0, -1); });
    expect_exception<invalid_argument>([&] { tree.radius_neighbors(0, 0, nan); });
    expect_exception<invalid_argument>([&] { tree.radius_neighbors(0, 0, inf); });
    expect_exception<overflow_error>([&] {
        tree.radius_neighbors(0, 0, numeric_limits<Scalar>::max());
    });

    Scalar large = numeric_limits<Scalar>::max() / 2;
    auto overflow_tree = make_kd_tree_2d(vector<Point>{{large, 0}});
    expect_exception<overflow_error>([&] { overflow_tree.nearest_neighbor(-large, 0); });

    KdTree2D<> empty;
    assert(!empty.nearest_neighbor(0, 0));
    assert(empty.k_nearest_neighbors(0, 0, 10).empty());
    assert(empty.radius_neighbors(0, 0, 10).empty());
    expect_exception<out_of_range>([&] { empty.point(0); });

    auto one = make_kd_tree_2d(vector<Point>{{2, 3}});
    assert(!one.nearest_neighbor_of(0));
    assert(one.k_nearest_neighbors_of(0, 10).empty());
    assert(one.radius_neighbors_of(0, 10).empty());
}

int main() {
    test_random();
    test_duplicates_and_ties();
    test_outside_and_collinear();
    test_fractional_and_large_coordinates();
    test_output_reuse();
    test_conversion_factory();
    test_move();
    test_invalid_inputs();
    cout << "ok\n";
}
