#include <vector>
#include <algorithm>
#include <utility>
#include "titan_cpplib/graph/warshall_floyd_path.cpp"
using namespace std;

// MinimumSteinerTree
namespace titan23 {

/**
 * @brief 最小シュタイナー木など
 * @details プリム法を用いた近似解法の実装
 * 時間 O(|V|^3), 空間 O(|V|^2)
 */
template<typename T>
class MinimumSteinerTree {
private:
    int n;
    T INF;
    titan23::WarshallFloydPath<T> dist_path;

    vector<int>  zp_is_terminal;
    vector<int>  zp_in_tree;
    vector<int>  zp_rem_t;
    vector<T>    zp_min_d;
    vector<int>  zp_closest;
    vector<int>  zp_tree_nodes;
    vector<int>  zp_deg;
    vector<int>  zp_prune_deg;
    vector<int>  zp_adj;
    vector<int>  zp_leaves;
    vector<int>  zp_deleted;
    vector<char> zp_edge_exists;

public:
    MinimumSteinerTree() {}

    MinimumSteinerTree(const vector<vector<pair<int, T>>> &G, const T INF) :
            n((int)G.size()), INF(INF) {
        dist_path = titan23::WarshallFloydPath<T>(G, INF);

        zp_is_terminal.assign(n, 0);
        zp_in_tree.assign(n, 0);
        zp_rem_t.reserve(n);
        zp_min_d.resize(n);
        zp_closest.resize(n);
        zp_tree_nodes.reserve(n);
        zp_deg.assign(n, 0);
        zp_prune_deg.assign(n, 0);
        zp_adj.assign(n * n, 0);
        zp_leaves.resize(n);
        zp_deleted.assign(n, 0);
        zp_edge_exists.assign(n * n, 0);
    }

    // terminal を含むシュタイナー木の辺集合を返す
    vector<pair<int, int>> build_prim(const vector<int>& terminal) {
        if (terminal.size() <= 1) return {};
        int t_sz = terminal.size();
        for (int u : zp_tree_nodes) {
            zp_in_tree[u] = 0;
            zp_deleted[u] = 0;
            for (int i = 0; i < zp_deg[u]; ++i) {
                zp_edge_exists[u * n + zp_adj[u * n + i]] = 0;
            }
            zp_deg[u] = 0;
        }
        zp_tree_nodes.clear();
        zp_rem_t.clear();
        for (int t : terminal) {
            zp_is_terminal[t] = 1;
            zp_rem_t.push_back(t);
        }
        int root = zp_rem_t.back();
        zp_rem_t.pop_back();
        zp_in_tree[root] = 1;
        zp_tree_nodes.push_back(root);
        zp_min_d.resize(zp_rem_t.size());
        zp_closest.resize(zp_rem_t.size());
        for (int i = 0; i < (int)zp_rem_t.size(); ++i) {
            int t = zp_rem_t[i];
            zp_min_d[i] = dist_path.get_dist(root, t);
            zp_closest[i] = root;
        }
        while (!zp_rem_t.empty()) {
            T best_dist = INF;
            int best_idx = -1;
            for (int i = 0; i < (int)zp_rem_t.size(); ++i) {
                if (zp_min_d[i] < best_dist) {
                    best_dist = zp_min_d[i];
                    best_idx = i;
                }
            }
            if (best_idx == -1) break;
            int target_t = zp_rem_t[best_idx];
            int start_v = zp_closest[best_idx];
            const vector<int>& path = dist_path.get_path(start_v, target_t);
            for (int i = 0; i + 1 < (int)path.size(); ++i) {
                int u = path[i];
                int v = path[i+1];
                if (!zp_edge_exists[u * n + v]) {
                    zp_edge_exists[u * n + v] = 1;
                    zp_edge_exists[v * n + u] = 1;
                    zp_adj[u * n + zp_deg[u]++] = v;
                    zp_adj[v * n + zp_deg[v]++] = u;
                }
                if (!zp_in_tree[v]) {
                    zp_in_tree[v] = 1;
                    zp_tree_nodes.push_back(v);
                    for (int j = 0; j < (int)zp_rem_t.size(); ++j) {
                        T d = dist_path.get_dist(v, zp_rem_t[j]);
                        if (d < zp_min_d[j]) {
                            zp_min_d[j] = d;
                            zp_closest[j] = v;
                        }
                    }
                }
            }
            zp_rem_t[best_idx] = zp_rem_t.back();
            zp_min_d[best_idx] = zp_min_d.back();
            zp_closest[best_idx] = zp_closest.back();
            zp_rem_t.pop_back();
            zp_min_d.pop_back();
            zp_closest.pop_back();
        }
        int head = 0, tail = 0;
        for (int u : zp_tree_nodes) {
            zp_prune_deg[u] = zp_deg[u];
            if (zp_prune_deg[u] == 1 && !zp_is_terminal[u]) {
                zp_leaves[tail++] = u;
            }
        }
        while (head < tail) {
            int u = zp_leaves[head++];
            zp_deleted[u] = 1;
            for (int i = 0; i < zp_deg[u]; ++i) {
                int v = zp_adj[u * n + i];
                if (!zp_deleted[v]) {
                    zp_prune_deg[v]--;
                    if (zp_prune_deg[v] == 1 && !zp_is_terminal[v]) {
                        zp_leaves[tail++] = v;
                    }
                }
            }
        }
        vector<pair<int, int>> final_edges;
        for (int u : zp_tree_nodes) {
            if (!zp_deleted[u]) {
                for (int i = 0; i < zp_deg[u]; ++i) {
                    int v = zp_adj[u * n + i];
                    if (!zp_deleted[v] && u < v) {
                        final_edges.emplace_back(u, v);
                    }
                }
            }
        }
        for (int t : terminal) zp_is_terminal[t] = 0;
        return final_edges;
    }

    // 始点を全探索し、最小コストのシュタイナー木(近似)の辺集合を返す
    vector<pair<int, int>> build_prim_fullsearch(const vector<int> &terminal) {
        if (terminal.size() <= 1) return {};
        bool first = true;
        T min_total_cost = 0;
        vector<pair<int, int>> best_edges;
        for (int s = 0; s < (int)terminal.size(); ++s) {
            vector<int> now = terminal;
            swap(now.back(), now[s]);
            vector<pair<int, int>> edges = build_prim(now);
            T cost = 0;
            for (const auto& e : edges) {
                cost += dist_path.get_dist(e.first, e.second);
            }
            if (first || cost < min_total_cost) {
                min_total_cost = cost;
                best_edges = move(edges);
                first = false;
            }
        }
        return best_edges;
    }
};
}  // namespace titan23
