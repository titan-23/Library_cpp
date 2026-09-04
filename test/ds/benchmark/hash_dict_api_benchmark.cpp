/// https://github.com/titan-23/Library_cpp/blob/main/test/ds/benchmark/hash_dict_api_benchmark.cpp
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

#ifndef HASH_BENCH_N
#define HASH_BENCH_N 200000
#endif

#ifndef HASH_BENCH_Q
#define HASH_BENCH_Q HASH_BENCH_N
#endif

#ifndef HASH_BENCH_REPS
#define HASH_BENCH_REPS 3
#endif

#ifndef HASH_BENCH_PATTERN
#define HASH_BENCH_PATTERN 0
#endif

using Clock = chrono::steady_clock;
using u64 = uint64_t;

u64 mix64(u64 x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

u64 reverse_bits(u64 x) {
    x = (x >> 32) | (x << 32);
    x = ((x & 0xffff0000ffff0000ULL) >> 16) | ((x & 0x0000ffff0000ffffULL) << 16);
    x = ((x & 0xff00ff00ff00ff00ULL) >> 8) | ((x & 0x00ff00ff00ff00ffULL) << 8);
    x = ((x & 0xf0f0f0f0f0f0f0f0ULL) >> 4) | ((x & 0x0f0f0f0f0f0f0f0fULL) << 4);
    x = ((x & 0xccccccccccccccccULL) >> 2) | ((x & 0x3333333333333333ULL) << 2);
    return ((x & 0xaaaaaaaaaaaaaaaaULL) >> 1) | ((x & 0x5555555555555555ULL) << 1);
}

u64 make_key(u64 x) {
    if constexpr (HASH_BENCH_PATTERN == 0) return mix64(x);
    if constexpr (HASH_BENCH_PATTERN == 1) return x;
    if constexpr (HASH_BENCH_PATTERN == 2) return x << 32;
    if constexpr (HASH_BENCH_PATTERN == 3) return reverse_bits(x);
    if constexpr (HASH_BENCH_PATTERN == 4) return x * 1000000007ULL;
    return (x << 32) ^ (x * x);
}

template<class F>
double measure(F &&f) {
    auto start = Clock::now();
    f();
    return chrono::duration<double>(Clock::now() - start).count();
}

double median(vector<double> a) {
    nth_element(a.begin(), a.begin() + a.size() / 2, a.end());
    return a[a.size() / 2];
}

struct Result {
    double contains_hit;
    double contains_miss;
    double add_hit;
    double add_miss;
    double min_hit;
    double min_miss;
    double sub_hit;
    double sub_miss;
    u64 sum;
};

template<class Map>
Result run_once(const vector<u64> &keys, const vector<u64> &miss, const vector<int> &idx) {
    constexpr int N = HASH_BENCH_N;
    constexpr int Q = HASH_BENCH_Q;
    u64 sum = 0;
    Map mp(N);
    for (int i = 0; i < N; ++i) mp.set(keys[i], i + 1);

    double contains_hit = measure([&] {
        for (int i = 0; i < Q; ++i) sum += mp.contains(keys[idx[i]]);
    });
    double contains_miss = measure([&] {
        for (int i = 0; i < Q; ++i) sum += mp.contains(miss[i]);
    });
    double add_hit = measure([&] {
        for (int i = 0; i < Q; ++i) mp.add(keys[idx[i]], 1);
    });
    double min_hit = measure([&] {
        for (int i = 0; i < Q; ++i) sum += mp.contains_set(keys[idx[i]], -i - 1);
    });
    double sub_hit = measure([&] {
        for (int i = 0; i < Q; ++i) sum += mp[keys[idx[i]]];
    });

    Map add_mp(N + Q);
    for (int i = 0; i < N; ++i) add_mp.set(keys[i], i + 1);
    double add_miss = measure([&] {
        for (int i = 0; i < Q; ++i) add_mp.add(miss[i], 1);
    });
    Map min_mp(N + Q);
    for (int i = 0; i < N; ++i) min_mp.set(keys[i], i + 1);
    double min_miss = measure([&] {
        for (int i = 0; i < Q; ++i) sum += min_mp.contains_set(miss[i], i + 1);
    });
    Map sub_mp(N + Q);
    for (int i = 0; i < N; ++i) sub_mp.set(keys[i], i + 1);
    double sub_miss = measure([&] {
        for (int i = 0; i < Q; ++i) sum += sub_mp[miss[i]];
    });
    sum += mp.len() + add_mp.len() + min_mp.len() + sub_mp.len();
    return {contains_hit, contains_miss, add_hit, add_miss, min_hit, min_miss, sub_hit, sub_miss, sum};
}

template<class Map>
u64 run(const string &name, const vector<u64> &keys, const vector<u64> &miss, const vector<int> &idx) {
    constexpr int Q = HASH_BENCH_Q;
    run_once<Map>(keys, miss, idx);
    vector<double> contains_hit, contains_miss, add_hit, add_miss;
    vector<double> min_hit, min_miss, sub_hit, sub_miss;
    u64 sum = 0;
    for (int rep = 0; rep < HASH_BENCH_REPS; ++rep) {
        Result res = run_once<Map>(keys, miss, idx);
        contains_hit.push_back(res.contains_hit);
        contains_miss.push_back(res.contains_miss);
        add_hit.push_back(res.add_hit);
        add_miss.push_back(res.add_miss);
        min_hit.push_back(res.min_hit);
        min_miss.push_back(res.min_miss);
        sub_hit.push_back(res.sub_hit);
        sub_miss.push_back(res.sub_miss);
        sum ^= res.sum;
    }
    auto ns = [=](vector<double> &a) { return median(a) * 1e9 / Q; };
    cout << left << setw(12) << name << right << setw(9) << ns(contains_hit) << setw(9) << ns(contains_miss);
    cout << setw(9) << ns(add_hit) << setw(9) << ns(add_miss) << setw(9) << ns(min_hit) << setw(9) << ns(min_miss);
    cout << setw(9) << ns(sub_hit) << setw(9) << ns(sub_miss) << '\n';
    return sum;
}

int main() {
    constexpr int N = HASH_BENCH_N;
    constexpr int Q = HASH_BENCH_Q;
    vector<u64> keys(N), miss(Q);
    for (int i = 0; i < N; ++i) keys[i] = make_key(u64(i) + 1);
    for (int i = 0; i < Q; ++i) miss[i] = make_key(u64(i) + N + 1);
    mt19937 rng(23);
    vector<int> idx(Q);
    for (int &v : idx) v = int(rng() % N);

    cout << fixed << setprecision(2);
    cout << "N=" << N << " Q=" << Q << " reps=" << HASH_BENCH_REPS << " pattern=" << HASH_BENCH_PATTERN << '\n';
    cout << left << setw(12) << "name" << right << setw(9) << "con_hit" << setw(9) << "con_miss";
    cout << setw(9) << "add_hit" << setw(9) << "add_miss" << setw(9) << "min_hit" << setw(9) << "min_miss";
    cout << setw(9) << "sub_hit" << setw(9) << "sub_miss" << '\n';
    u64 sum = run<HashDict<int>>("HashDict", keys, miss, idx);
    sum ^= run<LinearHashDict<int>>("Linear", keys, miss, idx);
    sum ^= run<SimpleTabulationHashDict<int>>("SimpleTab", keys, miss, idx);
    sum ^= run<UniversalHashDict<int>>("Universal", keys, miss, idx);
    cout << "checksum " << sum << '\n';
}
