#include <iostream>
#include <vector>
#include <cassert>
#include <stack>
#include "../DataStructures/SparseTable.cpp"
using namespace std;

namespace titan23 {

  int __LCA_op(int s, int t) { return min(s, t); }

  int __LCA_e() { return (int)1e9; }

  struct LCA {
    vector<int> path, nodein, depth;
    int bit, msk, n;
    SparseTable<int, __LCA_op, __LCA_e> st;

    LCA() {}

    LCA(const vector<vector<int>> &G, int root) {
      this->n = (int)G.size();

      path.resize(2*n);
      nodein.resize(n);
      depth.resize(n, -1);
      bit = 32 - __builtin_clz(n) + 1;
      msk = (1 << bit) - 1;

      int time = -1;
      stack<int> s;
      s.push(~root);
      s.push(root);
      depth[root] = 0;
      while (!s.empty()) {
        ++time;
        int v = s.top();
        s.pop();
        if (v >= 0) {
          nodein[v] = time;
          path[time] = v;
          for (const int &x: G[v]) {
            if (depth[x] != -1) continue;
            depth[x] = depth[v] + 1;
            s.push(~v);
            s.push(x);
          }
        } else {
          path[time] = ~v;
        }
      }
      vector<int> a(2*n);
      for (int i = 0; i < 2*n; ++i) {
        a[i] = (depth[path[i]]) << bit | i;
      }
      SparseTable<int, __LCA_op, __LCA_e> st(a);
      this->st = st;
    }
    
    int lca(const int u, const int v) const {
      int l = nodein[u], r = nodein[v];
      if (l > r) swap(l, r);
      return path[st.prod(l, r+1)&msk];
    }

    int lca_mul(const vector<int> &a) const {
      int l = n*2+1;
      int r = -l;
      for (const int &e: a) {
        int x = nodein[e];
        if (l > x) l = x;
        if (r < x) r = x;
      }
      return path[st.prod(l, r+1)&msk];
    }
  };
} // namspace titan23
