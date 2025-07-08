#include <vector>
#include <stack>
using namespace std;

namespace titan23 {


/*
apply_vertex(dp_x: T, v: int) -> T:
       v          } return
 --------------   }
|  /   |   \  |   }
| o    o    o | } dp_x (mergeしたもの)
| △   △   △ |
 --------------

apply_edge(dp_x: T, e: E, x: int, v: int) -> T:
  v      } return
  | } e
  x | } dp_x
  △|

using T = int; // dpの型
using E = int; // 辺の型
T apply_vertex(T dp_x, int v) {}
T merge(T s, T t) {}
T apply_edge(T dp_x, E edge, int x, int v) {}
T e() {}
*/

template<typename E,
        typename T, T (*merge)(T, T),
        T (*apply_vertex)(T, int),
        T (*apply_edge)(T, E, int, int),
        T (*e)()>
vector<T> rerooting_dp(const vector<vector<pair<int, E>>> G) {
    int n = G.size();

    vector<vector<T>> dp(n);
    for (int i = 0; i < n; ++i) {
        dp[i].resize(G[i].size());
    }
    vector<T> ans(n, e());

    int root = 0;
    vector<int> par(n, -2);
    par[root] = -1;
    vector<int> toposo = {root};
    stack<int> todo;
    todo.push(root);
    while (!todo.empty()) {
        int v = todo.top(); todo.pop();
        for (auto &[x, _] : G[v]) {
            if (par[x] != -2) continue;
            par[x] = v;
            toposo.emplace_back(x);
            todo.push(x);
        }
    }

    vector<T> arr(n, e());
    for (int idx = n-1; idx >= 0; --idx) {
        int v = toposo[idx];
        T acc = e();
        for (int i = 0; i < G[v].size(); ++i) {
            auto [x, c] = G[v][i];
            if (x == par[v]) continue;
            dp[v][i] = apply_edge(arr[x], c, x, v);
            acc = merge(acc, dp[v][i]);
        }
        arr[v] = apply_vertex(acc, v);
    }

    vector<T> dp_par(n, e()), acc_l(n+1, e()), acc_r(n+1, e());
    for (int v : toposo) {
        for (int i = 0; i < G[v].size(); ++i) {
            int x = G[v][i].first;
            if (x == par[v]) {
                dp[v][i] = dp_par[v];
                break;
            }
        }
        int d = dp[v].size();
        for (int i = 0; i < d; ++i) {
            acc_l[i+1] = merge(acc_l[i], dp[v][i]);
            acc_r[i+1] = merge(acc_r[i], dp[v][d-i-1]);
        }
        ans[v] = apply_vertex(acc_l[d], v);
        for (int i = 0; i < G[v].size(); ++i) {
            auto [x, c] = G[v][i];
            if (x == par[v]) continue;
            dp_par[x] = apply_edge(apply_vertex(merge(acc_l[i], acc_r[d-i-1]), v), c, v, x);
        }
    }
    return ans;
}

} // namespace titan23
