#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <vector>

#include "titan_cpplib/ds/dynamic_lazy_segment_tree_util.cpp"
#include "titan_cpplib/ds/lazysegutil.cpp"

using namespace std;
using ll = long long;
namespace sparse = titan23::dynamic_lazy_segment_tree;
namespace dense = titan23::lazy_segment_tree;

/// @brief 最小値または最大値を全区間で照合する
template <class Sparse, class Dense>
void verify_extrema(const Sparse& actual, Dense& reference, const vector<ll>& a, bool minimum) {
    const auto nodes = actual.node_count();
    const int n = a.size();
    const ll identity = minimum ? numeric_limits<ll>::max() : numeric_limits<ll>::lowest();
    for (int l = 0; l <= n; ++l) {
        ll value = identity;
        for (int r = l; r <= n; ++r) {
            assert(actual.prod(l, r) == value);
            assert(reference.prod(l, r) == value);
            if (r < n) value = minimum ? min(value, a[r]) : max(value, a[r]);
        }
    }
    for (int k = 0; k < n; ++k) assert(actual.get(k) == a[k]);
    assert(actual.all_prod() == reference.all_prod());
    assert(actual.node_count() == nodes);
}

/// @brief 加算とchminとchmaxをdense版および配列と比較する
template <class Sparse, class Dense, class Transform>
void check_extrema(bool minimum, Transform transform) {
    mt19937_64 rng(738223);
    for (int n : {1, 2, 3, 8, 17}) {
        vector<ll> a(n, 3);
        Sparse actual(n, 3);
        Dense reference(a);
        for (int step = 0; step < 300; ++step) {
            int l = rng() % (n + 1), r = rng() % (n + 1);
            if (l > r) swap(l, r);
            const ll f = static_cast<ll>(rng() % 11) - 5;
            if (step % 9 == 0) {
                const int k = rng() % n;
                a[k] = static_cast<ll>(rng() % 41) - 20;
                actual.set(k, a[k]);
                reference.set(k, a[k]);
            } else if (step % 7 == 0) {
                const int k = rng() % n;
                actual.apply(k, f);
                reference.apply_point(k, f);
                a[k] = transform(a[k], f);
            } else if (step % 13 == 0) {
                actual.all_apply(f);
                reference.apply(0, n, f);
                for (ll& x : a) x = transform(x, f);
            } else {
                actual.apply(l, r, f);
                reference.apply(l, r, f);
                for (int k = l; k < r; ++k) a[k] = transform(a[k], f);
            }
            if (step % 31 == 0) verify_extrema(actual, reference, a, minimum);
        }
        verify_extrema(actual, reference, a, minimum);
        actual.clear();
        assert(actual.all_prod() == 3);
    }
    Sparse empty(0, 3);
    empty.all_apply(2);
    assert(empty.prod(0, 0) == (minimum ? numeric_limits<ll>::max() : numeric_limits<ll>::lowest()));
}

/// @brief 代入と加算による統計値を全区間で照合する
void verify_stats(const sparse::LazySegSetAddStats<ll>& actual,
                  dense::LazySegSetAddStats<ll>& reference, const vector<ll>& a) {
    const int n = a.size();
    const auto nodes = actual.node_count();
    for (int l = 0; l <= n; ++l) {
        auto expected = sparse::e_stats<ll, ll>();
        for (int r = l; r <= n; ++r) {
            const auto got = actual.prod(l, r);
            const auto other = reference.prod(l, r);
            assert(got == expected);
            assert(got.sum == other.sum && got.mn == other.mn && got.mx == other.mx && got.len == other.size);
            if (r < n) expected = sparse::op_stats(expected, sparse::Stats<ll>{a[r], a[r], a[r], 1});
        }
    }
    assert(actual.node_count() == nodes);
}

