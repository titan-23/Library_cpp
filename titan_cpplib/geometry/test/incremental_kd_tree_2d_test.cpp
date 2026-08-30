/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/geometry/test/incremental_kd_tree_2d_test.cpp
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include "titan_cpplib/geometry/incremental_kd_tree_2d.cpp"

using namespace std;
using namespace titan23;

using Point = array<double, 2>;
using Neighbor = KdNeighborT<double>;

double dist2(const Point& a, const Point& b) {
    double dx = a[0] - b[0];
    double dy = a[1] - b[1];
    return dx * dx + dy * dy;
}

vector<Neighbor> brute(const vector<Point>& points, const Point& q, int exclude = -1) {
    vector<Neighbor> out;
    for (int i = 0; i < (int)points.size(); ++i) {
        if (i != exclude) out.push_back({i, dist2(points[i], q)});
    }
    sort(out.begin(), out.end(), [](const Neighbor& a, const Neighbor& b) {
        return pair(a.squared_distance, a.index) < pair(b.squared_distance, b.index);
    });
    return out;
}

void check(const vector<Neighbor>& actual, const vector<Neighbor>& expected) {
    assert(actual.size() == expected.size());
    for (int i = 0; i < (int)actual.size(); ++i) {
        assert(actual[i].index == expected[i].index);
        assert(actual[i].squared_distance == expected[i].squared_distance);
    }
}

void check_queries(const IncrementalKdTree2D<>& tree, const vector<Point>& points, const Point& q) {
    auto expected = brute(points, q);
    auto nearest = tree.nearest_neighbor(q);
    if (expected.empty()) assert(!nearest);
    else check(vector<Neighbor>{*nearest}, vector<Neighbor>{expected[0]});

    for (int k : {0, 1, 2, 7, 1000, numeric_limits<int>::max()}) {
        auto selected = expected;
        selected.resize(min(k, (int)selected.size()));
        check(tree.k_nearest_neighbors(q, k), selected);
        vector<Neighbor> out;
        tree.k_nearest_neighbors(q, k, out);
        check(out, selected);
    }

    for (double radius : {0.0, 1.0, 5.0, 1000.0}) {
        vector<Neighbor> selected;
        for (const Neighbor& neighbor : expected) {
            if (neighbor.squared_distance <= radius * radius) selected.push_back(neighbor);
        }
        check(tree.radius_neighbors(q, radius), selected);
        auto unsorted = tree.radius_neighbors(q, radius, -1, false);
        sort(unsorted.begin(), unsorted.end(), [](const Neighbor& a, const Neighbor& b) {
            return pair(a.squared_distance, a.index) < pair(b.squared_distance, b.index);
        });
        check(unsorted, selected);
    }
}

void test_random_add() {
    mt19937 rng(23);
    IncrementalKdTree2D<> tree;
    tree.reserve(400);
    vector<Point> points;
    for (int i = 0; i < 300; ++i) {
        Point p{(int)(rng() % 61) - 30.0, (int)(rng() % 61) - 30.0};
        int id = tree.add(p);
        assert(id == i);
        points.push_back(p);
        if (i < 30 || i % 7 == 0) {
            for (int trial = 0; trial < 4; ++trial) {
                Point q{(int)(rng() % 101) - 50.0, (int)(rng() % 101) - 50.0};
                check_queries(tree, points, q);
            }
        }
    }
    for (int id = 0; id < (int)points.size(); ++id) {
        auto expected = brute(points, points[id], id);
        check(vector<Neighbor>{*tree.nearest_neighbor_of(id)}, vector<Neighbor>{expected[0]});
        auto selected = expected;
        selected.resize(min(8, (int)selected.size()));
        check(tree.k_nearest_neighbors_of(id, 8), selected);
        vector<Neighbor> within;
        for (const Neighbor& neighbor : expected) {
            if (neighbor.squared_distance <= 100) within.push_back(neighbor);
        }
        check(tree.radius_neighbors_of(id, 10), within);
        vector<Neighbor> out;
        tree.radius_neighbors_of(id, 10, out);
        check(out, within);
    }
}

