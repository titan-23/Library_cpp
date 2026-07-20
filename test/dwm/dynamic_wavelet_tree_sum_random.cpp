#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <random>
#include <utility>
#include <vector>

#include "titan_cpplib/ds/dynamic_wavelet_tree_sum.cpp"

using namespace std;

namespace {

using Key = int;
using Weight = long long;
using Tree = titan23::DynamicWaveletTreeSum<Key, Weight>;

constexpr int SIGMA = 64;

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

vector<pair<Key, int>> naive_topk(const vector<Key> &keys, const int l, const int r, const int k) {
    vector<int> count(SIGMA);
    for (int i = l; i < r; ++i) ++count[keys[i]];
    vector<pair<Key, int>> result;
    for (int key = 0; key < SIGMA; ++key) {
        if (count[key] > 0) result.emplace_back(key, count[key]);
    }
    sort(result.begin(), result.end(), [](const auto &a, const auto &b) { return a.second != b.second ? a.second > b.second : a.first > b.first; });
    if (static_cast<int>(result.size()) > k) result.resize(k);
    return result;
}

void verify_all(const Tree &tree, const vector<Key> &keys, const vector<Weight> &weights) {
    assert(tree.len() == static_cast<int>(keys.size()));
    assert(tree.tovector() == keys);
    assert(tree.toweights() == weights);
    vector<pair<Key, Weight>> items(keys.size());
    for (int i = 0; i < static_cast<int>(keys.size()); ++i) items[i] = {keys[i], weights[i]};
    assert(tree.toitems() == items);
    for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
        assert(tree.access(i) == keys[i]);
        assert(tree.access_weight(i) == weights[i]);
        assert(tree.access_pair(i) == items[i]);
    }
}

void verify_query(const Tree &tree, const vector<Key> &keys, const vector<Weight> &weights, mt19937_64 &rng) {
    const int n = keys.size();
    int l = rng() % (n + 1);
    int r = rng() % (n + 1);
    if (l > r) swap(l, r);
    assert(tree.range_sum(l, r) == naive_range_sum(weights, l, r));
    if (l == r) {
        assert(tree.sum_k_smallest(l, r, 0) == 0);
        assert(tree.sum_k_largest(l, r, 0) == 0);
        assert(tree.min_count_smallest_sum_ge(l, r, 0) == 0);
        assert(tree.min_count_largest_sum_ge(l, r, 1) == -1);
        return;
    }

    const Key value = rng() % SIGMA;
    int rank = 0;
    for (int i = 0; i < r; ++i) rank += keys[i] == value;
    assert(tree.rank(r, value) == rank);
    int range_count = 0;
    for (int i = l; i < r; ++i) range_count += keys[i] == value;
    assert(tree.range_count(l, r, value) == range_count);

    const int k = rng() % (r - l);
    const vector<int> ascending = sorted_indices(keys, l, r, false);
    const vector<int> descending = sorted_indices(keys, l, r, true);
    assert(tree.kth_smallest(l, r, k) == keys[ascending[k]]);
    assert(tree.kth_largest(l, r, k) == keys[descending[k]]);

    const Key upper = rng() % (SIGMA + 1);
    int count_lt = 0;
    Weight sum_lt = 0;
    for (int i = l; i < r; ++i) {
        if (keys[i] >= upper) continue;
        ++count_lt;
        sum_lt += weights[i];
    }
    assert(tree.range_freq(l, r, upper) == count_lt);
    assert(tree.count_sum_lt(l, r, upper) == make_pair(count_lt, sum_lt));
    assert(tree.sum_lt(l, r, upper) == sum_lt);

    Key lower = rng() % (SIGMA + 1);
    Key upper2 = rng() % (SIGMA + 1);
    if (lower > upper2) swap(lower, upper2);
    int count_range = 0;
    Weight sum_range = 0;
    for (int i = l; i < r; ++i) {
        if (lower <= keys[i] && keys[i] < upper2) {
            ++count_range;
            sum_range += weights[i];
        }
    }
    assert(tree.range_freq(l, r, lower, upper2) == count_range);
    assert(tree.sum_range(l, r, lower, upper2) == sum_range);

    const int take = rng() % (r - l + 1);
    assert(tree.sum_k_smallest(l, r, take) == naive_sum_k(keys, weights, l, r, take, false));
    assert(tree.sum_k_largest(l, r, take) == naive_sum_k(keys, weights, l, r, take, true));

    const Weight total = naive_range_sum(weights, l, r);
    const Weight target = rng() % (total + 10);
    assert(tree.min_count_smallest_sum_ge(l, r, target) == naive_min_count(keys, weights, l, r, target, false));
    assert(tree.min_count_largest_sum_ge(l, r, target) == naive_min_count(keys, weights, l, r, target, true));

    Key previous = -1;
    Key next = SIGMA;
    for (int i = l; i < r; ++i) {
        if (keys[i] < upper) previous = max(previous, keys[i]);
        if (keys[i] >= lower) next = min(next, keys[i]);
    }
    assert(tree.prev_value(l, r, upper) == previous);
    assert(tree.next_value(l, r, lower) == (next == SIGMA ? -1 : next));

    vector<int> count(SIGMA);
    for (int i = l; i < r; ++i) ++count[keys[i]];
    Key majority = -1;
    for (int key = 0; key < SIGMA; ++key) {
        if (count[key] * 2 > r - l) majority = key;
    }
    const auto [found, majority_key] = tree.has_majority(l, r);
    assert(found == (majority != -1));
    if (found) assert(majority_key == majority);

    const int position = l + rng() % (r - l);
    const Key selected_key = keys[position];
    int occurrence = 0;
    for (int i = l; i < position; ++i) occurrence += keys[i] == selected_key;
    assert(tree.range_select(l, r, occurrence, selected_key) == position);
    const int top = rng() % (SIGMA + 1);
    assert(tree.topk(l, r, top) == naive_topk(keys, l, r, top));
}

