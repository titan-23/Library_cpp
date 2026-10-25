#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
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

struct Value {
    ll sum = 0;
    int count = 0;
    bool operator==(const Value &other) const {
        return sum == other.sum && count == other.count;
    }
};
struct Action {
    ll a = 1, b = 0;
    bool operator!=(const Action &other) const {
        return a != other.a || b != other.b;
    }
};
Value op(Value x, Value y) { return {(x.sum + y.sum) % MOD, x.count + y.count}; }
Value mapping(Action f, Value x) { return {(f.a * x.sum + f.b * x.count) % MOD, x.count}; }
Action composition(Action f, Action g) { return {f.a * g.a % MOD, (f.a * g.b + f.b) % MOD}; }
Value e() { return {}; }
Action id() { return {}; }
template<class Index>
using IndexedTree = titan23::PersistentLazyWBTree<Index, Value, Action, op, mapping, composition, e, id>;
using Tree = IndexedTree<int>;

vector<Value> values_of(const vector<ll> &a) {
    vector<Value> values;
    for (ll x : a) values.push_back({x, 1});
    return values;
}
void apply_oracle(vector<ll> &a, int l, int r, Action f) {
    for (int i = l; i < r; ++i) a[i] = (f.a * a[i] + f.b) % MOD;
}
Value prod_oracle(const vector<ll> &a, int l, int r) {
    Value result;
    for (int i = l; i < r; ++i) result = op(result, {a[i], 1});
    return result;
}
template<class Index>
void verify(const IndexedTree<Index> &tree, const vector<ll> &a) {
    const auto allocated = IndexedTree<Index>::ma.ptr;
    const int n = a.size();
    assert(tree.len() == n);
    assert(tree.tovector() == values_of(a));
    assert(tree.prod(0, n) == prod_oracle(a, 0, n));
    for (int k : {0, n / 3, n / 2, n ? n - 1 : 0}) {
        assert(tree.prod(k, k) == e());
        if (k < n) assert(tree.get(k) == (Value{a[k], 1}));
    }
    for (int i = 0; i < 9; ++i) {
        int l = n * i / 9, r = n * (i + 1) / 9;
        assert(tree.prod(l, r) == prod_oracle(a, l, r));
    }
    tree.check();
    assert(IndexedTree<Index>::ma.ptr == allocated);
}

struct SavedNode {
    Tree::MemoryAllocator::Node node;
    Tree::MemoryAllocator::Data data;
};
#ifdef PLWBT_SEPARATE_LEAF_POOLS
struct Snapshot {
    vector<SavedNode> branches;
    vector<Value> leaves;
    bool empty() const { return branches.empty() && leaves.empty(); }
};
#else
using Snapshot = vector<SavedNode>;
#endif
template<class Data>
const auto &stored_value(const Data &data) {
#ifdef PLWBT_LEAF_LAYOUT
    return data.data;
#else
    return data.key;
#endif
}
Snapshot snapshot() {
    Snapshot saved;
#ifdef PLWBT_SEPARATE_LEAF_POOLS
    for (size_t i = 0; i < Tree::ma.branch_ptr; ++i) {
        saved.branches.push_back({Tree::ma.pool[i].tree, Tree::ma.pool[i].data});
    }
    for (size_t i = 0; i < Tree::ma.leaf_ptr; ++i) saved.leaves.push_back(Tree::ma.leaves[i]);
#else
    for (size_t i = 0; i < Tree::ma.ptr; ++i) saved.push_back({Tree::ma.tree[i], Tree::ma.data[i]});
#endif
    return saved;
}
void verify_unchanged(const Snapshot &saved) {
#ifdef PLWBT_SEPARATE_LEAF_POOLS
    assert(Tree::ma.branch_ptr >= saved.branches.size());
    assert(Tree::ma.leaf_ptr >= saved.leaves.size());
    for (size_t i = 0; i < saved.leaves.size(); ++i) assert(Tree::ma.leaves[i] == saved.leaves[i]);
    const auto &records = saved.branches;
#else
    assert(Tree::ma.ptr >= saved.size());
    const auto &records = saved;
#endif
    for (size_t i = 0; i < records.size(); ++i) {
#ifdef PLWBT_SEPARATE_LEAF_POOLS
        const auto &node = Tree::ma.pool[i].tree;
        const auto &data = Tree::ma.pool[i].data;
#else
        const auto &node = Tree::ma.tree[i];
        const auto &data = Tree::ma.data[i];
#endif
        assert(node.left == records[i].node.left);
        assert(node.right == records[i].node.right);
        assert(node.size == records[i].node.size);
        assert(node.left_size == records[i].node.left_size);
        assert(stored_value(data) == stored_value(records[i].data));
        assert(data.data == records[i].data.data);
        assert(!(data.lazy != records[i].data.lazy));
        assert(data.rev == records[i].data.rev);
    }
}

