#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "titan_cpplib/ds/dynamic_segment_tree.cpp"
#include "titan_cpplib/ds/dynamic_segment_tree_init.cpp"
#include "titan_cpplib/ds/dynamic_lazy_segment_tree.cpp"

using namespace std;
using ll = long long;

// 所有権を持つ型でも再確保や作用の持ち回りで移動済みの値を読まないことを確認する
struct Owned {
    unique_ptr<array<ll, 2>> payload;

    Owned() = delete;

    Owned(ll sum, ll len) : payload(make_unique<array<ll, 2>>(array<ll, 2>{sum, len})) {}

    Owned(const Owned& other) : payload(make_unique<array<ll, 2>>(*other.payload)) {}

    Owned& operator=(const Owned& other) {
        assert(other.payload);
        payload = make_unique<array<ll, 2>>(*other.payload);
        return *this;
    }

    Owned(Owned&&) noexcept = default;

    Owned& operator=(Owned&&) noexcept = default;

    ll sum() const { assert(payload); return (*payload)[0]; }

    ll len() const { assert(payload); return (*payload)[1]; }
};

Owned own_op(Owned a, Owned b) { return {a.sum() + b.sum(), a.len() + b.len()}; }

Owned own_e() { return {0, 0}; }

Owned own_pow(Owned value, ll count) { return {value.sum() * count, value.len() * count}; }

// 作用もデフォルト構築と等値比較を提供せず、移動時には元の所有権を手放す
struct Action {
    inline static int live = 0;
    unique_ptr<array<ll, 2>> payload;

    Action() = delete;

    Action(ll a, ll b) : payload(make_unique<array<ll, 2>>(array<ll, 2>{a, b})) { ++live; }

    Action(const Action& other) : payload(make_unique<array<ll, 2>>(*other.payload)) { ++live; }

    Action& operator=(const Action& other) {
        assert(other.payload);
        payload = make_unique<array<ll, 2>>(*other.payload);
        return *this;
    }

    Action(Action&& other) noexcept : payload(move(other.payload)) { ++live; }

    Action& operator=(Action&&) noexcept = default;

    ~Action() { --live; }

    ll a() const { assert(payload); return (*payload)[0]; }

    ll b() const { assert(payload); return (*payload)[1]; }
};

Owned own_map(Action f, Owned value) { return {f.a() * value.sum() + f.b() * value.len(), value.len()}; }

Action own_compose(Action f, Action g) { return {f.a() * g.a(), f.a() * g.b() + f.b()}; }

Action own_id() { return {1, 0}; }

using Plain = titan23::DynamicSegmentTree<ll, Owned, own_op, own_e>;
using Initial = titan23::DynamicSegmentTreeInit<ll, Owned, own_op, own_e, own_pow>;
using Lazy = titan23::DynamicLazySegmentTree<ll, Owned, own_op, own_e, Action,
                                            own_map, own_compose, own_id, own_pow>;

template<class Tree>
void verify_owned(const Tree& tree, const vector<ll>& expected, const vector<ll>& lengths) {
    const ll n = static_cast<ll>(expected.size());
    const auto count = tree.node_count();
    ll total = 0, length = 0;
    for (ll value : expected) total += value;
    for (ll value : lengths) length += value;
    assert(tree.all_prod().sum() == total);
    assert(tree.all_prod().len() == length);
    for (ll l = 0; l <= n; ++l) {
        ll sum = 0, len = 0;
        for (ll r = l; r <= n; ++r) {
            auto value = tree.prod(l, r);
            assert(value.sum() == sum && value.len() == len);
            if (r != n) { sum += expected[r]; len += lengths[r]; }
        }
        for (ll limit : {0LL, 2LL, total / 2, total}) {
            const auto pred = [limit](const Owned& x) { return x.sum() <= limit; };
            ll r = l, accepted = 0;
            while (r < n && accepted + expected[r] <= limit) accepted += expected[r++];
            assert(tree.max_right(l, pred) == r);
            ll left = l;
            accepted = 0;
            while (left > 0 && expected[left - 1] + accepted <= limit) accepted += expected[--left];
            assert(tree.min_left(l, pred) == left);
        }
        if (l != n) {
            auto value = tree.get(l);
            assert(value.sum() == expected[l] && value.len() == lengths[l]);
        }
    }
    assert(tree.node_count() == count);
}

