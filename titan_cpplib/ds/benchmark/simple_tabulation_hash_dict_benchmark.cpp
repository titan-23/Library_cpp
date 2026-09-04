/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/benchmark/simple_tabulation_hash_dict_benchmark.cpp
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "titan_cpplib/ds/hash_dict.cpp"
#include "titan_cpplib/ds/linear_hash_dict.cpp"
#include "titan_cpplib/ds/simple_tabulation_hash_dict.cpp"
#include "titan_cpplib/ds/universal_hash_dict.cpp"

using namespace std;
using namespace titan23;

using Clock = chrono::steady_clock;

#ifndef HASH_BENCH_N
#define HASH_BENCH_N 200000
#endif

#ifndef HASH_BENCH_Q
#define HASH_BENCH_Q 500000
#endif

#ifndef HASH_BENCH_REPS
#define HASH_BENCH_REPS 3
#endif

#ifndef HASH_BENCH_PATTERN
#define HASH_BENCH_PATTERN 0
#endif

struct Result {
    double ctor;
    double insert;
    double hit;
    double miss;
    double set;
    double mixed;
    double candidate;
    uint64_t sum;
};

uint64_t mix64(uint64_t x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

uint64_t reverse_bits(uint64_t x) {
    x = (x >> 32) | (x << 32);
    x = ((x & 0xffff0000ffff0000ULL) >> 16) | ((x & 0x0000ffff0000ffffULL) << 16);
    x = ((x & 0xff00ff00ff00ff00ULL) >> 8) | ((x & 0x00ff00ff00ff00ffULL) << 8);
    x = ((x & 0xf0f0f0f0f0f0f0f0ULL) >> 4) | ((x & 0x0f0f0f0f0f0f0f0fULL) << 4);
    x = ((x & 0xccccccccccccccccULL) >> 2) | ((x & 0x3333333333333333ULL) << 2);
    return ((x & 0xaaaaaaaaaaaaaaaaULL) >> 1) | ((x & 0x5555555555555555ULL) << 1);
}

uint64_t make_key(uint64_t x) {
    if constexpr (HASH_BENCH_PATTERN == 0) return mix64(x);
    if constexpr (HASH_BENCH_PATTERN == 1) return x;
    if constexpr (HASH_BENCH_PATTERN == 2) return x << 32;
    if constexpr (HASH_BENCH_PATTERN == 3) return reverse_bits(x);
    return x * 1000000007ULL;
}

template<class F>
double measure(F &&f) {
    auto start = Clock::now();
    f();
    return chrono::duration<double>(Clock::now() - start).count();
}

template<class Map>
Result run_once(const vector<uint64_t> &keys, const vector<uint64_t> &miss, const vector<int> &idx) {
    constexpr int N = HASH_BENCH_N;
    constexpr int Q = HASH_BENCH_Q;
    auto start = Clock::now();
    Map mp(N);
    double ctor = chrono::duration<double>(Clock::now() - start).count();
    uint64_t sum = 0;

    double insert = measure([&] {
        for (int i = 0; i < N; ++i) mp.set(keys[i], i + 1);
    });
    double hit = measure([&] {
        for (int i = 0; i < Q; ++i) sum += mp.get(keys[idx[i]]);
    });
    double miss_time = measure([&] {
        for (int i = 0; i < Q; ++i) sum += mp.get(miss[i], -1);
    });
    double set_time = measure([&] {
        for (int i = 0; i < Q; ++i) mp.set(keys[idx[i]], i + 1);
    });
    double mixed = measure([&] {
        for (int i = 0; i < Q; ++i) {
            int type = i % 5;
            if (type < 3) sum += mp.get(keys[idx[i]]);
            else if (type == 3) sum += mp.get(miss[i], -1);
            else mp.set(keys[idx[i]], i + 1);
        }
    });

    Map cand(N + Q);
    for (int i = 0; i < N; ++i) cand.set(keys[i], i + 1);
    double candidate = measure([&] {
        for (int i = 0; i < Q; ++i) {
            uint64_t key = (i & 3) == 0 ? keys[idx[i]] : miss[i];
            auto dat = cand.get_pos(key);
            int val = cand.inner_get(dat, -1);
            if (val == -1) cand.inner_set(dat, key, i + 1);
            else sum += val;
        }
    });
    sum += mp.get(keys[0]) + mp.len() + cand.len();
    return {ctor, insert, hit, miss_time, set_time, mixed, candidate, sum};
}

double median(vector<double> a) {
    nth_element(a.begin(), a.begin() + a.size() / 2, a.end());
    return a[a.size() / 2];
}

template<class Map>
uint64_t run(const string &name, const vector<uint64_t> &keys, const vector<uint64_t> &miss,
             const vector<int> &idx) {
    constexpr int N = HASH_BENCH_N;
    constexpr int Q = HASH_BENCH_Q;
    run_once<Map>(keys, miss, idx);
    vector<double> ctor, insert, hit, miss_time, set_time, mixed, candidate;
    uint64_t sum = 0;
    for (int rep = 0; rep < HASH_BENCH_REPS; ++rep) {
        Result res = run_once<Map>(keys, miss, idx);
        ctor.push_back(res.ctor);
        insert.push_back(res.insert);
        hit.push_back(res.hit);
        miss_time.push_back(res.miss);
        set_time.push_back(res.set);
        mixed.push_back(res.mixed);
        candidate.push_back(res.candidate);
        sum ^= res.sum;
    }

    cout << left << setw(18) << name << right;
    cout << setw(10) << median(ctor) * 1e6 << " us";
    cout << setw(10) << median(insert) * 1e9 / N;
    cout << setw(10) << median(hit) * 1e9 / Q;
    cout << setw(10) << median(miss_time) * 1e9 / Q;
    cout << setw(10) << median(set_time) * 1e9 / Q;
    cout << setw(10) << median(mixed) * 1e9 / Q;
    cout << setw(10) << median(candidate) * 1e9 / Q << '\n';
    return sum;
}

int main() {
    constexpr int N = HASH_BENCH_N;
    constexpr int Q = HASH_BENCH_Q;
    static_assert(N >= 1 && Q >= 1 && HASH_BENCH_REPS >= 1);

    vector<uint64_t> keys(N), miss(Q);
    for (int i = 0; i < N; ++i) keys[i] = make_key(uint64_t(i) + 1);
    for (int i = 0; i < Q; ++i) miss[i] = make_key(uint64_t(i) + N + 1);
    mt19937 rng(23);
    vector<int> idx(Q);
    for (int &v : idx) v = (int)(rng() % N);

    cout << fixed << setprecision(3);
    cout << "N=" << N << " Q=" << Q << " reps=" << HASH_BENCH_REPS;
    cout << " pattern=" << HASH_BENCH_PATTERN << '\n';
    cout << left << setw(18) << "name" << right << setw(13) << "ctor";
    cout << setw(10) << "insert" << setw(10) << "hit" << setw(10) << "miss";
    cout << setw(10) << "set" << setw(10) << "mixed" << setw(10) << "candidate" << '\n';
    uint64_t sum1 = run<HashDict<int, true>>("HashDict", keys, miss, idx);
    uint64_t sum2 = run<LinearHashDict<int, true>>("Linear", keys, miss, idx);
    uint64_t sum3 = run<SimpleTabulationHashDict<int>>("SimpleTab", keys, miss, idx);
    uint64_t sum4 = run<UniversalHashDict<int, true>>("Universal", keys, miss, idx);
    cout << "checksum " << sum1 << ' ' << sum2 << ' ' << sum3 << ' ' << sum4 << '\n';
}
