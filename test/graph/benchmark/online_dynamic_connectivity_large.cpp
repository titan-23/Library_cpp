#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#if defined(__linux__)
#include <sys/resource.h>
#endif
#ifndef ODC_HEADER
#define ODC_HEADER "titan_cpplib/graph/online_dynamic_connectivity.cpp"
#endif
#include ODC_HEADER

using namespace std;
using Clock = chrono::steady_clock;
struct Operation { int kind, a, b; }; // add, erase, same, size

struct DisjointSet {
    vector<int> p;
    explicit DisjointSet(int n) : p(n, -1) {}
    int root(int v) { while (p[v] >= 0) { if (p[p[v]] >= 0) p[v] = p[p[v]]; v = p[v]; } return v; }
    void unite(int u, int v) {
        u = root(u); v = root(v);
        if (u == v) return;
        if (p[u] > p[v]) swap(u, v);
        p[u] += p[v]; p[v] = u;
    }
};

int main(int argc, char** argv) {
    int n = argc > 1 ? stoi(argv[1]) : 500000;
    int m = argc > 2 ? stoi(argv[2]) : 500000;
    int q = argc > 3 ? stoi(argv[3]) : 500000;
    string workload = argc > 4 ? argv[4] : "mixed";
    uint32_t seed = argc > 5 ? stoul(argv[5]) : 23;
    if (n < 2 || m < 0 || q < 0) throw invalid_argument("invalid sizes");
    bool after_split = workload == "queries_after_split";
    bool connected = workload == "connected_mixed" || after_split;
    bool balanced = workload == "balanced_cuts";
    if ((connected || balanced) && m < n - 1) throw invalid_argument("need at least n-1 initial edges");
    if (!(connected || balanced || workload == "mixed" || workload == "queries" || workload == "erase_all"))
        throw invalid_argument("unknown workload");
    mt19937 rng(seed);
    auto pick = [&](int bound) { return int(rng() % uint32_t(bound)); };
    vector<pair<int, int>> initial;
    initial.reserve(m);
    if (connected || balanced) for (int v = 1; v < n; ++v)
        initial.emplace_back(balanced ? v - 1 : pick(v), v);
    while ((int)initial.size() < m) {
        int u = pick(n), v = balanced ? u : pick(n);
        initial.emplace_back(u, v);
    }
    int leaf_edge = -1;
    if (after_split) {
        vector<int> degree(n);
        for (auto [u, v] : initial) if (u != v) { ++degree[u]; ++degree[v]; }
        for (int id = 0; id < m; ++id) {
            auto [u, v] = initial[id];
            if (u != v && (degree[u] == 1 || degree[v] == 1)) { leaf_edge = id; break; }
        }
        if (leaf_edge < 0) throw invalid_argument("queries_after_split requires a leaf");
    }
    vector<Operation> ops;
    ops.reserve(q);
    vector<char> live(m + q, false);
    fill(live.begin(), live.begin() + m, true);
    int issued = m;
    int counts[4]{};
    {
        vector<int> active(m);
        iota(active.begin(), active.end(), 0);
        vector<int> deletion_order;
        if (balanced) {
            vector<pair<int, int>> ranges{{0, n - 1}};
            for (size_t i = 0; i < ranges.size(); ++i) {
                auto [lo, hi] = ranges[i];
                if (lo == hi) continue;
                int mid = (lo + hi) / 2;
                deletion_order.push_back(mid);
                ranges.emplace_back(lo, mid);
                ranges.emplace_back(mid + 1, hi);
            }
        } else if (workload == "erase_all") {
            deletion_order = active;
            shuffle(deletion_order.begin(), deletion_order.end(), rng);
        }
        for (int i = 0; i < q; ++i) {
            Operation op;
            if (balanced || workload == "erase_all") {
                if (i < (int)deletion_order.size()) {
                    op = {1, deletion_order[i], 0}; live[op.a] = false;
                } else op = {2, pick(n), pick(n)};
            } else if (after_split) {
                if (i == 0) { op = {1, leaf_edge, 0}; live[leaf_edge] = false; }
                else op = {2, pick(n), pick(n)};
            } else if (workload == "queries") {
                op = {2, pick(n), pick(n)};
            } else if (i % 4 == 0 && !active.empty()) {
                int at = pick(active.size()), id = active[at];
                op = {1, id, 0}; live[id] = false;
                active[at] = active.back(); active.pop_back();
            } else if (i % 4 <= 1) {
                op = {0, pick(n), pick(n)};
                live[issued] = true; active.push_back(issued++);
            } else if (i % 4 == 2) op = {2, pick(n), pick(n)};
            else op = {3, pick(n), 0};
            ++counts[op.kind]; ops.push_back(op);
        }
    }
    auto begin = Clock::now();
    titan23::OnlineDynamicConnectivity dc(n, issued);
#ifdef ODC_EAGER
    dc.prepare_dynamic();
#endif
    for (auto [u, v] : initial) dc.add_edge(u, v);
#ifdef ODC_PREPARE
    dc.prepare_dynamic();
    string build = "prepared";
#elif defined(ODC_EAGER)
    string build = "eager";
#else
    string build = "incremental";
#endif
    double build_seconds = chrono::duration<double>(Clock::now() - begin).count();
    uint64_t checksum = 0;
    begin = Clock::now();
    for (auto [kind, a, b] : ops) {
        uint64_t value;
        if (kind == 0) value = dc.add_edge(a, b);
        else if (kind == 1) value = dc.erase_edge(a);
        else if (kind == 2) value = dc.same(a, b);
        else value = dc.size(a);
        checksum ^= value + 0x9e3779b97f4a7c15ULL + (checksum << 6) + (checksum >> 2);
    }
    double query_seconds = chrono::duration<double>(Clock::now() - begin).count();
    long peak_kib = 0;
#if defined(__linux__)
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    peak_kib = usage.ru_maxrss;
#endif
    // Rebuild the final graph independently, outside the timed region.
    DisjointSet expected(n);
    int active_count = 0, id = 0;
    for (auto [u, v] : initial) { if (live[id++]) { expected.unite(u, v); ++active_count; } }
    for (auto op : ops) if (op.kind == 0) { if (live[id++]) { expected.unite(op.a, op.b); ++active_count; } }
    int groups = 0;
    for (int v = 0; v < n; ++v) groups += expected.p[v] < 0;
    if (dc.group_count() != groups || dc.edge_count() != active_count || dc.edge_id_count() != issued)
        throw runtime_error("final counters differ from reference");
    for (int sample = 0; sample < min(n, 1024); ++sample) {
        int u = pick(n), v = pick(n);
        if (dc.same(u, v) != (expected.root(u) == expected.root(v)) || dc.size(u) != -expected.p[expected.root(u)])
            throw runtime_error("final component differs from reference");
    }
    cout << "n,m,q,workload,seed,build,add,erase,same,size,build_seconds,query_seconds,total_seconds,peak_kib,checksum,groups\n";
    cout << n << ',' << m << ',' << q << ',' << workload << ',' << seed << ',' << build;
    for (int c : counts) cout << ',' << c;
    cout << fixed << setprecision(6) << ',' << build_seconds << ',' << query_seconds << ',' << build_seconds + query_seconds
         << ',' << peak_kib << ',' << checksum << ',' << groups << '\n';
}
