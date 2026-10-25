#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#ifndef PERSISTENT_LAZY_WBTREE_HEADER
#define PERSISTENT_LAZY_WBTREE_HEADER "titan_cpplib/ds/persistent_lazy_wbtree.cpp"
#endif
#include PERSISTENT_LAZY_WBTREE_HEADER

using namespace std;
using ll = long long;

struct Counters {
    uint64_t op = 0, mapping = 0, composition = 0, nodes = 0;
    Counters &operator+=(const Counters &other) {
        op += other.op;
        mapping += other.mapping;
        composition += other.composition;
        nodes += other.nodes;
        return *this;
    }
};
Counters calls;

struct Value {
    ll sum = 0;
    int length = 0;
    bool operator==(const Value &other) const {
        return sum == other.sum && length == other.length;
    }
};
Value op(Value a, Value b) {
    ++calls.op;
    return {a.sum + b.sum, a.length + b.length};
}
Value mapping(ll f, Value value) {
    ++calls.mapping;
    return {value.sum + f * value.length, value.length};
}
ll composition(ll f, ll g) {
    ++calls.composition;
    return f + g;
}
Value e() { return {}; }
ll id() { return 0; }

using Tree = titan23::PersistentLazyWBTree<int, Value, ll, op, mapping, composition, e, id>;

enum class Operation { set, get, apply, full_apply, prod, insert, pop, split_merge, reverse };

Counters sample(Operation operation, Tree &base, const vector<ll> &prefix, mt19937_64 &rng) {
    const int n = base.len();
    const int k = rng() % n;
    const int insertion = rng() % (n + 1);
    int l = rng() % (n + 1), r = rng() % (n + 1);
    if (l > r) swap(l, r);
    const ll value = 200 + rng() % 101;
    const ll action = 1 + rng() % 19;
    const ll total = prefix.back();
    const size_t position = Tree::ma.ptr;
    calls = {};
    Counters measured;
    auto capture = [&] {
        measured = calls;
        measured.nodes = Tree::ma.ptr - position;
    };
    auto verify_total = [&] (const Tree &tree, ll sum, int size) {
        assert(tree.len() == size);
        assert(tree.prod(0, size) == (Value{sum, size}));
    };

    switch (operation) {
    case Operation::set: {
        auto changed = base.set(k, {value, 1});
        capture();
        verify_total(changed, total - k % 97 + value, n);
        assert(changed.get(k) == (Value{value, 1}));
        break;
    }
    case Operation::get: {
        const auto actual = base.get(k);
        capture();
        assert(actual == (Value{k % 97, 1}));
        break;
    }
    case Operation::apply: {
        auto changed = base.apply(l, r, action);
        capture();
        verify_total(changed, total + action * (r - l), n);
        if (l < r) assert(changed.get(l) == (Value{l % 97 + action, 1}));
        break;
    }
    case Operation::full_apply: {
        auto changed = base.apply(0, n, action);
        capture();
        verify_total(changed, total + action * n, n);
        assert(changed.get(k) == (Value{k % 97 + action, 1}));
        break;
    }
    case Operation::prod: {
        const auto actual = base.prod(l, r);
        capture();
        assert(actual == (Value{prefix[r] - prefix[l], r - l}));
        break;
    }
    case Operation::insert: {
        auto changed = base.insert(insertion, {value, 1});
        capture();
        verify_total(changed, total + value, n + 1);
        assert(changed.get(insertion) == (Value{value, 1}));
        break;
    }
    case Operation::pop: {
        auto [changed, removed] = base.pop(k);
        capture();
        assert(removed == (Value{k % 97, 1}));
        verify_total(changed, total - removed.sum, n - 1);
        break;
    }
    case Operation::split_merge: {
        auto [left, right] = base.split(insertion);
        auto joined = left.merge(right);
        capture();
        verify_total(joined, total, n);
        assert(joined.get(k) == (Value{k % 97, 1}));
        break;
    }
    case Operation::reverse: {
        auto changed = base.reverse(l, r);
        capture();
        verify_total(changed, total, n);
        if (l < r) {
            assert(changed.get(l) == (Value{(r - 1) % 97, 1}));
            assert(changed.get(r - 1) == (Value{l % 97, 1}));
        }
        break;
    }
    }
    // Verification happens after capture and does not contribute to counts.
    verify_total(base, total, n);
    assert(base.get(k) == (Value{k % 97, 1}));
    return measured;
}

void print_counts(const string &name, int n, int samples, const Counters &counts) {
    cout << name << ',' << n << ',' << samples << ','
         << static_cast<double>(counts.op) / samples << ','
         << static_cast<double>(counts.mapping) / samples << ','
         << static_cast<double>(counts.composition) / samples << ','
         << static_cast<double>(counts.nodes) / samples << '\n';
}

int main(int argc, char **argv) {
    const int n = argc > 1 ? stoi(argv[1]) : 500000;
    const int samples = argc > 2 ? stoi(argv[2]) : 1000;
    const uint64_t seed = argc > 3 ? stoull(argv[3]) : 20261025;
    assert(n > 0 && samples > 0);
    vector<Value> input(n);
    vector<ll> prefix(n + 1);
    for (int i = 0; i < n; ++i) {
        input[i] = {i % 97, 1};
        prefix[i + 1] = prefix[i] + input[i].sum;
    }
    Tree::ma.reset();
    calls = {};
    const size_t before_build = Tree::ma.ptr;
    Tree base(input);
    Counters build = calls;
    build.nodes = Tree::ma.ptr - before_build;
    cout << fixed << setprecision(3);
    cout << "operation,n,samples,op_per_operation,mapping_per_operation,composition_per_operation,nodes_per_operation\n";
    print_counts("build", n, 1, build);
    const pair<Operation, const char *> operations[] = {
        {Operation::set, "set"}, {Operation::get, "get"},
        {Operation::apply, "apply"}, {Operation::full_apply, "full_apply"},
        {Operation::prod, "prod"}, {Operation::insert, "insert"},
        {Operation::pop, "pop"}, {Operation::split_merge, "split_merge"},
        {Operation::reverse, "reverse"}
    };
    for (const auto &[operation, name] : operations) {
        mt19937_64 rng(seed);
        Counters total;
        for (int i = 0; i < samples; ++i) total += sample(operation, base, prefix, rng);
        print_counts(name, n, samples, total);
    }
}
