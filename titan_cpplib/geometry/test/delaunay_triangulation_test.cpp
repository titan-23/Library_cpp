/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/geometry/test/delaunay_triangulation_test.cpp
#include <bits/stdc++.h>
#include "titan_cpplib/geometry/delaunay_triangulation.cpp"
using namespace std;
using namespace titan23;

__int128 orientation(const IntegerPoint& a, const IntegerPoint& b, const IntegerPoint& c) {
    return ((__int128)b.x - a.x) * ((__int128)c.y - a.y) - ((__int128)b.y - a.y) * ((__int128)c.x - a.x);
}

__int128 in_circle(const IntegerPoint& a, const IntegerPoint& b, const IntegerPoint& c, const IntegerPoint& point) {
    __int128 ax = (__int128)a.x - point.x;
    __int128 ay = (__int128)a.y - point.y;
    __int128 bx = (__int128)b.x - point.x;
    __int128 by = (__int128)b.y - point.y;
    __int128 cx = (__int128)c.x - point.x;
    __int128 cy = (__int128)c.y - point.y;
    return (ax * ax + ay * ay) * (bx * cy - by * cx)
         - (bx * bx + by * by) * (ax * cy - ay * cx)
         + (cx * cx + cy * cy) * (ax * by - ay * bx);
}

__int128 doubled_hull_area(vector<IntegerPoint> points) {
    sort(points.begin(), points.end(), [](const auto& a, const auto& b) { return pair(a.x, a.y) < pair(b.x, b.y); });
    points.erase(unique(points.begin(), points.end()), points.end());
    if (points.size() <= 2) return 0;
    vector<IntegerPoint> hull;
    for (const auto& point : points) {
        while (hull.size() >= 2 && orientation(hull[hull.size() - 2], hull.back(), point) <= 0) hull.pop_back();
        hull.push_back(point);
    }
    size_t lower_size = hull.size();
    for (int i = (int)points.size() - 2; i >= 0; --i) {
        while (hull.size() > lower_size && orientation(hull[hull.size() - 2], hull.back(), points[i]) <= 0) hull.pop_back();
        hull.push_back(points[i]);
    }
    hull.pop_back();
    __int128 area = 0;
    for (int i = 0; i < (int)hull.size(); ++i) {
        const auto& a = hull[i];
        const auto& b = hull[(i + 1) % hull.size()];
        area += (__int128)a.x * b.y - (__int128)a.y * b.x;
    }
    return area;
}

void check_result(const DelaunayResult& result) {
    int n = (int)result.points.size();
    assert((int)result.neighbors.size() == n);
    set<pair<int, int>> edges;
    for (auto [a, b] : result.edges) {
        assert(0 <= a && a < b && b < n);
        assert(edges.emplace(a, b).second);
        assert(binary_search(result.neighbors[a].begin(), result.neighbors[a].end(), b));
        assert(binary_search(result.neighbors[b].begin(), result.neighbors[b].end(), a));
    }
    set<array<int, 3>> triangles;
    __int128 triangle_area = 0;
    for (auto [a, b, c] : result.triangles) {
        assert(0 <= a && a < n && 0 <= b && b < n && 0 <= c && c < n);
        assert(orientation(result.points[a], result.points[b], result.points[c]) > 0);
        triangle_area += orientation(result.points[a], result.points[b], result.points[c]);
        assert(triangles.emplace(array<int, 3>{a, b, c}).second);
        assert(edges.contains(minmax(a, b)));
        assert(edges.contains(minmax(b, c)));
        assert(edges.contains(minmax(c, a)));
        for (int point = 0; point < n; ++point) {
            if (point == a || point == b || point == c) continue;
            assert(in_circle(result.points[a], result.points[b], result.points[c], result.points[point]) <= 0);
        }
    }
    assert(triangle_area == doubled_hull_area(result.points));
}

void test_small_cases() {
    assert(delaunay_triangulation(vector<IntegerPoint>{}).points.empty());
    auto one = delaunay_triangulation(vector<IntegerPoint>{{3, 4}});
    assert(one.edges.empty() && one.triangles.empty());
    auto duplicate = delaunay_triangulation(vector<IntegerPoint>{{3, 4}, {3, 4}, {5, 6}});
    assert(duplicate.points.size() == 2);
    assert(duplicate.input_to_point == vector<int>({0, 0, 1}));
    assert(duplicate.point_to_input == vector<int>({0, 2}));
    assert(duplicate.edges.size() == 1);
    auto line = delaunay_triangulation(vector<IntegerPoint>{{3, 0}, {0, 0}, {2, 0}, {1, 0}});
    check_result(line);
    assert(line.edges.size() == 3);
    assert(line.triangles.empty());
    auto square = delaunay_triangulation(vector<IntegerPoint>{{0, 0}, {10, 0}, {10, 10}, {0, 10}});
    check_result(square);
    assert(square.edges.size() == 5);
    assert(square.triangles.size() == 2);
    vector<IntegerPoint> circle = {
        {5, 0}, {4, 3}, {3, 4}, {0, 5}, {-3, 4}, {-4, 3}, {-5, 0}, {-4, -3}, {-3, -4},
        {0, -5}, {3, -4}, {4, -3}
    };
    auto cocircular = delaunay_triangulation(circle);
    check_result(cocircular);
    assert(cocircular.triangles.size() == circle.size() - 2);
}

void test_random() {
    mt19937 random(23);
    for (int n = 2; n <= 80; ++n) {
        for (int trial = 0; trial < 20; ++trial) {
            set<pair<int, int>> used;
            vector<IntegerPoint> points;
            while ((int)points.size() < n) {
                int x = (int)(random() % 2001) - 1000;
                int y = (int)(random() % 2001) - 1000;
                if (used.emplace(x, y).second) points.push_back({x, y});
            }
            check_result(delaunay_triangulation(points));
        }
    }
}

void test_coordinate_getters() {
    vector<pair<long long, long long>> points = {{0, 0}, {5, 0}, {0, 5}};
    auto result = delaunay_triangulation(points, [](const auto& point) { return point.first; }, [](const auto& point) { return point.second; });
    assert(result.triangles.size() == 1);
    bool failed = false;
    try {
        vector<pair<long long, long long>> outside = {{(long long)numeric_limits<int>::max() + 1, 0}};
        delaunay_triangulation(outside, [](const auto& point) { return point.first; }, [](const auto& point) { return point.second; });
    } catch (const invalid_argument&) {
        failed = true;
    }
    assert(failed);
}

int main() {
    test_small_cases();
    test_random();
    test_coordinate_getters();
    cout << "ok\n";
}