void alias_and_immutability_test() {
    Tree::ma.reset();
    vector<ll> original(129);
    iota(original.begin(), original.end(), 1);
    Tree base(values_of(original));
    vector<pair<Tree, vector<ll>>> versions = {{base, original}};
    for (int round = 0; round < 20; ++round) {
        auto saved = snapshot();
        auto current = versions.back().first;
        auto expected = versions.back().second;
        const int n = expected.size();
        Action f{2 + round % 3, 7 + round};
        current = current.apply(0, n, f);
        apply_oracle(expected, 0, n, f);
        current = current.reverse(0, n);
        reverse(expected.begin(), expected.end());
        auto prefix = current.split_left(n * 3 / 4);
        auto suffix = current.split_right(n / 4);
        auto joined = prefix.merge(suffix);
        vector<ll> repeated(expected.begin(), expected.begin() + n * 3 / 4);
        repeated.insert(repeated.end(), expected.begin() + n / 4, expected.end());
        joined = joined.set(repeated.size() / 2, {round, 1});
        repeated[repeated.size() / 2] = round;
        joined = joined.apply(1, repeated.size() - 1, {3, 11});
        apply_oracle(repeated, 1, repeated.size() - 1, {3, 11});
        auto doubled = current.merge(current);
        auto twice = expected;
        twice.insert(twice.end(), expected.begin(), expected.end());
        verify(joined, repeated);
        verify(doubled, twice);
        verify(current, expected);
        verify_unchanged(saved);
        for (const auto &version : versions) verify(version.first, version.second);
        versions.push_back({current, expected});
        versions.push_back({joined.split_range(0, original.size()),
                            vector<ll>(repeated.begin(), repeated.begin() + original.size())});
    }
}

