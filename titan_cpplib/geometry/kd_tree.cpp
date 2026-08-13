#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
using namespace std;
namespace titan23 {
struct KdNeighbor {
    int index;
    long double squared_distance;

    long double distance() const { return sqrt(squared_distance); }
};

template <class Point, class GetCoordinate>
class KdTree {
public:
    using Coordinate = remove_cv_t<remove_reference_t<invoke_result_t<const GetCoordinate&, const Point&, int>>>;
    static_assert(is_convertible_v<Coordinate, long double>, "GetCoordinate must return a value convertible to long double");

    KdTree(vector<Point> points, int dimension, GetCoordinate get_coordinate)
        : points_(move(points)), dimension_(dimension), get_coordinate_(move(get_coordinate)) {
        if (dimension_ <= 0) throw invalid_argument("KdTree: dimension must be positive");
        if (points_.size() > (size_t)numeric_limits<int>::max()) throw invalid_argument("KdTree: points.size() must fit in int");
        size_t coordinate_count = points_.size();
        if (coordinate_count > numeric_limits<size_t>::max() / (size_t)dimension_) throw length_error("KdTree: coordinate table is too large");
        coordinates_.resize(coordinate_count * (size_t)dimension_);
        for (int point = 0; point < (int)points_.size(); ++point) {
            for (int axis = 0; axis < dimension_; ++axis) {
                long double coordinate = (long double)invoke(get_coordinate_, points_[point], axis);
                if (!isfinite(coordinate)) throw invalid_argument("KdTree: every coordinate must be finite");
                coordinates_[(size_t)point * dimension_ + axis] = coordinate;
            }
        }
        order_.resize(points_.size());
        for (int point = 0; point < (int)points_.size(); ++point) order_[point] = point;
        nodes_.reserve(points_.size());
        root_ = build(0, (int)order_.size());
        order_.clear();
        order_.shrink_to_fit();
    }

    int point_count() const { return (int)points_.size(); }
    int dimension() const { return dimension_; }
    const vector<Point>& points() const { return points_; }
    const Point& point(int index) const {
        check_point(index);
        return points_[index];
    }

    optional<KdNeighbor> nearest_neighbor(const Point& query, int excluded_index = -1) const {
        vector<long double> coordinates = query_coordinates(query);
        return nearest_neighbor(span<const long double>(coordinates), excluded_index);
    }

    optional<KdNeighbor> nearest_neighbor(span<const long double> query, int excluded_index = -1) const {
        check_query(query);
        check_excluded_index(excluded_index);
        KdNeighbor best{-1, numeric_limits<long double>::infinity()};
        search_nearest(root_, query.data(), excluded_index, best);
        if (best.index == -1) return nullopt;
        return best;
    }

    optional<KdNeighbor> nearest_neighbor_of(int point_index) const {
        check_point(point_index);
        return nearest_neighbor(point_coordinates(point_index), point_index);
    }

    vector<KdNeighbor> k_nearest_neighbors(const Point& query, int count, int excluded_index = -1) const {
        vector<long double> coordinates = query_coordinates(query);
        return k_nearest_neighbors(span<const long double>(coordinates), count, excluded_index);
    }

    vector<KdNeighbor> k_nearest_neighbors(span<const long double> query, int count, int excluded_index = -1) const {
        check_query(query);
        check_excluded_index(excluded_index);
        if (count < 0) throw invalid_argument("KdTree::k_nearest_neighbors: count must not be negative");
        int available = point_count() - (excluded_index == -1 ? 0 : 1);
        int result_count = min(count, available);
        if (result_count == 0) return {};
        using Candidate = pair<long double, int>;
        vector<Candidate> storage;
        storage.reserve((size_t)result_count);
        priority_queue<Candidate, vector<Candidate>> candidates(less<Candidate>(), move(storage));
        search_k_nearest(root_, query.data(), excluded_index, result_count, candidates);
        vector<KdNeighbor> result;
        result.reserve(candidates.size());
        while (!candidates.empty()) {
            auto [distance, index] = candidates.top();
            candidates.pop();
            result.push_back({index, distance});
        }
        sort_neighbors(result);
        return result;
    }

