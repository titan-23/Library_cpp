/// https://github.com/titan-23/Library_cpp/blob/main/test/graph/tree/benchmark/link_cut_tree_subtree_benchmark.cpp
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "titan_cpplib/graph/tree/link_cut_tree_subtree.cpp"

using namespace std;
using namespace titan23;

using Clock = chrono::steady_clock;

#ifndef LCTS_BENCH_N
#define LCTS_BENCH_N 200000
#endif

#ifndef LCTS_BENCH_Q
#define LCTS_BENCH_Q 200000
#endif

#ifndef LCTS_BENCH_SHAPE
#define LCTS_BENCH_SHAPE 0
#endif

long long op(long long x, long long y) {
    return x + y;
}

long long e() {
    return 0;
}

long long inv(long long x) {
    return -x;
}

using LCT = LinkCutTreeSubtree<long long, op, e, inv>;

struct Query {
    int u, v, root, k, c, p;
    long long x;
};

template <class T>
void escape(const T &v) {
    asm volatile("" : : "g"(&v) : "memory");
}

template <class F>
double measure(F &&f) {
    auto start = Clock::now();
    f();
    return chrono::duration<double>(Clock::now() - start).count();
}

void print_row(const string &name, double sec, size_t ops, const string &unit = "op") {
    cout << left << setw(22) << name << right << setw(10) << sec << " s  ";
    cout << setw(9) << sec * 1e6 / double(ops) << " us/" << unit << '\n';
}

template <class F>
uint64_t run(const string &name, const LCT &base, const vector<Query> &qs, F &&f, const string &unit = "op") {
    LCT tr = base;
    uint64_t sum = 0;
    double sec = measure([&] {
        for (const Query &q : qs) sum += f(tr, q);
    });
    print_row(name, sec, qs.size(), unit);
    return sum;
}

int main() {
    constexpr int N = LCTS_BENCH_N;
    constexpr int Q = LCTS_BENCH_Q;
    constexpr int SHAPE = LCTS_BENCH_SHAPE;
    static_assert(N >= 2 && Q >= 1);
    static_assert(0 <= SHAPE && SHAPE <= 3);

    mt19937 rng(23);
    vector<int> par(N, -1), dep(N);
    for (int v = 1; v < N; ++v) {
        if constexpr (SHAPE == 0) par[v] = int(rng() % v);
        if constexpr (SHAPE == 1) par[v] = v - 1;
        if constexpr (SHAPE == 2) par[v] = 0;
        if constexpr (SHAPE == 3) par[v] = (v - 1) / 2;
        dep[v] = dep[par[v]] + 1;
    }

    int lg = 1;
    while ((1 << lg) <= N) ++lg;
    vector<vector<int>> up(lg, vector<int>(N));
    for (int v = 1; v < N; ++v) up[0][v] = par[v];
    for (int h = 1; h < lg; ++h) {
        for (int v = 0; v < N; ++v) up[h][v] = up[h - 1][up[h - 1][v]];
    }

    auto lca = [&](int u, int v) {
        if (dep[u] < dep[v]) swap(u, v);
        int d = dep[u] - dep[v];
        for (int h = 0; h < lg; ++h) {
            if (d >> h & 1) u = up[h][u];
        }
        if (u == v) return u;
        for (int h = lg - 1; h >= 0; --h) {
            if (up[h][u] != up[h][v]) {
                u = up[h][u];
                v = up[h][v];
            }
        }
        return par[u];
    };

    vector<Query> qs(Q);
    for (Query &q : qs) {
        q.u = int(rng() % N);
        q.v = int(rng() % N);
        q.root = int(rng() % N);
        int w = lca(q.u, q.v);
        int len = dep[q.u] + dep[q.v] - dep[w] * 2 + 1;
        q.k = int(rng() % len);
        q.c = int(rng() % (N - 1)) + 1;
        q.p = par[q.c];
        q.x = rng() % 1000000000;
    }

    vector<long long> a(N);
    for (long long &v : a) v = rng() % 1000;
    auto start = Clock::now();
    LCT base(a);
    for (int v = 1; v < N; ++v) base.link(v, par[v]);
    double build = chrono::duration<double>(Clock::now() - start).count();

    cout << fixed << setprecision(6);
    constexpr const char *shape[] = {"random", "path", "star", "balanced"};
    cout << "N=" << N << " Q=" << Q << " shape=" << shape[SHAPE] << " seed=23\n";
    print_row("build", build, N, "vertex");

    uint64_t sum = 0;
    sum += run("get", base, qs, [](LCT &tr, const Query &q) {
        return uint64_t(tr.get(q.u));
    });
    sum += run("set", base, qs, [](LCT &tr, const Query &q) {
        tr.set(q.u, q.x);
        escape(tr);
        return uint64_t(0);
    });
    sum += run("expose", base, qs, [](LCT &tr, const Query &q) {
        return uint64_t(tr.expose(q.u));
    });
    sum += run("root", base, qs, [](LCT &tr, const Query &q) {
        return uint64_t(tr.root(q.u));
    });
    sum += run("same", base, qs, [](LCT &tr, const Query &q) {
        return uint64_t(tr.same(q.u, q.v));
    });
    sum += run("evert", base, qs, [](LCT &tr, const Query &q) {
        tr.evert(q.u);
        escape(tr);
        return uint64_t(0);
    });
    sum += run("lca", base, qs, [](LCT &tr, const Query &q) {
        return uint64_t(tr.lca(q.u, q.v));
    });
    sum += run("lca(root)", base, qs, [](LCT &tr, const Query &q) {
        return uint64_t(tr.lca(q.u, q.v, q.root));
    });
    sum += run("path_prod", base, qs, [](LCT &tr, const Query &q) {
        return uint64_t(tr.path_prod(q.u, q.v));
    });
    sum += run("subtree_prod", base, qs, [](LCT &tr, const Query &q) {
        return uint64_t(tr.subtree_prod(q.root, q.v));
    });
    sum += run("component_prod", base, qs, [](LCT &tr, const Query &q) {
        return uint64_t(tr.component_prod(q.u));
    });
    sum += run("path_length", base, qs, [](LCT &tr, const Query &q) {
        return uint64_t(tr.path_length(q.u, q.v));
    });
    sum += run("path_kth_elm", base, qs, [](LCT &tr, const Query &q) {
        return uint64_t(tr.path_kth_elm(q.u, q.v, q.k));
    });
    sum += run("subtree_size", base, qs, [](LCT &tr, const Query &q) {
        return uint64_t(tr.subtree_size(q.root, q.v));
    });
    sum += run("component_size", base, qs, [](LCT &tr, const Query &q) {
        return uint64_t(tr.component_size(q.u));
    });
    sum += run("cut + link", base, qs, [](LCT &tr, const Query &q) {
        tr.cut(q.c);
        tr.link(q.c, q.p);
        escape(tr);
        return uint64_t(0);
    }, "pair");
    sum += run("split + merge", base, qs, [](LCT &tr, const Query &q) {
        tr.split(q.c, q.p);
        tr.merge(q.c, q.p);
        escape(tr);
        return uint64_t(0);
    }, "pair");

    cout << "checksum " << sum << '\n';
}
