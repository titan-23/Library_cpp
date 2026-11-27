#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string_view>
#include <vector>
#include "titan_cpplib/graph/minimum_steiner_tree.cpp"

namespace {
using Weight = long long;
using Graph = std::vector<std::vector<std::pair<int, Weight>>>;
using Solver = titan23::MinimumSteinerTree<Weight>;
using Method = titan23::SteinerTreeMethod;
using Clock = std::chrono::steady_clock;
constexpr Weight inf = 1LL << 60;
constexpr int updates = 60;
constexpr int max_queries = 4;

// weight が 0 なら削除。生成するグラフの重みは正とする。
struct Update {
    int u, v;
    Weight weight;
    std::array<std::vector<int>, max_queries> queries;
};

struct Workload {
    Graph initial;
    std::vector<Update> trace;
};

Workload generate(int n, int m, int k, unsigned seed) {
    Workload work{Graph(n), {}};
    std::mt19937 rng(seed);
    std::vector<std::vector<bool>> present(n, std::vector<bool>(n));
    auto random_weight = [&]() -> Weight { return 1 + rng() % 100000000; };
    auto insert = [&](int u, int v) {
        Weight w = random_weight();
        work.initial[u].emplace_back(v, w);
        work.initial[v].emplace_back(u, w);
        present[u][v] = true;
    };
    // 連結性を保つパス以外から、追加・削除・重み変更の対象を一様に選ぶ。
    auto choose_pair = [&](bool exists) {
        int u, v;
        do {
            u = rng() % n;
            v = rng() % n;
            if (v < u) std::swap(u, v);
        } while (v - u < 2 || present[u][v] != exists);
        return std::pair{u, v};
    };
    for (int v = 1; v < n; ++v) insert(v - 1, v);
    for (int e = n - 1; e < m; ++e) {
        auto [u, v] = choose_pair(false);
        insert(u, v);
    }
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);
    for (int step = 0; step < updates; ++step) {
        bool adding = step % 3 == 0, removing = step % 3 == 2;
        auto [u, v] = choose_pair(!adding);
        Update update{u, v, removing ? 0 : random_weight(), {}};
        present[u][v] = !removing;
        for (auto& terminals : update.queries) {
            std::shuffle(order.begin(), order.end(), rng);
            terminals.assign(order.begin(), order.begin() + k);
        }
        work.trace.push_back(std::move(update));
    }
    return work;
}

void edit_adjacency(Graph& graph, int u, int v, Weight w) {
    auto& row = graph[u];
    auto it = std::find_if(row.begin(), row.end(), [v](auto edge) { return edge.first == v; });
    if (w == 0) {
        *it = row.back();
        row.pop_back();
    } else if (it == row.end()) {
        row.emplace_back(v, w);
    } else {
        it->second = w;
    }
}

struct Sample {
    double update_us = 0, solve_us = 0;
    std::uint64_t cost_sum = 0;
};

double microseconds(Clock::duration elapsed) {
    return std::chrono::duration<double, std::micro>(elapsed).count();
}

Sample measure(const Workload& work, Method method, int queries_per_update, bool rebuild) {
    Graph graph = work.initial;
    Solver solver(graph, inf);
    Sample sample;
    for (const auto& update : work.trace) {
        auto begin = Clock::now();
        if (rebuild) {
            edit_adjacency(graph, update.u, update.v, update.weight);
            edit_adjacency(graph, update.v, update.u, update.weight);
            solver = Solver(graph, inf);
        } else if (update.weight == 0) {
            solver.remove_edge(update.u, update.v);
        } else {
            solver.set_edge(update.u, update.v, update.weight);
        }
        auto edited = Clock::now();
        for (int i = 0; i < queries_per_update; ++i) {
            sample.cost_sum += solver.solve(update.queries[i], method).total_cost;
        }
        auto end = Clock::now();
        sample.update_us += microseconds(edited - begin);
        sample.solve_us += microseconds(end - edited);
    }
    sample.update_us /= work.trace.size();
    sample.solve_us /= work.trace.size() * queries_per_update;
    return sample;
}

