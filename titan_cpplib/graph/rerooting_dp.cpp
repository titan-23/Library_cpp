#include <vector>
#include <stack>
using namespace std;

namespace titan23 {

template<typename E,
        typename T, T (*merge)(T, T),
        T (*apply_vertex)(T, int),
        T (*apply_edge)(T, E, int, int),
        T (*e)()>
vector<T> rerooting_dp(const vector<vector<pair<int, E>>> G) {
    int n = G.size();
    vector<vector<T>> dp(n);
    for (int i = 0; i < n; ++i) {
        dp[i].resize(G[i].size(), e());
    }
    int root = 0;
    vector<int> pdx(n, -1);
    vector<int> topo;
    stack<int> s;
    pdx[root] = -2;
    s.emplace(root);
    while (!s.empty()) {
        int v = s.top(); s.pop();
        topo.emplace_back(v);
        for (int i = 0; i < G[v].size(); ++i) {
            int x = G[v][i].first;
            if (pdx[x] != -1) {
                pdx[v] = i;
                continue;
            }
            s.emplace(x);
        }
    }

    vector<T> ans(n), rs(n+1, e());
    for (int idx = n-1; idx >= 0; --idx) {
        int v = topo[idx];
        T ls = e();
        for (int i = 0; i < G[v].size(); ++i) if (i != pdx[v]) {
            auto [x, c] = G[v][i];
            dp[v][i] = apply_edge(ans[x], c, x, v);
            ls = merge(ls, dp[v][i]);
        }
        ans[v] = apply_vertex(ls, v);
    }
    for (int v : topo) {
        int d = G[v].size();
        for (int i = 0; i < d; ++i) {
            rs[i+1] = merge(rs[i], dp[v][d-i-1]);
        }
        T ls = e();
        for (int i = 0; i < d; ++i) {
            if (i != pdx[v]) {
                auto [x, c] = G[v][i];
                dp[x][pdx[x]] = apply_edge(apply_vertex(merge(ls, rs[d-i-1]), v), c, v, x);
            }
            ls = merge(ls, dp[v][i]);
        }
        ans[v] = apply_vertex(rs[d], v);
    }
    return ans;
}

/*
T apply_vertex(T dp_x, int v) {}
       v            } return
 --------------   }
|  /   |   \  |   } dp_x (mergeしたもの)
| o    o    o |
| △   △   △ |
 --------------

T apply_edge(T dp_x, E e, int x, int v) {}
  v
  | } e       } return
  x | } dp_x  }
  △|

using T = int; // dpの型
using E = int; // 辺の型

// 辺込みの部分木のdp値dp_xに頂点vを付加する
T apply_vertex(T dp_x, int v) {}

// 辺込みの部分木のdp値s,tを結合する
T merge(T s, T t) {}

// 部分木のdp値dp_xに辺edgeを付加する 頂点xの親がv
T apply_edge(T dp_x, E edge, int x, int v) {}

// dp値の単位元
T e() {}
*/
} // namespace titan23
