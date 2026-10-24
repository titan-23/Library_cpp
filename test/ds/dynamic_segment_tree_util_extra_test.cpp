#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

#include "titan_cpplib/ds/dynamic_segment_tree_util.cpp"
#include "titan_cpplib/ds/segutil.cpp"

using namespace std;
namespace normal = titan23::dynamic_segment_tree;
namespace dense = titan23::segment_tree;
using ll = long long;
using i128 = __int128_t;

template<bool Minimum, bool Count>
void pair_monoid_test() {
    using Plain = conditional_t<Minimum,
        conditional_t<Count, normal::SegMinCnt<ll>, normal::SegMinIdx<ll>>,
        conditional_t<Count, normal::SegMaxCnt<ll>, normal::SegMaxIdx<ll>>>;
    using Initial = conditional_t<Minimum,
        conditional_t<Count, normal::SegMinCntInit<ll>, normal::SegMinIdxInit<ll>>,
        conditional_t<Count, normal::SegMaxCntInit<ll>, normal::SegMaxIdxInit<ll>>>;
    using Dense = conditional_t<Minimum,
        conditional_t<Count, dense::SegMinCnt<ll>, dense::SegMinIdx<ll>>,
        conditional_t<Count, dense::SegMaxCnt<ll>, dense::SegMaxIdx<ll>>>;
    const auto compare = [](pair<ll, ll> actual, pair<ll, int> expected) {
        const ll index = !Count && expected.second == numeric_limits<int>::max()
            ? numeric_limits<ll>::max() : expected.second;
        assert(actual.first == expected.first && actual.second == index);
    };
    const auto repeat = [](pair<ll, ll> value, ll n) {
        if constexpr (Minimum && Count) return normal::pow_min_cnt(value, n);
        else if constexpr (Minimum) return normal::pow_min_idx(value, n);
        else if constexpr (Count) return normal::pow_max_cnt(value, n);
        else return normal::pow_max_idx(value, n);
    };
    const pair<ll, ll> identity{Minimum ? numeric_limits<ll>::max() : numeric_limits<ll>::lowest(),
                                Count ? 0 : numeric_limits<ll>::max()};
    assert(repeat({3, 7}, 0) == identity);
    assert((repeat({3, 7}, 5) == pair<ll, ll>{3, Count ? 35 : 7}));
    mt19937 rng(8371 + Minimum + Count * 2);
    for (int n : {0, 1, 17}) {
        Plain plain(n);
        Initial initial(n, {3, Count ? 1 : 7});
        Dense sparse(n), uniform(vector<pair<ll, int>>(n, {3, Count ? 1 : 7}));
        for (int step = -1; step < 24; ++step) {
            if (n != 0 && step >= 0) {
                const int k = rng() % n;
                const ll value = step == 0 ? numeric_limits<ll>::max()
                    : step == 1 ? numeric_limits<ll>::lowest() : ll(rng() % 7) - 3;
                const int index = Count ? 1 : k;
                plain.set(k, {value, index});
                initial.set(k, {value, index});
                sparse.set(k, {value, index});
                uniform.set(k, {value, index});
            }
            compare(plain.all_prod(), sparse.all_prod());
            compare(initial.all_prod(), uniform.all_prod());
            for (int l = 0; l <= n; ++l) {
                for (int r = l; r <= n; ++r) {
                    compare(plain.prod(l, r), sparse.prod(l, r));
                    compare(initial.prod(l, r), uniform.prod(l, r));
                }
                const auto pred = [](const auto& value) {
                    if constexpr (Minimum) return value.first >= 0;
                    else return value.first <= 0;
                };
                assert(plain.max_right(l, pred) == sparse.max_right(l, pred));
                assert(initial.max_right(l, pred) == uniform.max_right(l, pred));
                assert(plain.min_left(l, pred) == sparse.min_left(l, pred));
                assert(initial.min_left(l, pred) == uniform.min_left(l, pred));
            }
        }
        initial.clear();
        const auto value = initial.all_prod();
        if (n != 0) assert(value.first == 3 && value.second == (Count ? n : 7));
    }
}

