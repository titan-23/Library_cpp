/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/geometry/kd_tree.cpp
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include "titan_cpplib/geometry/kd_tree_common.cpp"

using namespace std;

namespace titan23 {

template <class Point, class GetCoordinate, class Scalar_ = long double>
class KdTree {
public:
    using Scalar = Scalar_;
    using Coordinate = remove_cv_t<remove_reference_t<invoke_result_t<const GetCoordinate&, const Point&, int>>>;
    using Neighbor = KdNeighborT<Scalar>;

    static_assert(is_floating_point_v<Scalar>, "Scalar must be a floating-point type");
    static_assert(is_convertible_v<Coordinate, Scalar>, "GetCoordinate must return a value convertible to Scalar");

    KdTree(const KdTree&) = default;

    KdTree& operator=(const KdTree& other)
        requires (is_copy_constructible_v<GetCoordinate> && is_nothrow_swappable_v<GetCoordinate>) {
        if (this == &other) return *this;
        KdTree copy(other);
        swap_with(copy);
        return *this;
    }

    KdTree& operator=(const KdTree&)
        requires (!(is_copy_constructible_v<GetCoordinate> && is_nothrow_swappable_v<GetCoordinate>)) = delete;

    KdTree(KdTree&& other) noexcept(is_nothrow_move_constructible_v<GetCoordinate>)
        : get_coordinate_(move(other.get_coordinate_)),
          points_(move(other.points_)),
          dimension_(other.dimension_),
          coordinates_(move(other.coordinates_)),
          nodes_(move(other.nodes_)),
          root_(other.root_) {
        other.points_.clear();
        other.coordinates_.clear();
        other.nodes_.clear();
        other.dimension_ = 0;
        other.root_ = -1;
    }

    KdTree& operator=(KdTree&& other) noexcept(is_nothrow_move_assignable_v<GetCoordinate>)
        requires is_move_assignable_v<GetCoordinate> {
        if (this == &other) return *this;
        get_coordinate_ = move(other.get_coordinate_);
        points_ = move(other.points_);
        dimension_ = other.dimension_;
        coordinates_ = move(other.coordinates_);
        nodes_ = move(other.nodes_);
        root_ = other.root_;
        other.points_.clear();
        other.coordinates_.clear();
        other.nodes_.clear();
        other.dimension_ = 0;
        other.root_ = -1;
        return *this;
    }

    KdTree(vector<Point> points, int dimension, GetCoordinate get_coordinate)
        : get_coordinate_(move(get_coordinate)), points_(move(points)), dimension_(dimension) {
        if (dimension_ <= 0) throw invalid_argument("KdTree: dimension must be positive");
        if (points_.size() > (size_t)numeric_limits<int>::max()) {
            throw invalid_argument("KdTree: points.size() must fit in int");
        }
        size_t point_count = points_.size();
        if (point_count > numeric_limits<size_t>::max() / (size_t)dimension_) {
            throw length_error("KdTree: coordinate table is too large");
        }
        coordinates_.resize(point_count * (size_t)dimension_);
        for (int point = 0; point < (int)points_.size(); ++point) {
            for (int axis = 0; axis < dimension_; ++axis) {
                Scalar value = detail::kd_checked_coordinate<Scalar>(
                    invoke(get_coordinate_, points_[point], axis),
                    "KdTree: every coordinate must be finite and fit in Scalar"
                );
                coordinates_[(size_t)point * dimension_ + axis] = value;
            }
        }

        vector<int> order(points_.size());
        for (int point = 0; point < (int)points_.size(); ++point) order[point] = point;
        nodes_.reserve(points_.size());
        root_ = build(order, 0, (int)order.size());
    }

    int point_count() const { return (int)points_.size(); }
    int dimension() const { return dimension_; }
    const vector<Point>& points() const { return points_; }
    const Point& point(int id) const {
        check_point(id);
        return points_[id];
    }

    optional<Neighbor> nearest_neighbor(const Point& q, int exclude = -1) const {
        check_excluded_index(exclude);
        if (dimension_ == 0) return nullopt;
        vector<Scalar> coordinates = query_coordinates(q);
        return nearest_neighbor_impl(coordinates, exclude);
    }

    optional<Neighbor> nearest_neighbor(span<const Scalar> q, int exclude = -1) const {
        check_excluded_index(exclude);
        if (dimension_ == 0) return nullopt;
        check_query(q);
        return nearest_neighbor_impl(q, exclude);
    }

