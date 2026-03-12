#pragma once

#include <vector>
#include <queue>
#include <iostream>
#include <string>
#include <cassert>
#include "titan_cpplib/graph/hld.cpp"
using namespace std;

namespace titan23 {

template<typename T = int>
class RootedTree {
public:
    vector<vector<pair<int, T>>> G;
    int n;
    int root;
    vector<T> dep_weight;
    vector<int> size, par, dep, nodein, nodeout, head, hld;

    RootedTree() {}
    RootedTree(vector<vector<int>> &G, int root) : root(root) {
        this->n = G.size();
        vector<vector<pair<int, T>>> F(n);
        for (int v = 0; v < n; ++v) {
            for (int x : G[v]) {
                F[v].push_back({x, 1});
            }
        }
        this->G = F;
        titan23::HLD H(this->G, root);
        size    = H.size;
        par     = H.par;
        dep     = H.dep;
        nodein  = H.nodein;
        nodeout = H.nodeout;
        head    = H.head;
        hld     = H.hld;

        dep_weight.resize(n);
        for (int i = 0; i < n; ++i) {
            dep_weight[i] = dep[i];
        }
    }

    RootedTree(vector<vector<pair<int, T>>> &G, int root) : G(G), n(G.size()), root(root) {
        vector<vector<int>> F(n);
        for (int v = 0; v < n; ++v) {
            for (auto &[x, _] : G[v]) {
                F[v].push_back(x);
            }
        }
        titan23::HLD H(F, root);
        size    = H.size;
        par     = H.par;
        dep     = H.dep;
        nodein  = H.nodein;
        nodeout = H.nodeout;
        head    = H.head;
        hld     = H.hld;

        { // dep_weight
            dep_weight.resize(n, -1);
            dep_weight[root] = 0;
            queue<int> todo;
            todo.push(root);
            while (!todo.empty()) {
                int v = todo.front(); todo.pop();
                for (auto [x, w] : G[v]) if (dep_weight[x] == -1) {
                    dep_weight[x] = dep_weight[v] + w;
                    todo.push(x);
                }
            }
        }
    }

    int dist(int u, int v) const {
        return dep[u] + dep[v] - 2 * dep[lca(u, v)];
    }

    T dist_weight(int u, int v) const {
        return dep_weight[u] + dep_weight[v] - 2 * dep_weight[lca(u, v)];
    }

    int lca(int u, int v) const {
        while (true) {
            if (nodein[u] > nodein[v]) swap(u, v);
            if (head[u] == head[v]) return u;
            v = par[head[v]];
        }
    }

    int la(int v, int k) const {
        if (k < 0 || dep[v] < k) return -1;
        while (1) {
            int h = head[v];
            int dist_to_head = dep[v] - dep[h];
            if (k <= dist_to_head) {
                return hld[nodein[v] - k];
            }
            k -= dist_to_head + 1;
            v = par[h];
        }
    }

    /// @brief s から t へ、k 個進んだ頂点を返す / O(logn)
    int path_kth_elm(int s, int t, int k) const {
        int l = lca(s, t);
        int d = dep[s] + dep[t] - 2 * dep[l];
        if (k < 0 || k > d) return -1;
        if (k <= dep[s] - dep[l]) {
            return la(s, k);
        } else {
            return la(t, d - k);
        }
    }

    /// @brief Return True if (a is on path(u - v)) else False. / O(logN)
    bool is_on_path(int u, int v, int a) const {
        return dist(u, a) + dist(a, v) == dist(u, v);
    }

    /// @brief vはtの祖先か？ / O(1)
    bool is_ancestor(int v, int t) const {
        return nodein[v] <= nodein[t] && nodeout[t] <= nodeout[v];
    }

    /// @brief vはtの子孫か？ / O(1)
    bool is_descendant(int v, int t) const {
        return nodein[t] <= nodein[v] && nodeout[v] <= nodeout[t];
    }