template<class Tree>
void verify_empty(const Tree& tree) {
    assert(tree.size() == 0 && tree.node_count() == 0);
    assert(tree.all_prod().sum() == 0 && tree.all_prod().len() == 0);
    assert(tree.prod(0, 0).sum() == 0 && tree.prod(0, 0).len() == 0);
    const auto pred = [](const Owned& value) { return value.sum() == 0; };
    assert(tree.max_right(0, pred) == 0);
    assert(tree.min_left(0, pred) == 0);
}

void owned_storage_test() {
    const ll n = 37;
    Plain plain(n);
    Initial initial(n, {2, 1});
    Lazy lazy(n, {2, 1});
    vector<ll> a(n), a_len(n), b(n, 2), lengths(n, 1);
    for (ll k : {19, 3, 34, 0, 29, 14, 36, 2, 17, 8, 25}) {
        const ll value = k % 5 + 1;
        plain.set(k, {value, 1});
        initial.set(k, {value, 1});
        lazy.set(k, {value, 1});
        a[k] = b[k] = value;
        a_len[k] = 1;
    }
    plain.reserve_nodes(128);
    initial.reserve_nodes(128);
    lazy.reserve_nodes(128);
    verify_owned(plain, a, a_len);
    verify_owned(initial, b, lengths);
    lazy.all_apply({0, 4});
    lazy.all_apply({1, 3});
    fill(b.begin(), b.end(), 7);
    lazy.apply(6, 32, {1, 2});
    for (ll k = 6; k < 32; ++k) b[k] += 2;
    lazy.all_apply({1, 1});
    for (ll& value : b) ++value;
    verify_owned(lazy, b, lengths);

    auto plain_copy = plain;
    auto initial_copy = initial;
    auto lazy_copy = lazy;
    plain_copy.set(0, {12, 1});
    initial_copy.set(0, {12, 1});
    lazy_copy.apply(1, n - 1, {0, 12});
    verify_owned(plain, a, a_len);
    vector<ll> initial_values(n, 2);
    for (ll k = 0; k < n; ++k) if (a_len[k] != 0) initial_values[k] = a[k];
    verify_owned(initial, initial_values, lengths);
    verify_owned(lazy, b, lengths);
    auto moved_plain = move(plain);
    auto moved_initial = move(initial);
    auto moved_lazy = move(lazy);
    verify_empty(plain);
    verify_empty(initial);
    verify_empty(lazy);
    plain = move(moved_plain);
    initial = move(moved_initial);
    lazy = move(moved_lazy);
    verify_empty(moved_plain);
    verify_empty(moved_initial);
    verify_empty(moved_lazy);
    verify_owned(plain, a, a_len);
    verify_owned(initial, initial_values, lengths);
    verify_owned(lazy, b, lengths);
    lazy_copy = lazy;
    lazy_copy.apply(0, n, {0, 1});
    verify_owned(lazy, b, lengths);
    plain.clear();
    initial.clear();
    lazy.clear();
    verify_owned(plain, vector<ll>(n, 0), vector<ll>(n, 0));
    verify_owned(initial, vector<ll>(n, 2), lengths);
    verify_owned(lazy, vector<ll>(n, 2), lengths);
}

