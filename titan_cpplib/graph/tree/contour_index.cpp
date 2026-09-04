/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/tree/contour_index.cpp
#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>
#include <utility>
#include <vector>
using namespace std;

namespace titan23 {

/// Shared index for distance range queries on a static tree
/// Construction is O(N log N)
class ContourIndex {
public:
    int n = 0;
    vector<int> path_off = {0}, path_dist, cpar, all_off = {0}, sub_off = {0};

    void build(const vector<vector<int>> &G) {
        n = (int)G.size();
        if (n == 0) return;

        vector<int> all_len, sub_len;
        all_len.reserve(n + 1);
        sub_len.reserve(n + 1);
        all_len.resize(n);
        sub_len.resize(n);
        cpar.assign(n, -1);

        {
            struct Record { int v, dist; };
            vector<int> path_len(n);
            const int levels = bit_width((unsigned)n) - 1;
            vector<Record> records;
            records.reserve(n * levels);

            {
                path_off.resize(n + 1);
                vector<int> &adj_off = path_off;
                vector<int> adj;
                adj.reserve(2 * (n - 1));
                for (int v = 0; v < n; ++v) {
                    adj.insert(adj.end(), G[v].begin(), G[v].end());
                    adj_off[v + 1] = (int)adj.size();
                }
                vector<uint8_t> dead(n);
                vector<int> par(n), sz(n);

                struct State { int v, p, dist; };
                vector<State> states;
                states.reserve(n);

                auto add_entry = [&](int v, int dist) {
                    records.push_back({v, dist});
                    ++path_len[v];
                };

                auto select_centroid = [&](int v, int total) {
                    while (true) {
                        int next = -1;
                        for (int i = adj_off[v]; i < adj_off[v + 1]; ++i) {
                            const int x = adj[i];
                            if (dead[x]) continue;
                            const int part = par[x] == v ? sz[x] : total - sz[v];
                            if (part * 2 > total) {
                                next = x;
                                break;
                            }
                        }
                        if (next == -1) return v;
                        v = next;
                    }
                };

                auto find_centroid = [&](int start) {
                    records.clear();
                    states.clear();
                    states.push_back({start, -1, 0});
                    while (!states.empty()) {
                        const State cur = states.back();
                        states.pop_back();
                        records.push_back({cur.v, 0});
                        par[cur.v] = cur.p;
                        sz[cur.v] = 1;
                        for (int i = adj_off[cur.v]; i < adj_off[cur.v + 1]; ++i) {
                            const int x = adj[i];
                            if (x == cur.p) continue;
                            states.push_back({x, cur.v, 0});
                        }
                    }
                    for (int i = (int)records.size() - 1; i > 0; --i) {
                        const int v = records[i].v;
                        sz[par[v]] += sz[v];
                    }
                    return select_centroid(records[records.size() / 2].v, (int)records.size());
                };

                auto decompose = [&](auto &&self, int c, int cp, int clen) -> void {
                    cpar[c] = cp;
                    sub_len[c] = clen;
                    dead[c] = 1;
                    int all_max = 0;
                    for (int j = adj_off[c]; j < adj_off[c + 1]; ++j) {
                        const int root = adj[j];
                        if (dead[root]) continue;
                        int sub_max = 0;
                        const int first = (int)records.size();
                        states.clear();
                        states.push_back({root, c, 1});
                        while (!states.empty()) {
                            const State cur = states.back();
                            states.pop_back();
                            par[cur.v] = cur.p;
                            sz[cur.v] = 1;
                            add_entry(cur.v, cur.dist);
                            sub_max = max(sub_max, cur.dist);
                            for (int i = adj_off[cur.v]; i < adj_off[cur.v + 1]; ++i) {
                                const int x = adj[i];
                                if (x == cur.p || dead[x]) continue;
                                states.push_back({x, cur.v, cur.dist + 1});
                            }
                        }
                        const int last = (int)records.size();
                        for (int i = last - 1; i > first; --i) {
                            const int v = records[i].v;
                            sz[par[v]] += sz[v];
                        }
                        all_max = max(all_max, sub_max);
                        const int mid = first + (last - first) / 2;
                        self(self, select_centroid(records[mid].v, last - first), c, sub_max);
                    }
                    all_len[c] = all_max + 1;
                };
                const int root = find_centroid(0);
                records.clear();
                decompose(decompose, root, -1, 0);
            }

            path_off.resize(n + 1);
            for (int v = 0; v < n; ++v) path_off[v + 1] = path_off[v] + path_len[v];
            for (int v = 0; v < n; ++v) path_len[v] = path_off[v + 1];
            path_dist.resize(path_off.back());
            for (const Record &rec : records) path_dist[--path_len[rec.v]] = rec.dist;
        }

        all_len.push_back(0);
        int pref = 0;
        for (int i = 0; i < n; ++i) {
            const int len = all_len[i];
            all_len[i] = pref;
            pref += len;
        }
        all_len[n] = pref;
        all_off = move(all_len);

        sub_len.push_back(0);
        pref = all_off.back();
        for (int i = 0; i < n; ++i) {
            const int len = sub_len[i];
            sub_len[i] = pref;
            pref += len;
        }
        sub_len[n] = pref;
        sub_off = move(sub_len);
    }

    ContourIndex() = default;

    explicit ContourIndex(const vector<vector<int>> &G) {
        build(G);
    }

};

}  // namespace titan23
