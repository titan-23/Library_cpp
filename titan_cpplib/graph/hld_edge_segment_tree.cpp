#pragma once

#include <vector>
#include "titan_cpplib/graph/hld.cpp"
#include "titan_cpplib/ds/segment_tree.cpp"
using namespace std;

namespace titan23 {

/// @brief セグ木搭載HLD（辺属性）/ 非可換に対応
template<class T, T (*op)(T, T), T (*e)()>
class HLDEdgeSegmentTree {
private:
    titan23::HLD hld;
    titan23::SegmentTree<T, op, e> seg, rseg;

public:
    HLDEdgeSegmentTree(const titan23::HLD &hld) : hld(hld), seg(hld.n), rseg(hld.n) {}

    /// @brief a[k] を辺 (k, par[k]) の値とみなして構築する。a[root] は使われない / O(N)
    HLDEdgeSegmentTree(const titan23::HLD &hld, const vector<T> &a) : hld(hld) {
        vector<T> b = hld.build_list(a);
        this->seg = titan23::SegmentTree<T, op, e>(b);
        reverse(b.begin(), b.end());
        this->rseg = titan23::SegmentTree<T, op, e>(b);
    }

    /// @brief 重み付き隣接リスト G と根から構築する。各辺の重みを深い側の頂点に持たせる / O(N)
    HLDEdgeSegmentTree(const vector<vector<pair<int, T>>> &G, const int root) {
        int n = G.size();
        vector<vector<int>> g(n);
        for (int v = 0; v < n; ++v) {
            for (const auto &[x, w] : G[v]) {
                g[v].emplace_back(x);
            }
        }
        this->hld = titan23::HLD(g, root);
        vector<T> a(n, e());
        for (int v = 0; v < n; ++v) {
            for (const auto &[x, w] : G[v]) {
                if (x == hld.par[v]) a[v] = w;
            }
        }
        vector<T> b = hld.build_list(a);
        this->seg = titan23::SegmentTree<T, op, e>(b);
        reverse(b.begin(), b.end());
        this->rseg = titan23::SegmentTree<T, op, e>(b);
    }

    /// @brief u から v へのパス上の辺の総積を返す / O(logN)
    T path_prod(int u, int v) const {
        T lres = e(), rres = e();
        while (hld.head[u] != hld.head[v]) {
            if (hld.dep[hld.head[u]] > hld.dep[hld.head[v]]) {
                lres = op(lres, rseg.prod(hld.n - hld.nodein[u] - 1, hld.n - hld.nodein[hld.head[u]]));
                u = hld.par[hld.head[u]];
            } else {
                rres = op(seg.prod(hld.nodein[hld.head[v]], hld.nodein[v] + 1), rres);
                v = hld.par[hld.head[v]];
            }
        }
        if (hld.dep[u] > hld.dep[v]) {
            lres = op(lres, rseg.prod(hld.n - hld.nodein[u] - 1, hld.n - hld.nodein[v] - 1));
        } else {
            lres = op(lres, seg.prod(hld.nodein[u] + 1, hld.nodein[v] + 1));
        }
        return op(lres, rres);
    }

    /// @brief 辺 (k, par[k]) の値を返す / O(1)
    T get(const int k) const {
        return seg.get(hld.nodein[k]);
    }

    /// @brief 辺 (k, par[k]) の値を v に更新する / O(logN)
    void set(const int k, const T v) {
        seg.set(hld.nodein[k], v);
        rseg.set(hld.n - hld.nodein[k] - 1, v);
    }

    /// @brief 辺 (u, v) の値を x に更新する。u, v は親子であること / O(logN)
    void set_edge(const int u, const int v, const T x) {
        set(hld.dep[u] > hld.dep[v] ? u : v, x);
    }

    /// @brief 頂点 v の部分木に含まれる辺の総積を返す / O(logN)
    T subtree_prod(const int v) const {
        return seg.prod(hld.nodein[v] + 1, hld.nodeout[v]);
    }
};
}  // namespace titan23