enum class Weights { random, unit, manhattan };

void measure_cold(const std::vector<std::array<int, 3>>& sizes, Weights weights) {
    std::cout << "seed,n,m,k,setup_us,cold_us,cost_sum\n";
    for (auto [n, m, k] : sizes) {
        for (unsigned seed = 1; seed <= 5; ++seed) {
            auto work = generate(n, m, k, seed);
            if (weights != Weights::random) {
                std::mt19937 rng(seed);
                std::vector<std::pair<int,int>> points(n);
                for (auto& [x,y] : points) { x = rng() % 10000; y = rng() % 10000; }
                for (int u = 0; u < n; ++u) for (auto& [v,w] : work.initial[u]) {
                    w = weights == Weights::unit ? 1 : std::abs(points[u].first - points[v].first)
                                                    + std::abs(points[u].second - points[v].second);
                }
            }
            double setup = 0, cold = 0;
            std::uint64_t sum = 0;
            for (int i = 0; i < 5; ++i) {
                auto start = Clock::now();
                Solver solver(work.initial, inf);
                auto built = Clock::now();
                sum += solver.solve(work.trace[i].queries[0]).total_cost;
                setup += microseconds(built - start);
                cold += microseconds(Clock::now() - built);
            }
            std::cout << seed << ',' << n << ',' << m << ',' << k << ','
                      << setup / 5 << ',' << cold / 5 << ',' << sum << '\n';
        }
    }
}
}  // namespace

int main(int argc, char** argv) {
    bool updates_only = false, large_dense = false, cold = false;
    Weights weights = Weights::random;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--updates-only") updates_only = true;
        else if (arg == "--large-dense") large_dense = true;
        else if (arg == "--cold") cold = true;
        else if (arg == "--unit") weights = Weights::unit;
        else if (arg == "--manhattan") weights = Weights::manhattan;
        else { std::cerr << "usage: " << argv[0] << " [--cold [--unit|--manhattan] | --updates-only] [--large-dense]\n"; return 1; }
    }
    if (!cold && weights != Weights::random) { std::cerr << "--unit and --manhattan require --cold\n"; return 1; }
    const std::pair<const char*, Method> methods[] = {
        {"cached", Method::cached_prim}, {"incremental", Method::incremental_prim},
        {"mehlhorn", Method::mehlhorn}, {"mst_prune", Method::mst_prune}
    };
    std::cout << std::fixed << std::setprecision(3);
    std::vector<std::array<int, 3>> sizes{{100, 400, 20}, {100, 4000, 20}, {400, 1600, 24}};
    if (large_dense) sizes = {{400, 60000, 24}};
    if (cold) {
        if (large_dense) sizes = {{400, 60000, 3}, {400, 60000, 24}, {800, 245000, 40}};
        measure_cold(sizes, weights);
        return 0;
    }
    std::cout << "seed,n,m,k,queries_per_update,method,mode,update_us,solve_us,total_us_per_update,cost_sum\n";
    for (auto [n, m, k] : sizes) {
        for (unsigned seed = 1; seed <= 5; ++seed) {
            auto work = generate(n, m, k, seed);
            for (auto [name, method] : methods) {
                for (int q : {1, max_queries}) {
                    for (int run = 0; run < 2; ++run) {
                        bool rebuild = ((run + seed) % 2) == 0;
                        if (updates_only && rebuild) continue;
                        auto s = measure(work, method, q, rebuild);
                        std::cout << seed << ',' << n << ',' << m << ',' << k << ',' << q
                                  << ',' << name << ',' << (rebuild ? "rebuild" : "update")
                                  << ',' << s.update_us << ',' << s.solve_us << ','
                                  << s.update_us + q * s.solve_us << ',' << s.cost_sum << '\n';
                    }
                }
            }
        }
    }
}
