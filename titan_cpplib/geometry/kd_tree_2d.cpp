/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/geometry/kd_tree_2d.cpp
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include "titan_cpplib/geometry/kd_tree_common.cpp"
using namespace std;

namespace titan23 {

template <class Scalar_ = double>
class KdTree2D {
public:
    using Scalar = Scalar_;
    using Point = array<Scalar, 2>;
    using Neighbor = KdNeighborT<Scalar>;

    static_assert(is_floating_point_v<Scalar>, "KdTree2D: Scalar must be a floating-point type");

    KdTree2D() = default;

    KdTree2D(const KdTree2D&) = default;
    KdTree2D& operator=(const KdTree2D& other) {
        if (this == &other) return *this;
        KdTree2D copy(other);
        swap_with(copy);
        return *this;
    }

    KdTree2D(KdTree2D&& other) noexcept {
        *this = move(other);
    }

    KdTree2D& operator=(KdTree2D&& other) noexcept {
        if (this == &other) return *this;
        points_ = move(other.points_);
        nodes_ = move(other.nodes_);
        root_ = other.root_;
        root_min_ = other.root_min_;
        root_max_ = other.root_max_;
        other.points_.clear();
        other.nodes_.clear();
        other.root_ = -1;
        other.root_min_ = {};
        other.root_max_ = {};
        return *this;
    }

    explicit KdTree2D(vector<Point> points) : points_(move(points)) {
        build_tree();
    }

    int point_count() const { return (int)points_.size(); }
    const vector<Point>& points() const { return points_; }

    const Point& point(int id) const {
        check_point(id);
        return points_[id];
    }

    optional<Neighbor> nearest_neighbor(Scalar x, Scalar y, int exclude = -1) const {
        check_query(x, y);
        check_exclude(exclude);
        if (root_ == -1) return nullopt;
        Neighbor best{-1, numeric_limits<Scalar>::infinity()};
        if (outside_root(x, y)) search_nearest_cell(root_, point_count(), x, y, exclude, root_box(), best);
        else search_nearest_plane(root_, point_count(), x, y, exclude, best);
        if (best.index == -1) return nullopt;
        return best;
    }

    optional<Neighbor> nearest_neighbor(const Point& q, int exclude = -1) const {
        return nearest_neighbor(q[0], q[1], exclude);
    }

    optional<Neighbor> nearest_neighbor_of(int id) const {
        check_point(id);
        const Point& p = points_[id];
        return nearest_neighbor(p[0], p[1], id);
    }

    vector<Neighbor> k_nearest_neighbors(Scalar x, Scalar y, int k, int exclude = -1) const {
        vector<Neighbor> out;
        k_nearest_neighbors(x, y, k, out, exclude);
        return out;
    }

    vector<Neighbor> k_nearest_neighbors(const Point& q, int k, int exclude = -1) const {
        return k_nearest_neighbors(q[0], q[1], k, exclude);
    }

    void k_nearest_neighbors(Scalar x, Scalar y, int k, vector<Neighbor>& out, int exclude = -1) const {
        check_query(x, y);
        check_exclude(exclude);
        if (k < 0) throw invalid_argument("KdTree2D::k_nearest_neighbors: count must not be negative");
        int available = point_count() - (exclude == -1 ? 0 : 1);
        k = min(k, available);
        out.clear();
        if (k == 0) return;
        out.reserve(k);
        if (k == available) {
            for (int id = 0; id < point_count(); ++id) {
                if (id != exclude) out.push_back({id, squared_distance(id, x, y)});
            }
            sort(out.begin(), out.end(), NeighborLess{});
            return;
        }
        if (outside_root(x, y)) search_k_cell(root_, point_count(), x, y, exclude, k, root_box(), out);
        else search_k_plane(root_, point_count(), x, y, exclude, k, out);
        sort_heap(out.begin(), out.end(), NeighborLess{});
    }

    void k_nearest_neighbors(const Point& q, int k, vector<Neighbor>& out, int exclude = -1) const {
        k_nearest_neighbors(q[0], q[1], k, out, exclude);
    }

    vector<Neighbor> k_nearest_neighbors_of(int id, int k) const {
        vector<Neighbor> out;
        k_nearest_neighbors_of(id, k, out);
        return out;
    }

    void k_nearest_neighbors_of(int id, int k, vector<Neighbor>& out) const {
        check_point(id);
        const Point& p = points_[id];
        k_nearest_neighbors(p[0], p[1], k, out, id);
    }

