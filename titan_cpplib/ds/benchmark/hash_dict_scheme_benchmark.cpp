/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/benchmark/hash_dict_scheme_benchmark.cpp
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "titan_cpplib/ds/hash_dict.cpp"
#include "titan_cpplib/ds/hopscotch_hash_dict.cpp"
#include "titan_cpplib/ds/linear_hash_dict.cpp"

using namespace std;
using namespace titan23;

using Clock = chrono::steady_clock;

#ifndef HASH_BENCH_N
#define HASH_BENCH_N 200000
#endif

#ifndef HASH_BENCH_Q
#define HASH_BENCH_Q 500000
#endif

uint64_t mix64(uint64_t x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

template <class F>
double measure(F &&f) {
    auto start = Clock::now();
    f();
    return chrono::duration<double>(Clock::now() - start).count();
}

void print_row(const string &name, double sec, int ops) {
    cout << left << setw(20) << name << right << setw(10) << sec << " s  ";
    cout << setw(9) << sec * 1e9 / ops << " ns/op\n";
}

template <template<class, bool> class Map>
uint64_t run(const string &name, const vector<uint64_t> &keys, const vector<uint64_t> &miss,
             const vector<int> &idx) {
    const int n = HASH_BENCH_N;
    const int q = HASH_BENCH_Q;
    Map<int, false> mp(n);
    uint64_t sum = 0;

    cout << '\n' << name << " capacity=" << mp.inner_len() << '\n';
    print_row("insert", measure([&] {
        for (int i = 0; i < n; ++i) mp.set(keys[i], i + 1);
    }), n);
    print_row("get hit", measure([&] {
        for (int i = 0; i < q; ++i) sum += mp.get(keys[idx[i]]);
    }), q);
    print_row("get miss", measure([&] {
        for (int i = 0; i < q; ++i) sum += mp.get(miss[i], -1);
    }), q);
    print_row("get_pos hit", measure([&] {
        for (int i = 0; i < q; ++i) {
            auto dat = mp.get_pos(keys[idx[i]]);
            sum += mp.inner_get(dat, -1);
        }
    }), q);
    print_row("set hit", measure([&] {
        for (int i = 0; i < q; ++i) mp.set(keys[idx[i]], i + 1);
    }), q);

    Map<int, false> cand(n + q);
    for (int i = 0; i < n; ++i) cand.set(keys[i], i + 1);
    cout << name << " candidate capacity=" << cand.inner_len() << '\n';
    print_row("75% miss + insert", measure([&] {
        for (int i = 0; i < q; ++i) {
            uint64_t key = (i & 3) == 0 ? keys[idx[i]] : miss[i];
            auto dat = cand.get_pos(key);
            int val = cand.inner_get(dat, -1);
            if (val == -1) cand.inner_set(dat, key, i + 1);
            else sum += val;
        }
    }), q);

    print_row("clear", measure([&] {
        mp.clear();
    }), 1);
    sum += mp.len() + cand.len();
    return sum;
}

int main() {
    constexpr int N = HASH_BENCH_N;
    constexpr int Q = HASH_BENCH_Q;
    static_assert(N >= 1 && Q >= 1);

    vector<uint64_t> keys(N), miss(Q);
    for (int i = 0; i < N; ++i) keys[i] = mix64(i + 1);
    for (int i = 0; i < Q; ++i) miss[i] = mix64(uint64_t(i) + (uint64_t(1) << 40));
    mt19937 rng(23);
    vector<int> idx(Q);
    for (int &v : idx) v = (int)(rng() % N);

    cout << fixed << setprecision(6);
    cout << "N=" << N << " Q=" << Q << " premixed uint64_t keys\n";
    uint64_t sum1 = run<HashDict>("Swiss", keys, miss, idx);
    uint64_t sum2 = run<HopscotchHashDict>("Hopscotch", keys, miss, idx);
    uint64_t sum3 = run<LinearHashDict>("Linear", keys, miss, idx);
    cout << "\nchecksum " << sum1 << ' ' << sum2 << ' ' << sum3 << '\n';
}
