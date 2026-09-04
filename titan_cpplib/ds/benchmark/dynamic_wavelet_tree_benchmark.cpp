/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/benchmark/dynamic_wavelet_tree_benchmark.cpp
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include "titan_cpplib/ds/dynamic_wavelet_tree.cpp"
#include "titan_cpplib/ds/dynamic_wavelet_tree_sum.cpp"

using namespace std;
using namespace titan23;

using Clock = chrono::steady_clock;

#ifndef DWT_BENCH_N
#define DWT_BENCH_N 200000
#endif

#ifndef DWT_BENCH_Q
#define DWT_BENCH_Q 200000
#endif

#ifndef DWT_BENCH_U
#define DWT_BENCH_U 20000
#endif

#ifndef DWT_BENCH_LOG
#define DWT_BENCH_LOG 20
#endif

struct Query {
    int l, r, k, pos, x;
};

template<class F>
double measure(F&& f) {
    auto start = Clock::now();
    f();
    return chrono::duration<double>(Clock::now() - start).count();
}

void print_row(const string& name, double sec, int ops) {
    cout << left << setw(18) << name << right << setw(10) << sec << " s  ";
    cout << setw(9) << sec * 1e6 / ops << " us/op\n";
}

int main() {
    constexpr int N = DWT_BENCH_N;
    constexpr int Q = DWT_BENCH_Q;
    constexpr int U = DWT_BENCH_U;
    constexpr int SIGMA = 1 << DWT_BENCH_LOG;
    mt19937 rng(23);

    vector<int> a(N), occ(N), freq(SIGMA);
    vector<long long> w(N);
    for (int i = 0; i < N; ++i) {
        a[i] = rng() % SIGMA;
        w[i] = rng() % 100 + 1;
        occ[i] = freq[a[i]]++;
    }

    vector<Query> qs(Q);
    for (auto& q : qs) {
        q.l = rng() % N;
        q.r = rng() % N;
        if (q.l > q.r) swap(q.l, q.r);
        ++q.r;
        q.k = rng() % (q.r - q.l);
        q.pos = rng() % N;
        q.x = rng() % SIGMA;
    }

    uint64_t sum = 0;
    cout << fixed << setprecision(6);
    cout << "N=" << N << " Q=" << Q << " U=" << U << " sigma=" << SIGMA << '\n';

    {
        auto start = Clock::now();
        DynamicWaveletTree<int> tr(SIGMA, a);
        double build = chrono::duration<double>(Clock::now() - start).count();
        cout << "\nDynamicWaveletTree\n";
        print_row("build", build, N);
        print_row("access", measure([&] {
            for (auto q : qs) sum += tr.access(q.pos);
        }), Q);
        print_row("rank", measure([&] {
            for (auto q : qs) sum += tr.rank(q.r, q.x);
        }), Q);
        print_row("range_count", measure([&] {
            for (auto q : qs) sum += tr.range_count(q.l, q.r, q.x);
        }), Q);
        print_row("range_freq", measure([&] {
            for (auto q : qs) sum += tr.range_freq(q.l, q.r, q.x);
        }), Q);
        print_row("kth_smallest", measure([&] {
            for (auto q : qs) sum += tr.kth_smallest(q.l, q.r, q.k);
        }), Q);
        print_row("prev_value", measure([&] {
            for (auto q : qs) sum += tr.prev_value(q.l, q.r, q.x) + 1;
        }), Q);
        print_row("next_value", measure([&] {
            for (auto q : qs) sum += tr.next_value(q.l, q.r, q.x) + 1;
        }), Q);
        print_row("select", measure([&] {
            for (auto q : qs) sum += tr.select(occ[q.pos], a[q.pos]);
        }), Q);
        print_row("insert + pop", measure([&] {
            for (int i = 0; i < U; ++i) {
                const auto& q = qs[i];
                tr.insert(q.pos, q.x);
                sum += tr.pop(q.pos);
            }
        }), U * 2);
        print_row("set", measure([&] {
            for (int i = 0; i < U; ++i) tr.set(qs[i].pos, qs[i].x);
        }), U);
    }

    {
        auto start = Clock::now();
        DynamicWaveletTreeSum<int, long long> tr(SIGMA, a, w);
        double build = chrono::duration<double>(Clock::now() - start).count();
        cout << "\nDynamicWaveletTreeSum\n";
        print_row("build", build, N);
        print_row("access", measure([&] {
            for (auto q : qs) sum += tr.access(q.pos);
        }), Q);
        print_row("access_pair", measure([&] {
            for (auto q : qs) {
                auto p = tr.access_pair(q.pos);
                sum += p.first + p.second;
            }
        }), Q);
        print_row("access_weight", measure([&] {
            for (auto q : qs) sum += tr.access_weight(q.pos);
        }), Q);
        print_row("rank", measure([&] {
            for (auto q : qs) sum += tr.rank(q.r, q.x);
        }), Q);
        print_row("range_count", measure([&] {
            for (auto q : qs) sum += tr.range_count(q.l, q.r, q.x);
        }), Q);
        print_row("range_freq", measure([&] {
            for (auto q : qs) sum += tr.range_freq(q.l, q.r, q.x);
        }), Q);
        print_row("kth_smallest", measure([&] {
            for (auto q : qs) sum += tr.kth_smallest(q.l, q.r, q.k);
        }), Q);
        print_row("range_sum", measure([&] {
            for (auto q : qs) sum += tr.range_sum(q.l, q.r);
        }), Q);
        print_row("count_sum_lt", measure([&] {
            for (auto q : qs) {
                auto p = tr.count_sum_lt(q.l, q.r, q.x);
                sum += p.first + p.second;
            }
        }), Q);
        print_row("sum_k_smallest", measure([&] {
            for (auto q : qs) sum += tr.sum_k_smallest(q.l, q.r, q.k);
        }), Q);
        print_row("select", measure([&] {
            for (auto q : qs) sum += tr.select(occ[q.pos], a[q.pos]);
        }), Q);
        print_row("insert + pop", measure([&] {
            for (int i = 0; i < U; ++i) {
                const auto& q = qs[i];
                tr.insert(q.pos, q.x, i + 1);
                auto p = tr.pop(q.pos);
                sum += p.first + p.second;
            }
        }), U * 2);
        print_row("set", measure([&] {
            for (int i = 0; i < U; ++i) tr.set(qs[i].pos, qs[i].x, i + 1);
        }), U);
        print_row("add_weight", measure([&] {
            for (int i = 0; i < U; ++i) tr.add_weight(qs[i].pos, 1);
        }), U);
        print_row("set_weight", measure([&] {
            for (int i = 0; i < U; ++i) tr.set_weight(qs[i].pos, i + 1);
        }), U);
    }

    cout << "\nchecksum " << sum << '\n';
}