    vector<KdNeighbor> k_nearest_neighbors_of(int point_index, int count) const {
        check_point(point_index);
        return k_nearest_neighbors(point_coordinates(point_index), count, point_index);
    }

    vector<KdNeighbor> radius_neighbors(const Point& query, long double radius, int excluded_index = -1, bool sort_by_distance = true) const {
        vector<long double> coordinates = query_coordinates(query);
        return radius_neighbors(span<const long double>(coordinates), radius, excluded_index, sort_by_distance);
    }

    vector<KdNeighbor> radius_neighbors(span<const long double> query, long double radius, int excluded_index = -1, bool sort_by_distance = true) const {
        check_query(query);
        check_excluded_index(excluded_index);
        if (!isfinite(radius) || radius < 0) throw invalid_argument("KdTree::radius_neighbors: radius must be finite and nonnegative");
        long double squared_radius = radius * radius;
        vector<KdNeighbor> result;
        search_radius(root_, query.data(), excluded_index, squared_radius, result);
        if (sort_by_distance) sort_neighbors(result);
        return result;
    }

    vector<KdNeighbor> radius_neighbors_of(int point_index, long double radius, bool sort_by_distance = true) const {
        check_point(point_index);
        return radius_neighbors(point_coordinates(point_index), radius, point_index, sort_by_distance);
    }

private:
    struct Node {
        int point;
        int left;
        int right;
        int axis;
    };

    vector<Point> points_;
    int dimension_;
    GetCoordinate get_coordinate_;
    vector<long double> coordinates_;
    vector<int> order_;
    vector<Node> nodes_;
    int root_ = -1;

    long double coordinate(int point, int axis) const {
        return coordinates_[(size_t)point * dimension_ + axis];
    }

    span<const long double> point_coordinates(int point) const {
        return span<const long double>(coordinates_.data() + (size_t)point * dimension_, (size_t)dimension_);
    }

    int build(int left, int right) {
        if (left >= right) return -1;
        int axis = widest_axis(left, right);
        int middle = left + (right - left) / 2;
        nth_element(order_.begin() + left, order_.begin() + middle, order_.begin() + right, [&](int point1, int point2) {
            long double coordinate1 = coordinate(point1, axis);
            long double coordinate2 = coordinate(point2, axis);
            if (coordinate1 != coordinate2) return coordinate1 < coordinate2;
            return point1 < point2;
        });
        int node = (int)nodes_.size();
        nodes_.push_back({order_[middle], -1, -1, axis});
        int left_child = build(left, middle);
        int right_child = build(middle + 1, right);
        nodes_[node].left = left_child;
        nodes_[node].right = right_child;
        return node;
    }

    int widest_axis(int left, int right) const {
        int best_axis = 0;
        long double best_width = -1;
        for (int axis = 0; axis < dimension_; ++axis) {
            long double minimum = coordinate(order_[left], axis);
            long double maximum = minimum;
            for (int i = left + 1; i < right; ++i) {
                long double value = coordinate(order_[i], axis);
                minimum = min(minimum, value);
                maximum = max(maximum, value);
            }
            long double width = maximum - minimum;
            if (width > best_width) {
                best_width = width;
                best_axis = axis;
            }
        }
        return best_axis;
    }

    vector<long double> query_coordinates(const Point& query) const {
        vector<long double> coordinates((size_t)dimension_);
        for (int axis = 0; axis < dimension_; ++axis) coordinates[axis] = (long double)invoke(get_coordinate_, query, axis);
        check_query(coordinates);
        return coordinates;
    }

    long double squared_distance(int point, const long double* query) const {
        long double result = 0;
        for (int axis = 0; axis < dimension_; ++axis) {
            long double difference = coordinate(point, axis) - query[axis];
            result += difference * difference;
        }
        if (!isfinite(result)) throw overflow_error("KdTree: squared distance does not fit in long double");
        return result;
    }

