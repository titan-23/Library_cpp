#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "titan_cpplib/ds/dynamic_segment_tree.cpp"
#include "titan_cpplib/ds/dynamic_segment_tree_init.cpp"
#include "titan_cpplib/ds/dynamic_lazy_segment_tree.cpp"

using ll = long long;

ll sum_op(ll a, ll b) { return a + b; }
ll sum_e() { return 0; }
ll sum_pow(ll a, ll n) { return a * n; }

using SumTree = titan23::DynamicSegmentTree<ll, ll, sum_op, sum_e>;
using InitSumTree = titan23::DynamicSegmentTreeInit<ll, ll, sum_op, sum_e, sum_pow>;

template<class T, class Op, class Pred>
int scan_right(const std::vector<T> &a, int l, T identity, Op op, Pred pred) {
    T aggregate = identity;
    int r = l;
    while (r < static_cast<int>(a.size())) {
        T next = op(aggregate, a[r]);
        if (!pred(next)) break;
        aggregate = std::move(next);
        ++r;
    }
    return r;
}

template<class T, class Op, class Pred>
int scan_left(const std::vector<T> &a, int r, T identity, Op op, Pred pred) {
    T aggregate = identity;
    int l = r;
    while (l > 0) {
        T next = op(a[l - 1], aggregate);
        if (!pred(next)) break;
        aggregate = std::move(next);
        --l;
    }
    return l;
}

template<class Tree>
void verify_sum(const Tree &tree, const std::vector<ll> &a, bool all_ranges) {
    const int n = static_cast<int>(a.size());
    const auto count = tree.node_count();
    assert(tree.size() == n);
    assert(tree.all_prod() == std::accumulate(a.begin(), a.end(), 0LL));
    assert(tree.prod(0, n) == tree.all_prod());
    for (int i = 0; i < n; ++i) assert(tree.get(i) == a[i]);
    for (int i = 0; i <= n; ++i) {
        assert(tree.prod(i, i) == 0);
        for (ll limit : {0LL, 1LL, 7LL, 43LL, 1000000LL}) {
            const auto pred = [limit](ll value) { return value <= limit; };
            assert(tree.max_right(i, pred) == scan_right(a, i, 0LL, sum_op, pred));
            assert(tree.min_left(i, pred) == scan_left(a, i, 0LL, sum_op, pred));
        }
    }
    if (all_ranges) {
        for (int l = 0; l <= n; ++l) {
            ll sum = 0;
            for (int r = l; r <= n; ++r) {
                assert(tree.prod(l, r) == sum);
                if (r < n) sum += a[r];
            }
        }
    }
    assert(tree.node_count() == count);
}

template<class Tree>
void random_sum(Tree tree, int n, ll initial, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<ll> a(n, initial);
    tree.reserve_nodes(1);
    verify_sum(tree, a, true);
    for (int step = 0; step < 600; ++step) {
        const int operation = static_cast<int>(rng() % 7);
        if (operation <= 2 && n != 0) {
            const int k = static_cast<int>(rng() % n);
            a[k] = static_cast<ll>(rng() % 30);
            tree.set(k, a[k]);
        } else if (operation == 3) {
            tree.reserve_nodes(tree.node_count() + 1 + rng() % 20);
        } else if (operation == 4 && step % 41 == 0) {
            tree.clear();
            std::fill(a.begin(), a.end(), initial);
        } else {
            int l = static_cast<int>(rng() % (n + 1));
            int r = static_cast<int>(rng() % (n + 1));
            if (l > r) std::swap(l, r);
            const Tree &read = tree;
            const auto count = read.node_count();
            assert(read.prod(l, r) == std::accumulate(a.begin() + l, a.begin() + r, 0LL));
            assert(read.node_count() == count);
        }
        if (step % 97 == 0) verify_sum(tree, a, false);
    }
    verify_sum(tree, a, true);

    auto copy = tree;
    if (n != 0) copy.set(n - 1, 101);
    verify_sum(tree, a, false);
    std::vector<ll> copied = a;
    if (n != 0) copied[n - 1] = 101;
    verify_sum(copy, copied, false);
    Tree assigned;
    assigned = copy;
    auto moved = std::move(assigned);
    verify_sum(moved, copied, false);
    Tree move_assigned;
    move_assigned = std::move(moved);
    verify_sum(move_assigned, copied, false);
    copy.clear();
    verify_sum(copy, std::vector<ll>(n, initial), true);
    verify_sum(move_assigned, copied, false);
}

