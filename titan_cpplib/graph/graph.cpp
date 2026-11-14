/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/graph.cpp
#pragma once

#include <vector>
#include <queue>
#include <stack>
#include <tuple>
#include <cassert>
#include <algorithm>

#include "titan_cpplib/ds/union_find.cpp"
using namespace std;

namespace titan23 {

    /// @brief 隣接リストで持つグラフ / add_edge は u -> v の向きだけ加える
    class Graph {
      private:
        int n;
        vector<vector<int>> G;
        vector<tuple<int, int, int>> E;  // (w, u, v)
        static const int inf = 1e9;

      public:
        Graph() : n(0), G(0) {}

        /// @brief 頂点数 n で構築する / O(n)
        Graph(int n) : n(n), G(n) {}

        /// @brief 重み w の辺 u -> v を加える / O(1)
        void add_edge(const int u, const int v, const int w = 1) {
            assert(0 <= u && u < n);
            assert(0 <= v && v < n);
            G[u].emplace_back(v);
            E.emplace_back(w, u, v);
        }

        /// @brief 隣接リストの写しを返す
        vector<vector<int>> get_G() const {
            return G;
        }

        /// @brief 加えた辺を (u, v) の列で返す
        vector<pair<int, int>> get_E() const {
            vector<pair<int, int>> res;
            res.reserve(E.size());
            for (const auto &[w, u, v] : E) {
                res.emplace_back(u, v);
            }
            return res;
        }

        /// @brief 二部グラフかを返す / 無向グラフは辺を両向きに加えておく / O(n + m)
        bool is_bipartite() {
            vector<int> col(n, -1);
            for (int v = 0; v < n; ++v) {
                if (col[v] != -1) continue;
                col[v] = 0;
                queue<int> qu;
                qu.push(v);
                while (!qu.empty()) {
                    int v = qu.front();
                    qu.pop();
                    int cx = 1 - col[v];
                    for (const auto &x: G[v]) {
                        if (col[x] == -1) {
                            col[x] = cx;
                            qu.push(x);
                        } else if (col[x] != cx) {
                            return false;
                        }
                    }
                }
            }
            for (const int &c: col) {
                if (c == -1) {
                return false;
                }
            }
            return true;
        }

        /// @brief start からの距離を返す / 届かない頂点は -1 / O(n + m)
        vector<int> bfs(const int start) {
            vector<int> dist(n, -1);
            queue<int> todo;
            todo.emplace(start);
            dist[start] = 0;
            while (!todo.empty()) {
                int v = todo.front(); todo.pop();
                for (const int &x : G[v]) {
                    if (dist[x] == -1) {
                        dist[x] = dist[v] + 1;
                        todo.emplace(x);
                    }
                }
            }
            return dist;
        }

        /// @brief s から t への最短路の頂点列を返す / 届かなければ空 / O(n + m)
        vector<int> bfs_path(int s, int t) {
            vector<int> prev(n, -1);
            vector<int> dist(n, inf);
            dist[s] = 0;
            queue<int> todo;
            todo.emplace(s);
            while (!todo.empty()) {
                int v = todo.front();
                todo.pop();
                for (const int x : G[v]) {
                    if (dist[x] == inf) {
                        dist[x] = dist[v] + 1;
                        prev[x] = v;
                        todo.emplace(x);
                    }
                }
            }
            if (dist[t] == inf) {
                return {};
            }
            vector<int> path;
            while (prev[t] != -1) {
                path.emplace_back(t);
                t = prev[t];
            }
            path.emplace_back(t);
            std::reverse(path.begin(), path.end());
            return path;
        }

        /// @brief 頂点数を返す
        int len() const {
            return n;
        }

        /// @brief トポロジカル順を返す / 閉路があると長さが n 未満になる / O(n + m)
        vector<int> topological_sort() {
            vector<int> d(n, 0);
            for (int i = 0; i < n; ++i) {
                for (const int &x: G[i]) {
                    ++d[x];
                }
            }
            vector<int> res;
            stack<int> todo;
            for (int i = 0; i < n; ++i) {
                if (d[i] == 0) todo.emplace(i);
            }
            while (!todo.empty()) {
                int v = todo.top(); todo.pop();
                res.emplace_back(v);
                for (const int &x: G[v]) {
                    --d[x];
                    if (d[x] == 0) {
                        todo.emplace(x);
                    }
                }
            }
            return res;
        }

        /// @brief Kruskal 法で最小全域森の辺 (u, v) を返す / O(m log m)
        vector<pair<int, int>> minimum_spanning_tree() {
            titan23::UnionFind uf(n);
            vector<pair<int, int>> ans;
            auto F = E;
            sort(F.begin(), F.end());
            for (const auto &[w, u, v] : F) {
                if (uf.same(u, v)) continue;
                uf.unite(u, v);
                ans.emplace_back(u, v);
            }
            return ans;
        }
    };
}  // namespace titan23
