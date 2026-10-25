#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <utility>
#include <vector>

#ifndef PERSISTENT_LAZY_WBTREE_HEADER
#define PERSISTENT_LAZY_WBTREE_HEADER "titan_cpplib/ds/persistent_lazy_wbtree.cpp"
#endif
#include PERSISTENT_LAZY_WBTREE_HEADER

using namespace std;
using ll = long long;
constexpr ll MOD = 998244353;

struct Sum {
    ll value;
    int length;
    bool operator==(const Sum &other) const {
        return value == other.value && length == other.length;
    }
};

struct Affine {
    ll a, b;
    bool operator!=(const Affine &other) const {
        return a != other.a || b != other.b;
    }
};

Sum sum_op(Sum x, Sum y) {
    return {(x.value + y.value) % MOD, x.length + y.length};
}
Sum sum_mapping(Affine f, Sum x) {
    return {(f.a * x.value + f.b * x.length) % MOD, x.length};
}
Affine affine_composition(Affine f, Affine g) {
    return {f.a * g.a % MOD, (f.a * g.b + f.b) % MOD};
}
Sum sum_e() { return {0, 0}; }
Affine affine_id() { return {1, 0}; }

using Tree = titan23::PersistentLazyWBTree<int, Sum, Affine, sum_op,
    sum_mapping, affine_composition, sum_e, affine_id>;

vector<Sum> elements(const vector<ll> &values) {
    vector<Sum> result;
    for (ll value : values) result.push_back({value, 1});
    return result;
}

void apply_oracle(vector<ll> &values, int l, int r, Affine f) {
    for (int i = l; i < r; ++i) values[i] = (f.a * values[i] + f.b) % MOD;
}

Sum prod_oracle(const vector<ll> &values, int l, int r) {
    ll result = 0;
    for (int i = l; i < r; ++i) result = (result + values[i]) % MOD;
    return {result, r - l};
}

// Queries must work on const versions and leave the shared arena untouched.
void verify(const Tree &tree, const vector<ll> &values, bool all_ranges = false) {
    const size_t allocated = Tree::ma.ptr;
    const int n = static_cast<int>(values.size());
    assert(tree.len() == n);
    assert(tree.tovector() == elements(values));
    assert(tree.prod(0, n) == prod_oracle(values, 0, n));
    for (int i = 0; i <= n; ++i) {
        assert(tree.prod(i, i) == sum_e());
        if (i < n) {
            assert(tree.get(i) == (Sum{values[i], 1}));
            assert(tree.prod(i, i + 1) == (Sum{values[i], 1}));
        }
    }
    if (all_ranges) {
        for (int l = 0; l <= n; ++l) {
            for (int r = l; r <= n; ++r) {
                assert(tree.prod(l, r) == prod_oracle(values, l, r));
            }
        }
    }
    tree.check();
    assert(Tree::ma.ptr == allocated);
}

void boundary_tests() {
    Tree::ma.reset();
    Tree empty;
    verify(empty, {}, true);
    verify(empty.copy(), {}, true);
    verify(empty.apply(0, 0, {2, 3}), {}, true);
    verify(empty.reverse(0, 0), {}, true);
    verify(empty.split_left(0), {}, true);
    verify(empty.split_right(0), {}, true);
    verify(empty.split_range(0, 0), {}, true);
    verify(empty.merge(empty), {}, true);
    auto [empty_left, empty_right] = empty.split(0);
    verify(empty_left, {});
    verify(empty_right, {});
    auto from_empty = empty.apply(0, 0, {2, 3}).reverse(0, 0).insert(0, {7, 1});
    verify(from_empty, {7}, true);
    auto [removed, value] = from_empty.pop(0);
    assert(value == (Sum{7, 1}));
    verify(removed, {}, true);

    for (int n : {1, 2, 3, 7, 16, 31}) {
        vector<ll> original(n);
        iota(original.begin(), original.end(), 1);
        Tree base(elements(original));
        auto first = base.apply(0, n, {2, 3});
        auto second = first.apply(0, n, {5, 7});
        auto expected = original;
        apply_oracle(expected, 0, n, {2, 3});
        apply_oracle(expected, 0, n, {5, 7});
        auto reversed = second.reverse(0, n);
        auto reversed_values = expected;
        reverse(reversed_values.begin(), reversed_values.end());
        // Lazy actions and reversals remain pending during every read.
        verify(reversed, reversed_values, true);
        verify(second, expected, true);
        verify(first, [&] {
            auto x = original;
            apply_oracle(x, 0, n, {2, 3});
            return x;
        }(), true);
        verify(base, original, true);

        for (int k = 0; k <= n; ++k) {
            auto [left, right] = reversed.split(k);
            vector<ll> left_values(reversed_values.begin(), reversed_values.begin() + k);
            vector<ll> right_values(reversed_values.begin() + k, reversed_values.end());
            verify(left, left_values, true);
            verify(right, right_values, true);
            verify(left.merge(right), reversed_values, true);
            verify(reversed.split_left(k), left_values, true);
            verify(reversed.split_right(k), right_values, true);
            verify(reversed, reversed_values, true);
        }
        for (int k = 0; k < n; ++k) {
            auto replacement = reversed_values;
            replacement[k] = 123;
            verify(reversed.set(k, {123, 1}), replacement, true);
            auto [rest, erased] = reversed.pop(k);
            assert(erased == (Sum{reversed_values[k], 1}));
            replacement = reversed_values;
            replacement.erase(replacement.begin() + k);
            verify(rest, replacement, true);
            verify(reversed, reversed_values, true);
        }
        auto doubled = reversed.merge(reversed);
        auto twice = reversed_values;
        twice.insert(twice.end(), reversed_values.begin(), reversed_values.end());
        verify(doubled, twice);
        auto changed = doubled.apply(n / 2, n + n / 2, {0, 13});
        auto changed_values = twice;
        apply_oracle(changed_values, n / 2, n + n / 2, {0, 13});
        verify(changed, changed_values);
        verify(doubled, twice);
        verify(reversed, reversed_values);
        verify(base, original);
    }
}