    optional<Neighbor> nearest_neighbor_of(int id) const {
        check_point(id);
        return nearest_neighbor_impl(point_coordinates(id), id);
    }

    vector<Neighbor> k_nearest_neighbors(const Point& q, int k, int exclude = -1) const {
        vector<Neighbor> out;
        k_nearest_neighbors(q, k, out, exclude);
        return out;
    }

    void k_nearest_neighbors(const Point& q, int k, vector<Neighbor>& out, int exclude = -1) const {
        check_excluded_index(exclude);
        int take = checked_result_count(k, exclude);
        if (dimension_ == 0) {
            out.clear();
            return;
        }
        vector<Scalar> coordinates = query_coordinates(q);
        k_nearest_neighbors_impl(coordinates, take, exclude, out);
    }

    vector<Neighbor> k_nearest_neighbors(span<const Scalar> q, int k, int exclude = -1) const {
        vector<Neighbor> out;
        k_nearest_neighbors(q, k, out, exclude);
        return out;
    }

    void k_nearest_neighbors(span<const Scalar> q, int k, vector<Neighbor>& out, int exclude = -1) const {
        check_excluded_index(exclude);
        int take = checked_result_count(k, exclude);
        if (dimension_ == 0) {
            out.clear();
            return;
        }
        check_query(q);
        k_nearest_neighbors_impl(q, take, exclude, out);
    }

    vector<Neighbor> k_nearest_neighbors_of(int id, int k) const {
        vector<Neighbor> out;
        k_nearest_neighbors_of(id, k, out);
        return out;
    }

    void k_nearest_neighbors_of(int id, int k, vector<Neighbor>& out) const {
        check_point(id);
        int take = checked_result_count(k, id);
        k_nearest_neighbors_impl(point_coordinates(id), take, id, out);
    }

    vector<Neighbor> radius_neighbors(const Point& q, Scalar radius, int exclude = -1,
                                      bool sort_by_distance = true) const {
        vector<Neighbor> out;
        radius_neighbors(q, radius, out, exclude, sort_by_distance);
        return out;
    }

    void radius_neighbors(const Point& q, Scalar radius, vector<Neighbor>& out, int exclude = -1,
                          bool sort_by_distance = true) const {
        check_excluded_index(exclude);
        Scalar radius2 = checked_squared_radius(radius);
        if (dimension_ == 0) {
            out.clear();
            return;
        }
        vector<Scalar> coordinates = query_coordinates(q);
        radius_neighbors_impl(coordinates, radius2, exclude, sort_by_distance, out);
    }

    vector<Neighbor> radius_neighbors(span<const Scalar> q, Scalar radius, int exclude = -1,
                                      bool sort_by_distance = true) const {
        vector<Neighbor> out;
        radius_neighbors(q, radius, out, exclude, sort_by_distance);
        return out;
    }

    void radius_neighbors(span<const Scalar> q, Scalar radius, vector<Neighbor>& out, int exclude = -1,
                          bool sort_by_distance = true) const {
        check_excluded_index(exclude);
        Scalar radius2 = checked_squared_radius(radius);
        if (dimension_ == 0) {
            out.clear();
            return;
        }
        check_query(q);
        radius_neighbors_impl(q, radius2, exclude, sort_by_distance, out);
    }

    vector<Neighbor> radius_neighbors_of(int id, Scalar radius, bool sort_by_distance = true) const {
        vector<Neighbor> out;
        radius_neighbors_of(id, radius, out, sort_by_distance);
        return out;
    }

    void radius_neighbors_of(int id, Scalar radius, vector<Neighbor>& out, bool sort_by_distance = true) const {
        check_point(id);
        Scalar radius2 = checked_squared_radius(radius);
        radius_neighbors_impl(point_coordinates(id), radius2, id, sort_by_distance, out);
    }

private:
    struct Node {
        int id;
        int left;
        int right;
        int axis;
    };

    struct NeighborLess {
        bool operator()(const Neighbor& a, const Neighbor& b) const {
            if (a.squared_distance != b.squared_distance) {
                return a.squared_distance < b.squared_distance;
            }
            return a.index < b.index;
        }
    };

    GetCoordinate get_coordinate_;
    vector<Point> points_;
    int dimension_;
    vector<Scalar> coordinates_;
    vector<Node> nodes_;
    int root_ = -1;

    void swap_with(KdTree& other) noexcept(is_nothrow_swappable_v<GetCoordinate>) {
        points_.swap(other.points_);
        swap(dimension_, other.dimension_);
        swap(get_coordinate_, other.get_coordinate_);
        coordinates_.swap(other.coordinates_);
        nodes_.swap(other.nodes_);
        swap(root_, other.root_);
    }