struct SumLength {
    ll sum;
    ll len;
};

// Deliberately no operator==: lazy tags must not require equality comparison.
struct Affine {
    ll a;
    ll b;
};

SumLength aggregate_op(SumLength a, SumLength b) { return {a.sum + b.sum, a.len + b.len}; }
SumLength aggregate_e() { return {0, 0}; }
SumLength aggregate_pow(SumLength a, ll n) { return {a.sum * n, a.len * n}; }
SumLength affine_map(Affine f, SumLength s) { return {f.a * s.sum + f.b * s.len, s.len}; }
Affine affine_compose(Affine f, Affine g) { return {f.a * g.a, f.a * g.b + f.b}; }
Affine affine_id() { return {1, 0}; }

using LazySumTree = titan23::DynamicLazySegmentTree<
    ll, SumLength, aggregate_op, aggregate_e, Affine,
    affine_map, affine_compose, affine_id, aggregate_pow>;

void assert_aggregate(SumLength actual, ll sum, ll len) {
    assert(actual.sum == sum);
    assert(actual.len == len);
}

void verify_lazy(const LazySumTree &tree, const std::vector<ll> &a, bool all_ranges) {
    const int n = static_cast<int>(a.size());
    const auto count = tree.node_count();
    assert(tree.size() == n);
    assert_aggregate(tree.all_prod(), std::accumulate(a.begin(), a.end(), 0LL), n);
    for (int i = 0; i < n; ++i) assert_aggregate(tree.get(i), a[i], 1);
    for (int i = 0; i <= n; ++i) {
        assert_aggregate(tree.prod(i, i), 0, 0);
        for (ll limit : {0LL, 1LL, 7LL, 43LL, 1000000LL}) {
            const auto pred = [limit](SumLength value) { return value.sum <= limit; };
            const auto scalar_pred = [limit](ll value) { return value <= limit; };
            assert(tree.max_right(i, pred) == scan_right(a, i, 0LL, sum_op, scalar_pred));
            assert(tree.min_left(i, pred) == scan_left(a, i, 0LL, sum_op, scalar_pred));
        }
    }
    if (all_ranges) {
        for (int l = 0; l <= n; ++l) {
            ll sum = 0;
            for (int r = l; r <= n; ++r) {
                assert_aggregate(tree.prod(l, r), sum, r - l);
                if (r < n) sum += a[r];
            }
        }
    }
    assert(tree.node_count() == count);
}

void random_lazy(int n, ll initial, uint64_t seed) {
    LazySumTree tree(n, {initial, 1});
    std::vector<ll> a(n, initial);
    std::mt19937_64 rng(seed);
    tree.reserve_nodes(1);
    verify_lazy(tree, a, true);
    for (int step = 0; step < 700; ++step) {
        int l = static_cast<int>(rng() % (n + 1));
        int r = static_cast<int>(rng() % (n + 1));
        if (l > r) std::swap(l, r);
        const int operation = static_cast<int>(rng() % 8);
        const Affine f{static_cast<ll>(rng() % 2), static_cast<ll>(rng() % 8)};
        if (operation == 0 && n != 0) {
            const int k = static_cast<int>(rng() % n);
            a[k] = static_cast<ll>(rng() % 20);
            tree.set(k, {a[k], 1});
        } else if (operation == 1 && n != 0) {
            const int k = static_cast<int>(rng() % n);
            tree.apply(k, f);
            a[k] = f.a * a[k] + f.b;
        } else if (operation <= 3) {
            tree.apply(l, r, f);
            for (int k = l; k < r; ++k) a[k] = f.a * a[k] + f.b;
        } else if (operation == 4) {
            tree.all_apply(f);
            for (ll &x : a) x = f.a * x + f.b;
        } else if (operation == 5) {
            tree.reserve_nodes(tree.node_count() + 1 + rng() % 20);
        } else if (operation == 6 && step % 43 == 0) {
            tree.clear();
            std::fill(a.begin(), a.end(), initial);
        } else {
            const LazySumTree &read = tree;
            const auto count = read.node_count();
            const ll sum = std::accumulate(a.begin() + l, a.begin() + r, 0LL);
            assert_aggregate(read.prod(l, r), sum, r - l);
            assert(read.node_count() == count);
        }
        if (step % 97 == 0) verify_lazy(tree, a, false);
    }
    // Leave an action at the root when copying, so tag storage must be independent too.
    tree.all_apply({1, 4});
    for (ll &x : a) x += 4;
    auto copy = tree;
    copy.all_apply({0, 11});
    verify_lazy(tree, a, true);
    verify_lazy(copy, std::vector<ll>(n, 11), true);
    LazySumTree assigned;
    assigned = tree;
    auto moved = std::move(assigned);
    verify_lazy(moved, a, false);
    LazySumTree move_assigned;
    move_assigned = std::move(moved);
    verify_lazy(move_assigned, a, false);
    copy.clear();
    verify_lazy(copy, std::vector<ll>(n, initial), true);
    verify_lazy(move_assigned, a, false);
}

