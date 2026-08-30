/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/geometry/incremental_kd_tree_2d.cpp
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
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

template <class Scalar_ = double>
class IncrementalKdTree2D {
public:
    using Scalar = Scalar_;
    using Point = array<Scalar, 2>;
    using Neighbor = KdNeighborT<Scalar>;

    static_assert(is_floating_point_v<Scalar>, "IncrementalKdTree2D: Scalar must be a floating-point type");

    IncrementalKdTree2D() { path_.reserve(PATH_CAPACITY); }

    IncrementalKdTree2D(const IncrementalKdTree2D&) = default;
    IncrementalKdTree2D& operator=(const IncrementalKdTree2D& other) {
        if (this == &other) return *this;
        IncrementalKdTree2D copy(other);
        swap_with(copy);
        return *this;
    }

    IncrementalKdTree2D(IncrementalKdTree2D&& other) noexcept {
        *this = move(other);
    }

    IncrementalKdTree2D& operator=(IncrementalKdTree2D&& other) noexcept {
        if (this == &other) return *this;
        points_ = move(other.points_);
        nodes_ = move(other.nodes_);
        work_ = move(other.work_);
        path_ = move(other.path_);
        root_ = other.root_;
        lo_ = other.lo_;
        hi_ = other.hi_;
        depth_limit_ = other.depth_limit_;
        other.points_.clear();
        other.nodes_.clear();
        other.work_.clear();
        other.path_.clear();
        other.root_ = -1;
        other.lo_ = {};
        other.hi_ = {};
        other.depth_limit_ = -1;
        return *this;
    }

    explicit IncrementalKdTree2D(vector<Point> points) : points_(move(points)) {
        path_.reserve(PATH_CAPACITY);
        check_point_count(points_.size());
        for (const Point& p : points_) check_coordinates(p[0], p[1]);
        nodes_.resize(points_.size());
        work_.resize(points_.size());
        for (int i = 0; i < point_count(); ++i) work_[i] = i;
        root_ = build(0, point_count());
        update_bounds();
        update_depth_limit();
        work_.clear();
    }

    int point_count() const { return (int)points_.size(); }
    const vector<Point>& points() const { return points_; }

    const Point& point(int id) const {
        check_point(id);
        return points_[id];
    }

    void reserve(int n) {
        if (n < 0) throw invalid_argument("IncrementalKdTree2D::reserve: n must not be negative");
        points_.reserve(n);
        nodes_.reserve(n);
        work_.reserve(n);
    }

    int add(Scalar x, Scalar y) {
        check_coordinates(x, y);
        int id = point_count();
        if (id == numeric_limits<int>::max()) {
            throw length_error("IncrementalKdTree2D::add: point count must fit in int");
        }
        reserve_for_add(id + 1);
        if (path_.capacity() < PATH_CAPACITY) path_.reserve(PATH_CAPACITY);
        points_.push_back({x, y});
        nodes_.push_back({-1, -1, id, 1U});
        update_depth_limit();
        if (root_ == -1) {
            root_ = id;
            lo_ = hi_ = points_[id];
            return id;
        }

        lo_[0] = min(lo_[0], x);
        lo_[1] = min(lo_[1], y);
        hi_[0] = max(hi_[0], x);
        hi_[1] = max(hi_[1], y);

        path_.clear();
        int node = root_;
        while (true) {
            path_.push_back(node);
            int axis = node_axis(nodes_[node]);
            int& next = point_less(id, node, axis) ? nodes_[node].left : nodes_[node].right;
            if (next == -1) {
                next = id;
                set_node_axis(nodes_[id], axis ^ 1);
                break;
            }
            node = next;
        }

        for (int i = (int)path_.size() - 1; i >= 0; --i) pull(path_[i]);
        int bad = -1;
        if ((int)path_.size() > depth_limit_) {
            int child = id;
            for (int i = (int)path_.size() - 1; i >= 0; --i) {
                int par = path_[i];
                if ((long long)node_size(child) * 4 > (long long)node_size(par) * 3) {
                    bad = i;
                    break;
                }
                child = par;
            }
        }
        if (bad != -1) rebuild_path_subtree(bad);
        return id;
    }

    int add(const Point& p) { return add(p[0], p[1]); }