void long_history_test(uint64_t seed) {
    Tree::ma.reset();
    mt19937_64 rng(seed);
    vector<ll> expected(1024);
    for (ll &x : expected) x = rng() % MOD;
    Tree latest(values_of(expected));
    vector<pair<Tree, vector<ll>>> history = {{latest, expected}};
    for (int step = 0; step < 2400; ++step) {
        Snapshot saved;
        if (step % 137 == 0) saved = snapshot();
        if (step % 5 == 0) {
            const auto &source = history[rng() % history.size()];
            latest = source.first;
            expected = source.second;
        }
        const int n = expected.size();
        int l = rng() % (n + 1), r = rng() % (n + 1);
        if (l > r) swap(l, r);
        switch (rng() % 8) {
        case 0: {
            Action f{static_cast<ll>(rng() % 7), static_cast<ll>(rng() % 1001)};
            latest = latest.apply(l, r, f);
            apply_oracle(expected, l, r, f);
            break;
        }
        case 1:
            latest = latest.reverse(l, r);
            reverse(expected.begin() + l, expected.begin() + r);
            break;
        case 2: {
            const ll x = rng() % MOD;
            latest = latest.insert(l, {x, 1});
            expected.insert(expected.begin() + l, x);
            break;
        }
        case 3:
            if (n) {
                const int k = rng() % n;
                auto [rest, erased] = latest.pop(k);
                assert(erased == (Value{expected[k], 1}));
                latest = rest;
                expected.erase(expected.begin() + k);
            }
            break;
        case 4:
            if (n) {
                const int k = rng() % n;
                const ll x = rng() % MOD;
                latest = latest.set(k, {x, 1});
                expected[k] = x;
            }
            break;
        case 5: {
            auto [left, right] = latest.split(l);
            latest = right.merge(left);
            rotate(expected.begin(), expected.begin() + l, expected.end());
            break;
        }
        case 6:
            if (n <= 2048) {
                auto part = latest.split_range(l, r);
                latest = latest.merge(part);
                vector<ll> copied(expected.begin() + l, expected.begin() + r);
                expected.insert(expected.end(), copied.begin(), copied.end());
            } else {
                latest = latest.split_range(n / 4, n * 3 / 4);
                expected = vector<ll>(expected.begin() + n / 4, expected.begin() + n * 3 / 4);
            }
            break;
        case 7:
            latest = latest.apply(0, n, {2, 9}).reverse(0, n);
            apply_oracle(expected, 0, n, {2, 9});
            reverse(expected.begin(), expected.end());
            break;
        }
        verify(latest, expected);
        if (!saved.empty()) verify_unchanged(saved);
        if (step % 11 == 0) {
            const auto &past = history[rng() % history.size()];
            verify(past.first, past.second);
        }
        if (step % 23 == 0) history.push_back({latest, expected});
    }
    for (const auto &past : history) verify(past.first, past.second);
}

template<class Index>
void small_index_test(int n, int rounds) {
    using SmallTree = IndexedTree<Index>;
    SmallTree::ma.reset();
    vector<ll> expected(n);
    iota(expected.begin(), expected.end(), 1);
    SmallTree original(values_of(expected));
    auto current = original;
    for (int i = 0; i < rounds; ++i) {
        current = current.apply(0, n, {2, 3}).reverse(0, n);
        apply_oracle(expected, 0, n, {2, 3});
        reverse(expected.begin(), expected.end());
        current = current.set(i % n, {i, 1});
        expected[i % n] = i;
        verify(current, expected);
        assert(SmallTree::ma.ptr <= static_cast<size_t>(numeric_limits<Index>::max()));
    }
    vector<ll> initial(n);
    iota(initial.begin(), initial.end(), 1);
    verify(original, initial);
}

struct alignas(64) TrackedValue {
    inline static int alive = 0;
    inline static int copies_until_throw = -1;
    string text;
    TrackedValue() { ++alive; }
    explicit TrackedValue(string s) : text(std::move(s)) { ++alive; }
    TrackedValue(const TrackedValue &other) : text(other.text) {
        if (copies_until_throw == 0) throw runtime_error("requested copy failure");
        if (copies_until_throw > 0) --copies_until_throw;
        ++alive;
    }
    TrackedValue(TrackedValue &&other) noexcept : text(std::move(other.text)) { ++alive; }
    TrackedValue &operator=(const TrackedValue &) = default;
    TrackedValue &operator=(TrackedValue &&) noexcept = default;
    ~TrackedValue() { --alive; }
    bool operator==(const TrackedValue &other) const { return text == other.text; }
};
struct TrackedAction {
    inline static int alive = 0;
    string payload;
    int shift = 0;
    TrackedAction() : payload(80, 'x') { ++alive; }
    explicit TrackedAction(int n) : payload(80, 'x'), shift(n) { ++alive; }
    TrackedAction(const TrackedAction &other) : payload(other.payload), shift(other.shift) { ++alive; }
    TrackedAction(TrackedAction &&other) noexcept
        : payload(std::move(other.payload)), shift(other.shift) { ++alive; }
    TrackedAction &operator=(const TrackedAction &) = default;
    TrackedAction &operator=(TrackedAction &&) noexcept = default;
    ~TrackedAction() { --alive; }
    bool operator!=(const TrackedAction &other) const { return shift != other.shift; }
};
TrackedValue tracked_op(TrackedValue a, TrackedValue b) {
    return TrackedValue(a.text + b.text);
}
TrackedValue tracked_mapping(TrackedAction f, TrackedValue value) {
    for (char &c : value.text) c = 'a' + (c - 'a' + f.shift) % 26;
    return value;
}
TrackedAction tracked_composition(TrackedAction f, TrackedAction g) {
    return TrackedAction((f.shift + g.shift) % 26);
}
TrackedValue tracked_e() { return {}; }
TrackedAction tracked_id() { return {}; }
using TrackedTree = titan23::PersistentLazyWBTree<int, TrackedValue, TrackedAction,
    tracked_op, tracked_mapping, tracked_composition, tracked_e, tracked_id>;

