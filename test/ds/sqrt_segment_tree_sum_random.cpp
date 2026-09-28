#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <vector>

#include "titan_cpplib/ds/sqrt_segment_tree_sum.cpp"

using namespace std;
using ll = long long;

void verify(const titan23::SqrtSegmentTreeSum<ll> &actual, const vector<ll> &expected) {
    const int n = (int)expected.size();
    assert(actual.size() == n);
    assert(actual.all_prod() == accumulate(expected.begin(), expected.end(), 0LL));
    assert(actual.prod(0, n) == actual.all_prod());
    for (int i = 0; i < n; ++i) {
        assert(actual.get(i) == expected[i]);
        assert(actual.prod(i, i + 1) == expected[i]);
    }
    for (int i = 0; i <= n; ++i) assert(actual.prod(i, i) == 0);
}

void verify_all_ranges(const titan23::SqrtSegmentTreeSum<ll> &actual,
                       const vector<ll> &expected) {
    const int n = (int)expected.size();
    for (int l = 0; l <= n; ++l) {
        ll sum = 0;
        for (int r = l; r <= n; ++r) {
            assert(actual.prod(l, r) == sum);
            if (r < n) sum += expected[r];
        }
    }
}

void random_test(int n, uint32_t seed) {
    mt19937_64 rng(seed);
    auto random_value = [&] () -> ll {
        return (ll)(rng() % 2000000000001ULL) - 1000000000000LL;
    };
    vector<ll> expected(n, 0);
    titan23::SqrtSegmentTreeSum<ll> zeros(n);
    verify(zeros, expected);
    for (ll &v : expected) v = random_value();
    titan23::SqrtSegmentTreeSum<ll> actual(expected);
    verify(actual, expected);
    if (n <= 100) verify_all_ranges(actual, expected);

    for (int q = 0; q < 3000; ++q) {
        const int op = (int)(rng() % 5);
        if (op <= 1 && n > 0) {
            const int i = (int)(rng() % n);
            const ll value = random_value();
            if (op == 0) {
                actual.set(i, value);
                expected[i] = value;
            } else {
                actual.add(i, value);
                expected[i] += value;
            }
        } else {
            int l = (int)(rng() % (n + 1)), r = (int)(rng() % (n + 1));
            if (l > r) swap(l, r);
            assert(actual.prod(l, r) == accumulate(expected.begin() + l, expected.begin() + r, 0LL));
        }
        if ((q & 255) == 0) verify(actual, expected);
    }
    verify(actual, expected);
    if (n <= 100) verify_all_ranges(actual, expected);

    // コピー後の独立性
    auto copy = actual;
    if (n > 0) copy.add(0, 7);
    verify(actual, expected);
    if (n > 0) expected[0] += 7;
    verify(copy, expected);
}

int main() {
    titan23::SqrtSegmentTreeSum<ll> empty;
    verify(empty, {});
    for (int n : {0, 1, 2, 3, 7, 8, 15, 16, 17, 24, 25, 26,
                  63, 64, 65, 99, 100, 101, 255, 256, 257, 1000}) {
        for (uint32_t seed = 0; seed < 6; ++seed) random_test(n, seed);
    }
    // int 型での更新
    titan23::SqrtSegmentTreeSum<int> counts(10);
    counts.set(9, 3);
    counts.add(9, -2);
    assert(counts.get(9) == 1);
    assert(counts.prod(0, 10) == 1);
    assert(counts.all_prod() == 1);
    // 新旧値の差が型の範囲を超える代入
    titan23::SqrtSegmentTreeSum<ll> single(vector<ll>{numeric_limits<ll>::min()});
    single.set(0, numeric_limits<ll>::max());
    verify(single, {numeric_limits<ll>::max()});
    single.set(0, numeric_limits<ll>::min());
    verify(single, {numeric_limits<ll>::min()});
    cout << "sqrt_segment_tree_sum: OK\n";
}