    int add_all(span<const Point> points) {
        int first = point_count();
        if (points.empty()) return first;
        if (points.size() > (size_t)numeric_limits<int>::max() - (size_t)first) {
            throw length_error("IncrementalKdTree2D::add_all: point count must fit in int");
        }
        for (const Point& p : points) check_coordinates(p[0], p[1]);
        if (overlaps_points(points)) {
            vector<Point> copy(points.begin(), points.end());
            return add_all(span<const Point>(copy));
        }
        int n = first + (int)points.size();
        reserve_for_add(n);
        points_.insert(points_.end(), points.begin(), points.end());
        nodes_.resize(n);
        rebuild();
        return first;
    }

    void rebuild() {
        int n = point_count();
        if (n == 0) {
            root_ = -1;
            depth_limit_ = -1;
            return;
        }
        work_.resize(n);
        for (int i = 0; i < n; ++i) work_[i] = i;
        root_ = build(0, n);
        update_bounds();
        update_depth_limit();
        work_.clear();
    }

    optional<Neighbor> nearest_neighbor(Scalar x, Scalar y, int exclude = -1) const {
        check_query(x, y, exclude);
        Neighbor best{-1, numeric_limits<Scalar>::infinity()};
        if (root_ == -1) return nullopt;
        if (outside_bounds(x, y)) search_nearest_cell(root_, x, y, lo_, hi_, exclude, best);
        else search_nearest(root_, x, y, exclude, best);
        if (best.index == -1) return nullopt;
        return best;
    }

    optional<Neighbor> nearest_neighbor(const Point& q, int exclude = -1) const {
        return nearest_neighbor(q[0], q[1], exclude);
    }

    optional<Neighbor> nearest_neighbor_of(int id) const {
        check_point(id);
        return nearest_neighbor(points_[id], id);
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
        check_query(x, y, exclude);
        if (k < 0) throw invalid_argument("IncrementalKdTree2D::k_nearest_neighbors: k must not be negative");
        out.clear();
        int available = point_count() - (exclude == -1 ? 0 : 1);
        k = min(k, available);
        if (k == 0) return;
        out.reserve(k);
        if (k == available) {
            for (int id = 0; id < point_count(); ++id) {
                if (id != exclude) out.push_back({id, squared_distance(id, x, y)});
            }
            sort(out.begin(), out.end(), NeighborLess{});
            return;
        }
        if (outside_bounds(x, y)) search_k_cell(root_, x, y, lo_, hi_, k, exclude, out);
        else search_k(root_, x, y, k, exclude, out);
        sort_heap(out.begin(), out.end(), NeighborLess{});
    }

    void k_nearest_neighbors(const Point& q, int k, vector<Neighbor>& out, int exclude = -1) const {
        k_nearest_neighbors(q[0], q[1], k, out, exclude);
    }

    vector<Neighbor> k_nearest_neighbors_of(int id, int k) const {
        check_point(id);
        return k_nearest_neighbors(points_[id], k, id);
    }

    void k_nearest_neighbors_of(int id, int k, vector<Neighbor>& out) const {
        check_point(id);
        k_nearest_neighbors(points_[id], k, out, id);
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
        check_query(x, y, exclude);
        Scalar radius2 = checked_square_radius(radius);
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
        if (outside_bounds(x, y)) search_radius_cell(root_, x, y, lo_, hi_, radius2, exclude, out);
        else search_radius(root_, x, y, radius2, exclude, out);
        if (sort_by_distance) sort(out.begin(), out.end(), NeighborLess{});
    }

    void radius_neighbors(const Point& q, Scalar radius, vector<Neighbor>& out, int exclude = -1,
                          bool sort_by_distance = true) const {
        radius_neighbors(q[0], q[1], radius, out, exclude, sort_by_distance);
    }

    vector<Neighbor> radius_neighbors_of(int id, Scalar radius, bool sort_by_distance = true) const {
        check_point(id);
        return radius_neighbors(points_[id], radius, id, sort_by_distance);
    }

    void radius_neighbors_of(int id, Scalar radius, vector<Neighbor>& out, bool sort_by_distance = true) const {
        check_point(id);
        radius_neighbors(points_[id], radius, out, id, sort_by_distance);
    }

private:
    struct Node {
        int left;
        int right;
        int min_id;
        uint32_t size_axis;
    };

    struct NeighborLess {
        bool operator()(const Neighbor& a, const Neighbor& b) const {
            if (a.squared_distance != b.squared_distance) return a.squared_distance < b.squared_distance;
            return a.index < b.index;
        }
    };

