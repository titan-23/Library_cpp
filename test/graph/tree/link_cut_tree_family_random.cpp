#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

#ifdef LCT_PLAIN_HEADER
#include LCT_PLAIN_HEADER
#else
#include "titan_cpplib/graph/tree/link_cut_tree.cpp"
#endif
#ifdef LCT_SUM_HEADER
#include LCT_SUM_HEADER
#else
#include "titan_cpplib/graph/tree/link_cut_tree_sum.cpp"
#endif
#ifdef LCT_LAZY_HEADER
#include LCT_LAZY_HEADER
#else
#include "titan_cpplib/graph/tree/lazy_link_cut_tree.cpp"
#endif

using namespace std;

struct Forest {
    vector<set<int>> adj;
    vector<int> roots;
    explicit Forest(int n) : adj(n), roots(n) { iota(roots.begin(), roots.end(), 0); }
    vector<int> parents(int r) const {
        vector<int> p(adj.size(), -2);
        queue<int> q;
        q.push(r);
        p[r] = -1;
        while (!q.empty()) {
            int v = q.front(); q.pop();
            for (int x : adj[v]) if (p[x] == -2) { p[x] = v; q.push(x); }
        }
        return p;
    }
    bool same(int u, int v) const { return roots[u] == roots[v]; }
    vector<int> component(int v) const {
        vector<int> a;
        for (int x = 0; x < int(adj.size()); ++x) if (same(v, x)) a.push_back(x);
        return a;
    }
    void evert(int v) { for (int x : component(v)) roots[x] = v; }
    void link(int c, int p) {
        assert(!same(c, p) && roots[c] == c);
        int r = roots[p];
        for (int x : component(c)) roots[x] = r;
        adj[c].insert(p); adj[p].insert(c);
    }
    bool merge(int u, int v) {
        if (same(u, v)) return false;
        evert(u); link(u, v); return true;
    }
    void cut(int c) {
        int p = parents(roots[c])[c];
        assert(p >= 0);
        adj[c].erase(p); adj[p].erase(c);
        auto par = parents(c);
        for (int x = 0; x < int(adj.size()); ++x) if (par[x] != -2) roots[x] = c;
    }
    void split(int u, int v) { assert(adj[u].contains(v)); evert(u); cut(v); }
    vector<pair<int, int>> edges() const {
        vector<pair<int, int>> es;
        for (int v = 0; v < int(adj.size()); ++v) for (int u : adj[v]) if (u < v) es.emplace_back(u, v);
        return es;
    }
    vector<int> path(int u, int v) const {
        assert(same(u, v));
        auto p = parents(u);
        vector<int> a;
        for (int x = v; x != -1; x = p[x]) a.push_back(x);
        reverse(a.begin(), a.end());
        return a;
    }
    int lca(int u, int v) const {
        auto a = path(roots[u], u), b = path(roots[u], v);
        size_t k = 0;
        while (k < min(a.size(), b.size()) && a[k] == b[k]) ++k;
        return a[k - 1];
    }
};

long long sum_op(long long a, long long b) { return a + b; }
long long sum_e() { return 0; }
struct SumCase {
    using V = long long;
    using Tree = titan23::LinkCutTreeSum<V, sum_op, sum_e>;
    static constexpr bool values = true, lazy = false;
    static V value(mt19937_64 &rng) { return int(rng() % 201) - 100; }
    static V combine(V a, V b) { return sum_op(a, b); }
    static V unit() { return 0; }
};
struct PlainCase : SumCase {
    using Tree = titan23::LinkCutTree;
    static constexpr bool values = false;
};

constexpr long long MOD = 998244353;
struct SumLen {
    long long sum, len;
    bool operator==(const SumLen &) const = default;
};
struct Affine {
    long long a, b;
    bool operator==(const Affine &) const = default;
};
SumLen affine_op(SumLen x, SumLen y) { return {(x.sum + y.sum) % MOD, x.len + y.len}; }
SumLen affine_e() { return {0, 0}; }
SumLen affine_mapping(Affine f, SumLen x) { return {(f.a * x.sum + f.b * x.len) % MOD, x.len}; }
Affine affine_composition(Affine f, Affine g) { return {f.a * g.a % MOD, (f.a * g.b + f.b) % MOD}; }
Affine affine_id() { return {1, 0}; }
struct AffineCase {
    using V = SumLen;
    using Tree = titan23::LazyLinkCutTree<V, affine_op, affine_e, Affine, affine_mapping, affine_composition, affine_id>;
    static constexpr bool values = true, lazy = true;
    static V value(mt19937_64 &rng) { return {static_cast<long long>(rng() % 1000), 1}; }
    static Affine update(mt19937_64 &rng) { return {static_cast<long long>(rng() % 4), static_cast<long long>(rng() % 1000)}; }
    static V map(Affine f, V x) { return affine_mapping(f, x); }
    static V combine(V x, V y) { return affine_op(x, y); }
    static V unit() { return affine_e(); }
};