    vector<Neighbor> radius_neighbors(Scalar x, Scalar y, Scalar radius, int exclude = -1,
                                      bool sort_by_distance = true) const {
        vector<Neighbor> out;
        radius_neighbors(x, y, radius, out, exclude, sort_by_distance);
        return out;
    }

    vector<Neighbor> radius_neighbors(const Point& q, Scalar radius, int exclude = -1,
                                      bool sort_by_distance = true) const {
        return radius_neighbors(q[0], q[1], radius, exclude, sort_by_distance);
    }

    void radius_neighbors(Scalar x, Scalar y, Scalar radius, vector<Neighbor>& out, int exclude = -1,
                          bool sort_by_distance = true) const {
        check_query(x, y);
        check_exclude(exclude);
        Scalar radius2 = check_radius(radius);
        out.clear();
        if (root_ == -1) return;
        if (box_covered(x, y, radius2)) {
            out.reserve(point_count() - (exclude == -1 ? 0 : 1));
            for (int id = 0; id < point_count(); ++id) {
                if (id == exclude) continue;
                out.push_back({id, squared_distance(id, x, y)});
            }
            if (sort_by_distance) sort(out.begin(), out.end(), NeighborLess{});
            return;
        }
        if (outside_root(x, y)) {
            Box box = root_box();
            search_radius_cell(root_, point_count(), x, y, exclude, radius2, box, out);
        } else {
            search_radius_plane(root_, point_count(), x, y, exclude, radius2, out);
        }
        if (sort_by_distance) sort(out.begin(), out.end(), NeighborLess{});
    }

    void radius_neighbors(const Point& q, Scalar radius, vector<Neighbor>& out, int exclude = -1,
                          bool sort_by_distance = true) const {
        radius_neighbors(q[0], q[1], radius, out, exclude, sort_by_distance);
    }

    vector<Neighbor> radius_neighbors_of(int id, Scalar radius, bool sort_by_distance = true) const {
        vector<Neighbor> out;
        radius_neighbors_of(id, radius, out, sort_by_distance);
        return out;
    }

    void radius_neighbors_of(int id, Scalar radius, vector<Neighbor>& out, bool sort_by_distance = true) const {
        check_point(id);
        const Point& p = points_[id];
        radius_neighbors(p[0], p[1], radius, out, id, sort_by_distance);
    }

private:
    // Preorder layout. For a subtree with count points, the child roots are recovered from count.
    struct Node {
        int id;
        uint32_t min_axis;
    };

    struct NeighborLess {
        bool operator()(const Neighbor& a, const Neighbor& b) const {
            if (a.squared_distance != b.squared_distance) return a.squared_distance < b.squared_distance;
            return a.index < b.index;
        }
    };

    static_assert(sizeof(Node) == 8);
    static constexpr uint32_t AXIS_BIT = uint32_t(1) << 31;
    static constexpr uint32_t MIN_MASK = AXIS_BIT - 1;

    struct Box {
        Scalar min_x;
        Scalar max_x;
        Scalar min_y;
        Scalar max_y;
    };

    vector<Point> points_;
    vector<Node> nodes_;
    int root_ = -1;
    Point root_min_{};
    Point root_max_{};

    void swap_with(KdTree2D& other) noexcept {
        points_.swap(other.points_);
        nodes_.swap(other.nodes_);
        swap(root_, other.root_);
        swap(root_min_, other.root_min_);
        swap(root_max_, other.root_max_);
    }

    static bool better(Scalar dist2, int id, const Neighbor& best) {
        return best.index == -1 || dist2 < best.squared_distance ||
               (dist2 == best.squared_distance && id < best.index);
    }

    void build_tree() {
        if (points_.size() > (size_t)numeric_limits<int>::max()) {
            throw invalid_argument("KdTree2D: points.size() must fit in int");
        }
        if (points_.empty()) return;
        root_min_ = points_[0];
        root_max_ = points_[0];
        for (const Point& p : points_) {
            if (!isfinite(p[0]) || !isfinite(p[1])) {
                throw invalid_argument("KdTree2D: every coordinate must be finite");
            }
            root_min_[0] = min(root_min_[0], p[0]);
            root_min_[1] = min(root_min_[1], p[1]);
            root_max_[0] = max(root_max_[0], p[0]);
            root_max_[1] = max(root_max_[1], p[1]);
        }
        vector<int> order(points_.size());
        for (int i = 0; i < point_count(); ++i) order[i] = i;
        nodes_.reserve(points_.size());
        root_ = build(order, 0, point_count());
    }

