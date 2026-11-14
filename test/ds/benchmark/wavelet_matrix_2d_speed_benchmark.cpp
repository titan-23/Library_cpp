// Example: g++ -std=c++17 -O3 -DNDEBUG -march=native -I. this_file.cpp -o /tmp/wm2d_bench
// /tmp/wm2d_bench --n 300000 --q 300000 --sigma 300000 --distribution uniform
// Use --operations sum.range_sum,min.range_min,monoid.range_prod to isolate work.
// Covers WaveletMatrix2DSum, WaveletMatrix2DMin and WaveletMatrix2DMonoid (none has updates).
// A structure is built only if one of its operations is selected, and freed before the next one.
#include "titan_cpplib/ds/wavelet_matrix_2d_sum.cpp"
#include "titan_cpplib/ds/wavelet_matrix_2d_min.cpp"
#include "titan_cpplib/ds/wavelet_matrix_2d_monoid.cpp"
#include "test/ds/benchmark/wavelet_matrix_speed_common.hpp"

namespace {

// Using-declarations, not a using-directive: the library's global `using namespace std`
// would otherwise make names such as fold/read ambiguous.
using wm_bench::Clock;
using wm_bench::Key;
using wm_bench::Options;
using wm_bench::W;
using wm_bench::fold;
using wm_bench::max_weight;
using wm_bench::parse_options;
using wm_bench::print_header;
using wm_bench::print_row;
using wm_bench::random_key;
using wm_bench::random_weight;
using wm_bench::read;

W op_max(W a, W b) { return std::max(a, b); }
W e_max() { return std::numeric_limits<W>::lowest(); }

using Point = std::tuple<Key, Key, W>;
using Sum = titan23::WaveletMatrix2DSum<Key, W>;
using Min = titan23::WaveletMatrix2DMin<Key, W>;
using Monoid = titan23::WaveletMatrix2DMonoid<Key, W, op_max, e_max>;

const std::vector<std::string> sum_operations = {
    "sum.build", "sum.range_sum_x", "sum.range_sum", "sum.count_sum_lt", "sum.sum_lt",
    "sum.range_count", "sum.kth_y", "sum.sum_k_smallest_y", "sum.sum_k_largest_y"
};
const std::vector<std::string> min_operations = {
    "min.build", "min.range_min", "min.range_argmin", "min.range_max", "min.range_argmax"
};
const std::vector<std::string> monoid_operations = {"monoid.build", "monoid.range_prod_x", "monoid.range_prod"};

std::vector<std::string> make_operations() {
    std::vector<std::string> operations = sum_operations;
    operations.insert(operations.end(), min_operations.begin(), min_operations.end());
    operations.insert(operations.end(), monoid_operations.begin(), monoid_operations.end());
    return operations;
}

const std::vector<std::string> operations = make_operations();

// Rectangle [x1, x2) x [y1, y2).
struct Rect {
    Key x1, x2, y1, y2;
    int k;  // in [0, #points with x in [x1, x2))
};

int run(const Options &o) {
    std::mt19937_64 rng(o.seed);
    std::vector<Point> points(o.n);
    for (Point &p : points) {
        std::get<0>(p) = random_key(rng, o);
        std::get<1>(p) = random_key(rng, o);
        std::get<2>(p) = random_weight(rng);
    }
    std::vector<Key> xs(o.n);
    for (int i = 0; i < o.n; ++i) xs[i] = std::get<0>(points[i]);
    std::sort(xs.begin(), xs.end());
    // Each rectangle contains the point it is anchored at, so argmin/argmax and kth_y have an answer.
    std::vector<Rect> rects(o.q);
    for (Rect &r : rects) {
        const auto &[px, py, pw] = points[rng() % o.n];
        r.x1 = rng() % (px + 1);
        r.x2 = px + 1 + rng() % (o.sigma - px);
        r.y1 = rng() % (py + 1);
        r.y2 = py + 1 + rng() % (o.sigma - py);
        const int count = std::lower_bound(xs.begin(), xs.end(), r.x2) - std::lower_bound(xs.begin(), xs.end(), r.x1);
        r.k = rng() % count;
    }

    print_header(o, " weight=[0," + std::to_string(max_weight) + ")");

    if (o.wants_any(sum_operations)) {
        const auto start = Clock::now();
        const Sum tree(points);
        const double build = std::chrono::duration<double>(Clock::now() - start).count();
        if (o.wants("sum.build")) {
            print_row("sum.build", build, o.n, fold(tree.count_sum_lt(0, o.sigma, o.sigma)) * 1000003 + tree.len());
        }
        read(o, "sum.range_sum_x", rects, o.q, [&](const Rect &r) { return tree.range_sum(r.x1, r.x2); });
        read(o, "sum.range_sum", rects, o.q, [&](const Rect &r) { return tree.range_sum(r.x1, r.x2, r.y1, r.y2); });
        read(o, "sum.count_sum_lt", rects, o.q, [&](const Rect &r) { return tree.count_sum_lt(r.x1, r.x2, r.y2); });
        read(o, "sum.sum_lt", rects, o.q, [&](const Rect &r) { return tree.sum_lt(r.x1, r.x2, r.y2); });
        read(o, "sum.range_count", rects, o.q, [&](const Rect &r) { return tree.range_count(r.x1, r.x2, r.y1, r.y2); });
        read(o, "sum.kth_y", rects, o.q, [&](const Rect &r) { return tree.kth_y(r.x1, r.x2, r.k); });
        read(o, "sum.sum_k_smallest_y", rects, o.q, [&](const Rect &r) { return tree.sum_k_smallest_y(r.x1, r.x2, r.k + 1); });
        read(o, "sum.sum_k_largest_y", rects, o.q, [&](const Rect &r) { return tree.sum_k_largest_y(r.x1, r.x2, r.k + 1); });
    }

    if (o.wants_any(min_operations)) {
        const auto start = Clock::now();
        const Min tree(points);
        const double build = std::chrono::duration<double>(Clock::now() - start).count();
        if (o.wants("min.build")) {
            print_row("min.build", build, o.n,
                      fold(tree.range_argmin(0, o.sigma, 0, o.sigma)) * 1000003 + fold(tree.range_argmax(0, o.sigma, 0, o.sigma)));
        }
        read(o, "min.range_min", rects, o.q, [&](const Rect &r) { return tree.range_min(r.x1, r.x2, r.y1, r.y2); });
        read(o, "min.range_argmin", rects, o.q, [&](const Rect &r) { return tree.range_argmin(r.x1, r.x2, r.y1, r.y2); });
        read(o, "min.range_max", rects, o.q, [&](const Rect &r) { return tree.range_max(r.x1, r.x2, r.y1, r.y2); });
        read(o, "min.range_argmax", rects, o.q, [&](const Rect &r) { return tree.range_argmax(r.x1, r.x2, r.y1, r.y2); });
    }

    if (o.wants_any(monoid_operations)) {
        const auto start = Clock::now();
        const Monoid tree(points);
        const double build = std::chrono::duration<double>(Clock::now() - start).count();
        if (o.wants("monoid.build")) print_row("monoid.build", build, o.n, fold(tree.range_prod(0, o.sigma)) * 1000003 + tree.len());
        read(o, "monoid.range_prod_x", rects, o.q, [&](const Rect &r) { return tree.range_prod(r.x1, r.x2); });
        read(o, "monoid.range_prod", rects, o.q, [&](const Rect &r) { return tree.range_prod(r.x1, r.x2, r.y1, r.y2); });
    }
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    try {
        return run(parse_options(argc, argv, operations,
                                 "Points: x and y follow the distribution in [0, sigma); weights are uniform in [0, 1000000000).\n"
                                 "Each rectangle [x1,x2)x[y1,y2) is anchored at a random point (px, py):\n"
                                 "x1 in [0, px], x2 in [px+1, sigma], and likewise for y, so it is never empty.\n"
                                 "*_x queries use only [x1, x2). count_sum_lt/sum_lt use upper = y2.\n"
                                 "kth_y uses k in [0, #points with x in [x1, x2)); sum_k_*_y take k+1.\n"
                                 "monoid uses op = max over weights, e = LLONG_MIN.\n"
                                 "build times the points constructor, including its copy of the points.\n",
                                 false));
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
