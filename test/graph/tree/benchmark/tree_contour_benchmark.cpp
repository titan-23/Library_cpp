/// https://github.com/titan-23/Library_cpp/blob/main/test/graph/tree/benchmark/tree_contour_benchmark.cpp
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <queue>
#include <random>
#include <string>
#include <vector>

#include "titan_cpplib/graph/tree/tree_contour_add.cpp"
#include "titan_cpplib/graph/tree/tree_contour_sum.cpp"

using namespace std;
using namespace titan23;

using Clock = chrono::steady_clock;

#ifndef TREE_CONTOUR_BENCH_N
#define TREE_CONTOUR_BENCH_N 200000
#endif

#ifndef TREE_CONTOUR_BENCH_Q
#define TREE_CONTOUR_BENCH_Q 200000
#endif

#ifndef TREE_CONTOUR_BENCH_REPS
#define TREE_CONTOUR_BENCH_REPS 3
#endif

struct Query {
    int v, l, r;
    long long x;
};

template<class T>
void escape(const T &v) {
    asm volatile("" : : "g"(&v) : "memory");
}

template<class F>
double measure(F &&f) {
    const auto start = Clock::now();
    f();
    return chrono::duration<double>(Clock::now() - start).count();
}

template<class Prep, class F>
double measure_median(Prep &&prep, F &&f) {
    array<double, TREE_CONTOUR_BENCH_REPS> times;
    for (double &sec : times) {
        prep();
        sec = measure(f);
    }
    sort(times.begin(), times.end());
    return times[times.size() / 2];
}

void print_row(const string &name, double sec, int ops, const string &unit = "op") {
    cout << left << setw(20) << name << right << setw(10) << sec << " s  ";
    cout << setw(9) << sec * 1e6 / ops << " us/" << unit << '\n';
}

vector<vector<int>> make_path(int n) {
    vector<vector<int>> g(n);
    for (int v = 1; v < n; ++v) {
        g[v - 1].push_back(v);
        g[v].push_back(v - 1);
    }
    return g;
}

vector<vector<int>> make_star(int n) {
    vector<vector<int>> g(n);
    for (int v = 1; v < n; ++v) {
        g[0].push_back(v);
        g[v].push_back(0);
    }
    return g;
}

vector<vector<int>> make_random_tree(int n, mt19937 &rng) {
    vector<vector<int>> g(n);
    for (int v = 1; v < n; ++v) {
        const int p = (int)(rng() % v);
        g[p].push_back(v);
        g[v].push_back(p);
    }
    return g;
}

vector<vector<int>> make_balanced_tree(int n) {
    vector<vector<int>> g(n);
    for (int v = 1; v < n; ++v) {
        const int p = (v - 1) / 2;
        g[p].push_back(v);
        g[v].push_back(p);
    }
    return g;
}

vector<vector<int>> make_broom(int n) {
    vector<vector<int>> g(n);
    const int h = n / 2;
    for (int v = 1; v < h; ++v) {
        g[v - 1].push_back(v);
        g[v].push_back(v - 1);
    }
    for (int v = h; v < n; ++v) {
        g[h - 1].push_back(v);
        g[v].push_back(h - 1);
    }
    return g;
}

pair<int, int> farthest(const vector<vector<int>> &g, int s) {
    vector<int> dist(g.size(), -1);
    queue<int> que;
    dist[s] = 0;
    que.push(s);
    int far = s;
    while (!que.empty()) {
        const int v = que.front();
        que.pop();
        if (dist[v] > dist[far]) far = v;
        for (int x : g[v]) {
            if (dist[x] != -1) continue;
            dist[x] = dist[v] + 1;
            que.push(x);
        }
    }
    return {far, dist[far]};
}

int diameter(const vector<vector<int>> &g) {
    return farthest(g, farthest(g, 0).first).second;
}