using Perm = array<int, 4>;
string word_op(string a, string b) { return a + b; }
string word_e() { return ""; }
string word_mapping(Perm f, string a) { for (char &c : a) c = char('a' + f[c - 'a']); return a; }
Perm word_composition(Perm f, Perm g) { for (int &x : g) x = f[x]; return g; }
Perm word_id() { return {0, 1, 2, 3}; }
struct WordCase {
    using V = string;
    using Tree = titan23::LazyLinkCutTree<V, word_op, word_e, Perm, word_mapping, word_composition, word_id>;
    static constexpr bool values = true, lazy = true;
    static V value(mt19937_64 &rng) { return string(1, char('a' + rng() % 4)); }
    static Perm update(mt19937_64 &rng) { auto f = word_id(); shuffle(f.begin(), f.end(), rng); return f; }
    static V map(Perm f, V x) { return word_mapping(f, x); }
    static V combine(V x, V y) { return word_op(x, y); }
    static V unit() { return word_e(); }
};

template<class Case>
void random_test() {
    using V = typename Case::V;
    using Tree = typename Case::Tree;
    {
        Tree tr(3);
        tr.link(1, 0); tr.link(2, 1); tr.evert(2);
        assert(!tr.merge(0, 1));
        assert(tr.root(0) == 2);
        assert(!tr.merge(0, 0));
        assert(tr.root(1) == 2);
    }
    for (unsigned seed : {23, 81, 2026, 998244353}) {
        mt19937_64 rng(seed);
        const int n = 37;
        Tree tr(n);
        Forest ref(n);
        vector<V> values(n);
        for (int v = 0; v < n; ++v) {
            values[v] = Case::value(rng);
            if constexpr (Case::values) tr.set(v, values[v]);
        }
        for (int v = 1; v < n; ++v) {
            int p = seed % 2 ? v - 1 : int(rng() % v);
            tr.link(v, p); ref.link(v, p);
        }
        auto fold = [&](const vector<int> &path) {
            V result = Case::unit();
            for (int v : path) result = Case::combine(result, values[v]);
            return result;
        };
        for (int step = 0; step < 16000; ++step) {
            int u = int(rng() % n), v = int(rng() % n);
            int kind = int(rng() % 16);
            if (kind == 0) {
                bool expected = ref.merge(u, v);
                assert(tr.merge(u, v) == expected);
            } else if (kind == 1) {
                if (!ref.same(u, v)) {
                    tr.evert(u); ref.evert(u);
                    tr.link(u, v); ref.link(u, v);
                }
            } else if (kind == 2 || kind == 3) {
                auto edges = ref.edges();
                if (!edges.empty()) {
                    tie(u, v) = edges[rng() % edges.size()];
                    if (kind == 2) {
                        if (ref.parents(ref.roots[u])[v] != u) swap(u, v);
                        tr.cut(v); ref.cut(v);
                    } else { tr.split(u, v); ref.split(u, v); }
                }
            } else if (kind == 4) { tr.evert(u); ref.evert(u); }
            else if (kind == 5) { assert(tr.same(u, v) == ref.same(u, v)); }
            else if (kind == 6) { assert(tr.root(u) == ref.roots[u]); }
            else if (kind == 11) {
                if constexpr (Case::values) { values[u] = Case::value(rng); tr.set(u, values[u]); }
            } else if (kind == 12) {
                if constexpr (Case::values) assert(tr.get(u) == values[u]);
            } else if (ref.same(u, v)) {
                auto path = ref.path(u, v);
                if (kind == 7) { assert(tr.path_length(u, v) == int(path.size())); ref.evert(u); }
                else if (kind == 8) {
                    int k = int(rng() % (path.size() + 1));
                    assert(tr.path_kth_elm(u, v, k) == (k == int(path.size()) ? -1 : path[k]));
                    ref.evert(u);
                } else if (kind == 9) {
                    auto comp = ref.component(u);
                    int r = comp[rng() % comp.size()];
                    ref.evert(r);
                    assert(tr.lca(u, v, r) == ref.lca(u, v));
                } else if (kind == 10) {
                    assert(tr.lca(u, v) == ref.lca(u, v));
                    tr.expose(u);
                    assert(tr.expose(v) == ref.lca(u, v));
                } else if (kind == 13) {
                    if constexpr (Case::values) { assert(tr.path_prod(u, v) == fold(path)); ref.evert(u); }
                } else if constexpr (Case::lazy) {
                    for (int rep = 0; rep < (kind == 15 ? 7 : 1); ++rep) {
                        auto f = Case::update(rng);
                        tr.path_apply(u, v, f);
                        for (int x : ref.path(u, v)) values[x] = Case::map(f, values[x]);
                        ref.evert(u);
                        auto comp = ref.component(u);
                        u = comp[rng() % comp.size()]; v = comp[rng() % comp.size()];
                    }
                }
            }
            if (step % 997 == 0) {
                for (int x = 0; x < n; ++x) {
                    assert(tr.root(x) == ref.roots[x]);
                    if constexpr (Case::values) assert(tr.get(x) == values[x]);
                    int y = int(rng() % n);
                    assert(tr.same(x, y) == ref.same(x, y));
                }
            }
        }
#ifndef LCT_TEST_OLD
        // Value ownership and the vector constructor, including pending lazy tags.
        Tree copy = tr;
        copy.evert(0);
        assert(tr.root(0) == ref.roots[0]);
        if constexpr (Case::values) {
            copy.set(0, Case::value(rng));
            assert(tr.get(0) == values[0]);
            Tree fresh(values);
            for (int x = 0; x < n; ++x) assert(fresh.get(x) == values[x]);
        }
        auto edges = ref.edges();
        if (!edges.empty()) {
            auto [u, v] = edges.front();
            copy.split(u, v);
            assert(!copy.same(u, v));
            assert(tr.same(u, v));
        }
        Tree assigned;
        assigned = tr;
        assigned.evert(n - 1);
        assert(tr.root(n - 1) == ref.roots[n - 1]);
        assert(tr.path_kth_elm(0, 0, -1) == -1);
#endif
    }
}