    int build(vector<int>& order, int l, int r) {
        if (l >= r) return -1;
        int axis = widest_axis(order, l, r);
        int mid = l + (r - l) / 2;
        nth_element(order.begin() + l, order.begin() + mid, order.begin() + r,
                    [&](int a, int b) {
                        if (points_[a][axis] != points_[b][axis]) {
                            return points_[a][axis] < points_[b][axis];
                        }
                        return a < b;
                    });
        int id = order[mid];
        int node = (int)nodes_.size();
        nodes_.push_back({id, 0});
        int lc = build(order, l, mid);
        int rc = build(order, mid + 1, r);
        int min_id = id;
        if (lc != -1) min_id = min(min_id, node_min_id(nodes_[lc]));
        if (rc != -1) min_id = min(min_id, node_min_id(nodes_[rc]));
        nodes_[node].min_axis = (uint32_t)min_id | ((uint32_t)axis << 31);
        return node;
    }

    int widest_axis(const vector<int>& order, int l, int r) const {
        Scalar min_x = points_[order[l]][0];
        Scalar max_x = min_x;
        Scalar min_y = points_[order[l]][1];
        Scalar max_y = min_y;
        for (int i = l + 1; i < r; ++i) {
            const Point& p = points_[order[i]];
            min_x = min(min_x, p[0]);
            max_x = max(max_x, p[0]);
            min_y = min(min_y, p[1]);
            max_y = max(max_y, p[1]);
        }
        Scalar width_x = max_x - min_x;
        Scalar width_y = max_y - min_y;
        if (!isfinite(width_x) || !isfinite(width_y)) {
            Scalar scale = max(max(abs(min_x), abs(max_x)), max(abs(min_y), abs(max_y)));
            width_x = max_x / scale - min_x / scale;
            width_y = max_y / scale - min_y / scale;
        }
        return width_y > width_x ? 1 : 0;
    }

    static int node_axis(const Node& node) { return (int)(node.min_axis >> 31); }
    static int node_min_id(const Node& node) { return (int)(node.min_axis & MIN_MASK); }

    static Scalar checked_squared_distance(Scalar dx, Scalar dy) {
        Scalar dist2 = dx * dx + dy * dy;
        if (!isfinite(dist2)) throw overflow_error("KdTree2D: squared distance does not fit in Scalar");
        return dist2;
    }

    Scalar squared_distance(int id, Scalar x, Scalar y) const {
        return checked_squared_distance(points_[id][0] - x, points_[id][1] - y);
    }

    static Scalar box_distance(Scalar x, Scalar y, const Box& box) {
        Scalar dx = 0;
        Scalar dy = 0;
        if (x < box.min_x) dx = box.min_x - x;
        else if (x > box.max_x) dx = x - box.max_x;
        if (y < box.min_y) dy = box.min_y - y;
        else if (y > box.max_y) dy = y - box.max_y;
        return dx * dx + dy * dy;
    }

    Box root_box() const {
        return {root_min_[0], root_max_[0], root_min_[1], root_max_[1]};
    }

    bool outside_root(Scalar x, Scalar y) const {
        return x < root_min_[0] || x > root_max_[0] || y < root_min_[1] || y > root_max_[1];
    }

    bool box_covered(Scalar x, Scalar y, Scalar radius2) const {
        Scalar dx = max(abs(x - root_min_[0]), abs(x - root_max_[0]));
        Scalar dy = max(abs(y - root_min_[1]), abs(y - root_max_[1]));
        return dx * dx + dy * dy <= radius2;
    }

    static void split_children(int node, int count, bool left_first, int& near, int& near_count, int& far,
                               int& far_count) {
        int left_count = count / 2;
        int right_count = count - left_count - 1;
        int left = node + 1;
        int right = left + left_count;
        near = left_first ? left : right;
        near_count = left_first ? left_count : right_count;
        far = left_first ? right : left;
        far_count = left_first ? right_count : left_count;
    }

