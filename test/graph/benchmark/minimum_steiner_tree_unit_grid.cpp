#include <algorithm>
#include <array>
#include <iostream>
#include <numeric>
#include <random>
#include <utility>
#include <vector>
#define MinimumSteinerTree LegacyMinimumSteinerTree
#include "fixtures/minimum_steiner_tree_20261112.cpp"
#undef MinimumSteinerTree
#include "titan_cpplib/graph/minimum_steiner_tree.cpp"

// Quality-only comparison with many equal-length shortest paths.
int main() {
    using namespace std;
    using namespace titan23;
    vector<vector<pair<int, int>>> graph(100);
    auto add = [&](int u, int v) {
        graph[u].emplace_back(v, 1);
        graph[v].emplace_back(u, 1);
    };
    for (int v = 0; v < 100; ++v) {
        if (v % 10) add(v, v - 1);
        if (v >= 10) add(v, v - 10);
    }
    LegacyMinimumSteinerTree<int> old(graph, 1000000);
    MinimumSteinerTree<int> now(graph, 1000000);
    mt19937 random(23);
    vector<int> order(100);
    iota(order.begin(), order.end(), 0);
    cout << "k,queries,legacy_sum,cached_sum,incremental_sum,mehlhorn_sum,mst_prune_sum,fullsearch_sum,cached_wins,cached_losses\n";
    for (int k : {5, 20, 50, 90}) {
        array<long long, 6> total{};
        int wins = 0, losses = 0;
        for (int query = 0; query < 200; ++query) {
            shuffle(order.begin(), order.end(), random);
            vector<int> terminals(order.begin(), order.begin() + k);
            array<size_t, 6> costs{
                old.build_prim(terminals).size(), now.build_prim(terminals).size(),
                now.build_prim_dijkstra(terminals).size(), now.build_mehlhorn(terminals).size(),
                now.build_mst_prune(terminals).size(), now.build_prim_fullsearch(terminals).size()
            };
            // Every edge has weight one, so edge count equals tree cost.
            for (size_t i = 0; i < total.size(); ++i) total[i] += costs[i];
            wins += costs[1] < costs[0];
            losses += costs[1] > costs[0];
        }
        cout << k << ",200";
        for (auto value : total) cout << ',' << value;
        cout << ',' << wins << ',' << losses << '\n';
    }
}