std::string string_op(std::string a, std::string b) { return a + b; }
std::string string_e() { return {}; }
std::string string_pow(std::string a, ll n) {
    std::string result;
    while (n-- > 0) result += a;
    return result;
}

struct AssignChar { bool active; char value; };
std::string string_map(AssignChar f, std::string s) {
    return f.active ? std::string(s.size(), f.value) : s;
}
AssignChar string_compose(AssignChar f, AssignChar g) { return f.active ? f : g; }
AssignChar string_id() { return {false, 'a'}; }

using StringTree = titan23::DynamicSegmentTree<ll, std::string, string_op, string_e>;
using InitStringTree = titan23::DynamicSegmentTreeInit<ll, std::string, string_op, string_e, string_pow>;
using LazyStringTree = titan23::DynamicLazySegmentTree<
    ll, std::string, string_op, string_e, AssignChar,
    string_map, string_compose, string_id, string_pow>;

template<class Tree>
void verify_strings(const Tree &tree, const std::vector<std::string> &a) {
    const int n = static_cast<int>(a.size());
    const auto count = tree.node_count();
    assert(tree.all_prod() == std::accumulate(a.begin(), a.end(), std::string{}));
    for (int i = 0; i < n; ++i) assert(tree.get(i) == a[i]);
    for (int l = 0; l <= n; ++l) {
        std::string expected;
        for (int r = l; r <= n; ++r) {
            assert(tree.prod(l, r) == expected);
            if (r < n) expected += a[r];
        }
        for (const std::string forbidden : {"ab", "ba", "ac", "aaa"}) {
            const auto pred = [&forbidden](const std::string &s) {
                return s.find(forbidden) == std::string::npos;
            };
            assert(tree.max_right(l, pred) == scan_right(a, l, std::string{}, string_op, pred));
            assert(tree.min_left(l, pred) == scan_left(a, l, std::string{}, string_op, pred));
        }
    }
    assert(tree.node_count() == count);
}

void noncommutative_test() {
    std::mt19937_64 rng(109471);
    for (int n : {0, 1, 3, 9, 17}) {
        StringTree plain(n);
        InitStringTree initial(n, "a");
        LazyStringTree lazy(n, "a");
        std::vector<std::string> a(n), b(n, "a"), c(n, "a");
        verify_strings(plain, a);
        verify_strings(initial, b);
        verify_strings(lazy, c);
        for (int step = 0; step < 240; ++step) {
            if (n != 0) {
                const int k = static_cast<int>(rng() % n);
                const std::string value(1, static_cast<char>('a' + rng() % 3));
                plain.set(k, value);
                initial.set(k, value);
                lazy.set(k, value);
                a[k] = b[k] = c[k] = value;
            }
            int l = static_cast<int>(rng() % (n + 1));
            int r = static_cast<int>(rng() % (n + 1));
            if (l > r) std::swap(l, r);
            const char value = static_cast<char>('a' + rng() % 3);
            lazy.apply(l, r, {true, value});
            for (int k = l; k < r; ++k) c[k] = std::string(1, value);
            if (step % 23 == 0) {
                verify_strings(plain, a);
                verify_strings(initial, b);
                verify_strings(lazy, c);
            }
        }
        verify_strings(plain, a);
        verify_strings(initial, b);
        verify_strings(lazy, c);
    }
}

