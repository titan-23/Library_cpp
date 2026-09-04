/// https://github.com/titan-23/Library_cpp/blob/main/test/graph/benchmark/online_dynamic_connectivity_benchmark.cpp
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include "titan_cpplib/graph/online_dynamic_connectivity.cpp"
using namespace std;

using Clock = chrono::steady_clock;

void barrier(const void *p) {
#if defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("" : : "g"(p) : "memory");
#else
    (void)p;
    atomic_signal_fence(memory_order_seq_cst);
#endif
}

struct Fixture {
    titan23::OnlineDynamicConnectivity dc;
    vector<int> active, pos, tree;
    int count = 0;

    Fixture(int n, int cap) : dc(n, cap), pos(cap, -1) {
        active.reserve(cap);
        tree.reserve(n);
    }

    void keep(int id) {
        pos[id] = count++;
        active.push_back(id);
    }

    void drop(int id) {
        int p = pos[id], z = active.back();
        active[p] = z;
        pos[z] = p;
        active.pop_back();
        pos[id] = -1;
        --count;
    }
};

double seconds(Clock::time_point l, Clock::time_point r) {
    return chrono::duration<double>(r - l).count();
}

void print_row(int n, int q, const string &name, int ops, double sec, uint64_t sum) {
    double us = ops ? sec * 1e6 / ops : 0;
    double ns = ops ? sec * 1e9 / ops : 0;
    cout << n << ',' << q << ',' << name << ',' << ops << ',' << fixed << setprecision(6) << sec << ','
         << setprecision(3) << us << ',' << setprecision(1) << ns << ',' << sum << '\n';
}

int pick(mt19937 &rng, int n) {
    return uniform_int_distribution<int>(0, n - 1)(rng);
}