#ifndef LCT_TEST_OLD
// The pending-action flag does not require an equality operation on F.
struct AddTag { long long value; };
SumLen tag_mapping(AddTag f, SumLen x) { return {(x.sum + f.value * x.len) % MOD, x.len}; }
AddTag tag_composition(AddTag f, AddTag g) { return {(f.value + g.value) % MOD}; }
AddTag tag_id() { return {0}; }
void pending_copy_test() {
    using Tree = titan23::LazyLinkCutTree<SumLen, affine_op, affine_e, AddTag, tag_mapping, tag_composition, tag_id>;
    Tree original(vector<SumLen>{{1, 1}, {2, 1}, {3, 1}});
    original.link(1, 0); original.link(2, 1);
    original.path_apply(0, 2, {7});
    Tree copy = original;
    copy.path_apply(2, 0, {11});
    assert(copy.path_prod(0, 2) == (SumLen{60, 3}));
    assert(original.path_prod(0, 2) == (SumLen{27, 3}));
    copy.split(0, 1);
    assert(!copy.same(0, 2) && original.same(0, 2));
}
#endif

void deep_test() {
    constexpr int n = 100000;
    titan23::LinkCutTree plain(n);
    SumCase::Tree sum(n);
    AffineCase::Tree lazy(n);
    for (int v = 0; v < n; ++v) {
        sum.set(v, 1);
        lazy.set(v, {1, 1});
        if (v) { plain.link(v, v - 1); sum.link(v, v - 1); lazy.link(v, v - 1); }
    }
    assert(plain.path_length(0, n - 1) == n);
    assert(sum.path_prod(0, n - 1) == n);
    lazy.path_apply(0, n - 1, {2, 3});
    lazy.path_apply(n - 1, 0, {3, 7});
    assert(lazy.get(n / 2) == (SumLen{22, 1}));
    assert(lazy.path_prod(0, n - 1) == (SumLen{22LL * n % MOD, n}));
    for (int k : {0, n / 2, n - 1}) {
        assert(plain.path_kth_elm(0, n - 1, k) == k);
        assert(sum.path_kth_elm(n - 1, 0, k) == n - 1 - k);
        assert(lazy.path_kth_elm(0, n - 1, k) == k);
    }
}

int main(int argc, char **argv) {
#ifndef LCT_TEST_OLD
    pending_copy_test();
#endif
    string kind = argc > 1 ? argv[1] : "all";
    if (kind == "all" || kind == "plain") { random_test<PlainCase>(); cout << "plain: OK\n"; }
    if (kind == "all" || kind == "sum") { random_test<SumCase>(); cout << "sum: OK\n"; }
    if (kind == "all" || kind == "affine") { random_test<AffineCase>(); cout << "lazy affine: OK\n"; }
    if (kind == "all" || kind == "word") { random_test<WordCase>(); cout << "lazy noncommutative: OK\n"; }
    if (kind == "all" || kind == "deep") { deep_test(); cout << "deep path: OK\n"; }
}
