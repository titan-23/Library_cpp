#include <bits/stdc++.h>
using namespace std;

// https://judge.yosupo.jp/submission/214503
template<typename T>
class OfflineDynamicConnectivity {
private:
    static const int ADD = 0;
    static const int DEL = 1;
    static const int ADD_POINT = 2;
    static const int GET_SUM = 3;

    int n, q, t;
    vector<tuple<int, int, int, T>> Q;
    vector<int> P, W, S, rd;
    vector<T> sum;
    unordered_map<long long, int> mp;

    int find(int u, int w=0) {
        while (W[u] <= w) {
            while (W[P[u]] <= W[u]) {
                sum[P[u]] -= sum[u];
                P[u] = P[P[u]];
            }
            u = P[u];
        }
        return u;
    }

    void disconnect(int u) {
        if (P[u] != u) {
            disconnect(P[u]);
            sum[P[u]] -= sum[u];
        }
    }

    int connect(int u, int w=0) {
        while (W[u] <= w) {
            sum[P[u]] += sum[u];
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

    void sub_add_edge(int u, int v, int w) {
        disconnect(u);
        disconnect(v);
        while (u != v) {
            u = connect(u, w);
            v = connect(v, w);
            if (rd[u] < rd[v]) swap(u, v);
            int np = P[v], nw = W[v];
            P[v] = u;
            W[v] = w;
            u = np;
            w = nw;
        }
        connect(u);
    }

    void sub_delete_edge(int u, int w) {
        while (P[u] != u) {
            if (W[u] == w) {
                int v = u;
                while (P[v] != v) {
                    v = P[v];
                    sum[v] -= sum[u];
                }
                P[u] = u;
                W[u] = 1;
                return;
            }
            while (W[P[u]] <= W[u]) {
                sum[P[u]] -= sum[u];
                P[u] = P[P[u]];
            }
            u = P[u];
        }
    }

    auto inner_add_edge(int u, int v, int w) {
        int p = max_edge(u, v);
        if (p == -1) {
            sub_add_edge(u, v, w);
        } else if (W[p] > w) {
            sub_delete_edge(p, W[p]);
            sub_add_edge(u, v, w);
        }
    }

    void inner_delete_edge(int u, int v, int w) {
        sub_delete_edge(u, w);
        sub_delete_edge(v, w);
    }

    void inner_add_point(int u, T v) {
        while (1) {
            sum[u] += v;
            if (u == P[u]) break;
            u = P[u];
        }
    }

    T inner_get_sum(int u) {
        return sum[find(u)];
    }

public:
    OfflineDynamicConnectivity() {}
    OfflineDynamicConnectivity(int n, int q) : n(n), q(q), t(0), P(n), W(n), rd(n), sum(n), S(q) {
        for (int i = 0; i < n; ++i) {
            P[i] = i;
            W[i] = 1;
            rd[i] = i;
        }
        for (int i = 0; i < q; ++i) {
            S[i] = -INT_MAX;
        }
        uint64_t seed = 41435421;
        shuffle(rd.begin(), rd.end(), mt19937(seed));
    }

    void add_edge(int u, int v) {
        if (u > v) swap(u, v);
        Q.emplace_back(ADD, u, v, -1);
        mp[(long long)u*n+v] = t;
        t++;
    }

    void delete_edge(int u, int v) {
        if (u > v) swap(u, v);
        Q.emplace_back(DEL, u, v, -1);
        S[mp[(long long)u*n+v]] = -t;
        S[t] = -t;
        t++;
    }

    void add_point(int u, T v) {
        Q.emplace_back(ADD_POINT, u, -1, v);
        t++;
    }

    void get_sum(int u) {
        Q.emplace_back(GET_SUM, u, -1, -1);
        t++;
    }

    vector<T> run() {
        vector<T> res(Q.size());
        for (int i = 0; i < Q.size(); ++i) {
            auto [type, u, v, w] = Q[i];
            switch (type) {
                case ADD : {
                    inner_add_edge(u, v, S[i]);
                    break;
                }
                case DEL : {
                    inner_delete_edge(u, v, S[i]);
                    break;
                }
                case ADD_POINT : {
                    inner_add_point(u, w);
                    break;
                }
                case GET_SUM : {
                    res[i] = inner_get_sum(u);
                    break;
                }
                default: assert(false);
            }
        }
        return res;
    }
};

