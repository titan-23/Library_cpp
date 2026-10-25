#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef LAZY_WB_TREE_HEADER
#define LAZY_WB_TREE_HEADER "titan_cpplib/ds/lazy_wb_tree.cpp"
#endif
#include LAZY_WB_TREE_HEADER

using namespace std;
using ll = long long;
constexpr ll MOD = 998244353;
struct Sum {
    ll value;
    int count;
    bool operator==(const Sum &x) const { return value == x.value && count == x.count; }
};
struct Affine {
    ll a, b;
    bool operator!=(const Affine &x) const { return a != x.a || b != x.b; }
};
int mapping_calls = 0, composition_calls = 0, op_calls = 0;
Sum op(Sum a, Sum b) { ++op_calls; return {(a.value+b.value)%MOD, a.count+b.count}; }
Sum mapping(Affine f, Sum x) { ++mapping_calls; return {(f.a*x.value+f.b*x.count)%MOD, x.count}; }
Affine composition(Affine f, Affine g) { ++composition_calls; return {f.a*g.a%MOD, (f.a*g.b+f.b)%MOD}; }
Sum e() { return {0, 0}; }
Affine id() { return {1, 0}; }
using Tree = titan23::LazyWBTree<Sum, Affine, op, mapping, composition, e, id>;

vector<Sum> elements(const vector<ll> &a) {
    vector<Sum> result;
    for (ll x : a) result.push_back({x, 1});
    return result;
}
void apply_reference(vector<ll> &a, int l, int r, Affine f) {
    for (int i = l; i < r; ++i) a[i] = (f.a*a[i]+f.b)%MOD;
}
Sum prod_reference(const vector<ll> &a, int l, int r) {
    ll sum = 0;
    for (int i = l; i < r; ++i) sum = (sum+a[i])%MOD;
    return {sum, r-l};
}

struct SavedNode {
    const void *node, *left, *right;
    Sum key, data;
    Affine lazy;
    bool rev;
    int size, left_size;
    bool operator==(const SavedNode &x) const {
        return node == x.node && left == x.left && right == x.right &&
               key == x.key && data == x.data && !(lazy != x.lazy) &&
               rev == x.rev && size == x.size && left_size == x.left_size;
    }
};
vector<SavedNode> snapshot(Tree &tree) {
    vector<SavedNode> result;
    auto walk = [&](auto &&self, auto node) -> void {
        if (!node) return;
        result.push_back({node, node->left, node->right, node->key, node->data,
                          node->lazy, node->rev, node->size, node->left_size});
        self(self, node->left); self(self, node->right);
    };
    walk(walk, tree.root);
    return result;
}
void verify(Tree &tree, const vector<ll> &expected, bool exhaustive = false) {
    const auto saved = snapshot(tree);
    const int n = expected.size();
    assert(tree.len() == n);
    assert(tree.tovector() == elements(expected));
    assert(tree.all_prod() == prod_reference(expected, 0, n));
    assert(tree.prod(0, n) == prod_reference(expected, 0, n));
    for (int i = 0; i < 8; ++i) {
        int l = n*i/8, r = n*(i+1)/8;
        assert(tree.prod(l, r) == prod_reference(expected, l, r));
        assert(tree.prod(l, l) == e());
        if (l < n) assert(tree.get(l) == (Sum{expected[l], 1}));
    }
    if (exhaustive) {
        for (int l = 0; l <= n; ++l) {
            if (l < n) assert(tree.get(l) == (Sum{expected[l], 1}));
            for (int r = l; r <= n; ++r) assert(tree.prod(l, r) == prod_reference(expected, l, r));
        }
    }
    tree.check();
    assert(snapshot(tree) == saved);
}

void boundary_and_alias_test() {
    Tree empty;
    assert(empty.root == nullptr);
    empty.apply(0, 0, {2, 3}); empty.all_apply({2, 3}); empty.reverse(0, 0);
    auto empty_parts = empty.split(0);
    empty_parts.first.merge(empty_parts.second);
    verify(empty_parts.first, {});

    for (int n : {1, 2, 3, 4, 7, 8, 15, 32, 65}) {
        vector<ll> expected(n); iota(expected.begin(), expected.end(), 0);
        Tree tree = elements(expected);  // Preserve implicit vector construction.
        auto alias = tree;
        auto original_root = tree.root;
        tree.set(n/2, {99, 1}); expected[n/2] = 99;
        assert(tree.root == original_root && alias.root == original_root);
        verify(alias, expected, n <= 8);
        tree.all_apply({3, 5}); apply_reference(expected, 0, n, {3, 5});
        tree.reverse(0, n); reverse(expected.begin(), expected.end());
        verify(alias, expected, n <= 8);

        mapping_calls = composition_calls = op_calls = 0;
        tree.apply(0, n, {2, 7}); apply_reference(expected, 0, n, {2, 7});
        assert(mapping_calls == 2 && composition_calls == 1 && op_calls == 0);
        mapping_calls = composition_calls = op_calls = 0;
        assert(tree.prod(0, n) == prod_reference(expected, 0, n));
        assert(mapping_calls == 0 && composition_calls == 0 && op_calls == 0);
        verify(tree, expected, n <= 8);

        for (int k : {0, n/2, n}) {
            auto source_root = tree.root;
            auto parts = tree.split(k);
            assert(tree.root == source_root);  // Destructive split keeps this field.
            vector<ll> left(expected.begin(), expected.begin()+k), right(expected.begin()+k, expected.end());
            verify(parts.first, left); verify(parts.second, right);
            auto other_root = parts.second.root;
            parts.first.merge(parts.second);
            assert(parts.second.root == other_root);  // Merge does not clear the consumed handle.
            tree = parts.first;
            verify(tree, expected);
        }
        while (!expected.empty()) {
            int k = expected.size()/2;
            assert(tree.pop(k) == (Sum{expected[k], 1}));
            expected.erase(expected.begin()+k);
            verify(tree, expected, expected.size() <= 8);
        }
    }
}

