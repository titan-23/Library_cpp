// Example: g++ -std=c++20 -O2 -DNDEBUG -march=native -I. this_file.cpp -o /tmp/hash_set_bench
// /tmp/hash_set_bench --n 300000
// 鍵は rnd (乱数) と seq (0, 1, 2, ...) の 2 種類。
// insert_grow: 容量を指定せずに n 個挿入する。insert_reserved: 容量 n を指定して n 個挿入する。
// contains_hit / contains_miss: n 要素の集合に q 回問い合わせる。contains_insert_half: 半分が既出の鍵の contains_insert。
// clear_cycle: clear と n 回の contains_insert を繰り返す。small_sets: 8 要素の集合を作って捨てることを繰り返す。
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include "titan_cpplib/ds/hash_set.cpp"

namespace {

using Clock = std::chrono::steady_clock;
using u64 = unsigned long long;

struct Options {
    int n = 300000, q = 2000000;
    uint64_t seed = 23;
};

Options parse_options(int argc, char **argv) {
    Options o;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string flag = argv[i];
        if (flag == "--n") o.n = std::stoi(argv[i + 1]);
        else if (flag == "--q") o.q = std::stoi(argv[i + 1]);
        else if (flag == "--seed") o.seed = std::stoull(argv[i + 1]);
        else throw std::invalid_argument("unknown option: " + flag);
    }
    return o;
}

u64 mix64(u64 x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

template<class F>
double measure(F &&f) {
    const auto start = Clock::now();
    f();
    return std::chrono::duration<double>(Clock::now() - start).count();
}

void print_row(const std::string &name, const double seconds, const int64_t count, const uint64_t checksum) {
    std::cout << std::left << std::setw(28) << name << std::right << " seconds=" << seconds
              << " ns/op=" << seconds * 1e9 / count << " ops=" << count << " checksum=" << checksum << '\n';
}

void run(const Options &o, const bool sequential) {
    const int n = o.n, q = o.q;
    const std::string tag = sequential ? "seq." : "rnd.";
    auto key_of = [&](const u64 i) { return sequential ? i : mix64(i + 1); };
    std::vector<u64> keys(n), miss(q);
    for (int i = 0; i < n; ++i) keys[i] = key_of(i);
    for (int i = 0; i < q; ++i) miss[i] = key_of(u64(n) + i);
    std::mt19937_64 rng(o.seed);
    std::vector<int> idx(q);
    for (int &v : idx) v = rng() % n;
    const int64_t reps = std::max<int64_t>(1, 4000000 / n);

    uint64_t checksum = 0;
    double seconds = measure([&] {
        for (int64_t r = 0; r < reps; ++r) {
            titan23::HashSet st;
            for (int i = 0; i < n; ++i) st.insert(keys[i]);
            checksum += st.len();
        }
    });
    print_row(tag + "insert_grow", seconds, reps * n, checksum);

    titan23::HashSet st(n);
    seconds = measure([&] {
        for (int i = 0; i < n; ++i) st.insert(keys[i]);
    });
    print_row(tag + "insert_reserved", seconds, n, st.len());

    checksum = 0;
    seconds = measure([&] {
        for (int i = 0; i < q; ++i) checksum += st.contains(keys[idx[i]]);
    });
    print_row(tag + "contains_hit", seconds, q, checksum);

    checksum = 0;
    seconds = measure([&] {
        for (int i = 0; i < q; ++i) checksum += st.contains(miss[i]);
    });
    print_row(tag + "contains_miss", seconds, q, checksum);

    {
        titan23::HashSet c(n);
        checksum = 0;
        seconds = measure([&] {
            for (int i = 0; i < n; ++i) {
                const u64 key = (i & 1) ? keys[u64(i) * 0x9E3779B1ULL % u64(i + 1)] : keys[i];
                checksum += c.contains_insert(key);
            }
        });
        print_row(tag + "contains_insert_half", seconds, n, checksum + c.len());
    }

    {
        titan23::HashSet c(n);
        checksum = 0;
        seconds = measure([&] {
            for (int64_t r = 0; r < reps; ++r) {
                c.clear();
                for (int i = 0; i < n; ++i) checksum += c.contains_insert(key_of(u64(r) * 1000003 + i % (n * 3 / 4 + 1)));
            }
        });
        print_row(tag + "clear_cycle", seconds, reps * n, checksum);
    }
}

void run_small_sets(const Options &o) {
    const int sets = 200000;
    uint64_t checksum = 0;
    const double seconds = measure([&] {
        for (int s = 0; s < sets; ++s) {
            titan23::HashSet st;
            for (int i = 0; i < 8; ++i) st.insert(mix64(u64(s) * 16 + i + o.seed));
            for (int i = 0; i < 16; ++i) checksum += st.contains(mix64(u64(s) * 16 + i + o.seed));
        }
    });
    print_row("small_sets", seconds, sets, checksum);
}

}  // namespace

int main(int argc, char **argv) {
    const Options o = parse_options(argc, argv);
    std::cout << std::fixed << std::setprecision(6) << "N=" << o.n << " Q=" << o.q << " seed=" << o.seed << '\n';
    run(o, false);
    run(o, true);
    run_small_sets(o);
    return 0;
}