int main(int argc, char **argv) {
    int n = argc > 1 ? atoi(argv[1]) : 200000;
    int q = argc > 2 ? atoi(argv[2]) : n;
    int base_edges = n > 0 ? n - 1 + n / 2 : 0;
    int cap = base_edges + q + 10;
    mt19937 rng(712367);
    vector<int> us(q), vs(q), rnd(q);
    for (int i = 0; i < q; ++i) {
        us[i] = pick(rng, n);
        vs[i] = pick(rng, n);
        rnd[i] = pick(rng, 1000000000);
    }
    vector<int> par(n), au(n / 2), av(n / 2);
    for (int v = 1; v < n; ++v) par[v] = pick(rng, v);
    for (int i = 0; i < n / 2; ++i) {
        au[i] = pick(rng, n);
        av[i] = pick(rng, n);
    }

    cout << "n,q,operation,operations,seconds,us_per_op,ns_per_op,checksum\n";
    {
        titan23::OnlineDynamicConnectivity dc(n, n - 1);
        uint64_t sum = 0;
        auto l = Clock::now();
        for (int v = 1; v < n; ++v) sum ^= dc.add_edge(par[v], v);
        auto r = Clock::now();
        print_row(n, q, "add_tree", n - 1, seconds(l, r), sum);
    }
    {
        titan23::OnlineDynamicConnectivity dc(n, base_edges);
        uint64_t sum = 0;
        auto l = Clock::now();
        for (int v = 1; v < n; ++v) sum ^= dc.add_edge(par[v], v);
        for (int i = 0; i < n / 2; ++i) sum ^= dc.add_edge(au[i], av[i]);
        auto r = Clock::now();
        print_row(n, q, "initial_add", base_edges, seconds(l, r), sum);
    }

    auto fill = [&](Fixture &f) {
        for (int v = 1; v < n; ++v) {
            int id = f.dc.add_edge(par[v], v);
            f.keep(id);
            f.tree.push_back(id);
        }
        for (int i = 0; i < n / 2; ++i) f.keep(f.dc.add_edge(au[i], av[i]));
    };
    Fixture base(n, cap);
    fill(base);

    {
        Fixture f = base;
        uint64_t sum = 0;
        auto l = Clock::now();
        for (int i = 0; i < q; ++i) {
            barrier(&f.dc);
            sum += f.dc.same(us[i], vs[i]);
        }
        auto r = Clock::now();
        print_row(n, q, "same_connected", q, seconds(l, r), sum);
    }
    {
        Fixture f = base;
        uint64_t sum = 0;
        auto l = Clock::now();
        for (int i = 0; i < q; ++i) {
            barrier(&f.dc);
            sum += f.dc.size(us[i]);
        }
        auto r = Clock::now();
        print_row(n, q, "size_connected", q, seconds(l, r), sum);
    }
    {
        titan23::OnlineDynamicConnectivity dc(n, n);
        int mid = n / 2;
        for (int v = 1; v < n; ++v) {
            if (v != mid) dc.add_edge(v - 1, v);
        }
        uint64_t sum = 0;
        auto l = Clock::now();
        for (int i = 0; i < q; ++i) {
            barrier(&dc);
            sum += dc.same(us[i], vs[i]);
        }
        auto r = Clock::now();
        print_row(n, q, "same_general", q, seconds(l, r), sum);
    }
    {
        titan23::OnlineDynamicConnectivity dc(n, n);
        int mid = n / 2;
        for (int v = 1; v < n; ++v) {
            if (v != mid) dc.add_edge(v - 1, v);
        }
        uint64_t sum = 0;
        auto l = Clock::now();
        for (int i = 0; i < q; ++i) {
            barrier(&dc);
            sum += dc.size(us[i]);
        }
        auto r = Clock::now();
        print_row(n, q, "size_general", q, seconds(l, r), sum);
    }
    {
        Fixture f(n, cap);
        fill(f);
        uint64_t sum = 0;
        auto l = Clock::now();
        for (int i = 0; i < q; ++i) sum ^= f.dc.add_edge(us[i], vs[i]);
        auto r = Clock::now();
        print_row(n, q, "add_non_tree", q, seconds(l, r), sum);
    }
    {
        Fixture f = base;
        vector<int> ids(q);
        for (int i = 0; i < q; ++i) ids[i] = f.dc.add_edge(us[i], vs[i]);
        uint64_t sum = 0;
        auto l = Clock::now();
        for (int id : ids) sum += f.dc.erase_edge(id);
        auto r = Clock::now();
        print_row(n, q, "erase_non_tree", q, seconds(l, r), sum);
    }
    {
        Fixture f = base;
        int ops = min(q, (int)f.tree.size());
        uint64_t sum = 0;
        auto l = Clock::now();
        for (int i = 0; i < ops; ++i) sum += f.dc.erase_edge(f.tree[i]);
        auto r = Clock::now();
        print_row(n, q, "erase_tree", ops, seconds(l, r), sum);
    }
    {
        Fixture f(n, cap);
        fill(f);
        f.active.reserve(f.pos.size());
        uint64_t sum = 0;
        auto l = Clock::now();
        for (int i = 0; i < q; ++i) {
            if (i % 3 == 0) {
                sum += f.dc.same(us[i], vs[i]);
            } else if (i % 3 == 1) {
                int id = f.dc.add_edge(us[i], vs[i]);
                f.keep(id);
            } else {
                int id = f.active[rnd[i] % f.count];
                sum += f.dc.erase_edge(id);
                f.drop(id);
            }
        }
        auto r = Clock::now();
        print_row(n, q, "mixed_dense", q, seconds(l, r), sum);
    }
    {
        Fixture f(n, q + 10);
        uint64_t sum = 0;
        auto l = Clock::now();
        for (int i = 0; i < q; ++i) {
            if (i % 4 == 0) {
                sum += f.dc.same(us[i], vs[i]);
            } else if (i % 4 != 3 || f.count == 0) {
                int id = f.dc.add_edge(us[i], vs[i]);
                f.keep(id);
            } else {
                int id = f.active[rnd[i] % f.count];
                sum += f.dc.erase_edge(id);
                f.drop(id);
            }
        }
        auto r = Clock::now();
        print_row(n, q, "mixed_empty", q, seconds(l, r), sum);
    }
}
