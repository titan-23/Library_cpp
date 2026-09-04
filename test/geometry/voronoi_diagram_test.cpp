/// https://github.com/titan-23/Library_cpp/blob/main/test/geometry/voronoi_diagram_test.cpp
#include <bits/stdc++.h>
#include "titan_cpplib/geometry/voronoi_diagram.cpp"
using namespace std;
using namespace titan23;

long double polygon_area(const vector<VoronoiPoint>& polygon) {
    long double area = 0;
    for (int i = 0; i < (int)polygon.size(); ++i) {
        const auto& a = polygon[i];
        const auto& b = polygon[(i + 1) % polygon.size()];
        area += a.x * b.y - a.y * b.x;
    }
    return area / 2;
}

void check_diagram(const VoronoiResult& result) {
    int n = (int)result.delaunay.points.size();
    assert((int)result.cells.size() == n);
    long double total_area = 0;
    for (int site = 0; site < n; ++site) {
        const auto& cell = result.cells[site];
        if (cell.empty()) continue;
        long double area = polygon_area(cell);
        assert(area >= -1e-10L);
        total_area += area;
        for (const auto& vertex : cell) {
            assert(result.bounds.min_x - 1e-10L <= vertex.x && vertex.x <= result.bounds.max_x + 1e-10L);
            assert(result.bounds.min_y - 1e-10L <= vertex.y && vertex.y <= result.bounds.max_y + 1e-10L);
            const auto& point = result.delaunay.points[site];
            long double dx = vertex.x - point.x;
            long double dy = vertex.y - point.y;
            long double site_distance = dx * dx + dy * dy;
            for (const auto& other : result.delaunay.points) {
                dx = vertex.x - other.x;
                dy = vertex.y - other.y;
                long double other_distance = dx * dx + dy * dy;
                long double scale = max({(long double)1, abs(site_distance), abs(other_distance)});
                assert(site_distance <= other_distance + 1e-12L * scale);
            }
        }
    }
    if (n > 0) {
        long double expected =
            (result.bounds.max_x - result.bounds.min_x) * (result.bounds.max_y - result.bounds.min_y);
        assert(abs(total_area - expected) <= 1e-11L * max((long double)1, expected));
    }
}

void test_small_cases() {
    VoronoiBounds bounds{0, 0, 10, 10};
    auto empty = voronoi_diagram(vector<IntegerPoint>{}, bounds);
    assert(empty.cells.empty());
    auto one = voronoi_diagram(vector<IntegerPoint>{{3, 7}}, bounds);
    assert(one.cells.size() == 1);
    assert(abs(polygon_area(one.cells[0]) - 100) < 1e-12L);
    auto two = voronoi_diagram(vector<IntegerPoint>{{0, 5}, {10, 5}}, bounds);
    check_diagram(two);
    assert(abs(polygon_area(two.cells[0]) - 50) < 1e-12L);
    assert(abs(polygon_area(two.cells[1]) - 50) < 1e-12L);
    auto square = voronoi_diagram(vector<IntegerPoint>{{0, 0}, {10, 0}, {10, 10}, {0, 10}}, bounds);
    check_diagram(square);
    for (const auto& cell : square.cells) assert(abs(polygon_area(cell) - 25) < 1e-12L);
    auto duplicate = voronoi_diagram(vector<IntegerPoint>{{0, 0}, {0, 0}, {10, 10}}, bounds);
    assert(duplicate.cells.size() == 2);
    assert(duplicate.delaunay.input_to_point == vector<int>({0, 0, 1}));
    check_diagram(duplicate);
}

void test_random() {
    mt19937 random(23);
    for (int n = 1; n <= 100; ++n) {
        for (int trial = 0; trial < 10; ++trial) {
            vector<IntegerPoint> points(n);
            for (auto& point : points) {
                point.x = (int)(random() % 401) - 200;
                point.y = (int)(random() % 401) - 200;
            }
            check_diagram(voronoi_diagram(points, {-100, -80, 120, 130}));
        }
    }
}

void test_coordinate_getters() {
    vector<pair<long long, long long>> points = {{0, 0}, {5, 0}, {0, 5}};
    auto result = voronoi_diagram(
        points,
        [](const auto& point) { return point.first; },
        [](const auto& point) { return point.second; },
        {-10, -10, 10, 10});
    check_diagram(result);
    bool failed = false;
    try {
        voronoi_diagram(vector<IntegerPoint>{{0, 0}}, {0, 0, 0, 1});
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
