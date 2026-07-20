#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <tuple>
#include <vector>

#include "titan_cpplib/ds/wavelet_matrix_2d_min.cpp"
#include "titan_cpplib/ds/wavelet_matrix_2d_monoid.cpp"
#include "titan_cpplib/ds/wavelet_matrix_2d_sum.cpp"

using namespace std;

namespace {

using Coord = int;
using Weight = long long;
using Point = tuple<Coord, Coord, Weight>;

Weight op_or(const Weight a, const Weight b) { return a | b; }
Weight e_or() { return 0; }

void run(const uint64_t seed) {
    mt19937_64 rng(seed);
    const int n = rng() % 120;
    vector<Point> points;
    points.reserve(n);
    for (int i = 0; i < n; ++i) points.emplace_back(static_cast<int>(rng() % 21) - 10, static_cast<int>(rng() % 17) - 8, static_cast<Weight>(rng() % 101) - 50);

    titan23::WaveletMatrix2DSum<Coord, Weight> sum;
    titan23::WaveletMatrix2DMin<Coord, Weight> minmax;
    titan23::WaveletMatrix2DMonoid<Coord, Weight, op_or, e_or> monoid;
    sum.reserve(n);
    minmax.reserve(n);
    monoid.reserve(n);
    for (int i = 0; i < n; ++i) {
        const auto [x, y, weight] = points[i];
        sum.add_point(x, y, weight);
        minmax.add_point(x, y, weight);
        monoid.add_point(x, y, Weight(1) << (i % 60));
    }
    sum.build();
    minmax.build();
    monoid.build();
    assert(sum.len() == n);
    assert(minmax.len() == n);
    assert(monoid.len() == n);

    vector<int> x_order(n);
    iota(x_order.begin(), x_order.end(), 0);
    stable_sort(x_order.begin(), x_order.end(), [&](const int a, const int b) { return get<0>(points[a]) < get<0>(points[b]); });

    for (int query = 0; query < 5000; ++query) {
        Coord x1 = static_cast<int>(rng() % 25) - 12;
        Coord x2 = static_cast<int>(rng() % 25) - 12;
        Coord y1 = static_cast<int>(rng() % 21) - 10;
        Coord y2 = static_cast<int>(rng() % 21) - 10;
        if (x1 > x2) swap(x1, x2);
        if (y1 > y2) swap(y1, y2);
        const Coord upper = static_cast<int>(rng() % 23) - 11;

        Weight all_sum = 0;
        Weight rectangle_sum = 0;
        Weight sum_lt = 0;
        Weight rectangle_or = 0;
        Weight all_or = 0;
        int count_lt = 0;
        vector<int> selected;
        vector<int> rectangle;
        for (const int index : x_order) {
            const auto [x, y, weight] = points[index];
            if (x1 <= x && x < x2) {
                selected.emplace_back(index);
                all_sum += weight;
                all_or |= Weight(1) << (index % 60);
                if (y < upper) {
                    ++count_lt;
                    sum_lt += weight;
                }
                if (y1 <= y && y < y2) {
                    rectangle.emplace_back(index);
                    rectangle_sum += weight;
                    rectangle_or |= Weight(1) << (index % 60);
                }
            }
        }

        assert(sum.range_sum(x1, x2) == all_sum);
        assert(sum.range_sum(x1, x2, y1, y2) == rectangle_sum);
        assert(sum.count_sum_lt(x1, x2, upper) == make_pair(count_lt, sum_lt));
        assert(sum.sum_lt(x1, x2, upper) == sum_lt);
        assert(sum.range_count(x1, x2, y1, y2) == static_cast<int>(rectangle.size()));
        assert(monoid.range_prod(x1, x2) == all_or);
        assert(monoid.range_prod(x1, x2, y1, y2) == rectangle_or);

        vector<int> ascending = selected;
        stable_sort(ascending.begin(), ascending.end(), [&](const int a, const int b) { return get<1>(points[a]) < get<1>(points[b]); });
        vector<int> descending = selected;
        stable_sort(descending.begin(), descending.end(), [&](const int a, const int b) { return get<1>(points[a]) > get<1>(points[b]); });
        if (!ascending.empty()) {
            const int k = rng() % ascending.size();
            assert(sum.kth_y(x1, x2, k) == get<1>(points[ascending[k]]));
        }
        const int take = selected.empty() ? 0 : rng() % (selected.size() + 1);
        Weight smallest_sum = 0;
        Weight largest_sum = 0;
        for (int i = 0; i < take; ++i) {
            smallest_sum += get<2>(points[ascending[i]]);
            largest_sum += get<2>(points[descending[i]]);
        }
        assert(sum.sum_k_smallest_y(x1, x2, take) == smallest_sum);
        assert(sum.sum_k_largest_y(x1, x2, take) == largest_sum);

        int min_index = -1;
        int max_index = -1;
        for (const int index : rectangle) {
            if (min_index == -1 || get<2>(points[index]) < get<2>(points[min_index]) || (get<2>(points[index]) == get<2>(points[min_index]) && index < min_index)) min_index = index;
            if (max_index == -1 || get<2>(points[max_index]) < get<2>(points[index]) || (get<2>(points[index]) == get<2>(points[max_index]) && index < max_index)) max_index = index;
        }
        const Weight expected_min = min_index == -1 ? numeric_limits<Weight>::max() : get<2>(points[min_index]);
        const Weight expected_max = max_index == -1 ? numeric_limits<Weight>::lowest() : get<2>(points[max_index]);
        assert(minmax.range_min(x1, x2, y1, y2) == expected_min);
        assert(minmax.range_max(x1, x2, y1, y2) == expected_max);
        if (min_index != -1) {
            assert(minmax.range_argmin(x1, x2, y1, y2) == make_tuple(get<2>(points[min_index]), get<0>(points[min_index]), get<1>(points[min_index])));
            assert(minmax.range_argmax(x1, x2, y1, y2) == make_tuple(get<2>(points[max_index]), get<0>(points[max_index]), get<1>(points[max_index])));
        }
    }
}

void test_duplicates() {
    const vector<Point> points{{1, 2, 3}, {1, 2, 5}, {1, 2, 7}};
    titan23::WaveletMatrix2DSum<Coord, Weight> matrix(points);
    assert(matrix.range_count(1, 2, 2, 3) == 3);
    assert(matrix.range_sum(1, 2, 2, 3) == 15);
    assert(matrix.sum_k_smallest_y(1, 2, 2) == 8);
    assert(matrix.sum_k_largest_y(1, 2, 2) == 8);
}

} // namespace

int main() {
    test_duplicates();
    for (uint64_t seed = 0; seed < 20; ++seed) run(seed * 1000003 + 211);
    return 0;
}