    Scalar coordinate(int id, int axis) const {
        return coordinates_[(size_t)id * dimension_ + axis];
    }

    span<const Scalar> point_coordinates(int id) const {
        return span<const Scalar>(coordinates_.data() + (size_t)id * dimension_, (size_t)dimension_);
    }

    int build(vector<int>& order, int l, int r) {
        if (l >= r) return -1;
        int axis = widest_axis(order, l, r);
        int mid = l + (r - l) / 2;
        nth_element(order.begin() + l, order.begin() + mid, order.begin() + r, [&](int a, int b) {
            Scalar x = coordinate(a, axis);
            Scalar y = coordinate(b, axis);
            if (x != y) return x < y;
            return a < b;
        });
        int node = (int)nodes_.size();
        nodes_.push_back({order[mid], -1, -1, axis});
        nodes_[node].left = build(order, l, mid);
        nodes_[node].right = build(order, mid + 1, r);
        return node;
    }

    int widest_axis(const vector<int>& order, int l, int r) const {
        int best_axis = 0;
        Scalar best_width = -1;
        for (int axis = 0; axis < dimension_; ++axis) {
            Scalar lo = coordinate(order[l], axis);
            Scalar hi = lo;
            for (int i = l + 1; i < r; ++i) {
                Scalar value = coordinate(order[i], axis);
                lo = min(lo, value);
                hi = max(hi, value);
            }
            Scalar width = hi - lo;
            if (!isfinite(width)) throw overflow_error("KdTree: coordinate spread does not fit in Scalar");
            if (width > best_width) {
                best_width = width;
                best_axis = axis;
            }
        }
        return best_axis;
    }

    vector<Scalar> query_coordinates(const Point& q) const {
        vector<Scalar> coordinates(dimension_);
        for (int axis = 0; axis < dimension_; ++axis) {
            coordinates[axis] = detail::kd_checked_coordinate<Scalar>(
                invoke(get_coordinate_, q, axis), "KdTree: every query coordinate must be finite and fit in Scalar"
            );
        }
        return coordinates;
    }

    Scalar squared_distance(int id, const Scalar* q) const {
        Scalar dist2 = 0;
        for (int axis = 0; axis < dimension_; ++axis) {
            Scalar diff = coordinate(id, axis) - q[axis];
            Scalar diff2 = diff * diff;
            if (!isfinite(diff2) || dist2 > numeric_limits<Scalar>::max() - diff2) {
                throw overflow_error("KdTree: squared distance does not fit in Scalar");
            }
            dist2 += diff2;
        }
        return dist2;
    }

    static bool better(Scalar dist2, int id, const Neighbor& best) {
        return best.index == -1 || dist2 < best.squared_distance ||
               (dist2 == best.squared_distance && id < best.index);
    }

    optional<Neighbor> nearest_neighbor_impl(span<const Scalar> q, int exclude) const {
        Neighbor best{-1, numeric_limits<Scalar>::infinity()};
        search_nearest(root_, q.data(), exclude, best);
        if (best.index == -1) return nullopt;
        return best;
    }

    void search_nearest(int node, const Scalar* q, int exclude, Neighbor& best) const {
        if (node == -1) return;
        const Node& cur = nodes_[node];
        Scalar diff = q[cur.axis] - coordinate(cur.id, cur.axis);
        int near = diff <= 0 ? cur.left : cur.right;
        int far = diff <= 0 ? cur.right : cur.left;
        search_nearest(near, q, exclude, best);
        if (cur.id != exclude) {
            Scalar dist2 = squared_distance(cur.id, q);
            if (better(dist2, cur.id, best)) best = {cur.id, dist2};
        }
        Scalar plane2 = diff * diff;
        if (best.index == -1 || plane2 <= best.squared_distance) {
            search_nearest(far, q, exclude, best);
        }
    }

    void k_nearest_neighbors_impl(span<const Scalar> q, int k, int exclude, vector<Neighbor>& out) const {
        out.clear();
        out.reserve(k);
        if (k == 0) return;
        int available = point_count() - (exclude == -1 ? 0 : 1);
        if (k == available) {
            for (int id = 0; id < point_count(); ++id) {
                if (id != exclude) {
                    out.push_back({id, squared_distance(id, q.data())});
                }
            }
            sort_neighbors(out);
            return;
        }
        search_k_nearest(root_, q.data(), exclude, k, out);
        sort_neighbors(out);
    }

