#include <algorithm>
#include <cassert>
#include <iostream>
#include <random>
#include <vector>

#include "titan_cpplib/ds/dynamic_wavelet_matrix.cpp"
#include "titan_cpplib/ds/dynamic_wavelet_tree.cpp"

using namespace std;

namespace {

constexpr int SIGMA = 64;

int naive_rank(const vector<int> &a, const int r, const int value) {
    return count(a.begin(), a.begin() + r, value);
}

int naive_freq(
    const vector<int> &a,
    const int l,
    const int r,
    const int upper
) {
    int result = 0;
    for (int i = l; i < r; ++i) result += a[i] < upper;
    return result;
}

int naive_prev(const vector<int> &a, const int l, const int r, const int upper) {
    int result = -1;
    for (int i = l; i < r; ++i) {
        if (a[i] < upper) result = max(result, a[i]);
    }
    return result;
}

int naive_next(const vector<int> &a, const int l, const int r, const int lower) {
    int result = SIGMA;
    for (int i = l; i < r; ++i) {
        if (a[i] >= lower) result = min(result, a[i]);
    }
    return result == SIGMA ? -1 : result;
}

void verify_all(
    const titan23::DynamicWaveletMatrix<int> &matrix,
    const titan23::DynamicWaveletTree<int> &tree,
    const vector<int> &expected
) {
    assert(matrix.tovector() == expected);
    assert(tree.tovector() == expected);
    for (int i = 0; i < static_cast<int>(expected.size()); ++i) {
        assert(matrix.access(i) == expected[i]);
        assert(tree.access(i) == expected[i]);
    }
}

void run(const uint64_t seed) {
    mt19937_64 rng(seed);
    vector<int> expected(200);
    for (int &value : expected) value = static_cast<int>(rng() % SIGMA);

    vector<int> initial = expected;
    titan23::DynamicWaveletMatrix<int> matrix(SIGMA, initial);
    titan23::DynamicWaveletTree<int> tree(SIGMA, initial);

    for (int query = 0; query < 30000; ++query) {
        int operation = static_cast<int>(rng() % 15);
        if (expected.empty()) operation = 0;
        if (expected.size() >= 500 && operation == 0) operation = 1;

        if (operation == 0) {
            const int k = static_cast<int>(rng() % (expected.size() + 1));
            const int value = static_cast<int>(rng() % SIGMA);
            matrix.insert(k, value);
            tree.insert(k, value);
            expected.insert(expected.begin() + k, value);
        } else if (operation == 1) {
            const int k = static_cast<int>(rng() % expected.size());
            const int value = expected[k];
            assert(matrix.pop(k) == value);
            assert(tree.pop(k) == value);
            expected.erase(expected.begin() + k);
        } else if (operation == 2) {
            const int k = static_cast<int>(rng() % expected.size());
            const int value = (rng() & 1) ? static_cast<int>(rng() % SIGMA) : expected[k] ^ 1;
            matrix.set(k, value);
            tree.set(k, value);
            expected[k] = value;
        } else if (operation == 3) {
            const int k = static_cast<int>(rng() % expected.size());
            assert(matrix.access(k) == expected[k]);
            assert(tree.access(k) == expected[k]);
        } else if (operation == 4) {
            const int r = static_cast<int>(rng() % (expected.size() + 1));
            const int value = static_cast<int>(rng() % SIGMA);
            const int answer = naive_rank(expected, r, value);
            assert(matrix.rank(r, value) == answer);
            assert(tree.rank(r, value) == answer);
        } else {
            int l = static_cast<int>(rng() % expected.size());
            int r = static_cast<int>(rng() % expected.size());
            if (l > r) swap(l, r);
            ++r;

            if (operation == 5) {
                const int k = static_cast<int>(rng() % (r - l));
                vector<int> values(expected.begin() + l, expected.begin() + r);
                sort(values.begin(), values.end());
                assert(matrix.kth_smallest(l, r, k) == values[k]);
                assert(tree.kth_smallest(l, r, k) == values[k]);
            } else if (operation == 6) {
                const int upper = static_cast<int>(rng() % (SIGMA + 1));
                const int answer = naive_freq(expected, l, r, upper);
                assert(matrix.range_freq(l, r, upper) == answer);
                assert(tree.range_freq(l, r, upper) == answer);
            } else if (operation == 7) {
                const int value = static_cast<int>(rng() % SIGMA);
                const int answer =
                    naive_rank(expected, r, value)
                  - naive_rank(expected, l, value);
                assert(matrix.range_count(l, r, value) == answer);
                assert(tree.range_count(l, r, value) == answer);
            } else if (operation == 8) {
                const int k = static_cast<int>(rng() % (r - l));
                vector<int> values(expected.begin() + l, expected.begin() + r);
                sort(values.begin(), values.end(), greater<int>());
                assert(matrix.kth_largest(l, r, k) == values[k]);
                assert(tree.kth_largest(l, r, k) == values[k]);
            } else if (operation == 9) {
                int lower = static_cast<int>(rng() % (SIGMA + 1));
                int upper = static_cast<int>(rng() % (SIGMA + 1));
                if (lower > upper) swap(lower, upper);
                const int answer = naive_freq(expected, l, r, upper) - naive_freq(expected, l, r, lower);
                assert(matrix.range_freq(l, r, lower, upper) == answer);
                assert(tree.range_freq(l, r, lower, upper) == answer);
            } else if (operation == 10) {
                const int upper = static_cast<int>(rng() % (SIGMA + 1));
                const int answer = naive_prev(expected, l, r, upper);
                assert(matrix.prev_value(l, r, upper) == answer);
                assert(tree.prev_value(l, r, upper) == answer);
            } else if (operation == 11) {
                const int lower = static_cast<int>(rng() % (SIGMA + 1));
                const int answer = naive_next(expected, l, r, lower);
                assert(matrix.next_value(l, r, lower) == answer);
                assert(tree.next_value(l, r, lower) == answer);
            } else if (operation == 12) {
                vector<int> count(SIGMA);
                for (int i = l; i < r; ++i) ++count[expected[i]];
                int majority = -1;
                for (int value = 0; value < SIGMA; ++value) {
                    if (count[value] * 2 > r - l) majority = value;
                }
                const auto [matrix_found, matrix_value] = matrix.has_majority(l, r);
                const auto [tree_found, tree_value] = tree.has_majority(l, r);
                assert(matrix_found == (majority != -1));
                assert(tree_found == (majority != -1));
                if (majority != -1) {
                    assert(matrix_value == majority);
                    assert(tree_value == majority);
                }
            } else {
                const int position = static_cast<int>(rng() % expected.size());
                const int value = expected[position];
                int occurrence = 0;
                for (int i = 0; i < position; ++i) occurrence += expected[i] == value;
                assert(matrix.select(occurrence, value) == position);
                assert(tree.select(occurrence, value) == position);
                if (operation == 14) {
                    assert(tree.select_remove(occurrence, value) == position);
                    assert(matrix.pop(position) == value);
                    expected.erase(expected.begin() + position);
                }
            }
        }

        if (query % 307 == 0) verify_all(matrix, tree, expected);
    }
    verify_all(matrix, tree, expected);
}

void test_sigma_one() {
    vector<int> expected(200, 0);
    titan23::DynamicWaveletTree<int> tree(1, expected);
    mt19937_64 rng(1234567);

    for (int query = 0; query < 10000; ++query) {
        int operation = static_cast<int>(rng() % 5);
        if (expected.empty()) operation = 0;
        if (operation == 0) {
            const int k = static_cast<int>(rng() % (expected.size() + 1));
            tree.insert(k, 0);
            expected.insert(expected.begin() + k, 0);
        } else if (operation == 1) {
            const int k = static_cast<int>(rng() % expected.size());
            assert(tree.pop(k) == 0);
            expected.erase(expected.begin() + k);
        } else if (operation == 2) {
            const int k = static_cast<int>(rng() % expected.size());
            tree.set(k, 0);
        } else if (operation == 3) {
            const int k = static_cast<int>(rng() % expected.size());
            assert(tree.access(k) == 0);
            assert(tree.kth_smallest(0, expected.size(), k) == 0);
        } else {
            const int occurrence = static_cast<int>(rng() % expected.size());
            assert(tree.select(occurrence, 0) == occurrence);
            assert(tree.select_remove(occurrence, 0) == occurrence);
            expected.erase(expected.begin() + occurrence);
        }
        assert(tree.len() == static_cast<int>(expected.size()));
        assert(tree.tovector() == expected);
        assert(tree.rank(expected.size(), 0) == static_cast<int>(expected.size()));
    }
}

void test_large_sigma() {
    using Value = long long;
    const Value sigma = 1LL << 40;
    vector<Value> expected = {0, 1, (1LL << 20) + 3, 1LL << 32, (1LL << 39) + 7, sigma - 1};
    titan23::DynamicWaveletTree<Value> tree(sigma, expected);
    assert(tree.tovector() == expected);

    for (int i = 0; i < static_cast<int>(expected.size()); ++i) {
        assert(tree.access(i) == expected[i]);
        assert(tree.rank(expected.size(), expected[i]) == 1);
        assert(tree.select(0, expected[i]) == i);
    }

    vector<Value> sorted = expected;
    sort(sorted.begin(), sorted.end());
    for (int k = 0; k < static_cast<int>(sorted.size()); ++k) {
        assert(tree.kth_smallest(0, expected.size(), k) == sorted[k]);
    }
    assert(tree.range_freq(0, expected.size(), sigma) == static_cast<int>(expected.size()));
    assert(tree.prev_value(0, expected.size(), sigma) == sigma - 1);
    assert(tree.next_value(0, expected.size(), 0) == 0);

    const Value inserted = (1LL << 35) + 11;
    tree.insert(2, inserted);
    expected.insert(expected.begin() + 2, inserted);
    tree.set(2, inserted ^ 1);
    expected[2] ^= 1;
    assert(tree.tovector() == expected);
    assert(tree.select_remove(0, expected[2]) == 2);
    expected.erase(expected.begin() + 2);
    assert(tree.tovector() == expected);
}

} // namespace

int main() {
    for (uint64_t seed = 0; seed < 8; ++seed) {
        run(seed * 1000003 + 91);
    }
    test_sigma_one();
    test_large_sigma();
    cout << "Dynamic wavelet random test: OK" << '\n';
    return 0;
}