void affine_test() {
    const normal::Affine<ll> f{2, 3}, g{5, 7};
    const auto combined = normal::op_affine(f, g);
    assert(combined.a == 10 && combined.b == 22);
    assert(combined.eval(4) == g.eval(f.eval(4)));
    assert(combined.eval(4) != f.eval(g.eval(4)));
    for (normal::Affine<ll> value : {f, normal::Affine<ll>{-1, 3}, {1, 7}, {0, -5}}) {
        for (ll n = 0; n <= 12; ++n) {
            const auto repeated = normal::pow_affine(value, n);
            for (ll x : {-2, 0, 3}) {
                ll expected = x;
                for (ll i = 0; i < n; ++i) expected = value.a * expected + value.b;
                assert(repeated.eval(x) == expected);
            }
        }
    }
    // n=1の処理で不要な二乗を行うと64 bitを超える係数を使う
    const auto once = normal::pow_affine(normal::Affine<ll>{10000000000LL, 7}, 1LL);
    assert(once.a == 10000000000LL && once.b == 7);
    mt19937 rng(471932);
    for (int n : {0, 1, 17}) {
        normal::SegAffine<ll> plain(n);
        normal::SegAffineInit<ll> initial(n, {1, 2});
        dense::SegAffine<ll> sparse(n), uniform(vector<dense::Affine<ll>>(n, {1, 2}));
        vector<normal::Affine<ll>> a(n, {1, 0}), b(n, {1, 2});
        for (int step = -1; step < 24; ++step) {
            if (n != 0 && step >= 0) {
                const int k = rng() % n;
                const normal::Affine<ll> value{ll(rng() % 3), ll(rng() % 7) - 3};
                plain.set(k, value);
                initial.set(k, value);
                sparse.set(k, {value.a, value.b});
                uniform.set(k, {value.a, value.b});
                a[k] = b[k] = value;
            }
            for (int l = 0; l <= n; ++l) {
                ll plain_value = 3, initial_value = 3;
                for (int r = l; r <= n; ++r) {
                    const auto p = plain.prod(l, r), q = initial.prod(l, r);
                    const auto x = sparse.prod(l, r), y = uniform.prod(l, r);
                    assert(p.a == x.a && p.b == x.b && p.eval(3) == plain_value);
                    assert(q.a == y.a && q.b == y.b && q.eval(3) == initial_value);
                    if (r != n) {
                        plain_value = a[r].a * plain_value + a[r].b;
                        initial_value = b[r].a * initial_value + b[r].b;
                    }
                }
            }
        }
        initial.clear();
        assert(initial.all_prod().a == 1 && initial.all_prod().b == 2LL * n);
    }
}

template<class A, class B>
void compare_sub_sum(const A& actual, const B& expected) {
    assert(actual.sum == expected.sum && actual.pre == expected.pre);
    assert(actual.suf == expected.suf && actual.ans == expected.ans);
}

normal::MaxSubSum<ll> reference_sub_sum(const vector<ll>& values, int l, int r) {
    normal::MaxSubSum<ll> result;
    ll suffix = 0, ending = 0;
    for (int i = l; i < r; ++i) {
        result.sum += values[i];
        result.pre = max(result.pre, result.sum);
        ending = max(0LL, ending + values[i]);
        result.ans = max(result.ans, ending);
    }
    for (int i = r; i > l;) {
        suffix += values[--i];
        result.suf = max(result.suf, suffix);
    }
    return result;
}

void max_sub_sum_test() {
    for (const vector<ll>& block : vector<vector<ll>>{{}, {-4}, {0}, {3},
             {4, -7, 6}, {4, -9, 3}, {4, -7, 3}, {-5, -2, -9}}) {
        const auto value = reference_sub_sum(block, 0, static_cast<int>(block.size()));
        vector<ll> repeated;
        for (ll n = 0; n < 8; ++n) {
            compare_sub_sum(normal::pow_max_sub_sum(value, n),
                            reference_sub_sum(repeated, 0, static_cast<int>(repeated.size())));
            repeated.insert(repeated.end(), block.begin(), block.end());
        }
    }
    mt19937 rng(717831);
    for (int n : {0, 1, 17}) {
        normal::SegMaxSubSum<ll> plain(n);
        normal::SegMaxSubSumInit<ll> initial(n, -2);
        dense::SegMaxSubSum<ll> sparse(n), uniform(vector<dense::MaxSubSum<ll>>(n, -2));
        vector<ll> a(n), b(n, -2);
        for (int step = -1; step < 24; ++step) {
            if (n != 0 && step >= 0) {
                const int k = rng() % n;
                a[k] = b[k] = ll(rng() % 19) - 9;
                plain.set(k, a[k]);
                initial.set(k, b[k]);
                sparse.set(k, a[k]);
                uniform.set(k, b[k]);
            }
            for (int l = 0; l <= n; ++l) {
                for (int r = l; r <= n; ++r) {
                    compare_sub_sum(plain.prod(l, r), sparse.prod(l, r));
                    compare_sub_sum(initial.prod(l, r), uniform.prod(l, r));
                    compare_sub_sum(plain.prod(l, r), reference_sub_sum(a, l, r));
                    compare_sub_sum(initial.prod(l, r), reference_sub_sum(b, l, r));
                }
                const auto pred = [](const auto& value) { return value.ans <= 8; };
                assert(plain.max_right(l, pred) == sparse.max_right(l, pred));
                assert(initial.max_right(l, pred) == uniform.max_right(l, pred));
                assert(plain.min_left(l, pred) == sparse.min_left(l, pred));
                assert(initial.min_left(l, pred) == uniform.min_left(l, pred));
            }
        }
        initial.clear();
        compare_sub_sum(initial.all_prod(), reference_sub_sum(vector<ll>(n, -2), 0, n));
    }
}

