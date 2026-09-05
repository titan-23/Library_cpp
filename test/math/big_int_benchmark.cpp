/// https://github.com/titan-23/Library_cpp/blob/main/test/math/big_int_benchmark.cpp
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "titan_cpplib/math/big_int.cpp"

using namespace std;
using namespace titan23;

#ifndef BIG_INT_BENCH_MS
#define BIG_INT_BENCH_MS 20.0
#endif

#ifndef BIG_INT_BENCH_ROUNDS
#define BIG_INT_BENCH_ROUNDS 5
#endif

#ifndef BIG_INT_BENCH_REPS
#define BIG_INT_BENCH_REPS 8192
#endif

using Clock = chrono::steady_clock;
using u64 = uint64_t;

struct Case {
    string tag;
    size_t n;
    size_t m;
    bool sq;
};

string make_num(size_t n, u64 salt, size_t id) {
    mt19937_64 rng(0x243f6a8885a308d3ULL ^ (n * 0x9e3779b97f4a7c15ULL) ^ salt ^ id);
    string s = to_string(1 + rng() % 99999999);
    for (size_t i = 1; i < n; ++i) {
        string t = to_string(rng() % 100000000);
        s.append(8 - t.size(), '0');
        s += t;
    }
    return s;
}

u64 eat(u64 h, const BigInt &x) {
    const string s = x.to_string();
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h ^ s.size();
}

u64 run(const Case &c) {
    constexpr size_t P = 8;
    vector<BigInt> a;
    vector<BigInt> b;
    a.reserve(P);
    b.reserve(P);
    for (size_t i = 0; i < P; ++i) {
        a.emplace_back(make_num(c.n, 0, i));
        if (!c.sq) b.emplace_back(make_num(c.m, c.n == c.m ? 0x94d049bb133111ebULL : 0, i));
    }

    auto mul = [&](size_t i) {
        if (c.sq) return a[i % P] * a[i % P];
        return a[i % P] * b[i % P];
    };

    u64 h = 1469598103934665603ULL;
    BigInt z = mul(0);
    h = eat(h, z);
    const auto t0 = Clock::now();
    z = mul(1);
    const double one = chrono::duration<double, milli>(Clock::now() - t0).count();
    h = eat(h, z);

    size_t reps = static_cast<size_t>(BIG_INT_BENCH_MS / max(one, 0.001)) + 1;
    reps = min(reps, size_t(BIG_INT_BENCH_REPS));
    const size_t bytes = max<size_t>(64, (c.n + c.m) * sizeof(uint32_t) + sizeof(BigInt));
    reps = min(reps, max<size_t>(1, (size_t{4} << 20) / bytes));

    vector<BigInt> out(reps);
    vector<double> ts;
    ts.reserve(BIG_INT_BENCH_ROUNDS);
    for (int r = 0; r < BIG_INT_BENCH_ROUNDS; ++r) {
        const auto st = Clock::now();
        for (size_t i = 0; i < reps; ++i) out[i] = mul(i);
        const double ms = chrono::duration<double, milli>(Clock::now() - st).count();
        ts.push_back(ms);
        for (const BigInt &x : out) h = eat(h, x);
    }
    sort(ts.begin(), ts.end());
    const double ms = ts[ts.size() / 2];
    cout << left << setw(5) << c.tag << right << setw(7) << c.n << setw(7) << c.m;
    cout << setw(8) << reps << setw(13) << fixed << setprecision(3) << ms;
    cout << setw(13) << fixed << setprecision(3) << ms * 1000 / static_cast<double>(reps) << ' ' << hex << h << dec
         << '\n';
    return h;
}

int main() {
    vector<Case> cs;
    const vector<size_t> ns = {
        31,  32,  33,  35,  36,  54,  55,  63,   64,   65,   75,   76,   90,   91,   127,  128,  129,  153,  154,  255,
        256, 257, 263, 264, 511, 512, 513, 1023, 1024, 1025, 1280, 2047, 2048, 2049, 4095, 4096, 4097, 8191, 8192, 8193,
    };
    for (size_t n : ns) {
        cs.push_back({"mul", n, n, false});
        cs.push_back({"sqr", n, n, true});
    }

    const vector<pair<size_t, size_t>> ps = {
        {39, 72},    {1, 4096},    {4096, 1},    {32, 4096},   {4096, 32}, {63, 4096},  {4096, 63},
        {64, 4096},  {4096, 64},   {100, 399},   {100, 400},   {100, 401}, {128, 512},  {128, 513},
        {200, 799},  {200, 800},   {200, 801},   {128, 384},   {256, 768}, {256, 1024}, {256, 1025},
        {512, 4096}, {1024, 3072}, {1024, 4096}, {1024, 5120},
    };
    for (auto [n, m] : ps) cs.push_back({"asym", n, m, false});

    cout << left << setw(5) << "kind" << right << setw(7) << "lhs" << setw(7) << "rhs";
    cout << setw(8) << "reps" << setw(13) << "median-ms" << setw(13) << "us/op"
         << " checksum\n";
    u64 h = 0;
    for (const Case &c : cs) h ^= run(c) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    cout << "final-checksum " << hex << h << dec << '\n';
}