void search_stack_lifetime_test() {
    const ll n = 129;
    Lazy tree(n, {2, 1});
    vector<ll> expected(n), lengths(n, 1);
    for (ll k = 0; k < n; ++k) {
        expected[k] = k % 7 + 1;
        tree.set(k, {expected[k], 1});
    }
    tree.all_apply({1, 3});
    for (ll& value : expected) value += 3;
    tree.apply(9, 120, {1, 2});
    for (ll k = 9; k < 120; ++k) expected[k] += 2;
    tree.all_apply({1, 1});
    for (ll& value : expected) ++value;
    const int live = Action::live;
    int right_failures = 0, left_failures = 0;
    // 探索途中で述語が例外を投げても生の領域に構築した全作用を破棄する
    for (int failure = 1; failure < 20; ++failure) {
        int calls = 0;
        try {
            tree.max_right(17, [&](const Owned& value) {
                if (++calls == failure) throw failure;
                return value.sum() <= 100;
            });
        } catch (int reached) {
            assert(reached == failure);
            ++right_failures;
        }
        assert(Action::live == live);
        calls = 0;
        try {
            tree.min_left(110, [&](const Owned& value) {
                if (++calls == failure) throw failure;
                return value.sum() <= 100;
            });
        } catch (int reached) {
            assert(reached == failure);
            ++left_failures;
        }
        assert(Action::live == live);
    }
    assert(right_failures > 2 && left_failures > 2);
    verify_owned(tree, expected, lengths);
    assert(Action::live == live);
}

int min_op(int a, int b) { return min(a, b); }

int min_e() { return numeric_limits<int>::max(); }

template<class Index>
int min_pow(int value, Index n) { return n == 0 ? min_e() : value; }

struct Assign { bool active; int value; };

int min_map(Assign f, int value) { return f.active ? f.value : value; }

Assign min_compose(Assign f, Assign g) { return f.active ? f : g; }

Assign min_id() { return {false, 0}; }

template<class Index>
void wide_index_test(Index n) {
    using P = titan23::DynamicSegmentTree<Index, int, min_op, min_e>;
    using I = titan23::DynamicSegmentTreeInit<Index, int, min_op, min_e, min_pow<Index>>;
    using L = titan23::DynamicLazySegmentTree<Index, int, min_op, min_e, Assign,
                                             min_map, min_compose, min_id, min_pow<Index>>;
    const Index middle = n / 2, split = (Index(1) << 80) + 31;
    P plain(n);
    I initial(n, 10);
    L lazy(n, 10);
    for (Index position : {Index(0), split, middle, n - 1}) {
        plain.set(position, 8);
        initial.set(position, 8);
    }
    plain.set(middle, 3);
    initial.set(middle, 3);
    lazy.apply(split, n, {true, 8});
    lazy.all_apply({true, 6});
    lazy.apply(middle, n - 1, {true, 3});
    lazy.set(n - 1, 2);
    const auto high = [](int value) { return value >= 4; };
    assert(plain.all_prod() == 3 && initial.all_prod() == 3 && lazy.all_prod() == 2);
    assert(plain.max_right(0, high) == middle);
    assert(initial.max_right(0, high) == middle);
    assert(lazy.max_right(0, high) == middle);
    assert(plain.min_left(n, high) == middle + 1);
    assert(initial.min_left(n, high) == middle + 1);
    assert(lazy.min_left(n, high) == n);
    assert(plain.prod(split + 1, middle) == min_e());
    assert(initial.prod(split + 1, middle) == 10);
    assert(lazy.prod(split + 1, middle) == 6);
    assert(lazy.prod(middle, n - 1) == 3);
    assert(lazy.get(n - 1) == 2);
    assert(lazy.prod(n, n) == min_e());
    assert(lazy.max_right(n, high) == n);
    assert(lazy.min_left(0, high) == 0);
}

using i128 = __int128_t;

struct WideSum { i128 sum; ll len; };

WideSum wide_op(WideSum a, WideSum b) { return {a.sum + b.sum, a.len + b.len}; }

WideSum wide_e() { return {0, 0}; }

WideSum wide_map(ll add, WideSum value) { return {value.sum + i128(add) * value.len, value.len}; }

ll wide_compose(ll f, ll g) { return f + g; }

ll wide_id() { return 0; }

WideSum wide_pow(WideSum value, ll count) { return {value.sum * count, value.len * count}; }

using WideTree = titan23::DynamicLazySegmentTree<ll, WideSum, wide_op, wide_e, ll,
                                                wide_map, wide_compose, wide_id, wide_pow>;