int min_op(int a, int b) { return std::min(a, b); }
int min_e() { return std::numeric_limits<int>::max(); }
int min_pow(int a, ll n) { return n == 0 ? min_e() : a; }
struct AssignInt { bool active; int value; };
// Assignment intentionally does not preserve e(): empty intervals must return e()
// before applying any ancestor tag.
int min_map(AssignInt f, int value) { return f.active ? f.value : value; }
AssignInt min_compose(AssignInt f, AssignInt g) { return f.active ? f : g; }
AssignInt min_id() { return {false, 0}; }
using InitMinTree = titan23::DynamicSegmentTreeInit<ll, int, min_op, min_e, min_pow>;
using LazyMinTree = titan23::DynamicLazySegmentTree<
    ll, int, min_op, min_e, AssignInt, min_map, min_compose, min_id, min_pow>;

void large_domain_test() {
    const ll n = std::numeric_limits<ll>::max();
    const ll mid = n / 2;
    SumTree sum(n);
    sum.set(0, 2);
    sum.set(mid, 3);
    sum.set(n - 1, 5);
    const SumTree &read = sum;
    auto count = read.node_count();
    assert(read.size() == n);
    assert(read.all_prod() == 10);
    assert(read.prod(n - 5, n) == 5);
    assert(read.get(n - 1) == 5);
    assert(read.max_right(0, [](ll x) { return x <= 2; }) == mid);
    assert(read.min_left(n, [](ll x) { return x <= 5; }) == mid + 1);
    assert(read.max_right(n, [](ll x) { return x == 0; }) == n);
    assert(read.min_left(0, [](ll x) { return x == 0; }) == 0);
    assert(read.node_count() == count);

    InitMinTree initial(n, 10);
    initial.set(mid, 6);
    initial.set(n - 1, 4);
    const InitMinTree &read_initial = initial;
    count = read_initial.node_count();
    assert(read_initial.size() == n);
    assert(read_initial.prod(0, mid) == 10);
    assert(read_initial.prod(mid, n - 1) == 6);
    assert(read_initial.all_prod() == 4);
    assert(read_initial.max_right(0, [](int x) { return x >= 7; }) == mid);
    assert(read_initial.min_left(n - 1, [](int x) { return x >= 7; }) == mid + 1);
    assert(read_initial.node_count() == count);

    LazyMinTree lazy(n, 10);
    lazy.all_apply({true, 9});
    lazy.apply(mid, n, {true, 7});
    lazy.set(n - 1, 3);
    const LazyMinTree &read_lazy = lazy;
    count = read_lazy.node_count();
    assert(read_lazy.size() == n);
    assert(read_lazy.all_prod() == 3);
    assert(read_lazy.prod(0, mid) == 9);
    assert(read_lazy.prod(mid, n - 1) == 7);
    assert(read_lazy.get(mid) == 7);
    for (ll k : {0LL, 1LL, mid, n - 1, n}) {
        assert(read_lazy.prod(k, k) == min_e());
    }
    assert(read_lazy.max_right(0, [](int x) { return x >= 8; }) == mid);
    assert(read_lazy.min_left(n - 1, [](int x) { return x >= 8; }) == n - 1);
    assert(read_lazy.min_left(n - 1, [](int x) { return x >= 7; }) == 0);
    assert(read_lazy.node_count() == count);
    lazy.clear();
    assert(lazy.size() == n);
    assert(lazy.all_prod() == 10);
    assert(lazy.get(n - 1) == 10);

    // The one-argument lazy constructor uses e() as its element initial value.
    LazyMinTree identity_initial(n);
    identity_initial.apply(n - 1, {true, 2});
    assert(identity_initial.prod(0, n - 1) == min_e());
    assert(identity_initial.get(n - 1) == 2);
    assert(identity_initial.all_prod() == 2);
}

bool sum_at_most_seven(ll x) { return x <= 7; }
bool aggregate_at_most_seven(SumLength x) { return x.sum <= 7; }
SumLength add_map(ll f, SumLength x) { return {x.sum + f * x.len, x.len}; }

template<class Index>
ll indexed_sum_pow(ll value, Index length) { return value * static_cast<ll>(length); }
template<class Index>
SumLength indexed_aggregate_pow(SumLength value, Index length) {
    return aggregate_pow(value, static_cast<ll>(length));
}

