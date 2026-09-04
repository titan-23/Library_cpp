/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/geometry/benchmark/manhattan_nearest_benchmark.cpp
#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>
#include "titan_cpplib/geometry/manhattan_nearest_neighbor.cpp"

using namespace std;
using namespace titan23;

using Clock = chrono::steady_clock;
using Tree = ManhattanNearest<long long>;

#ifndef MANHATTAN_BENCH_N
#define MANHATTAN_BENCH_N 200000
#endif

#ifndef MANHATTAN_BENCH_Q
#define MANHATTAN_BENCH_Q 200000
#endif

template<class F>
double measure(F&& f) {
    auto start = Clock::now();
    f();
    auto end = Clock::now();
    return chrono::duration<double>(end - start).count();
}

int main() {
    constexpr int N = MANHATTAN_BENCH_N;
    constexpr int Q = MANHATTAN_BENCH_Q;
    mt19937_64 rng(23);

    vector<Tree::Point> ps;
    ps.reserve(N);
    for (int i = 0; i < N; ++i) {
        long long x = static_cast<long long>(rng() % 2000000001ULL) - 1000000000LL;
        long long y = static_cast<long long>(rng() % 2000000001ULL) - 1000000000LL;
        ps.push_back({x, y});
    }

    vector<array<long long, 2>> qs(Q);
    for (auto& q : qs) {
        q[0] = static_cast<long long>(rng() % 2000000001ULL) - 1000000000LL;
        q[1] = static_cast<long long>(rng() % 2000000001ULL) - 1000000000LL;
    }

    double build_sec = 0;
    Tree tree;
    build_sec = measure([&] { tree = Tree(move(ps)); });
    double add_sec = measure([&] { tree.add_all(); });

    uint64_t checksum = 0;
    double nearest_sec = measure([&] {
        for (auto [x, y] : qs) checksum += static_cast<uint64_t>(tree.nearest(x, y) + 1);
    });
    double dist_sec = measure([&] {
        for (auto [x, y] : qs) checksum += static_cast<uint64_t>(tree.nearest_dist(x, y) + 1);
    });
    double four_sec = measure([&] {
        for (auto [x, y] : qs) {
            auto ids = tree.nearest_four(x, y);
            for (int id : ids) checksum += static_cast<uint64_t>(id + 1);
        }
    });
    tree.remove(0);
    double dynamic_nearest_sec = measure([&] {
        for (auto [x, y] : qs) checksum += static_cast<uint64_t>(tree.nearest(x, y) + 1);
    });
    double dynamic_dist_sec = measure([&] {
        for (auto [x, y] : qs) checksum += static_cast<uint64_t>(tree.nearest_dist(x, y) + 1);
    });
    double dynamic_four_sec = measure([&] {
        for (auto [x, y] : qs) {
            auto ids = tree.nearest_four(x, y);
            for (int id : ids) checksum += static_cast<uint64_t>(id + 1);
        }
    });
    tree.add(0);
    double remove_sec = measure([&] {
        for (int i = 0; i < N; ++i) tree.remove(i);
    });
    double readd_sec = measure([&] {
        for (int i = 0; i < N; ++i) tree.add(i);
    });

    cout << fixed << setprecision(6);
    cout << "N=" << N << " Q=" << Q << '\n';
    cout << "build        " << build_sec << " s\n";
    cout << "add_all      " << add_sec << " s\n";
    cout << "nearest Q    " << nearest_sec << " s\n";
    cout << "distance Q   " << dist_sec << " s\n";
    cout << "nearest4 Q   " << four_sec << " s\n";
    cout << "nearest dyn  " << dynamic_nearest_sec << " s\n";
    cout << "distance dyn " << dynamic_dist_sec << " s\n";
    cout << "nearest4 dyn " << dynamic_four_sec << " s\n";
    cout << "remove N     " << remove_sec << " s\n";
    cout << "re-add N     " << readd_sec << " s\n";
    cout << "checksum     " << checksum << '\n';
}