    static_assert(sizeof(Node) == 16);
    static constexpr uint32_t AXIS_BIT = uint32_t(1) << 31;
    static constexpr uint32_t SIZE_MASK = AXIS_BIT - 1;
    static constexpr int PATH_CAPACITY = 80;

    vector<Point> points_;
    vector<Node> nodes_;
    vector<int> work_;
    vector<int> path_;
    int root_ = -1;
    Point lo_{};
    Point hi_{};
    int depth_limit_ = -1;

    // DEPTH_SIZE[d] is ceil((4 / 3)^d); the next value no longer fits in int.
    inline static constexpr array<int, 75> DEPTH_SIZE = {
        1, 2, 2, 3, 4, 5, 6, 8, 10, 14, 18, 24, 32, 43, 57, 75, 100, 134, 178, 237, 316, 421, 561, 748, 997,
        1329, 1772, 2363, 3150, 4200, 5600, 7467, 9955, 13274, 17698, 23597, 31463, 41951, 55934, 74578,
        99438, 132584, 176778, 235704, 314272, 419029, 558705, 744939, 993252, 1324336, 1765781, 2354375,
        3139167, 4185555, 5580740, 7440987, 9921316, 13228421, 17637894, 23517192, 31356256, 41808341,
        55744455, 74325940, 99101253, 132135004, 176180005, 234906673, 313208897, 417611862, 556815816,
        742421088, 989894784, 1319859712, 1759812950
    };

    void swap_with(IncrementalKdTree2D& other) noexcept {
        points_.swap(other.points_);
        nodes_.swap(other.nodes_);
        work_.swap(other.work_);
        path_.swap(other.path_);
        swap(root_, other.root_);
        swap(lo_, other.lo_);
        swap(hi_, other.hi_);
        swap(depth_limit_, other.depth_limit_);
    }

    void update_depth_limit() {
        int n = point_count();
        while (depth_limit_ + 1 < (int)DEPTH_SIZE.size() && DEPTH_SIZE[depth_limit_ + 1] <= n) {
            ++depth_limit_;
        }
    }

    static void check_point_count(size_t n) {
        if (n > (size_t)numeric_limits<int>::max()) {
            throw length_error("IncrementalKdTree2D: point count must fit in int");
        }
    }

    static void check_coordinates(Scalar x, Scalar y) {
        if (!isfinite(x) || !isfinite(y)) {
            throw invalid_argument("IncrementalKdTree2D: every coordinate must be finite");
        }
    }

    void check_point(int id) const {
        if (id < 0 || id >= point_count()) throw out_of_range("IncrementalKdTree2D: point index is out of range");
    }

    void check_exclude(int id) const {
        if (id < -1 || id >= point_count()) {
            throw out_of_range("IncrementalKdTree2D: excluded index is out of range");
        }
    }

    void check_query(Scalar x, Scalar y, int exclude) const {
        check_coordinates(x, y);
        check_exclude(exclude);
    }

    static Scalar checked_square_radius(Scalar radius) {
        if (!isfinite(radius) || radius < 0) {
            throw invalid_argument("IncrementalKdTree2D::radius_neighbors: radius must be finite and nonnegative");
        }
        Scalar radius2 = radius * radius;
        if (!isfinite(radius2)) {
            throw overflow_error("IncrementalKdTree2D::radius_neighbors: squared radius does not fit in Scalar");
        }
        return radius2;
    }

    static size_t grown_capacity(size_t capacity, int needed) {
        size_t limit = (size_t)numeric_limits<int>::max();
        size_t grown = min(limit, capacity + max((size_t)1, capacity));
        return max(grown, (size_t)needed);
    }

    void reserve_for_add(int n) {
        if (points_.capacity() < (size_t)n) points_.reserve(grown_capacity(points_.capacity(), n));
        if (nodes_.capacity() < (size_t)n) nodes_.reserve(grown_capacity(nodes_.capacity(), n));
        if (work_.capacity() < (size_t)n) work_.reserve(grown_capacity(work_.capacity(), n));
    }

    bool point_less(int a, int b, int axis) const {
        if (points_[a][axis] != points_[b][axis]) return points_[a][axis] < points_[b][axis];
        return a < b;
    }