void large_index_and_count_test() {
    const i128 n = (i128(1) << 80) + 3, middle = n / 2;
    normal::SegMinCntInit<ll, i128> minimum(n, {7, 1});
    normal::SegMaxCntInit<ll, i128> maximum(n, {-7, 1});
    assert(minimum.all_prod().first == 7 && minimum.all_prod().second == n);
    assert(maximum.all_prod().first == -7 && maximum.all_prod().second == n);
    minimum.set(middle, {3, 1});
    maximum.set(n - 1, {-2, 1});
    assert(minimum.all_prod().first == 3 && minimum.all_prod().second == 1);
    assert(maximum.all_prod().first == -2 && maximum.all_prod().second == 1);
    minimum.set(middle, {7, 1});
    maximum.set(n - 1, {-7, 1});
    assert(minimum.all_prod().second == n && maximum.all_prod().second == n);
    const auto count_limit = [middle](const auto& value) { return value.second <= middle; };
    assert(minimum.max_right(7, count_limit) == middle + 7);
    assert(maximum.min_left(n - 7, count_limit) == n - 7 - middle);

    const i128 label = (i128(1) << 70) + 5;
    normal::SegMinIdxInit<ll, i128> min_index(n, {4, label});
    normal::SegMaxIdxInit<ll, i128> max_index(n, {4, label});
    assert(min_index.prod(1, middle).second == label);
    assert(max_index.prod(middle, n).second == label);
    min_index.set(n - 1, {4, 1});
    max_index.set(n - 1, {4, 1});
    assert(min_index.all_prod().second == 1 && max_index.all_prod().second == 1);
    assert(min_index.prod(0, n - 1).second == label);
    assert(max_index.prod(0, n - 1).second == label);

    normal::SegAffineInit<i128, i128> affine(n, {1, 2});
    assert(affine.all_prod().a == 1 && affine.all_prod().b == 2 * n);
    affine.set(middle, {0, 5});
    assert(affine.all_prod().eval(123) == 5 + 2 * (n - middle - 1));
    assert(affine.prod(middle - 2, middle + 3).eval(123) == 9);
    const auto translation = [](const auto& value) { return value.a == 1; };
    assert(affine.max_right(0, translation) == middle);
    assert(affine.min_left(n, translation) == middle + 1);

    normal::SegMaxSubSumInit<i128, i128> sub_sum(n, 2);
    assert(sub_sum.all_prod().sum == 2 * n && sub_sum.all_prod().ans == 2 * n);
    sub_sum.set(middle, -3);
    assert(sub_sum.all_prod().sum == 2 * n - 5 && sub_sum.all_prod().ans == 2 * n - 5);
    assert(sub_sum.prod(middle - 2, middle + 3).ans == 5);
    normal::MaxSubSum<i128> block;
    block.sum = 3;
    block.pre = 4;
    block.suf = block.ans = 6;
    const auto repeated = normal::pow_max_sub_sum(block, n);
    assert(repeated.sum == 3 * n && repeated.pre == 3 * n + 1);
    assert(repeated.suf == 3 * n + 3 && repeated.ans == 3 * n + 4);
}

int main() {
    pair_monoid_test<true, false>();
    pair_monoid_test<false, false>();
    pair_monoid_test<true, true>();
    pair_monoid_test<false, true>();
    affine_test();
    max_sub_sum_test();
    large_index_and_count_test();
    cout << "dynamic segment tree extra utilities: OK\n";
}
