#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <random>
#include <set>
#include <utility>
#include <vector>

#include "titan_cpplib/ds/wavelet_matrix_fenwick.cpp"

using namespace std;

namespace {

using Key = int;
using Weight = long long;
using Matrix = titan23::WaveletMatrixFenwick<Key, Weight>;

vector<int> sorted_indices(const vector<Key> &keys, const int l, const int r, const bool largest) {
    vector<int> order(r - l);
    iota(order.begin(), order.end(), l);
    stable_sort(order.begin(), order.end(), [&](const int i, const int j) {
        return largest ? keys[i] > keys[j] : keys[i] < keys[j];
    });
    return order;
}

Weight naive_range_sum(const vector<Weight> &weights, const int l, const int r) {
    return accumulate(weights.begin() + l, weights.begin() + r, Weight(0));
}

Weight naive_sum_k(const vector<Key> &keys, const vector<Weight> &weights, const int l, const int r, const int k, const bool largest) {
    const vector<int> order = sorted_indices(keys, l, r, largest);
    Weight result = 0;
    for (int i = 0; i < k; ++i) result += weights[order[i]];
    return result;
}

int naive_min_count(const vector<Key> &keys, const vector<Weight> &weights, const int l, const int r, Weight target, const bool largest) {
    if (target <= 0) return 0;
    const vector<int> order = sorted_indices(keys, l, r, largest);
    for (int i = 0; i < static_cast<int>(order.size()); ++i) {
        target -= weights[order[i]];
        if (target <= 0) return i + 1;
    }
    return -1;
}

void verify_topk(const Matrix &matrix, const vector<Key> &keys, const int sigma, const int l, const int r, const int k) {
    vector<int> count(sigma);
    for (int i = l; i < r; ++i) ++count[keys[i]];
    const vector<pair<Key, int>> result = matrix.topk(l, r, k);
    set<Key> used;
    for (const auto &[key, frequency] : result) {
        assert(used.insert(key).second);
        assert(frequency == count[key]);
    }
    for (const auto &[key, frequency] : result) {
        for (int other = 0; other < sigma; ++other) {
            if (!used.contains(other)) assert(frequency >= count[other]);
        }
    }
}

void verify_query(const Matrix &matrix, const vector<Key> &keys, const vector<Weight> &weights, const int sigma, mt19937_64 &rng, const bool verify_min_count, const int query) {
    const int n = keys.size();
    int l = rng() % (n + 1);
    int r = rng() % (n + 1);
    if (l > r) swap(l, r);
    assert(matrix.range_sum(l, r) == naive_range_sum(weights, l, r));

    const Key value = rng() % sigma;
    int rank = 0;
    for (int i = 0; i < r; ++i) rank += keys[i] == value;
    assert(matrix.rank(r, value) == rank);
    int range_count = 0;
    for (int i = l; i < r; ++i) range_count += keys[i] == value;
    assert(matrix.range_count(l, r, value) == range_count);

    const Key upper = rng() % (sigma + 1);
    int count_lt = 0;
    Weight sum_lt = 0;
    for (int i = l; i < r; ++i) {
        if (keys[i] >= upper) continue;
        ++count_lt;
        sum_lt += weights[i];
    }
    assert(matrix.range_freq(l, r, upper) == count_lt);
    assert(matrix.count_sum_lt(l, r, upper) == make_pair(count_lt, sum_lt));
    assert(matrix.sum_lt(l, r, upper) == sum_lt);

    Key lower = rng() % (sigma + 1);
    Key upper2 = rng() % (sigma + 1);
    if (lower > upper2) swap(lower, upper2);
    int count_range = 0;
    Weight sum_range = 0;
    for (int i = l; i < r; ++i) {
        if (lower <= keys[i] && keys[i] < upper2) {
            ++count_range;
            sum_range += weights[i];
        }
    }
    assert(matrix.range_freq(l, r, lower, upper2) == count_range);
    assert(matrix.sum_range(l, r, lower, upper2) == sum_range);

    const int take = rng() % (r - l + 1);
    assert(matrix.sum_k_smallest(l, r, take) == naive_sum_k(keys, weights, l, r, take, false));
    assert(matrix.sum_k_largest(l, r, take) == naive_sum_k(keys, weights, l, r, take, true));

    if (verify_min_count) {
        const Weight total = naive_range_sum(weights, l, r);
        const Weight target = rng() % (total + 20);
        assert(matrix.min_count_smallest_sum_ge(l, r, target) == naive_min_count(keys, weights, l, r, target, false));
        assert(matrix.min_count_largest_sum_ge(l, r, target) == naive_min_count(keys, weights, l, r, target, true));
    }

    if (l == r) return;
    const int k = rng() % (r - l);
    const vector<int> ascending = sorted_indices(keys, l, r, false);
    const vector<int> descending = sorted_indices(keys, l, r, true);
    assert(matrix.kth_smallest(l, r, k) == keys[ascending[k]]);
    assert(matrix.kth_largest(l, r, k) == keys[descending[k]]);

    Key previous = -1;
    Key next = sigma;
    for (int i = l; i < r; ++i) {
        if (keys[i] < upper) previous = max(previous, keys[i]);
        if (keys[i] >= lower) next = min(next, keys[i]);
    }
    assert(matrix.prev_value(l, r, upper) == previous);
    assert(matrix.next_value(l, r, lower) == (next == sigma ? -1 : next));

    vector<int> count(sigma);
    for (int i = l; i < r; ++i) ++count[keys[i]];
    Key majority = -1;
    for (int key = 0; key < sigma; ++key) {
        if (count[key] * 2 > r - l) majority = key;
    }
    const auto [found, majority_key] = matrix.has_majority(l, r);
    assert(found == (majority != -1));
    if (found) assert(majority_key == majority);

    const int position = l + rng() % (r - l);
    const Key selected_key = keys[position];
    int occurrence = 0;
    int range_occurrence = 0;
    for (int i = 0; i < position; ++i) {
        occurrence += keys[i] == selected_key;
        if (i >= l) range_occurrence += keys[i] == selected_key;
    }
    assert(matrix.select(occurrence, selected_key) == position);
    assert(matrix.range_select(l, r, range_occurrence, selected_key) == position);

    if (query % 101 == 0) verify_topk(matrix, keys, sigma, l, r, rng() % (sigma + 2));
}

void test_random_updates() {
    for (uint64_t seed = 0; seed < 16; ++seed) {
        mt19937_64 rng(seed * 1000003 + 113);
        const int sigma = seed % 4 == 0 ? 1 : seed % 4 == 1 ? 3 : seed % 4 == 2 ? 16 : 65;
        const int n = rng() % 180;
        vector<Key> keys(n);
        vector<Weight> weights(n);
        for (int i = 0; i < n; ++i) {
            keys[i] = rng() % sigma;
            weights[i] = rng() % 30;
        }

        Matrix matrix(sigma, keys, weights);
        assert(matrix.len() == n);
        assert(matrix.tovector() == keys);
        assert(matrix.toweights() == weights);

        for (int query = 0; query < 10000; ++query) {
            if (n > 0 && rng() % 4 == 0) {
                const int k = rng() % n;
                if (rng() & 1) {
                    const Weight weight = rng() % 50;
                    matrix.set_weight(k, weight);
                    weights[k] = weight;
                } else {
                    const Weight delta = static_cast<Weight>(rng() % (weights[k] + 31)) - weights[k];
                    matrix.add_weight(k, delta);
                    weights[k] += delta;
                }
                assert(matrix.access_weight(k) == weights[k]);
                assert(matrix.access_pair(k) == make_pair(keys[k], weights[k]));
                if (query % 127 == 0) {
                    assert(matrix.tovector() == keys);
                    assert(matrix.toweights() == weights);
                }
            } else {
                verify_query(matrix, keys, weights, sigma, rng, true, query);
            }
        }
    }
}

void test_negative_updates() {
    mt19937_64 rng(998244353);
    const int sigma = 8;
    vector<Key> keys = {3, 1, 7, 3, 0, 2, 1, 3, 6, 0};
    vector<Weight> weights = {-5, 7, -2, 4, -8, 1, 6, -3, 9, -4};
    Matrix matrix(sigma, keys, weights);
    for (int query = 0; query < 5000; ++query) {
        if (rng() % 3 == 0) {
            const int k = rng() % keys.size();
            if (rng() & 1) {
                const Weight weight = static_cast<Weight>(rng() % 41) - 20;
                matrix.set_weight(k, weight);
                weights[k] = weight;
            } else {
                const Weight delta = static_cast<Weight>(rng() % 11) - 5;
                matrix.add_weight(k, delta);
                weights[k] += delta;
            }
        } else {
            verify_query(matrix, keys, weights, sigma, rng, false, query);
        }
    }
    assert(matrix.toweights() == weights);
}

void test_value_weight() {
    vector<Key> keys = {7, 1, 4, 1, 9, 0, 9, 1};
    vector<Weight> weights(keys.begin(), keys.end());
    Matrix matrix(10, keys);
    for (int l = 0; l <= static_cast<int>(keys.size()); ++l) {
        for (int r = l; r <= static_cast<int>(keys.size()); ++r) {
            const Weight total = naive_range_sum(weights, l, r);
            for (Weight target = 0; target <= total + 1; ++target) {
                assert(matrix.min_count_smallest_sum_ge(l, r, target) == naive_min_count(keys, weights, l, r, target, false));
                assert(matrix.min_count_largest_sum_ge(l, r, target) == naive_min_count(keys, weights, l, r, target, true));
            }
        }
    }

    matrix.set_weight(3, 12);
    weights[3] = 12;
    matrix.add_weight(6, -5);
    weights[6] -= 5;
    mt19937_64 rng(1234567);
    for (int query = 0; query < 2000; ++query) verify_query(matrix, keys, weights, 10, rng, true, query);
}

void test_empty() {
    Matrix default_matrix;
    assert(default_matrix.len() == 0);
    assert(default_matrix.range_sum(0, 0) == 0);

    Matrix matrix(100);
    assert(matrix.len() == 0);
    assert(matrix.tovector().empty());
    assert(matrix.toweights().empty());
    assert(matrix.range_freq(0, 0, 50) == 0);
    assert(matrix.count_sum_lt(0, 0, 50) == make_pair(0, 0LL));
    assert(matrix.sum_k_smallest(0, 0, 0) == 0);
    assert(matrix.min_count_largest_sum_ge(0, 0, 1) == -1);
}

void test_large_sigma() {
    using LargeMatrix = titan23::WaveletMatrixFenwick<long long, long long>;
    const long long sigma = 1LL << 40;
    vector<long long> keys = {0, 1, (1LL << 20) + 3, 1LL << 32, (1LL << 39) + 7, sigma - 1};
    vector<long long> weights = {5, 4, 3, 2, 1, 6};
    LargeMatrix matrix(sigma, keys, weights);
    matrix.add_weight(4, 10);
    weights[4] += 10;
    matrix.set_weight(0, 20);
    weights[0] = 20;
    assert(matrix.tovector() == keys);
    assert(matrix.toweights() == weights);
    assert(matrix.sum_k_largest(0, keys.size(), 3) == 19);
    assert(matrix.sum_lt(0, keys.size(), 1LL << 32) == 27);
}

} // namespace

int main() {
    test_random_updates();
    test_negative_updates();
    test_value_weight();
    test_empty();
    test_large_sigma();
    cout << "Wavelet matrix Fenwick random test: OK" << '\n';
    return 0;
}
