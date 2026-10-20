/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/tree/dsu_on_tree.cpp
#pragma once

#include <vector>
#include <cassert>
using namespace std;

// DSUonTree
namespace titan23 {

class DSUonTree {
private:
    vector<vector<int>> G;
    int n, t, root;
    vector<int> sz, path, in, out;

    int dfs1(int v, int p=-1) {
        if (G[v].size() >= 2 && G[v][0] == p) swap(G[v][0], G[v][1]);
        for (int &x : G[v]) if (x != p) { // need `&`
            sz[v] += dfs1(x, v);
            if (sz[x] > sz[G[v][0]]) swap(x, G[v][0]);
        }
        return sz[v];
    }

    void dfs2(int v, int p=-1) {
        path[t] = v;
        in[v] = t++;
        for (int x : G[v]) if (x != p) dfs2(x, v);
        out[v] = t;
    }

public:
    DSUonTree(vector<vector<int>> G, int root) : G(G), n(G.size()), t(0), root(root), sz(n, 1), path(n), in(n), out(n) { dfs1(root); dfs2(root); }

    int idx(int u) const { return in[u]; }

    void run(const auto &update, const auto &query, const auto &clear, const auto &reset) {
        auto dsu = [&](auto dsu, int v, int p=-1, bool keep=false) -> void {
            for (int i = 0; i < (int)G[v].size(); ++i) if (i && G[v][i] != p) {
                dsu(dsu, G[v][i], v, false);
            }
            if (sz[v] != 1) {
                dsu(dsu, G[v][0], v, true);
                for (int i = out[G[v][0]]; i < out[v]; i++) update(path[i]);
            }
            update(v); query(v);
            if (keep) return;
            for (int i = in[v]; i < out[v]; i++) clear(path[i]);
            reset();
        };
        dsu(dsu, root);
    }
};

} // namespace titan23