void large_aggregate_test(ll n) {
    const vector<ll> edges{0, 1, 17, n / 3, n / 2, n / 2 + 19,
                           n / 2 + 20, n - 4099, n - 7, n - 1, n};
    vector<ll> values(edges.size() - 1, 3);
    WideTree tree(n, {3, 1});
    const auto add = [&](ll l, ll r, ll value) {
        tree.apply(l, r, value);
        for (size_t i = 0; i < values.size(); ++i) {
            if (l <= edges[i] && edges[i + 1] <= r) values[i] += value;
        }
    };
    add(0, n, 20);
    add(17, n - 7, 9);
    add(n / 3, n / 2 + 20, 17);
    tree.all_apply(5);
    for (ll& value : values) value += 5;
    for (ll k : {n / 2 + 19, n - 1}) {
        tree.set(k, {4, 1});
        values[lower_bound(edges.begin(), edges.end(), k) - edges.begin()] = 4;
    }

    // 少数の一様区間を直接走査し、64 bitに収まらない集約値と境界を照合する
    const auto verify = [&](const WideTree& actual, const vector<ll>& expected) {
        const auto count = actual.node_count();
        const auto reference_prod = [&](ll l, ll r) {
            i128 result = 0;
            for (size_t i = 0; i < expected.size(); ++i) {
                const ll length = max(0LL, min(r, edges[i + 1]) - max(l, edges[i]));
                result += i128(expected[i]) * length;
            }
            return result;
        };
        const i128 total = reference_prod(0, n);
        assert(actual.size() == n);
        assert(actual.all_prod().sum == total && actual.all_prod().len == n);
        vector<ll> probes = edges;
        probes.insert(probes.end(), {n / 7, n / 2 + 18, n - 1000});
        for (ll l : probes) {
            for (ll r : probes) if (l <= r) {
                const auto value = actual.prod(l, r);
                assert(value.sum == reference_prod(l, r) && value.len == r - l);
            }
            if (l < n) {
                const auto value = actual.get(l);
                assert(value.sum == reference_prod(l, l + 1) && value.len == 1);
            }
            for (i128 limit : {i128(0), i128(19), i128(numeric_limits<ll>::max()) + 1, total / 2, total}) {
                ll right = l, left = l;
                i128 remaining = limit;
                for (size_t i = 0; i < expected.size(); ++i) {
                    if (edges[i + 1] <= right) continue;
                    const ll take = static_cast<ll>(min(i128(edges[i + 1] - right), remaining / expected[i]));
                    remaining -= i128(take) * expected[i];
                    right += take;
                    if (right != edges[i + 1]) break;
                }
                remaining = limit;
                for (size_t i = expected.size(); i-- > 0;) {
                    if (left <= edges[i]) continue;
                    const ll take = static_cast<ll>(min(i128(left - edges[i]), remaining / expected[i]));
                    remaining -= i128(take) * expected[i];
                    left -= take;
                    if (left != edges[i]) break;
                }
                const auto pred = [limit](WideSum value) { return value.sum <= limit; };
                assert(actual.max_right(l, pred) == right);
                assert(actual.min_left(l, pred) == left);
            }
        }
        assert(actual.node_count() == count);
    };
    verify(tree, values);
    if (n == 1000000000000000000LL) {
        assert(tree.all_prod().sum > numeric_limits<ll>::max());
        assert(tree.prod(n / 3, n - 7).sum > numeric_limits<ll>::max());
    }
    auto copy = tree;
    copy.all_apply(3);
    auto copied_values = values;
    for (ll& value : copied_values) value += 3;
    verify(copy, copied_values);
    verify(tree, values);
    copy.clear();
    verify(copy, vector<ll>(values.size(), 3));
}

int main() {
    owned_storage_test();
    assert(Action::live == 0);
    search_stack_lifetime_test();
    assert(Action::live == 0);
    wide_index_test<__int128_t>(static_cast<__int128_t>((~__uint128_t(0)) >> 1));
    wide_index_test<__uint128_t>(~__uint128_t(0));
    large_aggregate_test(1000000000LL);
    large_aggregate_test(1000000000000000000LL);
    cout << "dynamic_segment_tree_generic: OK\n";
}