    bool overlaps_points(span<const Point> points) const {
        if (points.empty() || points_.empty()) return false;
        less<const Point*> before;
        const Point* first1 = points.data();
        const Point* last1 = first1 + points.size();
        const Point* first2 = points_.data();
        const Point* last2 = first2 + points_.size();
        return before(first1, last2) && before(first2, last1);
    }

    static int node_axis(const Node& node) { return (int)(node.size_axis >> 31); }
    static int packed_size(const Node& node) { return (int)(node.size_axis & SIZE_MASK); }
    static void set_node_axis(Node& node, int axis) {
        node.size_axis = (node.size_axis & SIZE_MASK) | ((uint32_t)axis << 31);
    }
    static void set_node_size(Node& node, int size) {
        node.size_axis = (node.size_axis & AXIS_BIT) | (uint32_t)size;
    }

    int node_size(int node) const { return node == -1 ? 0 : packed_size(nodes_[node]); }
    int node_min_id(int node) const { return node == -1 ? numeric_limits<int>::max() : nodes_[node].min_id; }

    void pull(int id) {
        Node& node = nodes_[id];
        set_node_size(node, 1 + node_size(node.left) + node_size(node.right));
        node.min_id = min(id, min(node_min_id(node.left), node_min_id(node.right)));
    }