struct NoncopyableSumPredicate {
    NoncopyableSumPredicate() = default;
    NoncopyableSumPredicate(const NoncopyableSumPredicate&) = delete;
    bool operator()(ll value) const { return value <= 7; }
};
struct NoncopyableAggregatePredicate {
    NoncopyableAggregatePredicate() = default;
    NoncopyableAggregatePredicate(const NoncopyableAggregatePredicate&) = delete;
    bool operator()(SumLength value) const { return value.sum <= 7; }
};

template<class Tree, class CheckValue>
void verify_moved_from(Tree &tree, CheckValue check_value) {
    assert(tree.size() == 0);
    assert(tree.node_count() == 0);
    check_value(tree.all_prod());
    check_value(tree.prod(0, 0));
    const auto accept = [](const auto&) { return true; };
    assert(tree.max_right(0, accept) == 0);
    assert(tree.min_left(0, accept) == 0);
    tree.clear();
    check_value(tree.all_prod());
}

template<class Index>
void api_and_index_test() {
    using Plain = titan23::DynamicSegmentTree<Index, ll, sum_op, sum_e>;
    using Initial = titan23::DynamicSegmentTreeInit<Index, ll, sum_op, sum_e, indexed_sum_pow<Index>>;
    using Lazy = titan23::DynamicLazySegmentTree<
        Index, SumLength, aggregate_op, aggregate_e, ll,
        add_map, sum_op, sum_e, indexed_aggregate_pow<Index>>;
    Plain plain(7);
    Initial initial(7, 2);
    Lazy lazy(7, {2, 1});
    auto count = lazy.node_count();
    lazy.apply(1, 6, 0);
    lazy.apply(4, 0);
    lazy.all_apply(0);
    assert(lazy.node_count() == count);
    assert_aggregate(lazy.all_prod(), 14, 7);
    lazy.apply(1, 6, 3);
    for (Index k = 0; k < 7; ++k) {
        const ll value = (k == 0 || k == 6) ? 2 : 5;
        plain.set(k, value);
        initial.set(k, value);
    }
    NoncopyableSumPredicate scalar_pred;
    NoncopyableAggregatePredicate aggregate_pred;
    const Plain &read_plain = plain;
    const Initial &read_initial = initial;
    const Lazy &read_lazy = lazy;
    assert(read_plain.template max_right<sum_at_most_seven>(0) == 2);
    assert(read_plain.template min_left<sum_at_most_seven>(7) == 5);
    assert(read_initial.template max_right<sum_at_most_seven>(0) == 2);
    assert(read_initial.template min_left<sum_at_most_seven>(7) == 5);
    assert(read_lazy.template max_right<aggregate_at_most_seven>(0) == 2);
    assert(read_lazy.template min_left<aggregate_at_most_seven>(7) == 5);
    assert(read_plain.max_right(0, scalar_pred) == 2);
    assert(read_plain.min_left(7, scalar_pred) == 5);
    assert(read_initial.max_right(0, scalar_pred) == 2);
    assert(read_initial.min_left(7, scalar_pred) == 5);
    assert(read_lazy.max_right(0, aggregate_pred) == 2);
    assert(read_lazy.min_left(7, aggregate_pred) == 5);
    assert_aggregate(read_lazy.all_prod(), 29, 7);
    count = lazy.node_count();
    lazy.apply(0, 7, 0);
    lazy.all_apply(0);
    assert(lazy.node_count() == count);

    const auto zero_scalar = [](ll x) { assert(x == 0); };
    const auto zero_aggregate = [](SumLength x) { assert_aggregate(x, 0, 0); };
    auto moved_plain = std::move(plain);
    auto moved_initial = std::move(initial);
    auto moved_lazy = std::move(lazy);
    verify_moved_from(plain, zero_scalar);
    verify_moved_from(initial, zero_scalar);
    verify_moved_from(lazy, zero_aggregate);
    plain = std::move(moved_plain);
    initial = std::move(moved_initial);
    lazy = std::move(moved_lazy);
    verify_moved_from(moved_plain, zero_scalar);
    verify_moved_from(moved_initial, zero_scalar);
    verify_moved_from(moved_lazy, zero_aggregate);
    assert(plain.all_prod() == 29);
    assert(initial.all_prod() == 29);
    assert_aggregate(lazy.all_prod(), 29, 7);
}

