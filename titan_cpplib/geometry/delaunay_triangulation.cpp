#pragma once
#include <algorithm>
#include <array>
#include <boost/polygon/point_data.hpp>
#include <boost/polygon/voronoi.hpp>
#include <cstddef>
#include <limits>
#include <map>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
using namespace std;
namespace titan23 {
struct IntegerPoint {
    int x;
    int y;

    bool operator==(const IntegerPoint&) const = default;
};

struct DelaunayEdge {
    int point1;
    int point2;

    bool operator==(const DelaunayEdge&) const = default;
};

struct DelaunayTriangle {
    int point1;
    int point2;
    int point3;

    bool operator==(const DelaunayTriangle&) const = default;
};

struct DelaunayResult {
    vector<IntegerPoint> points;
    vector<int> input_to_point;
    vector<int> point_to_input;
    vector<DelaunayEdge> edges;
    vector<DelaunayTriangle> triangles;
    vector<vector<int>> neighbors;
};

namespace delaunay_internal {
inline __int128 orientation(const IntegerPoint& a, const IntegerPoint& b, const IntegerPoint& c) {
    __int128 bax = (__int128)b.x - a.x;
    __int128 bay = (__int128)b.y - a.y;
    __int128 cax = (__int128)c.x - a.x;
    __int128 cay = (__int128)c.y - a.y;
    return bax * cay - bay * cax;
}

inline DelaunayEdge make_edge(int a, int b) {
    if (a > b) swap(a, b);
    return {a, b};
}

inline DelaunayTriangle make_triangle(int a, int b, int c, const vector<IntegerPoint>& points) {
    if (orientation(points[a], points[b], points[c]) < 0) swap(b, c);
    if (b < a && b < c) {
        int old_a = a;
        a = b;
        b = c;
        c = old_a;
    } else if (c < a && c < b) {
        int old_a = a;
        a = c;
        c = b;
        b = old_a;
    }
    return {a, b, c};
}

inline bool edge_less(const DelaunayEdge& a, const DelaunayEdge& b) {
    return pair(a.point1, a.point2) < pair(b.point1, b.point2);
}

inline bool triangle_less(const DelaunayTriangle& a, const DelaunayTriangle& b) {
    return array<int, 3>{a.point1, a.point2, a.point3} < array<int, 3>{b.point1, b.point2, b.point3};
}
}

inline DelaunayResult delaunay_triangulation(const vector<IntegerPoint>& input_points) {
    if (input_points.size() > (size_t)numeric_limits<int>::max()) throw invalid_argument("delaunay_triangulation: input_points.size() must fit in int");
    DelaunayResult result;
    result.input_to_point.resize(input_points.size());
    map<pair<int, int>, int> point_index;
    for (int input_index = 0; input_index < (int)input_points.size(); ++input_index) {
        const auto& point = input_points[input_index];
        auto [it, inserted] = point_index.emplace(pair(point.x, point.y), (int)result.points.size());
        if (inserted) {
            result.points.push_back(point);
            result.point_to_input.push_back(input_index);
        }
        result.input_to_point[input_index] = it->second;
    }
    int point_count = (int)result.points.size();
    result.neighbors.resize((size_t)point_count);
    if (point_count <= 1) return result;
    vector<boost::polygon::point_data<int>> boost_points;
    boost_points.reserve((size_t)point_count);
    for (const auto& point : result.points) boost_points.emplace_back(point.x, point.y);
    boost::polygon::voronoi_diagram<double> diagram;
    boost::polygon::construct_voronoi(boost_points.begin(), boost_points.end(), &diagram);
    for (const auto& half_edge : diagram.edges()) {
        if (!half_edge.is_primary()) continue;
        int a = (int)half_edge.cell()->source_index();
        int b = (int)half_edge.twin()->cell()->source_index();
        if (a < b) result.edges.push_back({a, b});
    }
    vector<size_t> last_seen((size_t)point_count, numeric_limits<size_t>::max());
    vector<int> incident_points;
    incident_points.reserve(8);
    size_t vertex_index = 0;
    for (const auto& vertex : diagram.vertices()) {
        size_t current_vertex = vertex_index++;
        const auto* start = vertex.incident_edge();
        if (start == nullptr) continue;
        incident_points.clear();
        const auto* half_edge = start;
        do {
            int point = (int)half_edge->cell()->source_index();
            if (last_seen[point] != current_vertex) {
                last_seen[point] = current_vertex;
                incident_points.push_back(point);
            }
            half_edge = half_edge->rot_next();
        } while (half_edge != start);
        if (incident_points.size() < 3) continue;
        for (int i = 1; i + 1 < (int)incident_points.size(); ++i) {
            int a = incident_points[0];
            int b = incident_points[i];
            int c = incident_points[i + 1];
            if (delaunay_internal::orientation(result.points[a], result.points[b], result.points[c]) == 0) continue;
            auto triangle = delaunay_internal::make_triangle(a, b, c, result.points);
            result.triangles.push_back(triangle);
            result.edges.push_back(delaunay_internal::make_edge(triangle.point1, triangle.point2));
            result.edges.push_back(delaunay_internal::make_edge(triangle.point2, triangle.point3));
            result.edges.push_back(delaunay_internal::make_edge(triangle.point3, triangle.point1));
        }
    }
    sort(result.edges.begin(), result.edges.end(), delaunay_internal::edge_less);
    result.edges.erase(unique(result.edges.begin(), result.edges.end()), result.edges.end());
    sort(result.triangles.begin(), result.triangles.end(), delaunay_internal::triangle_less);
    result.triangles.erase(unique(result.triangles.begin(), result.triangles.end()), result.triangles.end());
    for (auto [a, b] : result.edges) {
        result.neighbors[a].push_back(b);
        result.neighbors[b].push_back(a);
    }
    for (auto& neighbors : result.neighbors) sort(neighbors.begin(), neighbors.end());
    return result;
}

template <class Point, class GetX, class GetY>
DelaunayResult delaunay_triangulation(const vector<Point>& points, GetX get_x, GetY get_y) {
    using X = remove_cv_t<remove_reference_t<invoke_result_t<GetX&, const Point&>>>;
    using Y = remove_cv_t<remove_reference_t<invoke_result_t<GetY&, const Point&>>>;
    static_assert(is_integral_v<X> && is_integral_v<Y>, "coordinate getters must return integral values");
    vector<IntegerPoint> integer_points;
    integer_points.reserve(points.size());
    for (const auto& point : points) {
        auto x = get_x(point);
        auto y = get_y(point);
        if (!in_range<int>(x) || !in_range<int>(y)) {
            throw invalid_argument("delaunay_triangulation: every coordinate must fit in int");
        }
        integer_points.push_back({(int)x, (int)y});
    }
    return delaunay_triangulation(integer_points);
}
}
