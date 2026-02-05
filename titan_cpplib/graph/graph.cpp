#include <vector>
#include <queue>
#include <stack>
#include <cassert>
#include <algorithm>

#include "titan_cpplib/ds/union_find.cpp"
using namespace std;

namespace titan23 {

    class Graph {
      private:
        int n;
        vector<vector<int>> G;
        vector<pair<int, int>> E;
        static const int inf = 1e9;

      public:
        Graph() : n(0), G(0) {}

        Graph(int n) : n(n), G(n) {}

        //! 辺(u, v)を追加する
        void add_edge(const int u, const int v) {
            assert(0 <= u && u < n);
            assert(0 <= v && v < n);
            G[u].emplace_back(v);
            E.emplace_back(u, v);
        }

        vector<vector<int>> get_G() const {
            return G;
        }

        vector<pair<int, int>> get_E() const {
            return E;
        }

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

        int len() const {
            return n;
        }

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

        vector<pair<int, int>> minimum_spanning_tree() {
            titan23::UnionFind uf(n);
            vector<pair<int, int>> ans;
            for (const auto &[u, v] : E) {
                if (uf.same(u, v)) continue;
                uf.unite(u, v);
                ans.emplace_back(u, v);
            }
            return ans;
        }
    };
}  // namespace titan23
