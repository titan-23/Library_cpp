/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/geometry/voronoi_diagram.cpp
#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>
#include "titan_cpplib/geometry/delaunay_triangulation.cpp"
using namespace std;
namespace titan23 {
struct VoronoiPoint {
    long double x;
    long double y;
};

struct VoronoiBounds {
    long double min_x;
    long double min_y;
    long double max_x;
    long double max_y;
};

struct VoronoiResult {
    DelaunayResult delaunay;
    VoronoiBounds bounds;
    vector<vector<VoronoiPoint>> cells;
};

namespace voronoi_internal {
inline bool inside(const VoronoiPoint& point, long double a, long double b, long double c) {
    return a * point.x + b * point.y <= c;
}

inline VoronoiPoint intersection(const VoronoiPoint& from, const VoronoiPoint& to, long double a, long double b, long double c) {
    long double from_value = a * from.x + b * from.y - c;
    long double to_value = a * to.x + b * to.y - c;
    long double ratio = from_value / (from_value - to_value);
    ratio = clamp(ratio, (long double)0, (long double)1);
    return {from.x + (to.x - from.x) * ratio, from.y + (to.y - from.y) * ratio};
}

inline void clip(vector<VoronoiPoint>& polygon, vector<VoronoiPoint>& work, long double a, long double b, long double c) {
    work.clear();
    if (polygon.empty()) return;
    if (work.capacity() < polygon.size() + 1) work.reserve(polygon.size() + 1);
    VoronoiPoint previous = polygon.back();
    bool previous_inside = inside(previous, a, b, c);
    for (const auto& point : polygon) {
        bool point_inside = inside(point, a, b, c);
        if (previous_inside != point_inside) work.push_back(intersection(previous, point, a, b, c));
        if (point_inside) work.push_back(point);
        previous = point;
        previous_inside = point_inside;
    }
    if (work.size() >= 2 && work.front().x == work.back().x && work.front().y == work.back().y) work.pop_back();
    polygon.swap(work);
}

inline void check_bounds(const VoronoiBounds& bounds) {
    if (!isfinite(bounds.min_x) || !isfinite(bounds.min_y) || !isfinite(bounds.max_x) || !isfinite(bounds.max_y)) {
        throw invalid_argument("voronoi_diagram: bounds must be finite");
    }
    if (!(bounds.min_x < bounds.max_x) || !(bounds.min_y < bounds.max_y)) {
        throw invalid_argument("voronoi_diagram: bounds must have positive width and height");
    }
}
}

inline VoronoiResult voronoi_diagram(const vector<IntegerPoint>& input_points, VoronoiBounds bounds) {
    voronoi_internal::check_bounds(bounds);
    VoronoiResult result;
    result.delaunay = delaunay_triangulation(input_points);
    result.bounds = bounds;
    int point_count = (int)result.delaunay.points.size();
    result.cells.resize((size_t)point_count);
    vector<VoronoiPoint> work;
    for (int site = 0; site < point_count; ++site) {
        auto& polygon = result.cells[site];
        polygon.reserve(result.delaunay.neighbors[site].size() + 4);
        polygon = {
            {bounds.min_x, bounds.min_y},
            {bounds.max_x, bounds.min_y},
            {bounds.max_x, bounds.max_y},
            {bounds.min_x, bounds.max_y}
        };
        const auto& point = result.delaunay.points[site];
        for (int neighbor : result.delaunay.neighbors[site]) {
            const auto& other = result.delaunay.points[neighbor];
            long double a = 2 * ((long double)other.x - point.x);
            long double b = 2 * ((long double)other.y - point.y);
            long double c = (long double)other.x * other.x + (long double)other.y * other.y
                          - (long double)point.x * point.x - (long double)point.y * point.y;
            voronoi_internal::clip(polygon, work, a, b, c);
            if (polygon.empty()) break;
        }
    }
    return result;
}

template <class Point, class GetX, class GetY>
VoronoiResult voronoi_diagram(const vector<Point>& points, GetX get_x, GetY get_y, VoronoiBounds bounds) {
    using X = remove_cv_t<remove_reference_t<invoke_result_t<GetX&, const Point&>>>;
    using Y = remove_cv_t<remove_reference_t<invoke_result_t<GetY&, const Point&>>>;
    static_assert(is_integral_v<X> && is_integral_v<Y>, "coordinate getters must return integral values");
    vector<IntegerPoint> integer_points;
    integer_points.reserve(points.size());
    for (const auto& point : points) {
        auto x = get_x(point);
        auto y = get_y(point);
        if (!in_range<int>(x) || !in_range<int>(y)) {
            throw invalid_argument("voronoi_diagram: every coordinate must fit in int");
        }
        integer_points.push_back({(int)x, (int)y});
    }
    return voronoi_diagram(integer_points, bounds);
}
}