void print_test() {
    LazyMinTree tree(3, 4);
    tree.set(1, 2);
    std::ostringstream output;
    auto *saved = std::cout.rdbuf(output.rdbuf());
    tree.print();
    std::cout.rdbuf(saved);
    assert(output.str() == "4, 2, 4, \n");
}

ll sparse_sum(ll value) { return value; }
ll sparse_sum(SumLength value) { return value.sum; }
void check_sparse_result(ll actual, ll expected, ll) { assert(actual == expected); }
void check_sparse_result(SumLength actual, ll expected, ll length) {
    assert_aggregate(actual, expected, length);
}

ll sparse_prod(const std::vector<ll> &edges, const std::vector<ll> &values, ll l, ll r) {
    ll result = 0;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const ll length = std::max(0LL, std::min(r, edges[i + 1]) - std::max(l, edges[i]));
        result += values[i] * length;
    }
    return result;
}

ll sparse_right(const std::vector<ll> &edges, const std::vector<ll> &values, ll l, ll limit) {
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (edges[i + 1] <= l) continue;
        const ll start = std::max(l, edges[i]);
        const ll length = edges[i + 1] - start;
        const ll take = values[i] == 0 ? length : std::min(length, limit / values[i]);
        if (take != length) return start + take;
        limit -= take * values[i];
    }
    return edges.back();
}

ll sparse_left(const std::vector<ll> &edges, const std::vector<ll> &values, ll r, ll limit) {
    for (int i = static_cast<int>(values.size()) - 1; i >= 0; --i) {
        if (r <= edges[i]) continue;
        const ll end = std::min(r, edges[i + 1]);
        const ll length = end - edges[i];
        const ll take = values[i] == 0 ? length : std::min(length, limit / values[i]);
        if (take != length) return end - take;
        limit -= take * values[i];
    }
    return 0;
}

template<class Tree>
void verify_sparse(const Tree &tree, const std::vector<ll> &edges,
                   const std::vector<ll> &values, std::mt19937_64 &rng) {
    const ll n = edges.back();
    const auto count = tree.node_count();
    assert(tree.size() == n);
    check_sparse_result(tree.all_prod(), sparse_prod(edges, values, 0, n), n);
    for (int sample = 0; sample < 24; ++sample) {
        ll l = static_cast<ll>(rng() % (n + 1));
        ll r = static_cast<ll>(rng() % (n + 1));
        if (sample < 8) l = edges[rng() % edges.size()];
        if (l > r) std::swap(l, r);
        check_sparse_result(tree.prod(l, r), sparse_prod(edges, values, l, r), r - l);
        check_sparse_result(tree.prod(l, l), 0, 0);
        if (l < n) check_sparse_result(tree.get(l), sparse_prod(edges, values, l, l + 1), 1);
        const ll limit = sample % 4 == 0 ? 0 : static_cast<ll>(rng() % (n * 20));
        const auto pred = [limit](const auto &value) { return sparse_sum(value) <= limit; };
        assert(tree.max_right(l, pred) == sparse_right(edges, values, l, limit));
        assert(tree.min_left(r, pred) == sparse_left(edges, values, r, limit));
    }
    assert(tree.node_count() == count);
}