    void split_cells(int node, int count, int axis, Scalar split, Scalar x, Scalar y, const Box& box, int& near,
                     int& near_count, int& far, int& far_count, Box& near_box, Box& far_box) const {
        bool left_first = (axis == 0 ? x : y) <= split;
        split_children(node, count, left_first, near, near_count, far, far_count);
        near_box = far_box = box;
        if (axis == 0) {
            if (left_first) {
                near_box.max_x = split;
                far_box.min_x = split;
            } else {
                near_box.min_x = split;
                far_box.max_x = split;
            }
        } else {
            if (left_first) {
                near_box.max_y = split;
                far_box.min_y = split;
            } else {
                near_box.min_y = split;
                far_box.max_y = split;
            }
        }
    }

    bool can_improve(Scalar bound, int node, const Neighbor& best) const {
        if (node == -1) return false;
        if (best.index == -1 || bound < best.squared_distance) return true;
        return bound == best.squared_distance && node_min_id(nodes_[node]) < best.index;
    }

    bool can_improve(Scalar bound, int node, int k, const vector<Neighbor>& out) const {
        if (node == -1) return false;
        if ((int)out.size() < k) return true;
        const Neighbor& worst = out.front();
        if (bound < worst.squared_distance) return true;
        return bound == worst.squared_distance && node_min_id(nodes_[node]) < worst.index;
    }

