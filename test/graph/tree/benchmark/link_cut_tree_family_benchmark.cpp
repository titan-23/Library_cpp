#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
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
using Clock = chrono::steady_clock;
constexpr long long MOD = 998244353;
long long add(long long a, long long b) { return a + b; }
long long zero() { return 0; }
struct SumLen { long long sum, len; };
struct Affine {
    long long a, b;
    bool operator==(const Affine &) const = default;
};
SumLen op(SumLen x, SumLen y) { return {(x.sum + y.sum) % MOD, x.len + y.len}; }
SumLen e() { return {0, 0}; }
SumLen mapping(Affine f, SumLen x) { return {(f.a * x.sum + f.b * x.len) % MOD, x.len}; }
Affine composition(Affine f, Affine g) { return {f.a * g.a % MOD, (f.a * g.b + f.b) % MOD}; }
Affine id() { return {1, 0}; }

template<int KIND> struct Config;
template<> struct Config<0> { using Tree = titan23::LinkCutTree; };
template<> struct Config<1> { using Tree = titan23::LinkCutTreeSum<long long, add, zero>; };
template<> struct Config<2> { using Tree = titan23::LazyLinkCutTree<SumLen, op, e, Affine, mapping, composition, id>; };

struct Query { int u, v, root, c, p, k, value; };
template<class T> void escape(const T &x) { asm volatile("" : : "g"(&x) : "memory"); }

template<int KIND>
void run(int n, int q, int shape, unsigned seed, const string &workload) {
    mt19937 rng(seed);
    vector<int> par(n, -1), depth(n);
    for (int v = 1; v < n; ++v) {
        if (shape == 0) par[v] = int(rng() % v);
        if (shape == 1) par[v] = v - 1;
        if (shape == 2) par[v] = 0;
        if (shape == 3) par[v] = (v - 1) / 2;
        depth[v] = depth[par[v]] + 1;
    }
    vector<vector<int>> up;
    if (workload == "kth") {
        int lg = 1;
        while ((1LL << lg) <= n) ++lg;
        up.assign(lg, vector<int>(n));
        for (int v = 1; v < n; ++v) up[0][v] = par[v];
        for (int h = 1; h < lg; ++h) for (int v = 0; v < n; ++v) up[h][v] = up[h - 1][up[h - 1][v]];
    }
    auto lca = [&](int u, int v) {
        if (depth[u] < depth[v]) swap(u, v);
        int delta = depth[u] - depth[v];
        for (int h = 0; h < int(up.size()); ++h) if (delta >> h & 1) u = up[h][u];
        if (u == v) return u;
        for (int h = int(up.size()) - 1; h >= 0; --h) if (up[h][u] != up[h][v]) { u = up[h][u]; v = up[h][v]; }
        return par[u];
    };
    vector<Query> queries(q);
    for (Query &x : queries) {
        x.u = rng() % n; x.v = rng() % n; x.root = rng() % n;
        x.c = 1 + rng() % (n - 1); x.p = par[x.c];
        x.value = rng() % 1000;
        x.k = workload == "kth" ? int(rng() % (depth[x.u] + depth[x.v] - 2 * depth[lca(x.u, x.v)] + 1)) : 0;
    }
    auto start = Clock::now();
    typename Config<KIND>::Tree tr(n);
    if constexpr (KIND != 0) {
        for (int v = 0; v < n; ++v) {
            if constexpr (KIND == 1) tr.set(v, (v * 123LL + 17) % 1000);
            else tr.set(v, {(v * 123LL + 17) % 1000, 1});
        }
    }
    for (int v = 1; v < n; ++v) tr.link(v, par[v]);
    double build = chrono::duration<double>(Clock::now() - start).count();
    auto path = [&](int u, int v) -> uint64_t {
        if constexpr (KIND == 0) return tr.path_length(u, v);
        else if constexpr (KIND == 1) return tr.path_prod(u, v);
        else return tr.path_prod(u, v).sum;
    };
    auto point_set = [&](const Query &x) {
        if constexpr (KIND == 1) tr.set(x.u, x.value);
        else if constexpr (KIND == 2) tr.set(x.u, {x.value, 1});
    };
    auto point_get = [&](int v) -> uint64_t {
        if constexpr (KIND == 1) return tr.get(v);
        else if constexpr (KIND == 2) return tr.get(v).sum;
        else return 0;
    };
    uint64_t checksum = 0;
    start = Clock::now();
    if (workload == "path") {
        for (const auto &x : queries) checksum += path(x.u, x.v);
    } else if (workload == "same") {
        for (const auto &x : queries) checksum += tr.same(x.u, x.v);
    } else if (workload == "root") {
        for (const auto &x : queries) checksum += tr.root(x.u);
    } else if (workload == "kth") {
        for (const auto &x : queries) checksum += tr.path_kth_elm(x.u, x.v, x.k);
    } else if (workload == "lca") {
        for (const auto &x : queries) checksum += tr.lca(x.u, x.v, x.root);
    } else if (workload == "split_merge") {
        for (const auto &x : queries) { tr.split(x.c, x.p); checksum += tr.merge(x.c, x.p); }
    } else if (workload == "get") {
        for (const auto &x : queries) checksum += point_get(x.u);
    } else if (workload == "set") {
        for (const auto &x : queries) { point_set(x); escape(tr); }
    } else if (workload == "apply") {
        if constexpr (KIND == 2) {
            for (const auto &x : queries) { tr.path_apply(x.u, x.v, {x.value % 3, x.root}); escape(tr); }
        }
    } else if (workload == "rewire") {
        for (const auto &x : queries) {
            tr.evert(0); tr.cut(x.c); tr.link(x.c, x.root % x.c);
            checksum += path(x.u, x.v);
            tr.evert(0); tr.cut(x.c); tr.link(x.c, x.p);
        }
    } else if (workload == "mixed") {
        int step = 0;
        for (const auto &x : queries) {
            switch (step++ % 5) {
                case 0:
                    tr.evert(0); tr.cut(x.c); tr.link(x.c, x.root % x.c);
                    break;
                case 1: checksum += path(x.u, x.v); break;
                case 2:
                    if constexpr (KIND == 0) checksum += tr.same(x.u, x.v);
                    else point_set(x);
                    break;
                case 3:
                    if constexpr (KIND == 2) tr.path_apply(x.u, x.v, {x.value % 3, x.root});
                    else tr.evert(x.root);
                    break;
                case 4: checksum += tr.root(x.u); break;
            }
        }
    } else { cerr << "Unknown workload\n"; exit(1); }
    double elapsed = chrono::duration<double>(Clock::now() - start).count();
    // Validate observable values after mutation-only workloads, outside timing.
    checksum += path(0, n - 1);
    if constexpr (KIND != 0) for (int v = 0; v < min(n, 100); ++v) checksum += point_get(v);
    cout << fixed << setprecision(9) << build << ',' << elapsed << ',' << checksum << '\n';
}

int main(int argc, char **argv) {
    if (argc != 7) { cerr << "Usage: benchmark type workload N Q shape seed\n"; return 1; }
    string type = argv[1], workload = argv[2];
    int n = stoi(argv[3]), q = stoi(argv[4]), shape = stoi(argv[5]);
    unsigned seed = stoul(argv[6]);
    if (n < 2 || q < 1 || shape < 0 || shape > 3) return 1;
    if (type == "plain") run<0>(n, q, shape, seed, workload);
    else if (type == "sum") run<1>(n, q, shape, seed, workload);
    else if (type == "lazy") run<2>(n, q, shape, seed, workload);
    else return 1;
}