/// @brief 代入フラグと加算の合成順序および全値域の代入を確認する
void check_stats() {
    mt19937_64 rng(343233);
    for (int n : {1, 2, 3, 8, 17}) {
        vector<ll> a(n, 2);
        sparse::LazySegSetAddStats<ll> actual(n, {2, 2, 2, 1});
        dense::LazySegSetAddStats<ll> reference(vector<dense::Stats<ll>>(n, {2, 2, 2, 1}));
        for (int step = 0; step < 300; ++step) {
            int l = rng() % (n + 1), r = rng() % (n + 1);
            if (l > r) swap(l, r);
            const bool set = rng() % 2;
            const ll value = static_cast<ll>(rng() % 31) - 15;
            const ll add = static_cast<ll>(rng() % 11) - 5;
            const sparse::SetAdd<ll> f{set, value, add};
            const dense::F_SetAdd<ll> g{set, value, add};
            if (step % 11 == 0) {
                l = 0; r = n;
                actual.all_apply(f);
            } else actual.apply(l, r, f);
            reference.apply(l, r, g);
            for (int k = l; k < r; ++k) a[k] = (set ? value : a[k]) + add;
            if (step % 31 == 0) verify_stats(actual, reference, a);
        }
        verify_stats(actual, reference, a);
        actual.clear();
        assert((actual.all_prod() == sparse::Stats<ll>{2LL * n, 2, 2, n}));
    }
    sparse::LazySegSetAddStats<ll> empty(0, {0, 0, 0, 1});
    empty.all_apply(sparse::SetAdd<ll>::set(9));
    assert((empty.all_prod() == sparse::e_stats<ll, ll>()));
    sparse::LazySegSetAddStats<ll> one(1, {0, 0, 0, 1});
    for (ll value : {numeric_limits<ll>::max(), numeric_limits<ll>::lowest()}) {
        one.all_apply(sparse::SetAdd<ll>::set(value));
        assert((one.get(0) == sparse::Stats<ll>{value, value, value, 1}));
    }
}

/// @brief 等差数列加算の式をdense版と配列に照合する
void verify_ap(const sparse::LazySegAddAPSum<ll>& actual,
               dense::LazySegAddAPSum<ll>& reference, const vector<ll>& a) {
    const int n = a.size();
    const auto nodes = actual.node_count();
    for (int l = 0; l <= n; ++l) {
        ll sum = 0;
        for (int r = l; r <= n; ++r) {
            const auto got = actual.prod(l, r);
            assert(got.eval(l) == sum && got.len == r - l);
            assert(reference.prod(l, r).val == sum);
            if (r < n) sum += a[r];
        }
        for (ll limit : {0LL, 1LL, 20LL, 1000LL, 1000000LL}) {
            int right = l, left = l;
            ll value = 0;
            while (right < n && value + a[right] <= limit) value += a[right++];
            value = 0;
            while (left > 0 && value + a[left - 1] <= limit) value += a[--left];
            assert(actual.max_right(l, [=](auto s) { return s.eval(l) <= limit; }) == right);
            assert(actual.min_left(l, [=](auto s) { return s.eval(l - s.len) <= limit; }) == left);
        }
    }
    for (int k = 0; k < n; ++k) assert(actual.get(k).eval(k) == a[k]);
    assert(actual.all_prod().eval(0) == accumulate(a.begin(), a.end(), 0LL));
    assert(actual.node_count() == nodes);
}

