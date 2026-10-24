// g++ -std=c++17 -O2 -I. test/ds/dynamic_segment_tree_util_test.cpp -o /tmp/ds_util_test
// Two-translation-unit check: add -DDYNAMIC_SEGMENT_TREE_UTIL_LINK_TEST and
// test/ds/dynamic_segment_tree_util_link.cpp to the same compilation command.
#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <vector>

#include "titan_cpplib/ds/dynamic_segment_tree_util.cpp"
#include "titan_cpplib/ds/dynamic_lazy_segment_tree_util.cpp"

namespace normal = titan23::dynamic_segment_tree;
namespace lazy = titan23::dynamic_lazy_segment_tree;
using ll = long long;

#ifdef DYNAMIC_SEGMENT_TREE_UTIL_LINK_TEST
ll dynamic_segment_tree_util_other_tu();
#endif

template <class Plain, class Init, class Op, class Pow>
void check_normal(ll identity, ll initial, Op op, Pow pow) {
    for (ll a : {identity, initial}) {
        assert(pow(a, 0LL) == identity);
        assert(pow(a, 1LL) == a);
        for (ll n = 0; n < 9; ++n) {
            ll expected = identity;
            for (ll i = 0; i < n; ++i) expected = op(expected, a);
            assert(pow(a, n) == expected);
            for (ll m = 0; m < 9; ++m)
                assert(pow(a, n + m) == op(pow(a, n), pow(a, m)));
        }
    }
    std::mt19937_64 rng(23);
    for (int n : {0, 1, 2, 9, 17}) {
        Plain sparse(n);
        Init uniform(n, initial);
        std::vector<ll> a(n, identity), b(n, initial);
        for (int step = 0; step < 100; ++step) {
            if (n) {
                const int k = rng() % n;
                const ll value = rng() % 32;
                sparse.set(k, value);
                uniform.set(k, value);
                a[k] = b[k] = value;
            }
            for (int l = 0; l <= n; ++l) {
                ll x = identity, y = identity;
                for (int r = l; r <= n; ++r) {
                    assert(sparse.prod(l, r) == x);
                    assert(uniform.prod(l, r) == y);
                    if (r != n) { x = op(x, a[r]); y = op(y, b[r]); }
                }
            }
            assert(sparse.all_prod() == std::accumulate(a.begin(), a.end(), identity, op));
            assert(uniform.all_prod() == std::accumulate(b.begin(), b.end(), identity, op));
        }
        sparse.clear();
        uniform.clear();
        assert(sparse.all_prod() == identity);
        assert(uniform.all_prod() == pow(initial, static_cast<ll>(n)));
    }
}

template <class Tree>
void verify_sum(const Tree& tree, const std::vector<ll>& values) {
    using S = lazy::Sum<ll>;
    const auto nodes = tree.node_count();
    const int n = values.size();
    for (int l = 0; l <= n; ++l) {
        ll sum = 0;
        for (int r = l; r <= n; ++r) {
            assert((tree.prod(l, r) == S{sum, r - l}));
            if (r != n) sum += values[r];
        }
    }
    for (int i = 0; i < n; ++i) assert((tree.get(i) == S{values[i], 1}));
    assert((tree.all_prod() == S{std::accumulate(values.begin(), values.end(), 0LL), n}));
    assert(tree.node_count() == nodes);
}

void check_lazy_sum() {
    std::mt19937_64 rng(247);
    for (int n : {0, 1, 2, 17, 61}) {
        lazy::LazySegAddSum<ll> add(n, {-3, 1});
        lazy::LazySegSetSum<ll> set(n, {-3, 1});
        lazy::LazySegAffineSum<ll> affine(n, {-3, 1});
        std::vector<ll> a(n, -3), b(n, -3), c(n, -3);
        verify_sum(add, a);
        verify_sum(set, b);
        verify_sum(affine, c);
        for (int step = 0; step < 200; ++step) {
            int l = rng() % (n + 1), r = rng() % (n + 1);
            if (l > r) std::swap(l, r);
            const ll x = static_cast<ll>(rng() % 15) - 7;
            const lazy::Affine<ll> f{static_cast<ll>(rng() % 3) - 1, x};
            add.apply(l, r, x);
            set.apply(l, r, lazy::Assign<ll>::set(x));
            affine.apply(l, r, f);
            for (int i = l; i < r; ++i) {
                a[i] += x;
                b[i] = x;
                c[i] = f.a * c[i] + f.b;
            }
            if (n && step % 7 == 0) {
                const int k = rng() % n;
                add.set(k, {x, 1});
                set.set(k, {x, 1});
                affine.set(k, {x, 1});
                a[k] = b[k] = c[k] = x;
            }
            if (step % 19 == 0) {
                verify_sum(add, a);
                verify_sum(set, b);
                verify_sum(affine, c);
            }
        }
        verify_sum(add, a);
        verify_sum(set, b);
        verify_sum(affine, c);
    }
    // Coordinate/length types can be unsigned without converting negative sums
    // into unsigned intermediate arithmetic.
    lazy::LazySegAddSum<ll, unsigned long long> unsigned_index(10000000000ULL, {-2, 1});
    unsigned_index.apply(1, 9999999999ULL, -3);
    assert(unsigned_index.all_prod().sum == -49999999994LL);
    assert(unsigned_index.all_prod().len == 10000000000ULL);
    assert(unsigned_index.prod(100, 100).len == 0);

    lazy::LazySegAddSum<ll> search(1000000000LL, {2, 1});
    search.apply(100, 200, 3);
    assert(search.max_right(90, [](lazy::Sum<ll> s) { return s.sum <= 35; }) == 103);
    assert(search.min_left(210, [](lazy::Sum<ll> s) { return s.sum <= 35; }) == 197);
}

