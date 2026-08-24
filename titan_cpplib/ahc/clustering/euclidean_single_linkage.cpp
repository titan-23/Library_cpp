/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ahc/clustering/euclidean_single_linkage.cpp
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>
#include "titan_cpplib/ahc/clustering/hierarchical_clustering.cpp"
#include "titan_cpplib/geometry/delaunay_triangulation.cpp"
using namespace std;
namespace titan23 {
namespace euclidean_single_linkage_internal {
class UnionFind {
public:
    explicit UnionFind(int n) : parent_((size_t)n, -1) {}

    int root(int x) {
        while (parent_[x] >= 0) {
            if (parent_[parent_[x]] >= 0) parent_[x] = parent_[parent_[x]];
            x = parent_[x];
        }
        return x;
    }

    int unite(int x, int y) {
        x = root(x);
        y = root(y);
        if (x == y) return x;
        if (parent_[x] > parent_[y]) swap(x, y);
        parent_[x] += parent_[y];
        parent_[y] = x;
        return x;
    }

private:
    vector<int> parent_;
};

struct Edge {
    int point1;
    int point2;
    __int128 squared_distance;
};

inline __int128 squared_distance(const IntegerPoint& a, const IntegerPoint& b) {
    __int128 dx = (__int128)a.x - b.x;
    __int128 dy = (__int128)a.y - b.y;
    return dx * dx + dy * dy;
}

inline HierarchicalClusteringResult build_result(int input_point_count, const DelaunayResult& delaunay) {
    HierarchicalClusteringResult result;
    result.point_count = input_point_count;
    if (input_point_count <= 1) return result;
    int unique_count = (int)delaunay.points.size();
    vector<int> cluster_node((size_t)unique_count, -1);
    vector<int> cluster_size((size_t)unique_count);
    vector<int> minimum_point((size_t)unique_count, -1);
    result.merges.reserve((size_t)input_point_count - 1);
    for (int point = 0; point < input_point_count; ++point) {
        int unique_point = delaunay.input_to_point[point];
        if (cluster_node[unique_point] == -1) {
            cluster_node[unique_point] = point;
            cluster_size[unique_point] = 1;
            minimum_point[unique_point] = point;
        } else {
            int new_node = input_point_count + (int)result.merges.size();
            int new_size = cluster_size[unique_point] + 1;
            result.merges.push_back({cluster_node[unique_point], point, 0, new_size});
            cluster_node[unique_point] = new_node;
            cluster_size[unique_point] = new_size;
        }
    }
    if (unique_count <= 1) return result;
    vector<Edge> edges;
    edges.reserve(delaunay.edges.size());
    for (auto [point1, point2] : delaunay.edges) {
        edges.push_back({point1, point2, squared_distance(delaunay.points[point1], delaunay.points[point2])});
    }
    sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
        if (a.squared_distance != b.squared_distance) return a.squared_distance < b.squared_distance;
        return pair(a.point1, a.point2) < pair(b.point1, b.point2);
    });
    UnionFind union_find(unique_count);
    for (const auto& edge : edges) {
        int root1 = union_find.root(edge.point1);
        int root2 = union_find.root(edge.point2);
        if (root1 == root2) continue;
        int left = cluster_node[root1];
        int right = cluster_node[root2];
        if (minimum_point[root1] > minimum_point[root2]) swap(left, right);
        int new_node = input_point_count + (int)result.merges.size();
        int new_size = cluster_size[root1] + cluster_size[root2];
        long double distance = sqrt((long double)edge.squared_distance);
        result.merges.push_back({left, right, distance, new_size});
        int root = union_find.unite(root1, root2);
        cluster_node[root] = new_node;
        cluster_size[root] = new_size;
        minimum_point[root] = min(minimum_point[root1], minimum_point[root2]);
    }
    if ((int)result.merges.size() != input_point_count - 1) throw logic_error("euclidean_single_linkage_2d: Delaunay graph is disconnected");
    return result;
}
}

inline HierarchicalClusteringResult euclidean_single_linkage_2d(const vector<IntegerPoint>& points) {
    if (points.size() > (size_t)numeric_limits<int>::max()) throw invalid_argument("euclidean_single_linkage_2d: points.size() must fit in int");
    return euclidean_single_linkage_internal::build_result((int)points.size(), delaunay_triangulation(points));
}

template <class Point, class GetX, class GetY>
HierarchicalClusteringResult euclidean_single_linkage_2d(const vector<Point>& points, GetX get_x, GetY get_y) {
    if (points.size() > (size_t)numeric_limits<int>::max()) throw invalid_argument("euclidean_single_linkage_2d: points.size() must fit in int");
    return euclidean_single_linkage_internal::build_result((int)points.size(), delaunay_triangulation(points, get_x, get_y));
}
}
