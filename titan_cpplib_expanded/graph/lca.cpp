// #include "titan_cpplib/graph/lca.cpp"
#include <vector>
#include <cassert>
#include <stack>
#include <climits>
// #include "titan_cpplib/data_structures/sparse_table.cpp"
#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

// SparseTable
namespace titan23 {

    template <class T,
              T (*op)(T, T),
              T (*e)()>
    struct SparseTable {
      private:
        int n;
        vector<vector<T>> data;

      public:
        SparseTable() {}
        SparseTable(vector<T> &a) : n((int)a.size()) {
            int log = 32 - __builtin_clz(n) - 1;
            data.resize(log+1);
            data[0] = a;
            for (int i = 0; i < log; ++i) {
                int l = 1 << i;
                const vector<int> &pre = data[i];
                vector<int> &nxt = data[i+1];
                int s = pre.size();
                nxt.resize(s-l);
                for (int j = 0; j < s-l; ++j) {
                    nxt[j] = op(pre[j], pre[j+l]);
                }
            }
        }

        T prod(const int l, const int r) const {
            assert(0 <= l && l <= r && r < n);
            if (l == r) return e();
            int u = 32 - __builtin_clz(r-l) - 1;
            return op(data[u][l], data[u][r-(1<<u)]);
        }

        T get(const int k) const {
            assert(0 <= k && k < n);
            return data[0][k];
        }

        int len() const {
            return n;
        }

        void print() const {
            cout << '[';
            for (int i = 0; i < n-1; ++i) {
                cout << data[0][i] << ", ";
            }
            if (n > 0) {
                cout << data[0][n-1];
            }
            cout << ']' << endl;
        }
    };
}  // namespace titan23

using namespace std;

// LCA
namespace titan23 {

    /**
     * @brief 静的なグラフに対しLCAを求めるだけのライブラリ
     * <O(nlogn), O(1)>
     */
    class LCA {
      private:
        static int __LCA_op(int s, int t) { return min(s, t); }
        static int __LCA_e() { return INT_MAX; }
        int n;
        vector<int> path, nodein, par;
        SparseTable<int, __LCA_op, __LCA_e> st;

      public:
        LCA() {}

        //! 隣接リスト `G` 、根 `root` として前計算をする / `O(nlogn)`
        LCA(const vector<vector<int>> &G, const int root) :
                n((int)G.size()), path(n), nodein(n, -1), par(n, -1) {
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
            st = SparseTable<int, __LCA_op, __LCA_e>(a);
        }

        //! `u`, `v` の lca を求める / `O(1)`
        int lca(const int u, const int v) const {
            if (u == v) return u;
            int l = nodein[u], r = nodein[v];
            if (l > r) swap(l, r);
            return path[st.prod(l, r)];
        }

        //! 頂点集合 `a` の lca を求める / `O(|a|)`
        int lca_mul(const vector<int> &a) const {
            assert(!a.empty());
            int l = n*2+1;
            int r = -l;
            for (const int &e: a) {
                int x = nodein[e];
                if (l > x) l = x;
                if (r < x) r = x;
            }
            if (l == r) return a[0];
            return path[st.prod(l, r)];
        }
    };
}  // namspace titan23