void test_adversarial_add() {
    for (int mode = 0; mode < 4; ++mode) {
        IncrementalKdTree2D<> tree;
        vector<Point> points;
        for (int i = 0; i < 4000; ++i) {
            Point p;
            if (mode == 0) p = {(double)i, 0};
            else if (mode == 1) p = {0, (double)i};
            else if (mode == 2) p = {(double)i, (double)i};
            else p = {1, 1};
            tree.add(p);
            points.push_back(p);
        }
        for (Point q : {Point{-100, -100}, Point{1, 1}, Point{2000, 1000}, Point{10000, 10000}}) {
            check_queries(tree, points, q);
        }
        if (mode == 3) {
            auto nearest = tree.nearest_neighbor(1, 1);
            assert(nearest && nearest->index == 0 && nearest->squared_distance == 0);
            auto nearest_of = tree.nearest_neighbor_of(0);
            assert(nearest_of && nearest_of->index == 1 && nearest_of->squared_distance == 0);
        }
    }
}

void test_batch_rebuild_and_buffer() {
    vector<Point> initial = {{4, 4}, {0, 0}, {3, 1}};
    IncrementalKdTree2D<> tree(initial);
    vector<Point> added = {{2, 2}, {-3, 5}, {4, 4}};
    int first = tree.add_all(added);
    assert(first == 3);
    initial.insert(initial.end(), added.begin(), added.end());
    check_queries(tree, initial, {100, -100});
    auto before = tree.k_nearest_neighbors(1, 2, 100);
    tree.rebuild();
    check(tree.k_nearest_neighbors(1, 2, 100), before);

    vector<Neighbor> out;
    out.reserve(100);
    const Neighbor* ptr = out.data();
    tree.k_nearest_neighbors(0, 0, 4, out);
    assert(out.data() == ptr);
    tree.radius_neighbors(0, 0, 100, out);
    assert(out.data() == ptr);
}

void test_large_monotone_add() {
    const int n = 100000;
    IncrementalKdTree2D<> tree;
    for (int i = 0; i < n; ++i) tree.add((double)i, (double)i);
    assert(tree.nearest_neighbor(-1, -1)->index == 0);
    assert(tree.nearest_neighbor(n + 1.0, n + 1.0)->index == n - 1);
    auto knn = tree.k_nearest_neighbors(50000.1, 50000.1, 3);
    assert(knn[0].index == 50000);
    assert(knn[1].index == 50001);
    assert(knn[2].index == 49999);
}

void test_factory_and_long_double() {
    struct Data {
        int x;
        int y;
        int payload;
    };
    vector<Data> data = {{0, 0, 4}, {10, 0, 7}, {2, 3, 9}};
    auto getter = [](const Data& p, int axis) {
        return axis == 0 ? p.x : p.y;
    };
    auto tree = make_incremental_kd_tree_2d(data, getter);
    assert(tree.nearest_neighbor(2, 2)->index == 2);

    auto factory_precise = make_incremental_kd_tree_2d_as<long double>(data, getter);
    static_assert(is_same_v<decltype(factory_precise), IncrementalKdTree2D<long double>>);
    assert(factory_precise.nearest_neighbor(2.0L, 2.0L)->index == 2);

    IncrementalKdTree2D<long double> precise({{{0, 0}}, {{1, 1}}});
    assert(precise.nearest_neighbor(0.75L, 0.75L)->index == 1);

    struct WideData {
        long double x;
        long double y;
    };
    if (numeric_limits<long double>::max() > (long double)numeric_limits<double>::max()) {
        vector<WideData> wide = {{numeric_limits<long double>::max(), 0}};
        bool failed = false;
        try {
            make_incremental_kd_tree_2d(wide, [](const WideData& p, int axis) { return axis == 0 ? p.x : p.y; });
        } catch (const invalid_argument&) {
            failed = true;
        }
        assert(failed);
    }
}

