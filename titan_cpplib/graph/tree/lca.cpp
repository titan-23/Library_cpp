/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/tree/lca.cpp
#pragma once

#include <vector>
#include <cassert>
#include <stack>
#include <climits>
#include "titan_cpplib/ds/static_RmQ.cpp"
using namespace std;

namespace titan23 {

/// @brief 静的な木の LCA を求める / 前計算 O(n)、クエリ O(1)
class LCA {
private:
    int n;
    vector<int> path, nodein;
    titan23::StaticRmQ<int> st;

public:
    LCA() : n(0) {}

    /// @brief 隣接リスト G と根 root から前計算する / O(n)
    LCA(const vector<vector<int>> &G, const int root) :
            n((int)G.size()), path(n), nodein(n, -1) {
        vector<int> par(n, -1);
        int time = -1, ptr = 0;
        vector<int> s(n);
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
        st = titan23::StaticRmQ<int>(a, INT_MAX);
    }

    /// @brief u と v の LCA を返す / O(1)
    int lca(const int u, const int v) const {
        if (u == v) return u;
        int l = nodein[u], r = nodein[v];
        if (l > r) swap(l, r);
        return path[st.prod(l, r)];
    }

    /// @brief 頂点集合 a の LCA を返す / a は空でないこと / O(|a|)
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
}  // namespace titan23