void run(const uint64_t seed) {
    mt19937_64 rng(seed);
    vector<Key> keys(200);
    vector<Weight> weights(200);
    for (int i = 0; i < 200; ++i) {
        keys[i] = rng() % SIGMA;
        weights[i] = rng() % 30;
    }
    Tree tree(SIGMA, keys, weights);
    tree.reserve(500);

    for (int query = 0; query < 30000; ++query) {
        int operation = rng() % 14;
        if (keys.empty()) operation = 0;
        if (keys.size() >= 500 && operation == 0) operation = 1;

        if (operation == 0) {
            const int k = rng() % (keys.size() + 1);
            const Key key = rng() % SIGMA;
            const Weight weight = rng() % 30;
            tree.insert(k, key, weight);
            keys.insert(keys.begin() + k, key);
            weights.insert(weights.begin() + k, weight);
        } else if (operation == 1) {
            const int k = rng() % keys.size();
            assert(tree.pop(k) == make_pair(keys[k], weights[k]));
            keys.erase(keys.begin() + k);
            weights.erase(weights.begin() + k);
        } else if (operation == 2) {
            const int k = rng() % keys.size();
            const Key key = rng() % SIGMA;
            const Weight weight = rng() % 30;
            tree.set(k, key, weight);
            keys[k] = key;
            weights[k] = weight;
        } else if (operation == 3) {
            const int k = rng() % keys.size();
            const Weight weight = rng() % 30;
            tree.set_weight(k, weight);
            weights[k] = weight;
        } else if (operation == 4) {
            const int position = rng() % keys.size();
            const Key key = keys[position];
            int occurrence = 0;
            for (int i = 0; i < position; ++i) occurrence += keys[i] == key;
            assert(tree.select(occurrence, key) == position);
        } else if (operation == 5) {
            const int position = rng() % keys.size();
            const Key key = keys[position];
            int occurrence = 0;
            for (int i = 0; i < position; ++i) occurrence += keys[i] == key;
            assert(tree.select_remove(occurrence, key) == position);
            keys.erase(keys.begin() + position);
            weights.erase(weights.begin() + position);
        } else if (operation == 6) {
            const int k = rng() % keys.size();
            const Weight delta = rng() % 10;
            tree.add_weight(k, delta);
            weights[k] += delta;
        } else if (operation == 7) {
            const int k = rng() % keys.size();
            const Key key = rng() % SIGMA;
            tree.set_key(k, key);
            keys[k] = key;
        } else {
            verify_query(tree, keys, weights, rng);
        }

        if (query % 307 == 0) verify_all(tree, keys, weights);
    }
    verify_all(tree, keys, weights);

    Tree copied = tree;
    verify_all(copied, keys, weights);
    Tree moved = move(copied);
    verify_all(moved, keys, weights);
}