void random_test(uint64_t seed) {
    mt19937_64 rng(seed);
    vector<ll> expected(97); iota(expected.begin(), expected.end(), 0);
    Tree tree(elements(expected));
    for (int step = 0; step < 18000; ++step) {
        const int n = expected.size();
        int kind = rng()%10;
        if (!n) kind = 0;
        if (n >= 256 && kind == 0) kind = 1;
        int l = rng()%(n+1), r = rng()%(n+1);
        if (l > r) swap(l, r);
        if (step%37 == 0) { l = 0; r = n; }
        if (kind == 0) {
            ll value = rng()%1000;
            tree.insert(l, {value, 1}); expected.insert(expected.begin()+l, value);
        } else if (kind == 1) {
            int k = rng()%n;
            assert(tree.pop(k) == (Sum{expected[k], 1})); expected.erase(expected.begin()+k);
        } else if (kind == 2) {
            int k = rng()%n; ll value = rng()%1000;
            tree.set(k, {value, 1}); expected[k] = value;
        } else if (kind == 3 || kind == 4) {
            Affine action{static_cast<ll>(rng()%5), static_cast<ll>(rng()%13)};
            if (kind == 3) tree.apply(l, r, action);
            else { l = 0; r = n; tree.all_apply(action); }
            apply_reference(expected, l, r, action);
        } else if (kind == 5) {
            tree.reverse(l, r); reverse(expected.begin()+l, expected.begin()+r);
        } else if (kind == 6 || kind == 7) {
            auto old_root = tree.root;
            auto parts = tree.split(l);
            assert(tree.root == old_root);
            if (kind == 6) {
                auto consumed = parts.second.root;
                parts.first.merge(parts.second);
                assert(parts.second.root == consumed);
                tree = parts.first;
            } else {
                auto consumed = parts.first.root;
                parts.second.merge(parts.first);
                assert(parts.first.root == consumed);
                tree = parts.second;
                rotate(expected.begin(), expected.begin()+l, expected.end());
            }
        } else if (kind == 8) {
            assert(tree.prod(l, r) == prod_reference(expected, l, r));
        } else {
            int k = rng()%n; assert(tree.get(k) == (Sum{expected[k], 1}));
        }
        if (step%29 == 0) verify(tree, expected, expected.size() <= 24);
        else tree.check();
    }
    verify(tree, expected, expected.size() <= 24);
    while (tree.len()) tree.pop(tree.len()-1);
}

void growth_and_pointer_test() {
    vector<ll> expected(31); iota(expected.begin(), expected.end(), 0);
    Tree tree(elements(expected));
    auto retained = tree.root;
    const Sum retained_key = retained->key;
    for (int i = 31; i < 7000; ++i) {
        tree.insert(tree.len(), {i, 1}); expected.push_back(i);
        if (i%127 == 0) { tree.check(); assert(retained->key == retained_key); }
    }
    verify(tree, expected);
    for (ll value : expected) assert(tree.pop(0) == (Sum{value, 1}));
    verify(tree, {});
}

string string_op(string a, string b) { return a+b; }
string string_mapping(int f, string a) {
    for (char &c : a) c = 'a'+(c-'a'+f)%26;
    return a;
}
int string_composition(int f, int g) { return (f+g)%26; }
string string_e() { return {}; }
int string_id() { return 0; }
using StringTree = titan23::LazyWBTree<string, int, string_op, string_mapping,
                                      string_composition, string_e, string_id>;