void structural_lazy_test() {
    const ll n = 1000000000039LL;
    std::vector<ll> edges{0, 1, 2, 7, 19, 101, n / 3, n / 2, n / 2 + 1,
                          n - 1025, n - 31, n - 9, n - 2, n - 1, n};
    std::mt19937_64 rng(5962331);
    for (int i = 0; i < 64; ++i) edges.push_back(static_cast<ll>(rng() % n));
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    const int blocks = static_cast<int>(edges.size()) - 1;
    const int middle = static_cast<int>(std::lower_bound(edges.begin(), edges.end(), n / 2) - edges.begin());
    for (int order = 0; order < 3; ++order) {
        LazySumTree tree(n, {3, 1});
        std::vector<ll> values(blocks, 3);
        assert(tree.node_count() <= 1);
        verify_sparse(tree, edges, values, rng);
        std::vector<int> endpoints;
        for (int i = 1; i < blocks; ++i) endpoints.push_back(i);
        if (order == 1) std::reverse(endpoints.begin(), endpoints.end());
        if (order == 2) {
            endpoints.clear();
            for (int l = 1, r = blocks - 1; l <= r; ++l, --r) {
                endpoints.push_back(l);
                if (l != r) endpoints.push_back(r);
            }
        }
        // Prefix updates introduce arbitrary run boundaries in three insertion orders.
        std::size_t boundary_count = 0;
        for (int endpoint : endpoints) {
            const ll value = endpoint % 11;
            tree.apply(0, edges[endpoint], {0, value});
            std::fill(values.begin(), values.begin() + endpoint, value);
            ++boundary_count;
            assert(tree.node_count() <= 2 * (1 + boundary_count) - 1);
            if (endpoint % 13 == 0) verify_sparse(tree, edges, values, rng);
        }
        tree.set(n / 2, {6, 1});
        values[middle] = 6;
        const auto warmed_count = tree.node_count();
        assert(warmed_count <= 2 * (1 + boundary_count) - 1);
        for (int step = 0; step < 240; ++step) {
            int l = static_cast<int>(rng() % edges.size());
            int r = static_cast<int>(rng() % edges.size());
            if (l > r) std::swap(l, r);
            const Affine f{static_cast<ll>(rng() % 2), static_cast<ll>(rng() % 5)};
            if (step % 7 == 0) {
                tree.all_apply(f);
                for (ll &value : values) value = f.a * value + f.b;
            } else if (step % 11 == 0) {
                tree.set(n / 2, {f.b, 1});
                values[middle] = f.b;
            } else {
                tree.apply(edges[l], edges[r], f);
                for (int i = l; i < r; ++i) values[i] = f.a * values[i] + f.b;
            }
            assert(tree.node_count() <= warmed_count);
            if (step % 17 == 0) verify_sparse(tree, edges, values, rng);
        }
        verify_sparse(tree, edges, values, rng);
    }
}

void structural_point_test() {
    const ll n = 1000000000039LL;
    std::vector<ll> positions{0, 1, 7, n / 2 - 1, n / 2, n / 2 + 1, n - 2, n - 1};
    std::mt19937_64 rng(909587);
    for (int i = 0; i < 64; ++i) positions.push_back(static_cast<ll>(rng() % n));
    std::sort(positions.begin(), positions.end());
    positions.erase(std::unique(positions.begin(), positions.end()), positions.end());
    std::vector<ll> edges{0, n};
    for (ll k : positions) { edges.push_back(k); edges.push_back(k + 1); }
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    for (int order = 0; order < 3; ++order) {
        SumTree plain(n);
        InitSumTree initial(n, 3);
        std::vector<ll> zero_values(edges.size() - 1, 0);
        std::vector<ll> initial_values(edges.size() - 1, 3);
        auto sequence = positions;
        if (order == 1) std::reverse(sequence.begin(), sequence.end());
        if (order == 2) std::shuffle(sequence.begin(), sequence.end(), rng);
        for (std::size_t i = 0; i < sequence.size(); ++i) {
            const ll k = sequence[i];
            const ll value = static_cast<ll>(rng() % 10);
            plain.set(k, value);
            initial.set(k, value);
            const auto block = std::lower_bound(edges.begin(), edges.end(), k) - edges.begin();
            zero_values[block] = initial_values[block] = value;
            // A compressed point tree needs O(number of distinct modified positions) nodes.
            assert(plain.node_count() <= 2 * (i + 1));
            assert(initial.node_count() <= 2 * (i + 1));
            if (i % 11 == 0) {
                verify_sparse(plain, edges, zero_values, rng);
                verify_sparse(initial, edges, initial_values, rng);
            }
        }
        const auto plain_count = plain.node_count(), initial_count = initial.node_count();
        for (int step = 0; step < 100; ++step) {
            const ll k = positions[rng() % positions.size()];
            const ll value = static_cast<ll>(rng() % 10);
            plain.set(k, value);
            initial.set(k, value);
            const auto block = std::lower_bound(edges.begin(), edges.end(), k) - edges.begin();
            zero_values[block] = initial_values[block] = value;
            assert(plain.node_count() <= plain_count);
            assert(initial.node_count() <= initial_count);
        }
        verify_sparse(plain, edges, zero_values, rng);
        verify_sparse(initial, edges, initial_values, rng);
    }
    InitStringTree strings(25, "a");
    std::vector<std::string> expected(25, "a");
    int updates = 0;
    for (int k : {7, 19, 2, 23, 11}) {
        const std::string value(1, static_cast<char>('b' + updates));
        strings.set(k, value);
        expected[k] = value;
        ++updates;
        assert(strings.node_count() <= static_cast<std::size_t>(2 * updates));
        verify_strings(strings, expected);
    }
}

