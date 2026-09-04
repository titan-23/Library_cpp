/// https://github.com/titan-23/Library_cpp/blob/main/test/geometry/kd_tree_2d_consistency_test.cpp
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <optional>
#include <random>
#include <utility>
#include <vector>
#include "titan_cpplib/geometry/incremental_kd_tree_2d.cpp"
#include "titan_cpplib/geometry/kd_tree_2d.cpp"

using namespace std;
using namespace titan23;

using Point = array<double, 2>;
using Neighbor = KdNeighborT<double>;

void check(const vector<Neighbor>& a, const vector<Neighbor>& b) {
    assert(a.size() == b.size());
    for (int i = 0; i < (int)a.size(); ++i) {
        assert(a[i].index == b[i].index);
        assert(a[i].squared_distance == b[i].squared_distance);
    }
}

void compare(const KdTree2D<>& fixed, const IncrementalKdTree2D<>& incremental, const Point& q, int exclude) {
    optional<Neighbor> a = fixed.nearest_neighbor(q, exclude);
    optional<Neighbor> b = incremental.nearest_neighbor(q, exclude);
    assert(a.has_value() == b.has_value());
    if (a) {
        assert(a->index == b->index);
        assert(a->squared_distance == b->squared_distance);
    }
    for (int k : {0, 1, 3, 12, 1000}) {
        check(fixed.k_nearest_neighbors(q, k, exclude), incremental.k_nearest_neighbors(q, k, exclude));
    }
    for (double radius : {0.0, 0.1, 3.0, 30.0, 10000.0}) {
        check(fixed.radius_neighbors(q, radius, exclude), incremental.radius_neighbors(q, radius, exclude));
    }
}

void test_mixed_add_and_query() {
    for (int seed = 0; seed < 20; ++seed) {
        mt19937 rng(seed);
        vector<Point> points;
        IncrementalKdTree2D<> incremental;
        for (int step = 0; step < 300; ++step) {
            Point p;
            if (step % 17 == 0) p = {1, 1};
            else if (step % 11 == 0) p = {(double)step, (double)step};
            else {
                p = {
                    ((int)(rng() % 4001) - 2000) / 13.0,
                    ((int)(rng() % 4001) - 2000) / 17.0,
                };
            }
            incremental.add(p);
            points.push_back(p);
            if (step < 20 || step % 23 == 0) {
                KdTree2D<> fixed(points);
                for (int trial = 0; trial < 8; ++trial) {
                    Point q = {
                        ((int)(rng() % 20001) - 10000) / 19.0,
                        ((int)(rng() % 20001) - 10000) / 23.0,
                    };
                    int exclude = trial % 3 == 0 ? (int)(rng() % points.size()) : -1;
                    compare(fixed, incremental, q, exclude);
                }
            }
        }
        KdTree2D<> fixed(points);
        incremental.rebuild();
        for (int i = 0; i < 30; ++i) compare(fixed, incremental, points[i], i);
    }
}

void test_batch() {
    vector<Point> points = {{0, 0}, {1, 2}, {3, 5}};
    IncrementalKdTree2D<> incremental(points);
    vector<Point> batch = {{8, 13}, {21, 34}, {1, 2}, {-100, 50}};
    incremental.add_all(batch);
    points.insert(points.end(), batch.begin(), batch.end());
    KdTree2D<> fixed(points);
    for (Point q : {Point{0, 0}, Point{10, 10}, Point{-1000, -1000}, Point{1000, 1000}}) {
        compare(fixed, incremental, q, -1);
    }
}

int main() {
    test_mixed_add_and_query();
    test_batch();
    cout << "ok\n";
}
