// #include "titan_cpplib/graph/graph.cpp"
#include <vector>
#include <queue>
#include <stack>
using namespace std;

namespace titan23 {

  struct Graph {
    int n;
    vector<vector<int>> G;

    Graph() : n(0), G(0) {}

    Graph(int n) : n(n), G(n) {}

    void add_edge(const int u, const int v) {
      G[u].emplace_back(v);
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

    template <typename T>
    vector<T> bfs(const int start) {
      vector<T> dist(n, -1);
      queue<int> qu;
      qu.push(start);
      dist[start] = 0;
      while (!qu.empty()) {
        int v = qu.front();
        qu.pop();
        for (const auto &x: G[v]) {
          if (dist[x] == -1) {
            dist[x] = dist[v] + 1;
            qu.push(x);
          }
        }
      }
      return dist;
    }

    int len() const {
      return (int)G.size();
    }

    vector<int> topological_sort() {
      vector<int> d(n, 0);
      for (int i = 0; i < n; ++i) {
        for (const int &x: G[i]) {
          ++d[x];
        }
      }
      vector<int> res, todo;
      for (int i = 0; i < n; ++i) {
        if (d[i] == 0) todo.emplace_back(i);
      }
      while (!todo.empty()) {
        int v = todo.back();
        todo.pop_back();
        res.emplace_back(v);
        for (const int &x: G[v]) {
          --d[x];
          if (d[x] == 0) {
            todo.emplace_back(x);
          }
        }
      }
      return res;
    }

    vector<vector<int>> get_scc() {
      vector<vector<int>> rG(n);
      for (int v = 0; v < n; ++v) {
        for (const int &x: G[v]) {
          rG[x].emplace_back(v);
        }
      }
      vector<int> visited(n, 0), dfsid(n, 0);
      int now = n;
      for (int s = 0; s < n; ++s) {
        if (visited[s]) continue;
        stack<int> todo;
        todo.push(~s);
        todo.push(s);
        while (!todo.empty()) {
          int v = todo.top();
          todo.pop();
          if (v >= 0) {
            if (visited[v]) continue;
            visited[v] = 2;
            for (const int &x: G[v]) {
              if (visited[x]) continue;
              todo.push(~x);
              todo.push(x);
            }
          } else {
            v = ~v;
            if (visited[v] == 1) continue;
            visited[v] = 1;
            dfsid[--now] = v;
          }
        }
      }
      vector<vector<int>> res;
      for (const int &s: dfsid) {
        if (!visited[s]) continue;
        vector<int> todo = {s};
        visited[s] = 0;
        int idx = 0;
        while (idx < todo.size()) {
          int v = todo[idx++];
          for (const int &x: rG[v]) {
            if (!visited[x]) continue;
            visited[x] = 0;
            todo.emplace_back(x);
          }
        }
        res.emplace_back(todo);
      }
      return res;
    }
  };
}  // namespace titan23