void lazy_pending_boundaries_test() {
    const ll n = (1LL << 40) + 29;
    std::vector<ll> edges{0, 16, 21, 32, n - 256, n - 200, n - 128, n - 100,
                          n - 75, n - 50, n - 10, n - 5, n - 2, n - 1, n};
    std::sort(edges.begin(), edges.end());
    std::vector<ll> values(edges.size() - 1, 3), seen{0, n};
    LazySumTree tree(n, {3, 1});
    std::mt19937_64 rng(491471);
    const auto check = [&]() {
        assert(tree.node_count() == 2 * (seen.size() - 1) - 1);
        for (std::size_t i = 0; i < values.size(); ++i) {
            assert_aggregate(tree.get(edges[i]), values[i], 1);
            const ll length = edges[i + 1] - edges[i];
            assert_aggregate(tree.prod(edges[i], edges[i + 1]), values[i] * length, length);
        }
        verify_sparse(tree, edges, values, rng);
    };
    const auto apply = [&](ll l, ll r, Affine f) {
        tree.apply(l, r, f);
        for (ll endpoint : {l, r}) {
            if (std::find(seen.begin(), seen.end(), endpoint) == seen.end()) seen.push_back(endpoint);
        }
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (l <= edges[i] && edges[i + 1] <= r) values[i] = f.a * values[i] + f.b;
        }
        check();
    };
    const auto apply_all = [&](Affine f) {
        tree.all_apply(f);
        for (ll &value : values) value = f.a * value + f.b;
        check();
    };
    apply(16, 32, {1, 3});
    apply(n - 256, n - 128, {0, 7});
    // Both new endpoints cut existing runs beneath an inherited composed tag.
    apply_all({0, 4});
    apply_all({1, 5});
    apply(21, n - 200, {1, 2});
    apply_all({0, 2});
    apply_all({1, 3});
    apply(n - 100, n - 50, {0, 8});
    // The second endpoint crosses bit 40 and must move above the previous root.
    apply(n - 75, n - 10, {1, 4});
    apply_all({0, 6});
    apply(n - 5, n - 2, {1, 3});
    tree.set(n - 1, {2, 1});
    values.back() = 2;
    seen.push_back(n - 1);
    check();
}

void lazy_before_right_minimum_test() {
    LazySumTree tree(20, {3, 1});
    tree.apply(10, 12, {1, 1});
    tree.apply(14, 20, {1, 2});
    assert(tree.node_count() == 7);
    tree.all_apply({0, 4});
    tree.all_apply({1, 3});
    // The run starting at 8 comes from the left sibling, but shares the
    // right subtree's Patricia prefix and precedes its previous minimum 10.
    tree.apply(8, 13, {1, 5});
    assert(tree.node_count() == 11);
    std::vector<ll> expected(20, 7);
    std::fill(expected.begin() + 8, expected.begin() + 13, 12);
    verify_lazy(tree, expected, true);
    assert(tree.node_count() == 11);
}

int main() {
    verify_sum(SumTree{}, {}, true);
    verify_sum(InitSumTree{}, {}, true);
    verify_lazy(LazySumTree{}, {}, true);
    for (int n : {0, 1, 2, 3, 7, 8, 9, 31, 32, 33, 65}) {
        for (uint64_t seed = 0; seed < 3; ++seed) {
            random_sum(SumTree(n), n, 0, seed);
            random_sum(InitSumTree(n, 0), n, 0, seed);
            random_sum(InitSumTree(n, 3), n, 3, seed);
            random_lazy(n, 0, seed);
            random_lazy(n, 3, seed);
        }
    }
    noncommutative_test();
    large_domain_test();
    api_and_index_test<int>();
    api_and_index_test<unsigned>();
    api_and_index_test<unsigned long long>();
    print_test();
    structural_lazy_test();
    structural_point_test();
    lazy_pending_boundaries_test();
    lazy_before_right_minimum_test();
    std::cout << "dynamic_segment_tree: OK\n";
}
