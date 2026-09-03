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

#ifndef DWT_MIX_N
#define DWT_MIX_N 500000
#endif

#ifndef DWT_MIX_Q
#define DWT_MIX_Q 500000
#endif

#ifndef DWT_MIX_LOG
#define DWT_MIX_LOG 20
#endif

struct Query {
    int l, r, k, pos, x, weight;
};

template<class F>
double measure(F&& f) {
    auto start = Clock::now();
    f();
    return chrono::duration<double>(Clock::now() - start).count();
}

void run_plain(const vector<int>& a, const vector<Query>& qs, const int kinds) {
    constexpr int SIGMA = 1 << DWT_MIX_LOG;
    auto start = Clock::now();
    DynamicWaveletTree<int> tr(SIGMA, a);
    double build = chrono::duration<double>(Clock::now() - start).count();
    uint64_t sum = 0;
    double query = measure([&] {
        for (int i = 0; i < DWT_MIX_Q; ++i) {
            const Query& q = qs[i];
            if ((i & 1) == 0) {
                tr.set(q.pos, q.x);
            } else if (kinds == 2 || (i & 2) != 0) {
                sum += tr.kth_smallest(q.l, q.r, q.k);
            } else {
                sum += tr.range_freq(q.l, q.r, q.x);
            }
        }
    });
    cout << "tree mix=" << kinds << " build=" << build << " query=" << query;
    cout << " total=" << build + query << " checksum=" << sum << '\n';
}

void run_sum(const vector<int>& a, const vector<long long>& w, const vector<Query>& qs, const int kinds) {
    constexpr int SIGMA = 1 << DWT_MIX_LOG;
    auto start = Clock::now();
    DynamicWaveletTreeSum<int, long long> tr(SIGMA, a, w);
    double build = chrono::duration<double>(Clock::now() - start).count();
    uint64_t sum = 0;
    double query = measure([&] {
        for (int i = 0; i < DWT_MIX_Q; ++i) {
            const Query& q = qs[i];
            if ((i & 1) == 0) {
                tr.set(q.pos, q.x, q.weight);
            } else if (kinds == 2 || (i & 2) == 0) {
                auto res = tr.count_sum_lt(q.l, q.r, q.x);
                sum += res.first + res.second;
            } else {
                sum += tr.sum_k_smallest(q.l, q.r, q.k);
            }
        }
    });
    cout << "sum  mix=" << kinds << " build=" << build << " query=" << query;
    cout << " total=" << build + query << " checksum=" << sum << '\n';
}

int main(int argc, char** argv) {
    constexpr int N = DWT_MIX_N;
    constexpr int Q = DWT_MIX_Q;
    constexpr int SIGMA = 1 << DWT_MIX_LOG;
    mt19937 rng(23);
    vector<int> a(N);
    vector<long long> w(N);
    for (int i = 0; i < N; ++i) {
        a[i] = rng() % SIGMA;
        w[i] = rng() % 100 + 1;
    }
    vector<Query> qs(Q);
    for (Query& q : qs) {
        q.l = rng() % N;
        q.r = rng() % N;
        if (q.l > q.r) swap(q.l, q.r);
        ++q.r;
        q.k = rng() % (q.r - q.l);
        q.pos = rng() % N;
        q.x = rng() % SIGMA;
        q.weight = rng() % 100 + 1;
    }

    string mode = argc > 1 ? argv[1] : "tree3";
    cout << fixed << setprecision(6);
    cout << "N=" << N << " Q=" << Q << " sigma=" << SIGMA << '\n';
    if (mode == "tree2") run_plain(a, qs, 2);
    else if (mode == "tree3") run_plain(a, qs, 3);
    else if (mode == "sum2") run_sum(a, w, qs, 2);
    else if (mode == "sum3") run_sum(a, w, qs, 3);
    else return 1;
}