void test_sigma_one() {
    vector<Key> keys(200, 0);
    vector<Weight> weights(200);
    iota(weights.begin(), weights.end(), 0);
    Tree tree(1, keys, weights);
    assert(tree.tovector() == keys);
    assert(tree.toweights() == weights);
    assert(tree.range_freq(0, keys.size(), 1) == static_cast<int>(keys.size()));
    assert(tree.sum_k_smallest(0, keys.size(), 50) == accumulate(weights.begin(), weights.begin() + 50, Weight(0)));
    const Weight target = 1000;
    assert(tree.min_count_largest_sum_ge(0, keys.size(), target) == naive_min_count(keys, weights, 0, keys.size(), target, true));

    for (int i = 0; i < 1000; ++i) {
        const int k = i % keys.size();
        tree.set(k, 0, i);
        weights[k] = i;
        assert(tree.access_pair(k) == make_pair(0, static_cast<Weight>(i)));
    }
    while (!keys.empty()) {
        const int k = keys.size() / 2;
        assert(tree.pop(k) == make_pair(keys[k], weights[k]));
        keys.erase(keys.begin() + k);
        weights.erase(weights.begin() + k);
    }
    assert(tree.len() == 0);
    assert(tree.range_sum(0, 0) == 0);
}

void test_value_weight_convenience() {
    vector<int> values = {7, 1, 4, 1, 9, 0};
    Tree tree(10, values);
    assert(tree.tovector() == values);
    vector<Weight> expected(values.begin(), values.end());
    assert(tree.toweights() == expected);
    assert(tree.sum_k_smallest(0, values.size(), 3) == 2);
    assert(tree.sum_k_largest(0, values.size(), 2) == 16);
}

void test_negative_sums() {
    vector<Key> keys = {3, 1, 3, 0, 2, 1, 3};
    vector<Weight> weights = {-5, 7, -2, 4, -8, 1, 6};
    Tree tree(4, keys, weights);
    for (int l = 0; l <= static_cast<int>(keys.size()); ++l) {
        for (int r = l; r <= static_cast<int>(keys.size()); ++r) {
            assert(tree.range_sum(l, r) == naive_range_sum(weights, l, r));
            for (int upper = 0; upper <= 4; ++upper) {
                Weight sum = 0;
                for (int i = l; i < r; ++i) {
                    if (keys[i] < upper) sum += weights[i];
                }
                assert(tree.sum_lt(l, r, upper) == sum);
            }
            for (int k = 0; k <= r - l; ++k) {
                assert(tree.sum_k_smallest(l, r, k) == naive_sum_k(keys, weights, l, r, k, false));
                assert(tree.sum_k_largest(l, r, k) == naive_sum_k(keys, weights, l, r, k, true));
            }
        }
    }
    tree.set(2, 1, -11);
    keys[2] = 1;
    weights[2] = -11;
    assert(tree.tovector() == keys);
    assert(tree.toweights() == weights);
}

void test_empty_and_large_sigma() {
    titan23::DynamicWaveletTreeSum<long long, long long> tree(1LL << 40);
    vector<pair<long long, long long>> expected;
    const vector<long long> keys = {0, 1LL << 32, (1LL << 39) + 7, (1LL << 20) + 3};
    for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
        tree.insert(i, keys[i], i + 1);
        expected.emplace_back(keys[i], i + 1);
    }
    assert(tree.toitems() == expected);
    tree.set(1, (1LL << 35) + 11, 20);
    expected[1] = {(1LL << 35) + 11, 20};
    assert(tree.toitems() == expected);
    while (!expected.empty()) {
        assert(tree.pop(0) == expected.front());
        expected.erase(expected.begin());
    }
    assert(tree.len() == 0);
}

} // namespace

int main() {
    for (uint64_t seed = 0; seed < 8; ++seed) run(seed * 1000003 + 97);
    test_sigma_one();
    test_value_weight_convenience();
    test_negative_sums();
    test_empty_and_large_sigma();
    cout << "Dynamic wavelet tree sum random test: OK" << '\n';
    return 0;
}
