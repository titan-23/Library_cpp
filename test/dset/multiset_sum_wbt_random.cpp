#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

#include "titan_cpplib/ds/multiset_sum_wbt.cpp"

using namespace std;

namespace {

using T = long long;
constexpr T MISSING = -1000000000;

T naive_sum(const vector<T> &a, int l, int r) {
    return accumulate(a.begin() + l, a.begin() + r, T(0));
}

void erase_values(vector<T> &a, T key, int count) {
    auto it = lower_bound(a.begin(), a.end(), key);
    while (count-- && it != a.end() && *it == key) it = a.erase(it);
}

void verify(const titan23::MultisetSum<T> &multiset, const vector<T> &expected, mt19937_64 &rng) {
    multiset.check();
    assert(multiset.len() == expected.size());
    assert(multiset.size() == expected.size());
    assert(multiset.tovector() == expected);
    assert(multiset.all_prod() == naive_sum(expected, 0, expected.size()));

    if (!expected.empty()) {
        for (int repeat = 0; repeat < 20; ++repeat) {
            int k = rng() % expected.size();
            assert(multiset.get(k) == expected[k]);
            assert(multiset[k] == expected[k]);
        }
    }

    for (int repeat = 0; repeat < 100; ++repeat) {
        T key = (T)(rng() % 301) - 150;
        auto lower = lower_bound(expected.begin(), expected.end(), key);
        auto upper = upper_bound(expected.begin(), expected.end(), key);
        assert(multiset.index(key) == lower - expected.begin());
        assert(multiset.index_right(key) == upper - expected.begin());
        assert(multiset.count(key) == upper - lower);
        assert(multiset.contains(key) == (lower != expected.end() && *lower == key));
        assert(multiset.lt(key) == (lower == expected.begin() ? MISSING : *prev(lower)));
        assert(multiset.le(key) == (upper == expected.begin() ? MISSING : *prev(upper)));
        assert(multiset.ge(key) == (lower == expected.end() ? MISSING : *lower));
        assert(multiset.gt(key) == (upper == expected.end() ? MISSING : *upper));
        assert(multiset.sum(key) == naive_sum(expected, 0, lower - expected.begin()));
    }
}

void test_random(uint64_t seed) {
    mt19937_64 rng(seed);
    vector<T> initial(2000);
    for (T &key : initial) key = (T)(rng() % 201) - 100;
    vector<T> expected = initial;
    sort(expected.begin(), expected.end());
    titan23::MultisetSum<T> multiset(initial, MISSING);
    verify(multiset, expected, rng);

    for (int query = 0; query < 120000; ++query) {
        int operation = rng() % 7;
        if (expected.empty()) operation = 0;
        if (expected.size() >= 6000 && operation <= 2) operation = 3;

        if (operation <= 2) {
            T key = (T)(rng() % 301) - 150;
            int count = rng() % 3 + 1;
            multiset.add(key, count);
            expected.insert(lower_bound(expected.begin(), expected.end(), key), count, key);
        } else if (operation <= 4) {
            int k = rng() % expected.size();
            T key = expected[k];
            int count = rng() % 3 + 1;
            if (operation == 3) {
                multiset.remove(key, count);
            } else {
                assert(multiset.discard(key, count));
            }
            erase_values(expected, key, count);
        } else if (operation == 5) {
            T key = 1000 + rng() % 1000;
            assert(!multiset.discard(key));
        } else {
            int k = rng() % expected.size();
            if (rng() & 1) k = -1;
            int position = k < 0 ? expected.size() + k : k;
            T key = expected[position];
            assert(multiset.pop(k) == key);
            expected.erase(expected.begin() + position);
        }

        T high = (T)(rng() % 321) - 160;
        assert(multiset.sum(high) == naive_sum(expected, 0, lower_bound(expected.begin(), expected.end(), high) - expected.begin()));
        if (query % 509 == 0) verify(multiset, expected, rng);
    }
    verify(multiset, expected, rng);

    titan23::MultisetSum<T> copied = multiset;
    verify(copied, expected, rng);
    if (!expected.empty()) {
        copied.remove(expected[expected.size()/2]);
        assert(copied.len() + 1 == multiset.len());
    }
    titan23::MultisetSum<T> moved = move(copied);
    moved.check();
}

void test_sorted_updates() {
    mt19937_64 rng(123456789);
    titan23::MultisetSum<T> multiset(MISSING);
    vector<T> expected;
    for (int i = 0; i < 20000; ++i) {
        multiset.add(i);
        expected.emplace_back(i);
        if (i % 997 == 0) multiset.check();
    }
    verify(multiset, expected, rng);

    for (int parity = 0; parity < 2; ++parity) {
        for (int key = parity; key < 20000; key += 2) {
            multiset.remove(key);
            expected.erase(lower_bound(expected.begin(), expected.end(), key));
            if (key % 997 == parity) multiset.check();
        }
    }
    verify(multiset, expected, rng);

    for (int i = 19999; i >= 0; --i) {
        multiset.add(i);
        expected.insert(expected.begin(), i);
        if (i % 997 == 0) multiset.check();
    }
    verify(multiset, expected, rng);
}

void test_bisect_left_sum() {
    mt19937_64 rng(987654321);
    vector<T> expected(3000);
    for (T &key : expected) key = rng() % 100;
    sort(expected.begin(), expected.end());
    titan23::MultisetSum<T> multiset(expected, MISSING);
    for (int repeat = 0; repeat < 10000; ++repeat) {
        T limit = rng() % 100000;
        T remain = limit;
        int count = 0;
        for (const T key : expected) {
            if (key > remain) break;
            remain -= key;
            ++count;
        }
        assert(multiset.bisect_left_sum(limit) == count);
    }
}

} // namespace

int main() {
    test_random(0x123456789abcdef0ULL);
    test_random(0xfedcba9876543210ULL);
    test_sorted_updates();
    test_bisect_left_sum();
    cout << "OK\n";
    return 0;
}