    static bool better(long double distance, int index, const KdNeighbor& current) {
        return current.index == -1 || distance < current.squared_distance || (distance == current.squared_distance && index < current.index);
    }

    void search_nearest(int node, const long double* query, int excluded_index, KdNeighbor& best) const {
        if (node == -1) return;
        const auto& current = nodes_[node];
        long double difference = query[current.axis] - coordinate(current.point, current.axis);
        int near_child = difference <= 0 ? current.left : current.right;
        int far_child = difference <= 0 ? current.right : current.left;
        search_nearest(near_child, query, excluded_index, best);
        if (current.point != excluded_index) {
            long double distance = squared_distance(current.point, query);
            if (better(distance, current.point, best)) best = {current.point, distance};
        }
        long double plane_distance = difference * difference;
        if (best.index == -1 || plane_distance <= best.squared_distance) search_nearest(far_child, query, excluded_index, best);
    }

    using Candidate = pair<long double, int>;

    void search_k_nearest(int node, const long double* query, int excluded_index, int count, priority_queue<Candidate, vector<Candidate>>& candidates) const {
        if (node == -1) return;
        const auto& current = nodes_[node];
        long double difference = query[current.axis] - coordinate(current.point, current.axis);
        int near_child = difference <= 0 ? current.left : current.right;
        int far_child = difference <= 0 ? current.right : current.left;
        search_k_nearest(near_child, query, excluded_index, count, candidates);
        if (current.point != excluded_index) {
            long double distance = squared_distance(current.point, query);
            Candidate candidate{distance, current.point};
            if ((int)candidates.size() < count) candidates.push(candidate);
            else if (candidate < candidates.top()) {
                candidates.pop();
                candidates.push(candidate);
            }
        }
        long double plane_distance = difference * difference;
        if ((int)candidates.size() < count || plane_distance <= candidates.top().first) {
            search_k_nearest(far_child, query, excluded_index, count, candidates);
        }
    }

    void search_radius(int node, const long double* query, int excluded_index, long double squared_radius, vector<KdNeighbor>& result) const {
        if (node == -1) return;
        const auto& current = nodes_[node];
        long double difference = query[current.axis] - coordinate(current.point, current.axis);
        int near_child = difference <= 0 ? current.left : current.right;
        int far_child = difference <= 0 ? current.right : current.left;
        search_radius(near_child, query, excluded_index, squared_radius, result);
        if (current.point != excluded_index) {
            long double distance = squared_distance(current.point, query);
            if (distance <= squared_radius) result.push_back({current.point, distance});
        }
        if (difference * difference <= squared_radius) search_radius(far_child, query, excluded_index, squared_radius, result);
    }

    static void sort_neighbors(vector<KdNeighbor>& neighbors) {
        sort(neighbors.begin(), neighbors.end(), [](const KdNeighbor& a, const KdNeighbor& b) {
            if (a.squared_distance != b.squared_distance) return a.squared_distance < b.squared_distance;
            return a.index < b.index;
        });
    }

    void check_point(int point) const {
        if (point < 0 || point >= point_count()) throw out_of_range("KdTree: point index is out of range");
    }

    void check_excluded_index(int point) const {
        if (point < -1 || point >= point_count()) throw out_of_range("KdTree: excluded index is out of range");
    }

    void check_query(span<const long double> query) const {
        if (query.size() != (size_t)dimension_) throw invalid_argument("KdTree: query dimension does not match the tree");
        for (long double coordinate : query) {
            if (!isfinite(coordinate)) throw invalid_argument("KdTree: every query coordinate must be finite");
        }
    }
};

template <class Point, class GetCoordinate>
auto make_kd_tree(vector<Point> points, int dimension, GetCoordinate get_coordinate) {
    using Getter = decay_t<GetCoordinate>;
    return KdTree<Point, Getter>(move(points), dimension, move(get_coordinate));
}
}