    int widest_axis(int l, int r) const {
        Scalar min_x = points_[work_[l]][0];
        Scalar max_x = min_x;
        Scalar min_y = points_[work_[l]][1];
        Scalar max_y = min_y;
        for (int i = l + 1; i < r; ++i) {
            const Point& p = points_[work_[i]];
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

    int build(int l, int r) {
        if (l >= r) return -1;
        int axis = widest_axis(l, r);
        int mid = l + (r - l) / 2;
        nth_element(work_.begin() + l, work_.begin() + mid, work_.begin() + r, [&](int a, int b) {
            return point_less(a, b, axis);
        });
        int id = work_[mid];
        Node& node = nodes_[id];
        set_node_axis(node, axis);
        node.left = build(l, mid);
        node.right = build(mid + 1, r);
        pull(id);
        return id;
    }

    void collect(int id) {
        if (id == -1) return;
        work_.push_back(id);
        collect(nodes_[id].left);
        collect(nodes_[id].right);
    }

    void rebuild_path_subtree(int pos) {
        int old_root = path_[pos];
        work_.clear();
        collect(old_root);
        int new_root = build(0, (int)work_.size());
        if (pos == 0) root_ = new_root;
        else {
            int par = path_[pos - 1];
            if (nodes_[par].left == old_root) nodes_[par].left = new_root;
            else nodes_[par].right = new_root;
        }
        for (int i = pos - 1; i >= 0; --i) pull(path_[i]);
        work_.clear();
    }

    void update_bounds() {
        if (points_.empty()) return;
        lo_ = hi_ = points_[0];
        for (int i = 1; i < point_count(); ++i) {
            lo_[0] = min(lo_[0], points_[i][0]);
            lo_[1] = min(lo_[1], points_[i][1]);
            hi_[0] = max(hi_[0], points_[i][0]);
            hi_[1] = max(hi_[1], points_[i][1]);
        }
    }

    bool outside_bounds(Scalar x, Scalar y) const {
        return x < lo_[0] || x > hi_[0] || y < lo_[1] || y > hi_[1];
    }

    bool box_covered(Scalar x, Scalar y, Scalar radius2) const {
        Scalar dx = max(abs(x - lo_[0]), abs(x - hi_[0]));
        Scalar dy = max(abs(y - lo_[1]), abs(y - hi_[1]));
        return dx * dx + dy * dy <= radius2;
    }

    static Scalar checked_squared_distance(Scalar dx, Scalar dy) {
        Scalar dist2 = dx * dx + dy * dy;
        if (!isfinite(dist2)) {
            throw overflow_error("IncrementalKdTree2D: squared distance does not fit in Scalar");
        }
        return dist2;
    }

    Scalar squared_distance(int id, Scalar x, Scalar y) const {
        return checked_squared_distance(points_[id][0] - x, points_[id][1] - y);
    }

    static bool better(Scalar dist2, int id, const Neighbor& best) {
        return best.index == -1 || dist2 < best.squared_distance ||
               (dist2 == best.squared_distance && id < best.index);
    }

    bool can_improve(Scalar lb, int node, const Neighbor& best) const {
        if (node == -1) return false;
        if (best.index == -1 || lb < best.squared_distance) return true;
        if (lb > best.squared_distance) return false;
        return nodes_[node].min_id < best.index;
    }

    bool can_improve_k(Scalar lb, int node, int k, const vector<Neighbor>& out) const {
        if (node == -1) return false;
        if ((int)out.size() < k || lb < out.front().squared_distance) return true;
        if (lb > out.front().squared_distance) return false;
        return nodes_[node].min_id < out.front().index;
    }

    void add_candidate(int id, Scalar dist2, int k, vector<Neighbor>& out) const {
        Neighbor cand{id, dist2};
        if ((int)out.size() < k) {
            out.push_back(cand);
            push_heap(out.begin(), out.end(), NeighborLess{});
        } else if (NeighborLess{}(cand, out.front())) {
            // Replacing the heap root needs only one sift-down.
            int pos = 0;
            int size = (int)out.size();
            while (pos < size / 2) {
                int child = pos * 2 + 1;
                if (child + 1 < size && NeighborLess{}(out[child], out[child + 1])) ++child;
                if (!NeighborLess{}(cand, out[child])) break;
                out[pos] = out[child];
                pos = child;
            }
            out[pos] = cand;
        }
    }

    void search_nearest(int id, Scalar x, Scalar y, int exclude, Neighbor& best) const {
        if (id == -1) return;
        const Node& node = nodes_[id];
        int axis = node_axis(node);
        Scalar dx = points_[id][0] - x;
        Scalar dy = points_[id][1] - y;
        Scalar diff = axis == 0 ? -dx : -dy;
        int near = diff <= 0 ? node.left : node.right;
        int far = diff <= 0 ? node.right : node.left;
        if (id != exclude) {
            Scalar dist2 = checked_squared_distance(dx, dy);
            if (better(dist2, id, best)) best = {id, dist2};
        }
        if (near != -1) search_nearest(near, x, y, exclude, best);
        Scalar plane2 = diff * diff;
        if (can_improve(plane2, far, best)) search_nearest(far, x, y, exclude, best);
    }

    void search_k(int id, Scalar x, Scalar y, int k, int exclude, vector<Neighbor>& out) const {
        if (id == -1) return;
        const Node& node = nodes_[id];
        int axis = node_axis(node);
        Scalar dx = points_[id][0] - x;
        Scalar dy = points_[id][1] - y;
        Scalar diff = axis == 0 ? -dx : -dy;
        int near = diff <= 0 ? node.left : node.right;
        int far = diff <= 0 ? node.right : node.left;
        if (id != exclude) add_candidate(id, checked_squared_distance(dx, dy), k, out);
        if (near != -1) search_k(near, x, y, k, exclude, out);
        Scalar plane2 = diff * diff;
        if (can_improve_k(plane2, far, k, out)) search_k(far, x, y, k, exclude, out);
    }

    void search_radius(int id, Scalar x, Scalar y, Scalar radius2, int exclude, vector<Neighbor>& out) const {
        if (id == -1) return;
        const Node& node = nodes_[id];
        int axis = node_axis(node);
        Scalar dx = points_[id][0] - x;
        Scalar dy = points_[id][1] - y;
        Scalar diff = axis == 0 ? -dx : -dy;
        int near = diff <= 0 ? node.left : node.right;
        int far = diff <= 0 ? node.right : node.left;
        if (id != exclude) {
            Scalar dist2 = checked_squared_distance(dx, dy);
            if (dist2 <= radius2) out.push_back({id, dist2});
        }
        if (near != -1) search_radius(near, x, y, radius2, exclude, out);
        if (far != -1 && diff * diff <= radius2) search_radius(far, x, y, radius2, exclude, out);
    }

    static Scalar box_distance(Scalar x, Scalar y, const Point& lo, const Point& hi) {
        Scalar dx = x < lo[0] ? lo[0] - x : (x > hi[0] ? x - hi[0] : 0);
        Scalar dy = y < lo[1] ? lo[1] - y : (y > hi[1] ? y - hi[1] : 0);
        return dx * dx + dy * dy;
    }

    void child_cells(int id, Scalar x, Scalar y, const Point& lo, const Point& hi, int& near, int& far,
                     Point& near_lo, Point& near_hi, Point& far_lo, Point& far_hi) const {
        const Node& node = nodes_[id];
        int axis = node_axis(node);
        Scalar split = points_[id][axis];
        bool left_first = (axis == 0 ? x : y) <= split;
        near = left_first ? node.left : node.right;
        far = left_first ? node.right : node.left;
        near_lo = far_lo = lo;
        near_hi = far_hi = hi;
        if (left_first) {
            near_hi[axis] = split;
            far_lo[axis] = split;
        } else {
            near_lo[axis] = split;
            far_hi[axis] = split;
        }
    }

    void search_nearest_cell(int id, Scalar x, Scalar y, const Point& lo, const Point& hi, int exclude,
                             Neighbor& best) const {
        if (!can_improve(box_distance(x, y, lo, hi), id, best)) return;
        int near, far;
        Point near_lo, near_hi, far_lo, far_hi;
        child_cells(id, x, y, lo, hi, near, far, near_lo, near_hi, far_lo, far_hi);
        if (id != exclude) {
            Scalar dist2 = squared_distance(id, x, y);
            if (better(dist2, id, best)) best = {id, dist2};
        }
        search_nearest_cell(near, x, y, near_lo, near_hi, exclude, best);
        search_nearest_cell(far, x, y, far_lo, far_hi, exclude, best);
    }

    void search_k_cell(int id, Scalar x, Scalar y, const Point& lo, const Point& hi, int k, int exclude,
                       vector<Neighbor>& out) const {
        if (!can_improve_k(box_distance(x, y, lo, hi), id, k, out)) return;
        int near, far;
        Point near_lo, near_hi, far_lo, far_hi;
        child_cells(id, x, y, lo, hi, near, far, near_lo, near_hi, far_lo, far_hi);
        search_k_cell(near, x, y, near_lo, near_hi, k, exclude, out);
        if (id != exclude) add_candidate(id, squared_distance(id, x, y), k, out);
        search_k_cell(far, x, y, far_lo, far_hi, k, exclude, out);
    }

    void search_radius_cell(int id, Scalar x, Scalar y, const Point& lo, const Point& hi, Scalar radius2,
                            int exclude, vector<Neighbor>& out) const {
        if (id == -1 || box_distance(x, y, lo, hi) > radius2) return;
        int near, far;
        Point near_lo, near_hi, far_lo, far_hi;
        child_cells(id, x, y, lo, hi, near, far, near_lo, near_hi, far_lo, far_hi);
        search_radius_cell(near, x, y, near_lo, near_hi, radius2, exclude, out);
        if (id != exclude) {
            Scalar dist2 = squared_distance(id, x, y);
            if (dist2 <= radius2) out.push_back({id, dist2});
        }
        search_radius_cell(far, x, y, far_lo, far_hi, radius2, exclude, out);
    }
};

template <class Scalar = double>
auto make_incremental_kd_tree_2d(vector<array<Scalar, 2>> points) {
    return IncrementalKdTree2D<Scalar>(move(points));
}

template <class Scalar, class Point, class GetCoordinate>
auto make_incremental_kd_tree_2d_as(const vector<Point>& points, GetCoordinate get_coordinate) {
    using Coordinate = invoke_result_t<const GetCoordinate&, const Point&, int>;
    static_assert(is_convertible_v<Coordinate, Scalar>,
                  "make_incremental_kd_tree_2d_as: GetCoordinate must return a value convertible to Scalar");
    vector<array<Scalar, 2>> coordinates;
    coordinates.reserve(points.size());
    for (const Point& p : points) {
        Scalar x = detail::kd_checked_coordinate<Scalar>(
            invoke(get_coordinate, p, 0),
            "make_incremental_kd_tree_2d_as: every coordinate must be finite and fit in Scalar");
        Scalar y = detail::kd_checked_coordinate<Scalar>(
            invoke(get_coordinate, p, 1),
            "make_incremental_kd_tree_2d_as: every coordinate must be finite and fit in Scalar");
        coordinates.push_back({x, y});
    }
    return IncrementalKdTree2D<Scalar>(move(coordinates));
}

template <class Scalar = double, class Point, class GetCoordinate>
auto make_incremental_kd_tree_2d(const vector<Point>& points, GetCoordinate get_coordinate) {
    return make_incremental_kd_tree_2d_as<Scalar>(points, move(get_coordinate));
}

}