    static void offer(int id, Scalar dist2, int k, vector<Neighbor>& out) {
        Neighbor candidate{id, dist2};
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

    void search_nearest_plane(int node, int count, Scalar x, Scalar y, int exclude, Neighbor& best) const {
        if (count == 0) return;
        const Node& cur = nodes_[node];
        int id = cur.id;
        int axis = node_axis(cur);
        Scalar diff = (axis == 0 ? x : y) - points_[id][axis];
        int near, near_count, far, far_count;
        split_children(node, count, diff <= 0, near, near_count, far, far_count);
        search_nearest_plane(near, near_count, x, y, exclude, best);
        if (best.squared_distance == 0 && best.index <= node_min_id(cur)) return;
        if (id != exclude) {
            Scalar dist2 = squared_distance(id, x, y);
            if (better(dist2, id, best)) best = {id, dist2};
        }
        if (far_count != 0) {
            Scalar bound = diff * diff;
            if (can_improve(bound, far, best)) search_nearest_plane(far, far_count, x, y, exclude, best);
        }
    }

    void search_k_plane(int node, int count, Scalar x, Scalar y, int exclude, int k, vector<Neighbor>& out) const {
        if (count == 0) return;
        const Node& cur = nodes_[node];
        int id = cur.id;
        int axis = node_axis(cur);
        Scalar dx = points_[id][0] - x;
        Scalar dy = points_[id][1] - y;
        Scalar diff = axis == 0 ? -dx : -dy;
        int near, near_count, far, far_count;
        split_children(node, count, diff <= 0, near, near_count, far, far_count);
        search_k_plane(near, near_count, x, y, exclude, k, out);
        if (id != exclude) offer(id, checked_squared_distance(dx, dy), k, out);
        if (far_count != 0) {
            Scalar bound = diff * diff;
            if (can_improve(bound, far, k, out)) search_k_plane(far, far_count, x, y, exclude, k, out);
        }
    }

    void search_radius_plane(int node, int count, Scalar x, Scalar y, int exclude, Scalar radius2,
                             vector<Neighbor>& out) const {
        if (count == 0) return;
        const Node& cur = nodes_[node];
        int id = cur.id;
        int axis = node_axis(cur);
        Scalar dx = points_[id][0] - x;
        Scalar dy = points_[id][1] - y;
        Scalar diff = axis == 0 ? -dx : -dy;
        int near, near_count, far, far_count;
        split_children(node, count, diff <= 0, near, near_count, far, far_count);
        if (id != exclude) {
            Scalar dist2 = checked_squared_distance(dx, dy);
            if (dist2 <= radius2) out.push_back({id, dist2});
        }
        search_radius_plane(near, near_count, x, y, exclude, radius2, out);
        if (far_count != 0 && diff * diff <= radius2) {
            search_radius_plane(far, far_count, x, y, exclude, radius2, out);
        }
    }

    void search_nearest_cell(int node, int count, Scalar x, Scalar y, int exclude, const Box& box,
                             Neighbor& best) const {
        if (count == 0 || !can_improve(box_distance(x, y, box), node, best)) return;
        const Node& cur = nodes_[node];
        int id = cur.id;
        int axis = node_axis(cur);
        Scalar dx = points_[id][0] - x;
        Scalar dy = points_[id][1] - y;
        int near, near_count, far, far_count;
        Box near_box, far_box;
        split_cells(node, count, axis, points_[id][axis], x, y, box, near, near_count, far, far_count, near_box,
                    far_box);
        if (id != exclude) {
            Scalar dist2 = checked_squared_distance(dx, dy);
            if (better(dist2, id, best)) best = {id, dist2};
        }
        search_nearest_cell(near, near_count, x, y, exclude, near_box, best);
        search_nearest_cell(far, far_count, x, y, exclude, far_box, best);
    }

    void search_k_cell(int node, int count, Scalar x, Scalar y, int exclude, int k, const Box& box,
                       vector<Neighbor>& out) const {
        if (count == 0 || !can_improve(box_distance(x, y, box), node, k, out)) return;
        const Node& cur = nodes_[node];
        int id = cur.id;
        int axis = node_axis(cur);
        int near, near_count, far, far_count;
        Box near_box, far_box;
        split_cells(node, count, axis, points_[id][axis], x, y, box, near, near_count, far, far_count, near_box,
                    far_box);
        search_k_cell(near, near_count, x, y, exclude, k, near_box, out);
        if (id != exclude) offer(id, squared_distance(id, x, y), k, out);
        search_k_cell(far, far_count, x, y, exclude, k, far_box, out);
    }

    void search_radius_cell(int node, int count, Scalar x, Scalar y, int exclude, Scalar radius2, const Box& box,
                            vector<Neighbor>& out) const {
        if (count == 0 || box_distance(x, y, box) > radius2) return;
        const Node& cur = nodes_[node];
        int id = cur.id;
        int axis = node_axis(cur);
        int near, near_count, far, far_count;
        Box near_box, far_box;
        split_cells(node, count, axis, points_[id][axis], x, y, box, near, near_count, far, far_count, near_box,
                    far_box);
        search_radius_cell(near, near_count, x, y, exclude, radius2, near_box, out);
        if (id != exclude) {
            Scalar dist2 = squared_distance(id, x, y);
            if (dist2 <= radius2) out.push_back({id, dist2});
        }
        search_radius_cell(far, far_count, x, y, exclude, radius2, far_box, out);
    }

    void check_point(int id) const {
        if (id < 0 || id >= point_count()) throw out_of_range("KdTree2D: point index is out of range");
    }

    void check_exclude(int id) const {
        if (id < -1 || id >= point_count()) throw out_of_range("KdTree2D: excluded index is out of range");
    }

    static void check_query(Scalar x, Scalar y) {
        if (!isfinite(x) || !isfinite(y)) {
            throw invalid_argument("KdTree2D: every query coordinate must be finite");
        }
    }

    static Scalar check_radius(Scalar radius) {
        if (!isfinite(radius) || radius < 0) {
            throw invalid_argument("KdTree2D::radius_neighbors: radius must be finite and nonnegative");
        }
        Scalar radius2 = radius * radius;
        if (!isfinite(radius2)) {
            throw overflow_error("KdTree2D::radius_neighbors: squared radius does not fit in Scalar");
        }
        return radius2;
    }
};

template <class Scalar = double>
auto make_kd_tree_2d(vector<array<Scalar, 2>> points) {
    return KdTree2D<Scalar>(move(points));
}

template <class Scalar = double, class Point, class GetCoordinate>
auto make_kd_tree_2d(const vector<Point>& points, GetCoordinate get_coordinate) {
    using Coordinate = invoke_result_t<const GetCoordinate&, const Point&, int>;
    static_assert(is_convertible_v<Coordinate, Scalar>,
                  "make_kd_tree_2d: GetCoordinate must return a value convertible to Scalar");
    vector<array<Scalar, 2>> converted;
    converted.reserve(points.size());
    for (const Point& point : points) {
        Scalar x = detail::kd_checked_coordinate<Scalar>(
            invoke(get_coordinate, point, 0), "make_kd_tree_2d: every coordinate must be finite and fit in Scalar");
        Scalar y = detail::kd_checked_coordinate<Scalar>(
            invoke(get_coordinate, point, 1), "make_kd_tree_2d: every coordinate must be finite and fit in Scalar");
        converted.push_back({x, y});
    }
    return KdTree2D<Scalar>(move(converted));
}

template <class Scalar, class Point, class GetCoordinate>
auto make_kd_tree_2d_as(const vector<Point>& points, GetCoordinate get_coordinate) {
    return make_kd_tree_2d<Scalar>(points, move(get_coordinate));
}

}
