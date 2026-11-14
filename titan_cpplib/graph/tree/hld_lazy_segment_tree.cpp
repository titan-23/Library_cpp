/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/tree/hld_lazy_segment_tree.cpp
#pragma once

#include <vector>
#include "titan_cpplib/graph/tree/hld.cpp"
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/ds/lazy_segment_tree.cpp"
using namespace std;

namespace titan23 {

/// @brief 遅延セグメント木を載せた HLD / op は非可換でもよい
template <class T,
        T (*op)(T, T),
        T (*e)(),
        class F,
        T (*mapping)(F, T),
        F (*composition)(F, F),
        F (*id)()>
class HLDLazySegmentTree {
private:
    titan23::HLD hld;
    titan23::LazySegmentTree<T, op, e, F, mapping, composition, id> seg, rseg;

public:
    HLDLazySegmentTree() {}

    /// @brief hld の上に、全頂点の値を e() として構築する / O(N)
    HLDLazySegmentTree(titan23::HLD &hld) : hld(hld) {
        seg = titan23::LazySegmentTree<T, op, e, F, mapping, composition, id>(hld.n);
        rseg = titan23::LazySegmentTree<T, op, e, F, mapping, composition, id>(hld.n);
    }

    /// @brief hld の上に、頂点 k の値を a[k] として構築する / O(N)
    HLDLazySegmentTree(titan23::HLD &hld, vector<T> a) : hld(hld) {
        vector<T> b = hld.build_list(a);
        seg = titan23::LazySegmentTree<T, op, e, F, mapping, composition, id>(b);
        reverse(b.begin(), b.end());
        rseg = titan23::LazySegmentTree<T, op, e, F, mapping, composition, id>(b);
    }

    /// @brief 隣接リスト G と根から構築する / O(N)
    HLDLazySegmentTree(const vector<vector<int>> &G, const int root) {
        hld = titan23::HLD(G, root);
        seg = titan23::LazySegmentTree<T, op, e, F, mapping, composition, id>(hld.n);
        rseg = titan23::LazySegmentTree<T, op, e, F, mapping, composition, id>(hld.n);
    }

    /// @brief 隣接リスト G と根から構築し、頂点 k の値を a[k] とする / O(N)
    HLDLazySegmentTree(const vector<vector<int>> &G, const int root, const vector<T> &a) {
        hld = titan23::HLD(G, root);
        vector<T> b = hld.build_list(a);
        seg = titan23::LazySegmentTree<T, op, e, F, mapping, composition, id>(b);
        reverse(b.begin(), b.end());
        rseg = titan23::LazySegmentTree<T, op, e, F, mapping, composition, id>(b);
    }

    /// @brief u から v へのパスの総積を返す / O(log^2 N)
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
            lres = op(lres, rseg.prod(hld.n - hld.nodein[u] - 1, hld.n - hld.nodein[v]));
        } else {
            lres = op(lres, seg.prod(hld.nodein[u], hld.nodein[v] + 1));
        }
        return op(lres, rres);
    }

    /// @brief u から v へのパス上の頂点に f を作用させる / O(log^2 N)
    void path_apply(int u, int v, F f) {
        while (hld.head[u] != hld.head[v]) {
            if (hld.dep[hld.head[u]] < hld.dep[hld.head[v]]) swap(u, v);
            seg.apply(hld.nodein[hld.head[u]], hld.nodein[u] + 1, f);
            rseg.apply(hld.n - (hld.nodein[u]) - 1, hld.n - hld.nodein[hld.head[u]], f);
            u = hld.par[hld.head[u]];
        }
        if (hld.dep[u] < hld.dep[v]) swap(u, v);
        seg.apply(hld.nodein[v], hld.nodein[u] + 1, f);
        rseg.apply(hld.n - (hld.nodein[u]) - 1, hld.n - hld.nodein[v], f);
    }

    /// @brief 頂点 k の値を返す / O(logN)
    T get(int k) {
        return seg.get(hld.nodein[k]);
    }

    /// @brief 頂点 k の値を v に更新する / O(logN)
    void set(int k, T v) {
        seg.set(hld.nodein[k], v);
        rseg.set(hld.n - hld.nodein[k] - 1, v);
    }

    /// @brief 頂点 v の部分木の総積を返す / O(logN)
    T subtree_prod(int v) {
        return seg.prod(hld.nodein[v], hld.nodeout[v]);
    }

    /// @brief 頂点 v の部分木に f を作用させる / O(logN)
    void subtree_apply(int v, F f) {
        seg.apply(hld.nodein[v], hld.nodeout[v], f);
        rseg.apply(hld.n - hld.nodeout[v], hld.n - hld.nodein[v], f);
    }
};
} // namespace titan23