struct Version {
    Tree tree;
    vector<ll> values;
};

void random_test(uint64_t seed, int initial_size) {
    Tree::ma.reset();
    mt19937_64 rng(seed);
    vector<ll> initial(initial_size);
    for (ll &value : initial) value = rng() % MOD;
    vector<Version> versions = {{Tree(elements(initial)), initial}};
    for (int step = 0; step < 1600; ++step) {
        const size_t source = rng() % versions.size();
        // Copy these handles before appending versions, which may reallocate.
        auto tree = versions[source].tree;
        auto expected = versions[source].values;
        const int n = static_cast<int>(expected.size());
        int l = rng() % (n + 1), r = rng() % (n + 1);
        if (l > r) swap(l, r);
        const int operation = rng() % 15;
        if (operation <= 2) {
            if (operation == 0) l = 0, r = n;
            Affine f{static_cast<ll>(rng() % 7), static_cast<ll>(rng() % 101)};
            tree = tree.apply(l, r, f);
            apply_oracle(expected, l, r, f);
        } else if (operation == 3) {
            tree = tree.reverse(l, r);
            reverse(expected.begin() + l, expected.begin() + r);
        } else if (operation == 4 && n < 128) {
            const ll value = rng() % MOD;
            tree = tree.insert(l, {value, 1});
            expected.insert(expected.begin() + l, value);
        } else if (operation == 5 && n > 0) {
            const int k = rng() % n;
            auto result = tree.pop(k);
            assert(result.second == (Sum{expected[k], 1}));
            tree = result.first;
            expected.erase(expected.begin() + k);
        } else if (operation == 6 && n > 0) {
            const int k = (step % 2) ? rng() % n : n / 2;
            const ll value = rng() % MOD;
            tree = tree.set(k, {value, 1});
            expected[k] = value;
        } else if (operation == 7) {
            auto [left, right] = tree.split(l);
            vector<ll> a(expected.begin(), expected.begin() + l);
            vector<ll> b(expected.begin() + l, expected.end());
            verify(left, a);
            verify(right, b);
            versions.push_back({left, a});
            versions.push_back({right, b});
            tree = left.merge(right);
        } else if (operation == 8) {
            const auto &other = versions[rng() % versions.size()];
            if (n + other.values.size() <= 128) {
                tree = tree.merge(other.tree);
                expected.insert(expected.end(), other.values.begin(), other.values.end());
            }
        } else if (operation == 9 && n <= 64) {
            tree = tree.merge(tree);
            auto duplicate = expected;
            expected.insert(expected.end(), duplicate.begin(), duplicate.end());
        } else if (operation == 10) {
            tree = tree.split_range(l, r);
            expected = vector<ll>(expected.begin() + l, expected.begin() + r);
        } else if (operation == 11) {
            tree = tree.split_left(r);
            expected.resize(r);
        } else if (operation == 12) {
            tree = tree.split_right(l);
            expected.erase(expected.begin(), expected.begin() + l);
        } else if (operation == 13) {
            const Tree &immutable = tree;
            tree = immutable.copy();
        } else {
            assert(tree.prod(l, r) == prod_oracle(expected, l, r));
        }
        verify(tree, expected, step % 101 == 0 && expected.size() <= 32);
        verify(versions[source].tree, versions[source].values);
        const auto &historical = versions[rng() % versions.size()];
        verify(historical.tree, historical.values);
        versions.push_back({tree, expected});
    }
    for (const auto &version : versions) verify(version.tree, version.values);
}