void noncommutative_test() {
    vector<string> expected{"ab", "c", "de", "f", "g"};
    StringTree tree(expected);
    for (int step = 0; step < 80; ++step) {
        int l = step%expected.size(), r = expected.size();
        tree.apply(l, r, 3);
        for (int i = l; i < r; ++i) expected[i] = string_mapping(3, expected[i]);
        tree.set(l, "xyz"); expected[l] = "xyz";
        tree.insert(l, "hi"); expected.insert(expected.begin()+l, "hi");
        int k = (step*7)%expected.size();
        assert(tree.pop(k) == expected[k]); expected.erase(expected.begin()+k);
        assert(tree.tovector() == expected);
        for (int a = 0; a <= tree.len(); ++a) for (int b = a; b <= tree.len(); ++b) {
            string aggregate;
            for (int i = a; i < b; ++i) aggregate += expected[i];
            assert(tree.prod(a, b) == aggregate);
        }
        tree.check();
    }
    while (tree.len()) tree.pop(0);
}

struct Tracked {
    inline static int alive = 0;
    ll value = 0;
    int count = 0;
    string storage;
    Tracked(ll v = 0, int n = 0) : value(v), count(n), storage(80, 'x') { ++alive; }
    Tracked(const Tracked &x) : value(x.value), count(x.count), storage(x.storage) { ++alive; }
    Tracked(Tracked &&x) noexcept : value(x.value), count(x.count), storage(std::move(x.storage)) { ++alive; }
    Tracked &operator=(const Tracked &) = default;
    Tracked &operator=(Tracked &&) noexcept = default;
    ~Tracked() { --alive; }
};
struct TrackedAction {
    inline static int alive = 0;
    int shift;
    string storage;
    TrackedAction(int n = 0) : shift(n), storage(96, 'a') { ++alive; }
    TrackedAction(const TrackedAction &x) : shift(x.shift), storage(x.storage) { ++alive; }
    TrackedAction(TrackedAction &&x) noexcept : shift(x.shift), storage(std::move(x.storage)) { ++alive; }
    TrackedAction &operator=(const TrackedAction &) = default;
    TrackedAction &operator=(TrackedAction &&) noexcept = default;
    ~TrackedAction() { --alive; }
    bool operator!=(const TrackedAction &x) const { return shift != x.shift; }
};
Tracked tracked_op(Tracked a, Tracked b) { return {a.value+b.value, a.count+b.count}; }
Tracked tracked_mapping(TrackedAction f, Tracked x) { return {x.value+f.shift*x.count, x.count}; }
TrackedAction tracked_composition(TrackedAction f, TrackedAction g) { return {f.shift+g.shift}; }
Tracked tracked_e() { return {}; }
TrackedAction tracked_id() { return {}; }
using TrackedTree = titan23::LazyWBTree<Tracked, TrackedAction, tracked_op, tracked_mapping,
                                      tracked_composition, tracked_e, tracked_id>;

struct alignas(128) PoolRecord {
    Tracked value;
    TrackedAction action;
    explicit PoolRecord(int n) : value(n, 1), action(n) {
        if (n == -1) throw runtime_error("construction failure");
    }
};
void nontrivial_lifetime_test() {
    vector<Tracked> initial;
    for (int i = 0; i < 65; ++i) initial.emplace_back(i, 1);
    const int values_before = Tracked::alive, actions_before = TrackedAction::alive;
    {
        TrackedTree tree(initial);
        assert(Tracked::alive == values_before+130);
        assert(TrackedAction::alive == actions_before+65);
        tree.all_apply(TrackedAction(7));
        tree.reverse(0, tree.len());
        while (tree.len()) {
            const int before = Tracked::alive, actions = TrackedAction::alive;
            auto removed = tree.pop(tree.len()/2);
            assert(removed.count == 1);
            assert(Tracked::alive == before-1);  // Two node values die; one returned value remains.
            assert(TrackedAction::alive == actions-1);
        }
    }
    assert(Tracked::alive == values_before && TrackedAction::alive == actions_before);
    {
        titan23::detail::WBNodePool<PoolRecord, 2> pool;
        vector<PoolRecord*> records;
        for (int i = 0; i < 11; ++i) {
            records.push_back(pool.emplace(i));
            assert(reinterpret_cast<uintptr_t>(records.back())%alignof(PoolRecord) == 0);
        }
        auto recycled = records[3];
        pool.release(recycled);
        const int values = Tracked::alive, actions = TrackedAction::alive;
        try { pool.emplace(-1); assert(false); } catch (const runtime_error &) {}
        assert(Tracked::alive == values && TrackedAction::alive == actions);
        assert(pool.emplace(99) == recycled);
        assert(records.front()->value.value == 0 && records.back()->value.value == 10);
        // Remaining live records are destroyed by the pool, including across blocks.
    }
    assert(Tracked::alive == values_before && TrackedAction::alive == actions_before);
}

int main() {
    boundary_and_alias_test();
    random_test(0x1234abcd);
    random_test(0x98765432);
    growth_and_pointer_test();
    noncommutative_test();
    nontrivial_lifetime_test();
    cout << "lazy_wb_tree: OK\n";
}