void run_shape(const string &shape, const vector<vector<int>> &g, mt19937 &rng, uint64_t &checksum) {
    const int n = (int)g.size();
    constexpr int Q = TREE_CONTOUR_BENCH_Q;
    const int diam = diameter(g);
    vector<Query> qs(Q);
    for (Query &q : qs) {
        q.v = (int)(rng() % n);
        q.l = (int)(rng() % (diam + 2));
        q.r = (int)(rng() % (diam + 2));
        if (q.l > q.r) swap(q.l, q.r);
        if (q.l == q.r) ++q.r;
        q.x = (int)(rng() % 2001) - 1000;
    }
    vector<long long> a(n);
    for (long long &x : a) x = (int)(rng() % 2001) - 1000;

    unique_ptr<TreeContourSum<long long>> sum;
    const double sum_build_sec = measure_median([&] { sum.reset(); }, [&] {
        sum = make_unique<TreeContourSum<long long>>(g, a);
    });

    cout << "\n" << shape << " diameter=" << diam << '\n';
    print_row("sum build", sum_build_sec, n, "vertex");

    auto reset_sum = [&] {
        sum.reset();
        sum = make_unique<TreeContourSum<long long>>(g, a);
    };
    double sec = measure_median(reset_sum, [&] {
        for (const Query &q : qs) sum->add(q.v, q.x);
        escape(*sum);
    });
    print_row("point add", sec, Q);

    sec = measure_median(reset_sum, [&] {
        for (const Query &q : qs) sum->set(q.v, q.x);
        escape(*sum);
    });
    print_row("point set", sec, Q);

    sec = measure_median(reset_sum, [&] {
        long long res = 0;
        for (const Query &q : qs) res += sum->prod(q.v, q.l, q.r);
        checksum += res;
    });
    print_row("prod", sec, Q);

    sec = measure_median(reset_sum, [&] {
        long long res = 0;
        for (const Query &q : qs) res += sum->get(q.v);
        checksum += res;
    });
    print_row("sum get", sec, Q);

    sum.reset();
    unique_ptr<TreeContourAdd<long long>> add;
    const double add_build_sec = measure_median([&] { add.reset(); }, [&] {
        add = make_unique<TreeContourAdd<long long>>(g, a);
    });
    print_row("add build", add_build_sec, n, "vertex");

    auto reset_add = [&] {
        add.reset();
        add = make_unique<TreeContourAdd<long long>>(g, a);
    };
    sec = measure_median(reset_add, [&] {
        for (const Query &q : qs) add->add(q.v, q.l, q.r, q.x);
        escape(*add);
    });
    print_row("range add", sec, Q);

    auto prepare_get = [&] {
        reset_add();
        for (const Query &q : qs) add->add(q.v, q.l, q.r, q.x);
    };
    sec = measure_median(prepare_get, [&] {
        long long res = 0;
        for (const Query &q : qs) res += add->get(q.v);
        checksum += res;
    });
    print_row("range get", sec, Q);
}

int main(int argc, char **argv) {
    constexpr int N = TREE_CONTOUR_BENCH_N;
    constexpr int Q = TREE_CONTOUR_BENCH_Q;
    constexpr int REPS = TREE_CONTOUR_BENCH_REPS;
    static_assert(N >= 2 && Q >= 1 && REPS >= 1);

    mt19937 rng(23);
    uint64_t checksum = 0;
    const string only = argc >= 2 ? argv[1] : "";
    auto selected = [&](const string &shape) { return only.empty() || only == shape; };
    cout << fixed << setprecision(6);
    cout << "N=" << N << " Q=" << Q << " reps=" << REPS << " seed=23\n";
    if (selected("random")) run_shape("random", make_random_tree(N, rng), rng, checksum);
    if (selected("balanced")) run_shape("balanced", make_balanced_tree(N), rng, checksum);
    if (selected("broom")) run_shape("broom", make_broom(N), rng, checksum);
    if (selected("path")) run_shape("path", make_path(N), rng, checksum);
    if (selected("star")) run_shape("star", make_star(N), rng, checksum);
    cout << "checksum " << checksum << '\n';
}
