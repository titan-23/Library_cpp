/// https://github.com/titan-23/Library_cpp/blob/main/test/geometry/benchmark/kd_tree_benchmark.cpp
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include "titan_cpplib/geometry/incremental_kd_tree_2d.cpp"
#include "titan_cpplib/geometry/kd_tree.cpp"
#include "titan_cpplib/geometry/kd_tree_2d.cpp"

using namespace std;
using namespace titan23;

using Point = array<double, 2>;
using Clock = chrono::steady_clock;

template <class F>
double measure(F f) {
    auto begin = Clock::now();
    f();
    return chrono::duration<double, milli>(Clock::now() - begin).count();
}

template <class F>
double measure_median(F f) {
    array<double, 7> times;
    for (double& time : times) time = measure(f);
    sort(times.begin(), times.end());
    return times[3];
}

void print_time(const string& name, double ms, int count = 1) {
    cout << left << setw(34) << name << right << setw(11) << fixed << setprecision(3) << ms << " ms";
    if (count > 1) cout << "  " << setw(9) << setprecision(1) << ms * 1e6 / count << " ns/query";
    cout << '\n';
}

int main() {
    const int n = 100000;
    const int query_count = 20000;
    const int slow_query_count = 100;
    mt19937_64 rng(23);
    uniform_real_distribution<double> real(0, 1);
    vector<Point> points(n);
    vector<Point> queries(query_count);
    vector<array<long double, 2>> wide_queries(query_count);
    for (Point& p : points) p = {real(rng), real(rng)};
    for (int i = 0; i < query_count; ++i) {
        queries[i] = {real(rng), real(rng)};
        wide_queries[i] = {queries[i][0], queries[i][1]};
    }
    auto getter = [](const Point& p, int axis) { return p[axis]; };

    optional<decltype(make_kd_tree(points, 2, getter))> generic;
    optional<decltype(make_kd_tree_as<double>(points, 2, getter))> generic_double;
    optional<KdTree2D<>> fixed;
    optional<IncrementalKdTree2D<>> incremental;
    vector<Point> copy = points;
    print_time("build generic long double", measure([&] { generic.emplace(move(copy), 2, getter); }));
    copy = points;
    print_time("build generic double", measure([&] { generic_double.emplace(move(copy), 2, getter); }));
    copy = points;
    print_time("build static 2D", measure([&] { fixed.emplace(move(copy)); }));
    copy = points;
    print_time("build incremental 2D", measure([&] { incremental.emplace(move(copy)); }));

    long double checksum = 0;
    print_time("NN generic long double", measure_median([&] {
        for (const auto& q : wide_queries) {
            checksum += generic->nearest_neighbor(span<const long double>(q))->squared_distance;
        }
    }), query_count);
    print_time("NN generic double", measure_median([&] {
        for (const Point& q : queries) {
            checksum += generic_double->nearest_neighbor(span<const double>(q))->squared_distance;
        }
    }), query_count);
    print_time("NN static 2D", measure_median([&] {
        for (const Point& q : queries) checksum += fixed->nearest_neighbor(q)->squared_distance;
    }), query_count);
    print_time("NN incremental 2D", measure_median([&] {
        for (const Point& q : queries) checksum += incremental->nearest_neighbor(q)->squared_distance;
    }), query_count);

    vector<Point> outside(query_count);
    for (int i = 0; i < query_count; ++i) outside[i] = {10.0 + i * 1e-6, 10.0 - i * 1e-6};
    print_time("outside NN generic double", measure_median([&] {
        for (int i = 0; i < slow_query_count; ++i) {
            checksum += generic_double->nearest_neighbor(span<const double>(outside[i]))->squared_distance;
        }
    }), slow_query_count);
    print_time("outside NN static 2D", measure_median([&] {
        for (const Point& q : outside) checksum += fixed->nearest_neighbor(q)->squared_distance;
    }), query_count);
    print_time("outside NN incremental 2D", measure_median([&] {
        for (const Point& q : outside) checksum += incremental->nearest_neighbor(q)->squared_distance;
    }), query_count);

    vector<KdNeighborT<double>> out;
    out.reserve(64);
    print_time("k=8 generic double", measure_median([&] {
        for (const Point& q : queries) {
            generic_double->k_nearest_neighbors(span<const double>(q), 8, out);
            checksum += out[0].squared_distance;
        }
    }), query_count);
    print_time("k=8 static 2D", measure_median([&] {
        for (const Point& q : queries) {
            fixed->k_nearest_neighbors(q, 8, out);
            checksum += out[0].squared_distance;
        }
    }), query_count);
    print_time("k=8 incremental 2D", measure_median([&] {
        for (const Point& q : queries) {
            incremental->k_nearest_neighbors(q, 8, out);
            checksum += out[0].squared_distance;
        }
    }), query_count);
    print_time("outside k=8 static 2D", measure_median([&] {
        for (const Point& q : outside) {
            fixed->k_nearest_neighbors(q, 8, out);
            checksum += out[0].squared_distance;
        }
    }), query_count);
    print_time("outside k=8 incremental 2D", measure_median([&] {
        for (const Point& q : outside) {
            incremental->k_nearest_neighbors(q, 8, out);
            checksum += out[0].squared_distance;
        }
    }), query_count);

    double radius = sqrt(10.0 / (acos(-1.0) * n));
    print_time("radius~10 generic double unsorted", measure_median([&] {
        for (const Point& q : queries) {
            generic_double->radius_neighbors(span<const double>(q), radius, out, -1, false);
            checksum += out.size();
            if (!out.empty()) checksum += out.back().squared_distance;
        }
    }), query_count);
    print_time("radius~10 static 2D unsorted", measure_median([&] {
        for (const Point& q : queries) {
            fixed->radius_neighbors(q, radius, out, -1, false);
            checksum += out.size();
            if (!out.empty()) checksum += out.back().squared_distance;
        }
    }), query_count);
    print_time("radius~10 incr 2D unsorted", measure_median([&] {
        for (const Point& q : queries) {
            incremental->radius_neighbors(q, radius, out, -1, false);
            checksum += out.size();
            if (!out.empty()) checksum += out.back().squared_distance;
        }
    }), query_count);

    vector<Point> equal(n, {1, 1});
    auto equal_generic = make_kd_tree_as<double>(equal, 2, getter);
    auto equal_fixed = make_kd_tree_2d(equal);
    IncrementalKdTree2D<> equal_incremental(equal);
    Point equal_query = {1, 1};
    print_time("duplicate NN generic double", measure_median([&] {
        for (int i = 0; i < slow_query_count; ++i) {
            auto nearest = equal_generic.nearest_neighbor(span<const double>(equal_query));
            checksum += nearest->index + nearest->squared_distance + 1;
        }
    }), slow_query_count);
    print_time("duplicate NN static 2D", measure_median([&] {
        for (int i = 0; i < query_count; ++i) {
            auto nearest = equal_fixed.nearest_neighbor(1, 1);
            checksum += nearest->index + nearest->squared_distance + 1;
        }
    }), query_count);
    print_time("duplicate NN incremental 2D", measure_median([&] {
        for (int i = 0; i < query_count; ++i) {
            auto nearest = equal_incremental.nearest_neighbor(1, 1);
            checksum += nearest->index + nearest->squared_distance + 1;
        }
    }), query_count);

    IncrementalKdTree2D<> random_add;
    random_add.reserve(n);
    print_time("add random 100k reserved", measure([&] {
        for (Point p : points) random_add.add(p);
    }));
    IncrementalKdTree2D<> monotone_add;
    monotone_add.reserve(n);
    print_time("add monotone 100k reserved", measure([&] {
        for (int i = 0; i < n; ++i) monotone_add.add((double)i, (double)i);
    }));
    checksum += random_add.point_count() + monotone_add.point_count();

    cerr << "checksum=" << (double)checksum << '\n';
}