void check_assignment_extrema() {
    constexpr ll low = std::numeric_limits<ll>::lowest();
    constexpr ll high = std::numeric_limits<ll>::max();
    lazy::LazySegSetMin<ll> minimum(17, 0);
    lazy::LazySegSetMax<ll> maximum(17, 0);
    for (ll value : {low, high, 0LL, -1LL}) {
        minimum.apply(0, 17, lazy::Assign<ll>::set(value));
        maximum.apply(0, 17, lazy::Assign<ll>::set(value));
        assert(minimum.prod(0, 17) == value);
        assert(maximum.prod(0, 17) == value);
        minimum.apply(3, 11, lazy::Assign<ll>::set(7));
        maximum.apply(3, 11, lazy::Assign<ll>::set(7));
        minimum.apply(0, 17, lazy::id_set<ll>());
        maximum.apply(0, 17, lazy::id_set<ll>());
        for (int l = 0; l <= 17; ++l) {
            ll mn = high, mx = low;
            for (int r = l; r <= 17; ++r) {
                assert(minimum.prod(l, r) == mn);
                assert(maximum.prod(l, r) == mx);
                const ll current = 3 <= r && r < 11 ? 7 : value;
                mn = std::min(mn, current);
                mx = std::max(mx, current);
            }
        }
    }
    // The identity aggregate is also a valid per-element value for min/max.
    lazy::LazySegSetMin<ll> default_minimum(100);
    default_minimum.apply(4, 90, lazy::Assign<ll>::set(low));
    assert(default_minimum.prod(0, 4) == high);
    assert(default_minimum.prod(4, 90) == low);

    lazy::LazySegSetSum<ll> single(1, {0, 1});
    single.apply(0, 1, lazy::Assign<ll>::set(high));
    assert(single.get(0).sum == high);
    single.apply(0, 1, lazy::Assign<ll>::set(low));
    assert(single.get(0).sum == low);
    assert(single.prod(0, 0).sum == 0);
}

void check_action_laws() {
    using S = lazy::Sum<ll>;
    for (ll value : {-7LL, 0LL, 13LL}) {
        const S one{value, 1};
        for (ll n = 0; n < 10; ++n) {
            const S aggregate = lazy::pow_sum(one, n);
            assert((aggregate == S{value * n, n}));
            for (ll m = 0; m < 10; ++m)
                assert(lazy::pow_sum(one, n + m) == lazy::op_sum(aggregate, lazy::pow_sum(one, m)));
            for (const lazy::Affine<ll> f : {lazy::Affine<ll>{2, -3}, {0, 7}, {-1, 4}}) {
                const lazy::Affine<ll> g{3, 1};
                assert(lazy::map_affine_sum(lazy::comp_affine(f, g), aggregate) ==
                       lazy::map_affine_sum(f, lazy::map_affine_sum(g, aggregate)));
                assert(lazy::map_affine_sum(f, aggregate) == lazy::pow_sum(lazy::map_affine_sum(f, one), n));
            }
            const auto f = lazy::Assign<ll>::set(-3), g = lazy::Assign<ll>::set(9);
            assert(lazy::map_set_sum(lazy::comp_set(f, g), aggregate) ==
                   lazy::map_set_sum(f, lazy::map_set_sum(g, aggregate)));
            assert(lazy::map_set_sum(lazy::id_set<ll>(), aggregate) == aggregate);
            assert(lazy::map_add_sum(2LL, aggregate) == lazy::pow_sum(lazy::map_add_sum(2LL, one), n));
        }
    }
}

int main() {
    check_normal<normal::SegSum<ll>, normal::SegSumInit<ll>>(0, -3, normal::op_sum<ll>, normal::pow_sum<ll, ll>);
    check_normal<normal::SegMin<ll>, normal::SegMinInit<ll>>(normal::e_min<ll>(), -3, normal::op_min<ll>, normal::pow_min<ll, ll>);
    check_normal<normal::SegMax<ll>, normal::SegMaxInit<ll>>(normal::e_max<ll>(), -3, normal::op_max<ll>, normal::pow_max<ll, ll>);
    check_normal<normal::SegGcd<ll>, normal::SegGcdInit<ll>>(0, 12, normal::op_gcd<ll>, normal::pow_gcd<ll, ll>);
    check_normal<normal::SegXor<ll>, normal::SegXorInit<ll>>(0, 13, normal::op_xor<ll>, normal::pow_xor<ll, ll>);
    check_normal<normal::SegOr<ll>, normal::SegOrInit<ll>>(0, 13, normal::op_or<ll>, normal::pow_or<ll, ll>);
    check_normal<normal::SegAnd<ll>, normal::SegAndInit<ll>>(normal::e_and<ll>(), 13, normal::op_and<ll>, normal::pow_and<ll, ll>);
    normal::SegSumInit<ll, unsigned long long> unsigned_index(10000000000ULL, -3);
    assert(unsigned_index.all_prod() == -30000000000LL);
    check_lazy_sum();
    check_assignment_extrema();
    check_action_laws();
#ifdef DYNAMIC_SEGMENT_TREE_UTIL_LINK_TEST
    assert(dynamic_segment_tree_util_other_tu() == -30);
#endif
    std::cout << "dynamic segment tree utilities: OK\n";
}