    void search_k_nearest(int node, const Scalar* q, int exclude, int k, vector<Neighbor>& out) const {
        if (node == -1) return;
        const Node& cur = nodes_[node];
        Scalar diff = q[cur.axis] - coordinate(cur.id, cur.axis);
        int near = diff <= 0 ? cur.left : cur.right;
        int far = diff <= 0 ? cur.right : cur.left;
        search_k_nearest(near, q, exclude, k, out);
        if (cur.id != exclude) {
            Neighbor candidate{cur.id, squared_distance(cur.id, q)};
            if ((int)out.size() < k) {
                out.push_back(candidate);
                push_heap(out.begin(), out.end(), NeighborLess{});
            } else if (NeighborLess{}(candidate, out.front())) {
                // Replacing the heap root needs only one sift-down.
                int pos = 0;
                int size = (int)out.size();
                while (pos < size / 2) {
                    int child = pos * 2 + 1;
                    if (child + 1 < size && NeighborLess{}(out[child], out[child + 1])) ++child;
                    if (!NeighborLess{}(candidate, out[child])) break;
                    out[pos] = out[child];
                    pos = child;
                }
                out[pos] = candidate;
            }
        }
        Scalar plane2 = diff * diff;
        if ((int)out.size() < k || plane2 <= out.front().squared_distance) {
            search_k_nearest(far, q, exclude, k, out);
        }
    }

    void radius_neighbors_impl(span<const Scalar> q, Scalar radius2, int exclude, bool sort_by_distance,
                               vector<Neighbor>& out) const {
        out.clear();
        search_radius(root_, q.data(), exclude, radius2, out);
        if (sort_by_distance) sort_neighbors(out);
    }

    void search_radius(int node, const Scalar* q, int exclude, Scalar radius2, vector<Neighbor>& out) const {
        if (node == -1) return;
        const Node& cur = nodes_[node];
        Scalar diff = q[cur.axis] - coordinate(cur.id, cur.axis);
        int near = diff <= 0 ? cur.left : cur.right;
        int far = diff <= 0 ? cur.right : cur.left;
        search_radius(near, q, exclude, radius2, out);
        if (cur.id != exclude) {
            Scalar dist2 = squared_distance(cur.id, q);
            if (dist2 <= radius2) out.push_back({cur.id, dist2});
        }
        if (diff * diff <= radius2) search_radius(far, q, exclude, radius2, out);
    }

    static void sort_neighbors(vector<Neighbor>& out) {
        sort(out.begin(), out.end(), NeighborLess{});
    }

    int checked_result_count(int k, int exclude) const {
        if (k < 0) throw invalid_argument("KdTree::k_nearest_neighbors: count must not be negative");
        int available = point_count() - (exclude == -1 ? 0 : 1);
        return min(k, available);
    }

    static Scalar checked_squared_radius(Scalar radius) {
        if (!isfinite(radius) || radius < 0) {
            throw invalid_argument("KdTree::radius_neighbors: radius must be finite and nonnegative");
        }
        Scalar radius2 = radius * radius;
        if (!isfinite(radius2)) {
            throw overflow_error("KdTree::radius_neighbors: squared radius does not fit in Scalar");
        }
        return radius2;
    }

    void check_point(int id) const {
        if (id < 0 || id >= point_count()) throw out_of_range("KdTree: point index is out of range");
    }

    void check_excluded_index(int id) const {
        if (id < -1 || id >= point_count()) throw out_of_range("KdTree: excluded index is out of range");
    }

    void check_query(span<const Scalar> q) const {
        if (q.size() != (size_t)dimension_) {
            throw invalid_argument("KdTree: query dimension does not match the tree");
        }
        for (Scalar value : q) {
            if (!isfinite(value)) throw invalid_argument("KdTree: every query coordinate must be finite");
        }
    }
};

template <class Point, class GetCoordinate>
auto make_kd_tree(vector<Point> points, int dimension, GetCoordinate get_coordinate) {
    using Getter = decay_t<GetCoordinate>;
    return KdTree<Point, Getter>(move(points), dimension, move(get_coordinate));
}

template <class Scalar, class Point, class GetCoordinate>
auto make_kd_tree_as(vector<Point> points, int dimension, GetCoordinate get_coordinate) {
    using Getter = decay_t<GetCoordinate>;
    return KdTree<Point, Getter, Scalar>(move(points), dimension, move(get_coordinate));
}

}
