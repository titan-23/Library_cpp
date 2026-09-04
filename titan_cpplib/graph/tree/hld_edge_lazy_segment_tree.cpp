/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/tree/hld_edge_lazy_segment_tree.cpp
#pragma once

#include <vector>
#include "titan_cpplib/graph/tree/hld.cpp"
#include "titan_cpplib/ds/lazy_segment_tree.cpp"
using namespace std;

namespace titan23 {

/// @brief 遅延セグ木搭載HLD（辺属性）/ 非可換に対応
template<class T,
        T (*op)(T, T),
        T (*e)(),
        class F,
        T (*mapping)(F, T),
        F (*composition)(F, F),
        F (*id)()>
class HLDEdgeLazySegmentTree {
private:
    titan23::HLD hld;
    titan23::LazySegmentTree<T, op, e, F, mapping, composition, id> seg, rseg;

public:
    HLDEdgeLazySegmentTree() {}

    HLDEdgeLazySegmentTree(titan23::HLD &hld, int n) : hld(hld) {
        this->seg = titan23::LazySegmentTree<T, op, e, F, mapping, composition, id>(n);
        this->rseg = titan23::LazySegmentTree<T, op, e, F, mapping, composition, id>(n);
    }

    /// @brief a[k] を辺 (k, par[k]) の値とみなして構築する。a[root] は使われない / O(N)
    HLDEdgeLazySegmentTree(titan23::HLD &hld, vector<T> a) : hld(hld) {
        vector<T> b = hld.build_list(a);
        this->seg = titan23::LazySegmentTree<T, op, e, F, mapping, composition, id>(b);
        reverse(b.begin(), b.end());
        this->rseg = titan23::LazySegmentTree<T, op, e, F, mapping, composition, id>(b);
    }

    /// @brief 重み付き隣接リスト G と根から構築する
    /// 各辺の重みを深い側の頂点に持たせる / O(N)
    HLDEdgeLazySegmentTree(const vector<vector<pair<int, T>>> &G, const int root) {
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
        this->seg = titan23::LazySegmentTree<T, op, e, F, mapping, composition, id>(b);
        reverse(b.begin(), b.end());
        this->rseg = titan23::LazySegmentTree<T, op, e, F, mapping, composition, id>(b);
    }

    /// @brief u から v へのパス上の辺の総積を返す / O(logN)
    T path_prod(int u, int v) {
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

    /// @brief u から v へのパス上の辺に f を作用させる / O(logN)
    void path_apply(int u, int v, F f) {
        while (hld.head[u] != hld.head[v]) {
            if (hld.dep[hld.head[u]] < hld.dep[hld.head[v]]) swap(u, v);
            seg.apply(hld.nodein[hld.head[u]], hld.nodein[u] + 1, f);
            rseg.apply(hld.n - hld.nodein[u] - 1, hld.n - hld.nodein[hld.head[u]], f);
            u = hld.par[hld.head[u]];
        }
        if (hld.dep[u] < hld.dep[v]) swap(u, v);
        if (u != v) {
            seg.apply(hld.nodein[v] + 1, hld.nodein[u] + 1, f);
            rseg.apply(hld.n - hld.nodein[u] - 1, hld.n - hld.nodein[v] - 1, f);
        }
    }

    /// @brief 辺 (k, par[k]) の値を返す / O(logN)
    T get(int k) {
        return seg.get(hld.nodein[k]);
    }

    /// @brief 辺 (k, par[k]) の値を v に更新する / O(logN)
    void set(int k, T v) {
        seg.set(hld.nodein[k], v);
        rseg.set(hld.n - hld.nodein[k] - 1, v);
    }

    /// @brief 辺 (u, v) の値を x に更新する。u, v は親子であること / O(logN)
    void set_edge(int u, int v, T x) {
        set(hld.dep[u] > hld.dep[v] ? u : v, x);
    }

    /// @brief 頂点 v の部分木に含まれる辺の総積を返す / O(logN)
    T subtree_prod(int v) {
        return seg.prod(hld.nodein[v] + 1, hld.nodeout[v]);
    }

    /// @brief 頂点 v の部分木に含まれる辺に f を作用させる / O(logN)
    void subtree_apply(int v, F f) {
        seg.apply(hld.nodein[v] + 1, hld.nodeout[v], f);
        rseg.apply(hld.n - hld.nodeout[v], hld.n - hld.nodein[v] - 1, f);
    }
};
} // namespace titan23