#ifdef PLWBT_SEPARATE_LEAF_POOLS
void separate_pool_allocator_test() {
    const int before_values = TrackedValue::alive;
    const int before_actions = TrackedAction::alive;
    {
        TrackedTree::MemoryAllocator allocator;
        const int reserved_values = TrackedValue::alive;
        const int reserved_actions = TrackedAction::alive;
        allocator.reserve(2048);
        assert(TrackedValue::alive == reserved_values);
        assert(TrackedAction::alive == reserved_actions);
        assert(allocator.ptr == 1 && allocator.branch_ptr == 1 && allocator.leaf_ptr == 0);
        const TrackedValue value(string(80, 'b'));
        const TrackedAction action(7);
        const int first_leaf = allocator.new_leaf(value);
        const int first_branch = allocator.new_branch(value, action);
        assert(first_leaf == 1 && first_branch == 2);
        assert(allocator.is_leaf(first_leaf) && !allocator.is_leaf(first_branch));
        const int actions_before_leaves = TrackedAction::alive;
        for (int i = 1; i < 65536; ++i) {
            const int node = i % 7 ? allocator.new_leaf(value) : allocator.copy(first_leaf);
            assert(allocator.is_leaf(node) && allocator.record_index(node) == size_t(i));
            assert(allocator.value(node) == value);
        }
        assert(TrackedAction::alive == actions_before_leaves);
        for (int i = 2; i < 65536; ++i) {
            const int node = i % 7 ? allocator.new_branch(value, action) : allocator.copy(first_branch);
            assert(!allocator.is_leaf(node) && allocator.record_index(node) == size_t(i));
            assert(allocator.value(node) == value);
        }
        assert(allocator.leaves.size() == 65536 && allocator.pool.size() == 65536);
        assert(allocator.ptr == allocator.branch_ptr + allocator.leaf_ptr);

        for (int operation = 0; operation < 4; ++operation) {
            const size_t position = allocator.ptr;
            const size_t branches = allocator.branch_ptr, leaves = allocator.leaf_ptr;
            const size_t branch_size = allocator.pool.size(), leaf_size = allocator.leaves.size();
            const int alive_values = TrackedValue::alive, alive_actions = TrackedAction::alive;
            TrackedValue::copies_until_throw = 0;
            bool failed = false;
            try {
                if (operation == 0) allocator.copy(first_leaf);
                else if (operation == 1) allocator.new_leaf(value);
                else if (operation == 2) allocator.copy(first_branch);
                else allocator.new_branch(value, action);
            } catch (const runtime_error &) { failed = true; }
            TrackedValue::copies_until_throw = -1;
            assert(failed);
            assert(allocator.ptr == position && allocator.branch_ptr == branches && allocator.leaf_ptr == leaves);
            assert(allocator.pool.size() == branch_size && allocator.leaves.size() == leaf_size);
            assert(allocator.cap == allocator.pool.capacity() + allocator.leaves.capacity());
            assert(TrackedValue::alive == alive_values && TrackedAction::alive == alive_actions);
            const int recovered = operation < 2 ? allocator.copy(first_leaf) : allocator.copy(first_branch);
            assert(allocator.value(recovered) == value);
            assert(allocator.value(first_leaf) == value && allocator.value(first_branch) == value);
        }
        assert(reinterpret_cast<uintptr_t>(&allocator.value(first_leaf)) % alignof(TrackedValue) == 0);
        assert(reinterpret_cast<uintptr_t>(&allocator.value(first_branch)) % alignof(TrackedValue) == 0);
        const auto frontier = allocator.frontier();
        assert(!allocator.owns(first_leaf, frontier) && !allocator.owns(first_branch, frontier));
        const int copied_leaf = allocator.copy(first_leaf), copied_branch = allocator.copy(first_branch);
        assert(allocator.owns(copied_leaf, frontier) && allocator.owns(copied_branch, frontier));
        const TrackedValue *first_address = &allocator.value(first_leaf);
        const size_t allocated = allocator.ptr;
        auto moved = std::move(allocator);
        assert(moved.ptr == allocated && &moved.value(first_leaf) == first_address);
        assert(moved.tree[first_leaf].size == 1);
        assert(moved.data[first_leaf].data == value);
        assert(!(moved.data[first_branch].lazy != action));
        allocator = std::move(moved);
        assert(allocator.ptr == allocated && &allocator.value(first_leaf) == first_address);
        allocator.reset();
        assert(allocator.ptr == 1 && allocator.branch_ptr == 1 && allocator.leaf_ptr == 0);
        const TrackedValue replacement(string(97, 'z'));
        const TrackedAction replacement_action(19);
        for (int i = 0; i < 4099; ++i) {
            const int leaf = allocator.new_leaf(replacement);
            const int branch = allocator.new_branch(replacement, replacement_action);
            assert(leaf == 2 * i + 1 && branch == 2 * (i + 1));
            assert(allocator.value(leaf) == replacement && allocator.value(branch) == replacement);
            assert(!(allocator.pool[allocator.record_index(branch)].data.lazy != replacement_action));
        }
    }
    assert(TrackedValue::alive == before_values);
    assert(TrackedAction::alive == before_actions);

    TrackedTree::ma.reset();
    vector<TrackedValue> initial;
    for (int i = 0; i < 257; ++i) initial.emplace_back(string(32, 'a' + i % 26));
    TrackedTree original(initial);
    auto changed = original.apply(0, initial.size(), TrackedAction(3));
    changed = changed.set(128, TrackedValue(string(64, 'm')));
    const auto allocation = TrackedTree::ma.ptr;
    assert(original.tovector() == initial);
    auto expected = initial;
    for (auto &value : expected) value = tracked_mapping(TrackedAction(3), value);
    expected[128] = TrackedValue(string(64, 'm'));
    assert(changed.tovector() == expected);
    TrackedValue total;
    for (const auto &value : expected) total = tracked_op(total, value);
    assert(changed.prod(0, expected.size()) == total);
    assert(TrackedTree::ma.ptr == allocation);
}
#endif