/// @brief 一様初期値から等差数列を部分加算して照合する
void check_ap() {
    mt19937_64 rng(192113);
    for (int n : {1, 2, 3, 8, 17}) {
        vector<ll> a(n, 2);
        vector<dense::S_AP<ll>> initial;
        for (int k = 0; k < n; ++k) initial.push_back({2, 1, k});
        sparse::LazySegAddAPSum<ll> actual(n, {2, 0, 1});
        dense::LazySegAddAPSum<ll> reference(initial);
        for (int step = 0; step < 300; ++step) {
            int l = rng() % (n + 1), r = rng() % (n + 1);
            if (l > r) swap(l, r);
            const ll first = rng() % 7, difference = rng() % 4;
            if (step % 11 == 0) {
                const int k = rng() % n;
                a[k] = rng() % 20;
                actual.set(k, {a[k], 0, 1});
                reference.set(k, {a[k], 1, k});
            } else {
                if (step % 13 == 0) {
                    l = 0; r = n;
                    actual.all_apply({first, difference});
                } else actual.apply(l, r, {first - difference * l, difference});
                reference.apply(l, r, {first - difference * l, difference});
                for (int k = l; k < r; ++k) a[k] += first + difference * (k - l);
            }
            if (step % 31 == 0) verify_ap(actual, reference, a);
        }
        verify_ap(actual, reference, a);
        actual.clear();
        assert(actual.all_prod().eval(0) == 2LL * n);
    }
    sparse::LazySegAddAPSum<ll> empty(0, {0, 0, 1});
    empty.all_apply({1, 2});
    assert((empty.prod(0, 0) == sparse::e_ap_sum<ll, ll>()));
}

/// @brief 等差数列用モノイドと作用の法則を確認する
void check_ap_laws() {
    using S = sparse::APSum<ll>;
    using F = sparse::APAdd<ll>;
    mt19937_64 rng(10919);
    const auto random_s = [&]() { return S{ll(rng() % 17) - 8, ll(rng() % 7) - 3, ll(rng() % 5) + 1}; };
    for (int step = 0; step < 1000; ++step) {
        const S a = random_s(), b = random_s(), c = random_s();
        const F f{ll(rng() % 7) - 3, ll(rng() % 5) - 2}, g{ll(rng() % 7) - 3, ll(rng() % 5) - 2};
        assert(sparse::op_ap_sum(sparse::op_ap_sum(a, b), c) == sparse::op_ap_sum(a, sparse::op_ap_sum(b, c)));
        assert((sparse::op_ap_sum(a, sparse::e_ap_sum<ll, ll>()) == a));
        assert((sparse::op_ap_sum(sparse::e_ap_sum<ll, ll>(), a) == a));
        assert(sparse::map_ap_add(f, sparse::op_ap_sum(a, b)) == sparse::op_ap_sum(sparse::map_ap_add(f, a), sparse::map_ap_add(f, b)));
        assert(sparse::map_ap_add(sparse::comp_ap_add(f, g), a) == sparse::map_ap_add(f, sparse::map_ap_add(g, a)));
        S repeated = sparse::e_ap_sum<ll, ll>();
        for (ll n = 0; n < 9; ++n) {
            assert(sparse::pow_ap_sum(a, n) == repeated);
            repeated = sparse::op_ap_sum(repeated, a);
        }
    }
}

struct ModNoDivision {
    ll value;

    /// @brief 整数を法101へ写す
    ModNoDivision(ll x = 0) : value((x % 101 + 101) % 101) {}

    /// @brief 法101で加算する
    friend ModNoDivision operator+(ModNoDivision a, ModNoDivision b) { return a.value + b.value; }

    /// @brief 法101で乗算する
    friend ModNoDivision operator*(ModNoDivision a, ModNoDivision b) { return a.value * b.value; }

    /// @brief 値が等しいか判定する
    bool operator==(ModNoDivision other) const { return value == other.value; }
};

/// @brief Tの除算を要求しないこととunsignedの長さ0を確認する
void check_modular_ap() {
    sparse::LazySegAddAPSum<ModNoDivision, unsigned long long> tree(1000000000ULL, {3, 0, 1});
    tree.all_apply({2, 5});
    assert(tree.node_count() == 1);
    assert((sparse::scaled_triangular(ModNoDivision(1), 0ULL) == ModNoDivision(0)));
    const auto value = tree.prod(17, 123);
    ModNoDivision expected = 0;
    for (ll i = 17; i < 123; ++i) expected = expected + ModNoDivision(5 + 5 * i);
    assert(value.eval(17) == expected);
}

