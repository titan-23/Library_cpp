#pragma once

#include <vector>
#include <stack>
using namespace std;

// HLD
namespace titan23 {

class HLD {
public:
    vector<vector<int>> G;
    int root, n;
    vector<int> size, par, dep, nodein, nodeout, head, hld;

private:
    void _dfs() {
        dep[root] = 0;
        stack<int> st;
        st.emplace(~root);
        st.emplace(root);
        while (!st.empty()) {
            int v = st.top(); st.pop();
            if (v >= 0) {
                int dep_nxt = dep[v] + 1;
                for (const int x: G[v]) {
                    if (dep[x] != -1) continue;
                    dep[x] = dep_nxt;
                    st.emplace(~x);
                    st.emplace(x);
                }
            } else {
                v = ~v;
                for (int i = 0; i < (int)G[v].size(); ++i) {
                    if (dep[G[v][i]] < dep[v]) {
                        par[v] = G[v][i];
                        swap(G[v][i], G[v].back());
                        G[v].pop_back();
                        break;
                    }
                }
                for (int i = 0; i < (int)G[v].size(); ++i) {
                    int x = G[v][i];
                    size[v] += size[x];
                    if (size[x] > size[G[v][0]]) {
                        swap(G[v][0], G[v][i]);
                    }
                }
            }
        }

        int curtime = 0;
        st.emplace(~root);
        st.emplace(root);
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
                    head[x] = (x == G_v0? head[v]: x);
                    st.emplace(~x);
                    st.emplace(x);
                }
            } else {
                nodeout[~v] = curtime;
            }
        }
    }

public:
    HLD() {}
    HLD(const vector<vector<int>> &G, const int root) :
            G(G), root(root), n(G.size()),
            size(n, 1), par(n, -1), dep(n, -1),
            nodein(n, -1), nodeout(n, -1), head(n, -1), hld(n, -1) {
        _dfs();
    }

    template<typename T>
    vector<T> build_list(vector<T> a) const {
        vector<T> res(a.size());
        for (int i = 0; i < n; ++i) {
            res[i] = a[hld[i]];
        }
        return res;
    }

    //! `u`, `v` の lca を返す / `O(logn)`
    int lca(int u, int v) const {
        while (true) {
            if (nodein[u] > nodein[v]) swap(u, v);
            if (head[u] == head[v]) return u;
            v = par[head[v]];
        }
    }

    int get(int v) const {
        return nodein[v];
    }

    vector<pair<int, int>> for_each_vertex_path(int u, int v) const {
        vector<pair<int, int>> res;
        while (head[u] != head[v]) {
            if (dep[head[u]] < dep[head[v]]) swap(u, v);
            res.emplace_back(nodein[head[u]], nodein[u]+1);
            u = par[head[u]];
        }
        if (dep[u] < dep[v]) swap(u, v);
        res.emplace_back(nodein[v], nodein[u]+1);
        return res;
    }

    pair<int, int> for_each_vertex_subtree(int v) const {
        return {nodein[v], nodeout[v]};
    }

    int dist(int u, int v) const {
        return dep[u] + dep[v] - 2 * dep[lca(u, v)];
    }

    int la(int v, int k) const {
        if (k < 0 || dep[v] < k) return -1;
        while (1) {
            int h = head[v];
            int dist_to_head = dep[v] - dep[h];
            if (k <= dist_to_head) {
                return hld[nodein[v] - k];
            }
            k -= dist_to_head + 1;
            v = par[h];
        }
    }

    int path_kth_elm(int s, int t, int k) const {
        int l = lca(s, t);
        int d = dep[s] + dep[t] - 2 * dep[l];
        if (k < 0 || k > d) return -1;
        if (k <= dep[s] - dep[l]) {
            return la(s, k);
        } else {
            return la(t, d - k);
        }
    }
};
}  // namespace titan23