    /// @brief Pの頂点をすべて通るパスが存在するか？ / O(|P|+logN)
    bool is_passable_path(const vector<int> &P) const {
        int k = P.size();
        vector<bool> seen(k, false);
        int prev_maxv = -1;
        for (int i = 0; i < 2; ++i) {
            int max_dep = -1, max_v = -1;
            for (int j = 0; j < k; ++j) {
                if (seen[j]) continue;
                if (dep[P[j]] > max_dep) {
                    max_dep = dep[P[j]];
                    max_v = P[j];
                }
            }
            if (max_v == -1) break;
            if (i == 1) {
                int u = lca(max_v, prev_maxv);
                for (int v : P) {
                    if (v == u) continue;
                    if (is_ancestor(v, u)) return false;
                }
            }
            for (int j = 0; j < k; ++j) {
                if (P[j] == max_v || is_ancestor(P[j], max_v)) {
                    seen[j] = true;
                }
            }
            prev_maxv = max_v;
        }
        for (int i = 0; i < k; ++i) {
            if (!seen[i]) return false;
        }
        return true;
    }

    /// @brief 直径を返す
    /// @return {直径, {端点u, 端点v}}
    pair<T, pair<int, int>> get_diameter() const {
        if (G.empty()) return {0, {-1, -1}};
        vector<T> dist(n, -1);
        auto bfs = [&] (int s) -> void {
            fill(dist.begin(), dist.end(), -1);
            dist[s] = 0;
            queue<int> todo;
            todo.push(s);
            while (!todo.empty()) {
                int v = todo.front(); todo.pop();
                for (auto &[x, w] : G[v]) {
                    if (dist[x] == -1) {
                        dist[x] = dist[v] + w;
                        todo.push(x);
                    }
                }
            }
        };
        bfs(0);
        T max_d = -1;
        int u = -1, v = -1;
        for (int i = 0; i < n; ++i) {
            if (max_d < dist[i]) {
                max_d = dist[i];
                u = i;
            }
        }
        bfs(u);
        max_d = -1;
        for (int i = 0; i < n; ++i) {
            if (max_d < dist[i]) {
                max_d = dist[i];
                v = i;
            }
        }
        return {max_d, {u, v}};
    }

    /// @brief 直径を返す
    vector<int> get_diameter_path() const {
        if (G.empty()) return {};
        vector<T> dist(n, -1);
        vector<int> prev(n, -1);

        auto bfs = [&] (int s) -> void {
            fill(dist.begin(), dist.end(), -1);
            fill(prev.begin(), prev.end(), -1);
            dist[s] = 0;
            queue<int> todo;
            todo.push(s);
            while (!todo.empty()) {
                int v = todo.front();
                todo.pop();
                for (auto &[x, w] : G[v]) {
                    if (dist[x] == -1) {
                        dist[x] = dist[v] + w;
                        prev[x] = v;
                        todo.push(x);
                    }
                }
            }
        };

        bfs(0);
        T max_d = -1;
        int u = -1, v = -1;
        for (int i = 0; i < n; ++i) {
            if (max_d < dist[i]) {
                max_d = dist[i];
                u = i;
            }
        }

        bfs(u);
        max_d = -1;
        for (int i = 0; i < n; ++i) {
            if (max_d < dist[i]) {
                max_d = dist[i];
                v = i;
            }
        }

        vector<int> path;
        while (v != -1) {
            path.push_back(v);
            v = prev[v];
        }
        return path;
    }

    /// @brief デバッグ用
    void debug(int root = 0) {
        auto dfs = [&] (auto &&dfs, int v, int p, const string &pref, bool is_last) -> void {
            cerr << pref;
            if (v != root) {
                cerr << (is_last ? "└─ " : "├─ ");
            }
            cerr << v << '\n';
            vector<int> nxt;
            for (auto [x, _] : G[v]) if (x != p) {
                nxt.push_back(x);
            }
            string np = pref;
            if (v != root) {
                np += (is_last ? "   " : "│  ");
            }
            for (int i = 0; i < (int)nxt.size(); ++i) {
                bool next_is_last = (i == (int)nxt.size() - 1);
                dfs(dfs, nxt[i], v, np, next_is_last);
            }
        };
        cerr << "--- Tree Debug (root: " << root << ") ---\n";
        if (G.empty()) {
            cerr << "empty tree" << "\n";
        } else {
            dfs(dfs, root, -1, "", true);
        }
        cerr << "---------------------------\n";
    }
};
} // nsmaepace titan23