/// @brief 巨大座標でも点ごとの初期化なしに等差数列の和を得る
void check_large_ap() {
    using Wide = __int128_t;
    const ll n = 1000000000000000000LL;
    sparse::LazySegAddAPSum<Wide> tree(n, {2, 0, 1});
    tree.all_apply({3, 2});
    assert(tree.node_count() == 1);
    const auto interval_sum = [](ll l, ll r) -> Wide {
        return Wide(5) * (r - l) + (Wide(l) + r - 1) * (r - l);
    };
    assert(tree.all_prod().eval(0) == interval_sum(0, n));
    tree.apply(n - 1000, n - 5, {-Wide(3) * (n - 1000) + 7, 3});
    assert(tree.node_count() == 5);
    for (const auto& range : {pair<ll, ll>{0, n}, {n - 1001, n}, {n - 990, n - 10}, {n - 5, n}}) {
        const ll l = range.first, r = range.second;
        const ll a = max(l, n - 1000), b = min(r, n - 5);
        Wide expected = interval_sum(l, r);
        if (a < b) expected += Wide(7) * (b - a) + Wide(3) * (Wide(a - (n - 1000)) + b - (n - 1000) - 1) * (b - a) / 2;
        assert(tree.prod(l, r).eval(l) == expected);
    }
}

/// @brief 傾き0なら巨大な長さでも不要な三角数を計算しないことを確認する
void check_large_constant_ap() {
    const ll n = 1000000000000000000LL;
    sparse::LazySegAddAPSum<ll> zero(n, {0, 0, 1});
    assert(zero.all_prod().eval(0) == 0);
    assert(zero.prod(17, n - 3).eval(17) == 0);
    zero.all_apply({2, 0});
    assert(zero.all_prod().eval(0) == 2 * n);
    assert(zero.node_count() == 1);
    zero.apply(n - 100, n - 1, {3, 0});
    assert(zero.all_prod().eval(0) == 2 * n + 3 * 99);
    assert(zero.prod(n - 120, n).eval(n - 120) == 2 * 120 + 3 * 99);
    sparse::LazySegAddAPSum<ll> constant(n, {3, 0, 1});
    constant.all_apply({1, 0});
    assert(constant.all_prod().eval(0) == 4 * n);
    const sparse::APSum<ll> large_slope{0, numeric_limits<ll>::max(), 2};
    assert((sparse::pow_ap_sum(large_slope, 0LL) == sparse::e_ap_sum<ll, ll>()));
    assert(sparse::pow_ap_sum(large_slope, 1LL) == large_slope);
}

int main() {
    check_extrema<sparse::LazySegAddMin<ll>, dense::LazySegAddMin<ll>>(true, [](ll x, ll f) { return x + f; });
    check_extrema<sparse::LazySegAddMax<ll>, dense::LazySegAddMax<ll>>(false, [](ll x, ll f) { return x + f; });
    check_extrema<sparse::LazySegChminMin<ll>, dense::LazySegChminMin<ll>>(true, [](ll x, ll f) { return min(x, f); });
    check_extrema<sparse::LazySegChminMax<ll>, dense::LazySegChminMax<ll>>(false, [](ll x, ll f) { return min(x, f); });
    check_extrema<sparse::LazySegChmaxMin<ll>, dense::LazySegChmaxMin<ll>>(true, [](ll x, ll f) { return max(x, f); });
    check_extrema<sparse::LazySegChmaxMax<ll>, dense::LazySegChmaxMax<ll>>(false, [](ll x, ll f) { return max(x, f); });
    check_stats();
    check_ap();
    check_ap_laws();
    check_modular_ap();
    check_large_ap();
    check_large_constant_ap();
    cout << "dynamic lazy segment tree extra utilities: OK\n";
}