void allocator_and_rebuild_tests() {
    Tree::ma.reset();
    Tree::ma.reserve(2048);
    const size_t capacity = Tree::ma.cap;
    const size_t position = Tree::ma.ptr;
    Tree::ma.reserve(2048);
    Tree::ma.reserve(16);
    assert(Tree::ma.cap == capacity);
    assert(Tree::ma.ptr == position);
    assert(Tree::ma.tree.capacity() >= 2048);
    assert(Tree::ma.data.capacity() >= 2048);

    vector<ll> expected(127);
    iota(expected.begin(), expected.end(), 1);
    Tree tree(elements(expected));
    for (int round = 0; round < 8; ++round) {
        tree = tree.apply(0, tree.len(), {3, 5});
        apply_oracle(expected, 0, expected.size(), {3, 5});
        tree = tree.reverse(0, tree.len());
        reverse(expected.begin(), expected.end());
        tree = tree.apply(11, 103, {2, 7});
        apply_oracle(expected, 11, 103, {2, 7});
        // No other version is live when rebuilding the shared arena.
        Tree::rebuild(tree);
        verify(tree, expected);
#ifdef PLWBT_LEAF_LAYOUT
        assert(Tree::ma.ptr == (expected.empty() ? 1 : 2 * expected.size()));
#else
        assert(Tree::ma.ptr == expected.size() + 1);
#endif
    }
    tree = Tree();
    Tree::rebuild(tree);
    verify(tree, {}, true);
    assert(Tree::ma.ptr == 1);
    tree = tree.insert(0, {19, 1});
    verify(tree, {19}, true);
}

string string_op(string x, string y) { return x + y; }
string string_mapping(int shift, string value) {
    for (char &c : value) c = 'a' + (c - 'a' + shift) % 26;
    return value;
}
int shift_composition(int f, int g) { return (f + g) % 26; }
string string_e() { return {}; }
int shift_id() { return 0; }
using StringTree = titan23::PersistentLazyWBTree<int, string, int, string_op,
    string_mapping, shift_composition, string_e, shift_id>;

void verify_string(const StringTree &tree, const string &expected) {
    const size_t allocated = StringTree::ma.ptr;
    const int n = static_cast<int>(expected.size());
    assert(tree.len() == n);
    const auto values = tree.tovector();
    assert(values.size() == expected.size());
    for (int i = 0; i < n; ++i) {
        assert(values[i] == expected.substr(i, 1));
        assert(tree.get(i) == values[i]);
    }
    for (int l = 0; l <= n; ++l) {
        for (int r = l; r <= n; ++r) {
            assert(tree.prod(l, r) == expected.substr(l, r - l));
        }
    }
    tree.check();
    assert(StringTree::ma.ptr == allocated);
}

// Concatenation checks operand order. Reversal requires an invariant aggregate
// in this implementation, so this noncommutative specialization never reverses.
void noncommutative_test() {
    StringTree::ma.reset();
    vector<string> initial;
    for (char c : string("abcdefghijklmnopq")) initial.emplace_back(1, c);
    vector<pair<StringTree, string>> versions = {{StringTree(initial), "abcdefghijklmnopq"}};
    mt19937 rng(0x5317);
    for (int step = 0; step < 320; ++step) {
        const int source = rng() % versions.size();
        auto tree = versions[source].first;
        string expected = versions[source].second;
        const int n = static_cast<int>(expected.size());
        int l = rng() % (n + 1), r = rng() % (n + 1);
        if (l > r) swap(l, r);
        switch (rng() % 7) {
        case 0: {
            const int shift = rng() % 26;
            tree = tree.apply(l, r, shift);
            expected.replace(l, r - l, string_mapping(shift, expected.substr(l, r - l)));
            break;
        }
        case 1: {
            auto [left, right] = tree.split(l);
            tree = right.merge(left);
            expected = expected.substr(l) + expected.substr(0, l);
            break;
        }
        case 2:
            if (n < 48) {
                string value(1, 'a' + rng() % 26);
                tree = tree.insert(l, value);
                expected.insert(l, value);
            }
            break;
        case 3:
            if (n > 0) {
                const int k = rng() % n;
                auto [rest, erased] = tree.pop(k);
                assert(erased == expected.substr(k, 1));
                tree = rest;
                expected.erase(k, 1);
            }
            break;
        case 4:
            if (n > 0) {
                const int k = rng() % n;
                const string value(1, 'z');
                tree = tree.set(k, value);
                expected[k] = 'z';
            }
            break;
        case 5:
            tree = tree.split_range(l, r);
            expected = expected.substr(l, r - l);
            break;
        case 6:
            tree = tree.copy();
            break;
        }
        verify_string(tree, expected);
        verify_string(versions[source].first, versions[source].second);
        versions.emplace_back(tree, expected);
    }
    for (const auto &version : versions) verify_string(version.first, version.second);
}

int main() {
    boundary_tests();
    const vector<int> initial_sizes = {0, 1, 2, 3, 7, 31, 64, 127};
    for (int i = 0; i < static_cast<int>(initial_sizes.size()); ++i) {
        random_test(0x9e3779b97f4a7c15ULL + i, initial_sizes[i]);
    }
    allocator_and_rebuild_tests();
    noncommutative_test();
    cout << "persistent_lazy_wbtree: OK\n";
}