#ifndef PLWBT_SEPARATE_LEAF_POOLS
// Tagged leaf pools use a separate allocator lifecycle suite: diagnostic views
// synthesize leaf records and intentionally do not expose a leaf lazy field.
void allocator_exception_test() {
    const int before_values = TrackedValue::alive;
    const int before_actions = TrackedAction::alive;
    {
        TrackedTree::MemoryAllocator allocator;
        const TrackedValue value(string(80, 'b'));
        const TrackedAction action(3);
        const int first = allocator.new_node(value, action);
        for (bool copying : {true, false}) {
            const size_t position = allocator.ptr;
            const size_t tree_size = allocator.tree.size();
            const size_t data_size = allocator.data.size();
            // With either value or reference parameters, allowing one copy
            // makes new_node fail while constructing Data's value members.
#ifdef PLWBT_LEAF_LAYOUT
            TrackedValue::copies_until_throw = 0;
#else
            TrackedValue::copies_until_throw = copying ? 0 : 1;
#endif
            bool failed = false;
            try {
                if (copying) allocator.copy(first);
                else allocator.new_node(value, action);
            } catch (const runtime_error &) {
                failed = true;
            }
            TrackedValue::copies_until_throw = -1;
            assert(failed);
            assert(allocator.ptr == position);
            assert(allocator.tree.size() == tree_size);
            assert(allocator.data.size() == data_size);
            const int recovered = allocator.new_node(value, action);
            assert(recovered == static_cast<int>(position));
            assert(stored_value(allocator.data[recovered]) == value);
            assert(stored_value(allocator.data[first]) == value);
        }
    }
    assert(TrackedValue::alive == before_values);
    assert(TrackedAction::alive == before_actions);
}

