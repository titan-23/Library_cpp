#include <iostream>
#include <vector>
#include <cassert>
#include <stack>
#include "titan_cpplib/data_structures/sparse_table.cpp"
using namespace std;

namespace titan23 {

  int __LCA_op(int s, int t) { return min(s, t); }

  int __LCA_e() { return 1000000000; }

  struct LCA {
    int n;
    vector<int> path, nodein, par;
    SparseTable<int, __LCA_op, __LCA_e> st;

    LCA() {}

    LCA(const vector<vector<int>> &G, int root) {
      n = (int)G.size();
      path.resize(n);
      nodein.resize(n, -1);
      par.resize(n, -1);
      int time = -1, ptr = 0;
      int s[n];
      s[ptr++] = root;
      while (ptr) {
        int v = s[--ptr];
        if (time >= 0) {
          path[time] = par[v];
        }
        ++time;
        nodein[v] = time;
        for (const int &x: G[v]) {
          if (nodein[x] != -1) continue;
          par[x] = v;
          s[ptr++] = x;
        }
      }
      vector<int> a(n);
      for (int i = 0; i < n; ++i) {
        a[i] = nodein[path[i]];
      }
      SparseTable<int, __LCA_op, __LCA_e> st(a);
      this->st = st;
    }
    
    int lca(const int u, const int v) const {
      int l = nodein[u], r = nodein[v];
      if (l > r) swap(l, r);
      return path[st.prod(l, r)];
    }

    int lca_mul(const vector<int> &a) const {
      int l = n*2+1;
      int r = -l;
      for (const int &e: a) {
        int x = nodein[e];
        if (l > x) l = x;
        if (r < x) r = x;
      }
      return path[st.prod(l, r)];
    }
  };
}  // namspace titan23