void test_alias_and_move() {
    IncrementalKdTree2D<> tree(vector<Point>{{0, 0}, {1, 2}, {3, 4}});
    int first = tree.add_all(span<const Point>(tree.points()));
    assert(first == 3);
    assert(tree.point_count() == 6);
    for (int i = 0; i < 3; ++i) assert(tree.point(i) == tree.point(i + 3));

    span<const Point> part(tree.points().data() + 1, 2);
    first = tree.add_all(part);
    assert(first == 6);
    assert(tree.point(6) == Point({1, 2}));
    assert(tree.point(7) == Point({3, 4}));

    IncrementalKdTree2D<> copy_assigned(vector<Point>{{9, 9}});
    copy_assigned = tree;
    copy_assigned = copy_assigned;
    copy_assigned.add(10, 10);
    assert(copy_assigned.point_count() == 9);
    assert(tree.point_count() == 8);

    IncrementalKdTree2D<> moved(move(tree));
    assert(moved.point_count() == 8);
    assert(tree.point_count() == 0);
    assert(!tree.nearest_neighbor(0, 0));
    assert(tree.add(7, 8) == 0);
    assert(tree.nearest_neighbor(7, 8)->index == 0);

    IncrementalKdTree2D<> assigned;
    assigned = move(moved);
    assert(assigned.point_count() == 8);
    assert(moved.point_count() == 0);
    assert(moved.add(-1, -2) == 0);
}

void test_invalid() {
    IncrementalKdTree2D<> tree;
    bool failed = false;
    try {
        tree.reserve(-1);
    } catch (const invalid_argument&) {
        failed = true;
    }
    assert(failed);

    failed = false;
    try {
        tree.add(numeric_limits<double>::infinity(), 0);
    } catch (const invalid_argument&) {
        failed = true;
    }
    assert(failed && tree.point_count() == 0);

    tree.add(0, 0);
    vector<Point> before = tree.points();
    vector<Point> bad_batch = {{1, 2}, {numeric_limits<double>::infinity(), 3}};
    failed = false;
    try {
        tree.add_all(bad_batch);
    } catch (const invalid_argument&) {
        failed = true;
    }
    assert(failed && tree.points() == before);
    for (double x : {numeric_limits<double>::quiet_NaN(), numeric_limits<double>::infinity()}) {
        failed = false;
        try {
            tree.nearest_neighbor(x, 0);
        } catch (const invalid_argument&) {
            failed = true;
        }
        assert(failed);
    }
    failed = false;
    try {
        tree.k_nearest_neighbors(0, 0, -1);
    } catch (const invalid_argument&) {
        failed = true;
    }
    assert(failed);
    failed = false;
    try {
        tree.radius_neighbors(0, 0, -1);
    } catch (const invalid_argument&) {
        failed = true;
    }
    assert(failed);
    failed = false;
    try {
        tree.nearest_neighbor(0, 0, 1);
    } catch (const out_of_range&) {
        failed = true;
    }
    assert(failed);

    failed = false;
    try {
        tree.point(-1);
    } catch (const out_of_range&) {
        failed = true;
    }
    assert(failed);

    failed = false;
    try {
        tree.nearest_neighbor_of(1);
    } catch (const out_of_range&) {
        failed = true;
    }
    assert(failed);

    vector<Neighbor> out = {{23, 42}};
    failed = false;
    try {
        tree.k_nearest_neighbors(0, 0, -1, out);
    } catch (const invalid_argument&) {
        failed = true;
    }
    assert(failed && out.size() == 1 && out[0].index == 23);

    failed = false;
    try {
        tree.radius_neighbors(0, 0, numeric_limits<double>::quiet_NaN(), out);
    } catch (const invalid_argument&) {
        failed = true;
    }
    assert(failed && out.size() == 1 && out[0].index == 23);

    failed = false;
    try {
        tree.radius_neighbors(0, 0, numeric_limits<double>::max());
    } catch (const overflow_error&) {
        failed = true;
    }
    assert(failed);
}

int main() {
    test_random_add();
    test_adversarial_add();
    test_batch_rebuild_and_buffer();
    test_large_monotone_add();
    test_factory_and_long_double();
    test_alias_and_move();
    test_invalid();
    cout << "ok\n";
}
