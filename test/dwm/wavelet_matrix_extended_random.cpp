#include <algorithm>
#include <cassert>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

#include "titan_cpplib/ds/dynamic_wavelet_matrix.cpp"
#include "titan_cpplib/ds/dynamic_wavelet_tree.cpp"
#include "titan_cpplib/ds/dynamic_wavelet_tree_sum.cpp"
#include "titan_cpplib/ds/wavelet_matrix.cpp"
#include "titan_cpplib/ds/wavelet_matrix_fenwick.cpp"
#include "titan_cpplib/ds/wavelet_matrix_sum.cpp"

using namespace std;

namespace {

using Key = int;
using Weight = long long;

vector<int> sorted_indices(const vector<Key> &keys, const int l, const int r, const bool largest) {
    vector<int> order(r - l);
    iota(order.begin(), order.end(), l);
    stable_sort(order.begin(), order.end(), [&](const int i, const int j) {
        return largest ? keys[i] > keys[j] : keys[i] < keys[j];
    });
    return order;
}

template<class Matrix>
void verify_positions(const Matrix &matrix, const vector<Key> &keys, const int sigma, mt19937_64 &rng) {
    const int n = keys.size();
    int l = rng() % (n + 1);
    int r = rng() % (n + 1);
    if (l > r) swap(l, r);
    int lower = rng() % (sigma + 1);
    int upper = rng() % (sigma + 1);
    if (lower > upper) swap(lower, upper);

    vector<int> positions;
    for (int i = l; i < r; ++i) {
        if (lower <= keys[i] && keys[i] < upper) positions.emplace_back(i);
    }
    assert(matrix.next_index_in_value_range(l, r, lower, upper) == (positions.empty() ? -1 : positions.front()));
    assert(matrix.prev_index_in_value_range(l, r, lower, upper) == (positions.empty() ? -1 : positions.back()));
    for (int k = 0; k < static_cast<int>(positions.size()); ++k) {
        assert(matrix.kth_index_in_value_range(l, r, lower, upper, k) == positions[k]);
    }
}

template<class Matrix>
void verify_value_search(const Matrix &matrix, const vector<Key> &keys, const vector<Weight> &weights, mt19937_64 &rng) {
    const int n = keys.size();
    int l = rng() % (n + 1);
    int r = rng() % (n + 1);
    if (l > r) swap(l, r);
    const Weight total = accumulate(weights.begin() + l, weights.begin() + r, Weight(0));
    const Weight budget = rng() % (total + 10);

    for (const bool largest : {false, true}) {
        const vector<int> order = sorted_indices(keys, l, r, largest);
        int count = 0;
        Weight aggregate = 0;
        while (count < static_cast<int>(order.size()) && aggregate + weights[order[count]] <= budget) {
            aggregate += weights[order[count++]];
        }
        const Key boundary = count == static_cast<int>(order.size()) ? -1 : keys[order[count]];
        if (largest) {
            const auto [result_count, result_boundary, result_aggregate] = matrix.max_right_largest(l, r, [&](const Weight sum) { return sum <= budget; });
            assert(result_count == count);
            assert(result_boundary == boundary);
            assert(result_aggregate == aggregate);
            assert(matrix.max_count_largest_sum_le(l, r, budget) == count);
        } else {
            const auto [result_count, result_boundary, result_aggregate] = matrix.max_right_smallest(l, r, [&](const Weight sum) { return sum <= budget; });
            assert(result_count == count);
            assert(result_boundary == boundary);
            assert(result_aggregate == aggregate);
            assert(matrix.max_count_smallest_sum_le(l, r, budget) == count);
        }
    }

    if (total == 0) return;
    const Weight target = rng() % total + 1;
    const vector<int> order = sorted_indices(keys, l, r, false);
    Weight aggregate = 0;
    Key expected = -1;
    for (const int index : order) {
        aggregate += weights[index];
        if (aggregate >= target) {
            expected = keys[index];
            break;
        }
    }
    assert(matrix.weighted_quantile(l, r, target) == expected);

    const long long denominator = rng() % 8 + 1;
    const long long numerator = rng() % denominator + 1;
    const Weight ratio_target = (total * numerator + denominator - 1) / denominator;
    aggregate = 0;
    for (const int index : order) {
        aggregate += weights[index];
        if (aggregate >= ratio_target) {
            expected = keys[index];
            break;
        }
    }
    assert(matrix.weighted_quantile(l, r, numerator, denominator) == expected);

    const Weight median_target = (total + 1) / 2;
    aggregate = 0;
    for (const int index : order) {
        aggregate += weights[index];
        if (aggregate >= median_target) {
            expected = keys[index];
            break;
        }
    }
    assert(matrix.weighted_median(l, r) == expected);
}

void test_static(const uint64_t seed) {
    mt19937_64 rng(seed);
    const int sigma = seed % 4 == 0 ? 1 : seed % 4 == 1 ? 3 : seed % 4 == 2 ? 16 : 65;
    const int n = rng() % 150;
    vector<Key> keys(n);
    vector<Weight> weights(n);
    for (int i = 0; i < n; ++i) {
        keys[i] = rng() % sigma;
        weights[i] = rng() % 30;
    }

    vector<Key> dynamic_keys = keys;
    titan23::WaveletMatrix<Key> matrix(sigma, keys);
    titan23::WaveletMatrixSum<Key, Weight> sum_matrix(sigma, keys, weights);
    titan23::WaveletMatrixFenwick<Key, Weight> fenwick_matrix(sigma, keys, weights);
    titan23::DynamicWaveletMatrix<Key> dynamic_matrix(sigma, dynamic_keys);
    titan23::DynamicWaveletTree<Key> dynamic_tree(sigma, keys);
    titan23::DynamicWaveletTreeSum<Key, Weight> sum_tree(sigma, keys, weights);

    assert(matrix.tovector() == keys);
    assert(matrix.topk(0, n, 0).empty());
    for (int query = 0; query < 3000; ++query) {
        verify_positions(matrix, keys, sigma, rng);
        verify_positions(sum_matrix, keys, sigma, rng);
        verify_positions(fenwick_matrix, keys, sigma, rng);
        verify_positions(dynamic_matrix, keys, sigma, rng);
        verify_positions(dynamic_tree, keys, sigma, rng);
        verify_positions(sum_tree, keys, sigma, rng);
        verify_value_search(sum_matrix, keys, weights, rng);
        verify_value_search(fenwick_matrix, keys, weights, rng);
        verify_value_search(sum_tree, keys, weights, rng);

        if (n == 0) continue;
        int l = rng() % n;
        int r = rng() % n;
        if (l > r) swap(l, r);
        ++r;
        vector<int> count(sigma);
        for (int i = l; i < r; ++i) ++count[keys[i]];
        Key majority = -1;
        for (int key = 0; key < sigma; ++key) {
            if (count[key] * 2 > r - l) majority = key;
        }
        const auto [found, value] = matrix.has_majority(l, r);
        assert(found == (majority != -1));
        if (found) assert(value == majority);

        const int position = l + rng() % (r - l);
        const Key key = keys[position];
        int occurrence = 0;
        for (int i = l; i < position; ++i) occurrence += keys[i] == key;
        assert(matrix.range_select(l, r, occurrence, key) == position);
    }
}

void test_updates(const uint64_t seed) {
    mt19937_64 rng(seed);
    const int sigma = 64;
    vector<Key> keys(100);
    vector<Weight> weights(100);
    for (int i = 0; i < 100; ++i) {
        keys[i] = rng() % sigma;
        weights[i] = rng() % 30;
    }
    titan23::WaveletMatrixFenwick<Key, Weight> matrix(sigma, keys, weights);
    titan23::DynamicWaveletTreeSum<Key, Weight> tree(sigma, keys, weights);
    for (int query = 0; query < 5000; ++query) {
        if (rng() % 4 == 0) {
            const int k = rng() % keys.size();
            const Weight weight = rng() % 40;
            matrix.set_weight(k, weight);
            tree.set_weight(k, weight);
            weights[k] = weight;
        } else {
            verify_value_search(matrix, keys, weights, rng);
            verify_value_search(tree, keys, weights, rng);
        }
    }
}

} // namespace

int main() {
    for (uint64_t seed = 0; seed < 12; ++seed) test_static(seed * 1000003 + 131);
    for (uint64_t seed = 0; seed < 4; ++seed) test_updates(seed * 998244353 + 17);
    return 0;
}