void nontrivial_allocator_test() {
    const int before_values = TrackedValue::alive;
    const int before_actions = TrackedAction::alive;
    {
        TrackedTree::MemoryAllocator allocator;
        const int reserved_values = TrackedValue::alive;
        const int reserved_actions = TrackedAction::alive;
        allocator.reserve(17);
        assert(TrackedValue::alive == reserved_values);
        assert(TrackedAction::alive == reserved_actions);
        const TrackedValue value(string(80, 'a'));
        const TrackedAction action(7);
        for (int i = 0; i < 65539; ++i) {
            const int node = i % 7 == 0 && i ? allocator.copy(1) : allocator.new_node(value, action);
            assert(stored_value(allocator.data[node]) == value);
            assert(!(allocator.data[node].lazy != action));
            assert(allocator.tree[node].size == 1);
            assert(reinterpret_cast<uintptr_t>(&stored_value(allocator.data[node])) % alignof(TrackedValue) == 0);
        }
        // AoS allocator views must follow the storage when an allocator moves.
        const size_t allocated = allocator.ptr;
        auto moved = std::move(allocator);
        assert(moved.ptr == allocated);
        assert(stored_value(moved.data[1]) == value);
        assert(moved.tree[allocated - 1].size == 1);
        allocator = std::move(moved);
        assert(allocator.ptr == allocated);
        assert(stored_value(allocator.data[allocated - 1]) == value);
        allocator.reset();
        const TrackedValue replacement(string(97, 'z'));
        const TrackedAction replacement_action(19);
        for (int i = 0; i < 4099; ++i) {
            const int node = allocator.new_node(replacement, replacement_action);
            assert(node == i + 1);
            assert(stored_value(allocator.data[node]) == replacement);
            assert(!(allocator.data[node].lazy != replacement_action));
        }
    }
    assert(TrackedValue::alive == before_values);
    assert(TrackedAction::alive == before_actions);

    TrackedTree::ma.reset();
    TrackedTree::ma.reserve(8193);
    vector<TrackedValue> initial;
    for (int i = 0; i < 257; ++i) initial.emplace_back(string(32, 'a' + i % 26));
    TrackedTree original(initial);
    auto changed = original.apply(0, initial.size(), TrackedAction(3));
    changed = changed.set(128, TrackedValue(string(64, 'm')));
    const auto allocation = TrackedTree::ma.ptr;
    assert(original.tovector() == initial);
    auto expected = initial;
    for (auto &value : expected) value = tracked_mapping(TrackedAction(3), value);
    expected[128] = TrackedValue(string(64, 'm'));
    assert(changed.tovector() == expected);
    TrackedValue total;
    for (const auto &value : expected) total = tracked_op(total, value);
    assert(changed.prod(0, expected.size()) == total);
    assert(TrackedTree::ma.ptr == allocation);
}
#endif

int main() {
    alias_and_immutability_test();
    long_history_test(0x7123a5b8);
    long_history_test(0x9381b6c4);
    small_index_test<int8_t>(7, 5);
    small_index_test<int16_t>(129, 160);
    small_index_test<uint8_t>(7, 5);
    small_index_test<uint16_t>(129, 160);
#ifndef PLWBT_SEPARATE_LEAF_POOLS
    allocator_exception_test();
    nontrivial_allocator_test();
#else
    separate_pool_allocator_test();
#endif
    cout << "persistent_lazy_wbtree_fast_extra: OK\n";
}
