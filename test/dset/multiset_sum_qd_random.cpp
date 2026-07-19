#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

#include "titan_cpplib/ds/multiset_sum_qd.cpp"

using namespace std;

namespace {

using T = long long;

T naive_sum(const vector<T> &a, int l, int r) {
    return accumulate(a.begin() + l, a.begin() + r, T(0));
}

int naive_count_by_sum_limit(const vector<T> &a, T w) {
    int result = 0;
    for (const T x : a) {
        if (w <= x) break;
        w -= x;
        ++result;
    }
    return result;
}

void verify(const titan23::MultisetSum<T> &multiset, const vector<T> &expected, mt19937_64 &rng) {
    assert(multiset.size() == expected.size());
    assert(multiset.len() == expected.size());
    assert(multiset.tovector() == expected);
    assert(multiset.all_prod() == naive_sum(expected, 0, expected.size()));

    if (!expected.empty()) {
        for (int repeat = 0; repeat < 20; ++repeat) {
            int k = rng() % expected.size();
            assert(multiset[k] == expected[k]);
        }
    }

    for (int repeat = 0; repeat < 80; ++repeat) {
        T x = rng() % 260;
        auto lower = lower_bound(expected.begin(), expected.end(), x);
        auto upper = upper_bound(expected.begin(), expected.end(), x);
        assert(multiset.index(x) == lower - expected.begin());
        assert(multiset.index_right(x) == upper - expected.begin());
        assert(multiset.count(x) == upper - lower);
        assert(multiset.contains(x) == (lower != expected.end() && *lower == x));
        assert(multiset.lt(x) == (lower == expected.begin() ? -1 : *prev(lower)));
        assert(multiset.le(x) == (upper == expected.begin() ? -1 : *prev(upper)));
        assert(multiset.ge(x) == (lower == expected.end() ? -1 : *lower));
        assert(multiset.gt(x) == (upper == expected.end() ? -1 : *upper));
        assert(multiset.sum(x) == naive_sum(expected, 0, lower - expected.begin()));

        int l = rng() % (expected.size() + 1);
        int r = rng() % (expected.size() + 1);
        if (l > r) swap(l, r);
        assert(multiset.sum(l, r) == naive_sum(expected, l, r));

        T w = rng() % 5000;
        assert(multiset.count_by_sum_limit(w) == naive_count_by_sum_limit(expected, w));
    }
}

void test_random(const uint64_t seed) {
    mt19937_64 rng(seed);
    vector<T> expected(3000);
    for (T &x : expected) x = rng() % 200;
    sort(expected.begin(), expected.end());
    titan23::MultisetSum<T> multiset(expected, -1);
    verify(multiset, expected, rng);

    for (int query = 0; query < 100000; ++query) {
        if (expected.empty() || (expected.size() < 6000 && rng() % 5 < 3)) {
            T x = rng() % 250;
            multiset.add(x);
            expected.insert(lower_bound(expected.begin(), expected.end(), x), x);
        } else {
            int k = rng() % expected.size();
            T x = expected[k];
            multiset.remove(x);
            expected.erase(expected.begin() + k);
        }

        T high = rng() % 260;
        assert(multiset.sum(high) == naive_sum(expected, 0, lower_bound(expected.begin(), expected.end(), high) - expected.begin()));
        int l = rng() % (expected.size() + 1);
        int r = rng() % (expected.size() + 1);
        if (l > r) swap(l, r);
        assert(multiset.sum(l, r) == naive_sum(expected, l, r));

        if (query % 997 == 0) verify(multiset, expected, rng);
    }
    verify(multiset, expected, rng);
}

void test_clear_and_reuse() {
    titan23::MultisetSum<T> multiset(-1);
    vector<T> expected;
    for (int i = 0; i < 10000; ++i) {
        T x = i % 137;
        multiset.add(x);
        expected.insert(lower_bound(expected.begin(), expected.end(), x), x);
    }
    for (T high = 0; high <= 140; ++high) {
        assert(multiset.sum(high) == naive_sum(expected, 0, lower_bound(expected.begin(), expected.end(), high) - expected.begin()));
    }
    while (!expected.empty()) {
        int k = expected.size() / 2;
        T x = expected[k];
        multiset.remove(x);
        expected.erase(expected.begin() + k);
        if (expected.size() % 251 == 0) {
            T high = expected.size() % 141;
            assert(multiset.sum(high) == naive_sum(expected, 0, lower_bound(expected.begin(), expected.end(), high) - expected.begin()));
        }
    }
    assert(multiset.len() == 0);
    assert(multiset.all_prod() == 0);
    multiset.add(42);
    assert(multiset.sum(T(43)) == 42);
    assert(multiset.sum(0, 1) == 42);
}

} // namespace

int main() {
    test_random(0x123456789abcdef0ULL);
    test_random(0xfedcba9876543210ULL);
    test_clear_and_reuse();
    cout << "OK\n";
    return 0;
}
