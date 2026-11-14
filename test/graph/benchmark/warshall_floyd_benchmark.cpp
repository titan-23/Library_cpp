// Example: g++ -std=c++17 -O2 -DNDEBUG -march=native -I. this_file.cpp -o /tmp/wf_bench
// /tmp/wf_bench --n 600 --q 600
// build: 密なランダムグラフの構築。build_neg: 負閉路を多く含むグラフの構築。add_edge: 構築後に q 本の辺を追加する。
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <tuple>
#include <vector>
#include "titan_cpplib/graph/warshall_floyd.cpp"
#include "titan_cpplib/graph/warshall_floyd_path.cpp"
#include "titan_cpplib/graph/warshall_floyd_simd.cpp"
#include "titan_cpplib/graph/warshall_floyd_simd_nonnegative.cpp"

namespace {

using Clock = std::chrono::steady_clock;
using Edge = std::tuple<int, int, long long>;

struct Options {
    int n = 600, q = 600;
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

// 各頂点から out_degree 本の辺を張る。負の辺は negative_percent % の割合で混ぜる
std::vector<Edge> random_edges(std::mt19937_64 &rng, const int n, const int out_degree, const int negative_percent) {
    std::vector<Edge> edges;
    for (int u = 0; u < n; ++u) {
        for (int d = 0; d < out_degree; ++d) {
            const int v = rng() % n;
            long long w = 1 + rng() % 1000;
            if (int(rng() % 100) < negative_percent) w = -w;
            edges.emplace_back(u, v, w);
        }
    }
    return edges;
}

template<class T>
std::vector<std::vector<std::pair<int, T>>> to_graph(const int n, const std::vector<Edge> &edges) {
    std::vector<std::vector<std::pair<int, T>>> graph(n);
    for (const auto &[u, v, w] : edges) graph[u].emplace_back(v, T(w));
    return graph;
}

template<class F>
double measure(F &&f) {
    const auto start = Clock::now();
    f();
    return std::chrono::duration<double>(Clock::now() - start).count();
}

void print_row(const std::string &name, const double seconds, const int64_t count, const uint64_t checksum) {
    std::cout << std::left << std::setw(36) << name << std::right << " seconds=" << seconds
              << " ns/op=" << seconds * 1e9 / count << " ops=" << count << " checksum=" << checksum << '\n';
}

// 到達可能性と、負閉路を経由しない距離をまとめたチェックサム
template<class WF>
uint64_t general_checksum(const WF &wf, const int n) {
    uint64_t h = 0;
    for (int s = 0; s < n; s += 7) {
        for (int t = 0; t < n; t += 5) {
            uint64_t x = wf.is_neg_inf(s, t) ? 1 : wf.reachable(s, t) ? 2 + uint64_t(int64_t(wf.get_dist(s, t))) : 0;
            h = h * 1000003 + x;
        }
    }
    return h;
}

template<class WF>
uint64_t nonnegative_checksum(const WF &wf, const int n) {
    uint64_t h = 0;
    for (int s = 0; s < n; s += 7) {
        for (int t = 0; t < n; t += 5) h = h * 1000003 + uint64_t(int64_t(wf.get_dist(s, t)));
    }
    return h;
}

template<class T>
void run_type(const Options &o, const std::string &type) {
    std::mt19937_64 rng(o.seed);
    const int n = o.n;
    const std::vector<Edge> edges = random_edges(rng, n, 8, 0);
    const std::vector<Edge> negative_edges = random_edges(rng, n, 8, 30);
    const std::vector<Edge> nonnegative_edges = random_edges(rng, n, 8, 0);
    std::vector<Edge> additions;
    for (int i = 0; i < o.q; ++i) {
        const int u = rng() % n, v = rng() % n;
        additions.emplace_back(u, v, 1 + rng() % 200);
    }
    const T inf = std::numeric_limits<T>::max() / 2;
    const int64_t cubic = int64_t(n) * n * n;

    auto general = [&](const std::string &name, auto make) {
        uint64_t checksum = 0;
        double seconds = measure([&] { checksum = general_checksum(make(edges), n); });
        print_row(name + "<" + type + ">.build", seconds, cubic, checksum);
        seconds = measure([&] { checksum = general_checksum(make(negative_edges), n); });
        print_row(name + "<" + type + ">.build_neg", seconds, cubic, checksum);
        auto wf = make(edges);
        seconds = measure([&] {
            for (const auto &[u, v, w] : additions) wf.add_edge(u, v, T(w));
        });
        print_row(name + "<" + type + ">.add_edge", seconds, o.q, general_checksum(wf, n));
    };
    general("WarshallFloyd", [&](const std::vector<Edge> &e) { return titan23::WarshallFloyd<T>(to_graph<T>(n, e), inf); });
    general("WarshallFloydSIMD", [&](const std::vector<Edge> &e) { return titan23::WarshallFloydSIMD<T>(to_graph<T>(n, e)); });
    general("WarshallFloydPath", [&](const std::vector<Edge> &e) { return titan23::WarshallFloydPath<T>(to_graph<T>(n, e), inf); });

    uint64_t checksum = 0;
    double seconds = measure([&] {
        checksum = nonnegative_checksum(titan23::WarshallFloydSIMDNonnegative<T>(to_graph<T>(n, nonnegative_edges)), n);
    });
    print_row("WarshallFloydSIMDNonnegative<" + type + ">.build", seconds, cubic, checksum);
    titan23::WarshallFloydSIMDNonnegative<T> wf(to_graph<T>(n, nonnegative_edges));
    seconds = measure([&] {
        for (const auto &[u, v, w] : additions) wf.add_edge(u, v, T(w));
    });
    print_row("WarshallFloydSIMDNonnegative<" + type + ">.add_edge", seconds, o.q, nonnegative_checksum(wf, n));
}

}  // namespace

int main(int argc, char **argv) {
    const Options o = parse_options(argc, argv);
    std::cout << std::fixed << std::setprecision(6) << "N=" << o.n << " Q=" << o.q << " seed=" << o.seed << '\n';
    run_type<int>(o, "int");
    run_type<long long>(o, "ll");
    return 0;
}
