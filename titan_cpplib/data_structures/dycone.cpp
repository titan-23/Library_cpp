#include <bits/stdc++.h>
using namespace std;

// https://judge.yosupo.jp/submission/214503
template<typename T>
class OfflineDynamicConnectivity {
private:
  static constexpr int ADD = 0, DEL = 1, ADD_POINT = 2, GET_SUM = 3, GET_COUNT = 4, GET_SIZE = 5, IS_SAME = 6;

  int n, q, t, group_count;
  vector<tuple<int, int, int, T>> Q;
  vector<int> P, W, S, rd, sz;
  vector<T> sum;
  unordered_map<long long, int> mp;

  int find(int u, int w = 0) {
    while (W[u] <= w) {
      while (W[P[u]] <= W[u]) {
        sum[P[u]] -= sum[u];
        sz[P[u]] -= sz[u];
        P[u] = P[P[u]];
      }
      u = P[u];
    }
    return u;
  }

  void disconnect(int u) {
    if (P[u] == u) return;
    disconnect(P[u]);
    sum[P[u]] -= sum[u];
    sz[P[u]] -= sz[u];
  }

  int connect(int u, int w = 0) {
    while (W[u] <= w) {
      sum[P[u]] += sum[u];
      sz[P[u]] += sz[u];
      u = P[u];
    }
    return u;
  }

  int max_edge(int u, int v) {
    if (find(u) != find(v)) return -1;
    while (1) {
      if (W[u] > W[v]) swap(u, v);
      if (P[u] == v) break;
      u = P[u];
    }
    return u;
  }

  void sub_add(int u, int v, int w) {
    disconnect(u);
    disconnect(v);
    if (find(u) == find(v)) {
      connect(u);
      connect(v);
      return;
    }
    group_count--;
    while (u != v) {
      u = connect(u, w);
      v = connect(v, w);
      if (rd[u] < rd[v]) swap(u, v);
      int np = P[v];
      int nw = W[v];
      P[v] = u;
      W[v] = w;
      u = np;
      w = nw;
    }
    connect(u);
  }

  void sub_del(int u, int v, int w) {
    while (P[u] != u) {
      if (W[u] == w) {
        int x = u;
        while (P[x] != x) {
          x = P[x];
          sum[x] -= sum[u];
          sz[x] -= sz[u];
        }
        P[u] = u;
        W[u] = 1;
        if (find(u) != find(v)) group_count++;
        return;
      }
      while (W[P[u]] <= W[u]) {
        sum[P[u]] -= sum[u];
        sz[P[u]] -= sz[u];
        P[u] = P[P[u]];
      }
      u = P[u];
    }
  }

  auto inner_add_edge(int u, int v, int w) {
    int p = max_edge(u, v);
    if (p == -1) {
      sub_add(u, v, w);
    } else if (W[p] > w) {
      sub_del(p, P[p], W[p]);
      sub_del(P[p], p, W[p]);
      sub_add(u, v, w);
    }
  }

  void inner_delete_edge(int u, int v, int w) {
    sub_del(u, v, w);
    sub_del(v, u, w);
  }

  void inner_add_point(int u, T v) {
    while (1) {
      sum[u] += v;
      if (u == P[u]) break;
      u = P[u];
    }
  }

  T inner_get_sum(int u) { return sum[find(u)]; }

public:
  OfflineDynamicConnectivity(int n, int q)
    : n(n), q(q), t(0), group_count(n), P(n), W(n), sz(n), rd(n), sum(n), S(q) {
    // rep is used elsewhere in original code; keep initialization explicit here
    for (int i = 0; i < n; ++i) {
      P[i] = i;
      rd[i] = i;
      W[i] = 1;
      sz[i] = 1;
    }
    for (int i = 0; i < q; ++i) S[i] = -INT_MAX;
    shuffle(rd.begin(), rd.end(), mt19937(1321312));
  }

  int add_edge(int u, int v) {
    assert(t < q);
    if (u > v) swap(u, v);
    Q.emplace_back(ADD, u, v, -1);
    mp[(long long)u * n + v] = t;
    return t++;
  }

  int delete_edge(int u, int v) {
    assert(t < q);
    if (u > v) swap(u, v);
    Q.emplace_back(DEL, u, v, -1);
    S[mp[(long long)u * n + v]] = -t;
    S[t] = -t;
    return t++;
  }

  int add_point(int u, T v) {
    assert(t < q);
    Q.emplace_back(ADD_POINT, u, -1, v);
    return t++;
  }

  int get_sum(int u) {
    assert(t < q);
    Q.emplace_back(GET_SUM, u, -1, -1);
    return t++;
  }

  int get_component_count() {
    assert(t < q);
    Q.emplace_back(GET_COUNT, -1, -1, -1);
    return t++;
  }

  int get_size(int u) {
    assert(t < q);
    Q.emplace_back(GET_SIZE, u, -1, -1);
    return t++;
  }

  int is_same(int u, int v) {
    assert(t < q);
    Q.emplace_back(IS_SAME, u, v, -1);
    return t++;
  }

  vector<T> run() {
    assert(t <= q);
    vector<T> res(Q.size());
    for (size_t i = 0; i < Q.size(); ++i) {
      auto [type, u, v, w] = Q[i];
      switch (type) {
        case ADD: {
          inner_add_edge(u, v, S[i]);
          break;
        }
        case DEL: {
          inner_delete_edge(u, v, S[i]);
          break;
        }
        case ADD_POINT: {
          inner_add_point(u, w);
          break;
        }
        case GET_SUM: {
          res[i] = inner_get_sum(u);
          break;
        }
        case GET_COUNT: {
          res[i] = group_count;
          break;
        }
        case GET_SIZE: {
          res[i] = sz[find(u)];
          break;
        }
        case IS_SAME: {
          res[i] = find(u) == find(v);
          break;
        }
        default:
          assert(false);
      }
    }
    return res;
  }
};
