#include <iostream>
#include <vector>
#include <stack>
using namespace std;

namespace titan23 {

  struct HLD {
    vector<vector<int>> G;
    vector<int> size, par, dep, nodein, nodeout, head, hld;
    int root, n;

    HLD () {}

    HLD(vector<vector<int>> G, int root) {
      this->G = G;
      this->root = root;
      this->n = (int)G.size();
      size.resize(n, 1);
      par.resize(n, -1);
      dep.resize(n, -1);
      nodein.resize(n, -1);
      nodeout.resize(n, -1);
      head.resize(n, -1);
      hld.resize(n, -1);
      _dfs();
    }

    void _dfs() {
      dep[root] = 0;
      stack<int> st;
      st.push(root);
      while (!st.empty()) {
        int v = st.top(); st.pop();
        if (v >= 0) {
          int dep_nxt = dep[v] + 1;
          for (const int x: G[v]) {
            if (dep[x] != -1) continue;
            dep[x] = dep_nxt;
            st.push(~x);
            st.push(x);
          }
        } else {
          v = ~v;
          for (int i = 0; i < (int)G[v].size(); ++i) {
            int x = G[v][i];
            if (dep[x] < dep[v]) {
              par[v] = x;
              continue;
            }
            size[v] += size[x];
            if (size[x] > size[G[v][0]]) {
              swap(G[v][0], G[v][i]);
            }
          }
        }
      }

      int curtime = 0;
      st.push(~root);
      st.push(root);
      while (!st.empty()) {
        int v = st.top(); st.pop();
        if (v >= 0) {
          if (par[v] == -1) {
            head[v] = v;
          }
          nodein[v] = curtime;
          hld[curtime] = v;
          ++curtime;
          if (G[v].empty()) continue;
          int G_v0 = (int)G[v][0];
          for (int i = (int)G[v].size()-1; i >= 0; --i) {
            int x = G[v][i];
            if (x == par[v]) continue;
            head[x] = (x == G_v0? head[v]: x);
            st.push(~x); 
            st.push(x); 
          }
        } else {
          nodeout[~v] = curtime;          
        }
      }
    }

    template<typename T>
    vector<T> build_list(vector<T> a) {
      vector<T> res(a.size());
      for (int i = 0; i < n; ++i) {
        res[i] = a[hld[i]];
      }
      return res;
    }

    int lca(int u, int v) {
      while (true) {
        if (nodein[u] > nodein[v]) {
          swap(u, v);
        }
        if (head[u] == head[v]) {
          return u;
        }
        v = par[head[v]];
      }
    }
  };
}  // namespace titan23
