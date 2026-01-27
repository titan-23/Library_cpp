#pragma once

#include <vector>
#include <cassert>
#include "titan_cpplib/graph/hld.cpp"
using namespace std;

namespace titan23 {

class RootedTree {
public:
    int root;
    vector<vector<int>> G;
    vector<int> size, par, dep, nodein, nodeout, head, hld;

    RootedTree() {}
    RootedTree(vector<vector<int>> &G, int root) : G(G), root(root) {
        titan23::HLD H(G, root);
        size    = H.size;
        par     = H.par;
        dep     = H.dep;
        nodein  = H.nodein;
        nodeout = H.nodeout;
        head    = H.head;
        hld     = H.hld;
    }

    int lca(int u, int v) const {
        while (true) {
            if (nodein[u] > nodein[v]) swap(u, v);
            if (head[u] == head[v]) return u;
            v = par[head[v]];
        }
    }

    //! `s` から `t` へ、`k` 個進んだ頂点を返す / `O(logn)`
    int path_kth_elm(int s, int t, int k) const {
        int x = lca(s, t);
        int d = dep[s] + dep[t] - 2*dep[x];
        if (d < k) return -1;
        if (dep[s] - dep[x] < k) {
            s = t;
            k = d - k;
        }
        int hs = head[s];
        while (dep[s] - dep[hs] < k) {
            k -= dep[s] - dep[hs] + 1;
            s = par[hs];
            hs = head[s];
        }
        return hld[nodein[s] - k];
    }

    int dist(int u, int v) const {
        return dep[u] + dep[v] - 2 * dep[lca(u, v)];
    }

    // Return True if (a is on path(u - v)) else False. / O(logN)
    bool is_on_path(int u, int v, int a) const {
        return dist(u, a) + dist(a, v) == dist(u, v);
    }

    // vはtの祖先か？ / O(1)
    bool is_ancestor(int v, int t) const {
        return nodein[v] <= nodein[t] && nodeout[t] <= nodeout[v];
    }

    // vはtの子孫か？ / O(1)
    bool is_descendant(int v, int t) const {
        return nodein[t] <= nodein[v] && nodeout[v] <= nodeout[t];
    }

    // Pの頂点をすべて通るパスが存在するか？ / O(|P|+logN)
    bool is_passable_path(const vector<int> &P) const {
        int k = P.size();
        vector<bool> seen(k, false);
        int prev_maxv = -1;
        for (int i = 0; i < 2; ++i) {
            int max_dep = -1, max_v = -1;
            for (int j = 0; j < k; ++j) {
                if (seen[j]) continue;
                if (dep[P[j]] > max_dep) {
                    max_dep = dep[P[j]];
                    max_v = P[j];
                }
            }
            if (max_v == -1) break;
            if (i == 1) {
                int u = lca(max_v, prev_maxv);
                for (int v : P) {
                    if (v == u) continue;
                    if (is_ancestor(v, u)) return false;
                }
            }
            for (int j = 0; j < k; ++j) {
                if (P[j] == max_v || is_ancestor(P[j], max_v)) {
                    seen[j] = true;
                }
            }
            prev_maxv = max_v;
        }
        for (int i = 0; i < k; ++i) {
            if (!seen[i]) return false;
        }
        return true;
    }
};
} // nsmaepace titan23
