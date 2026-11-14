/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/tree/euler_tour.cpp
#pragma once

#include <vector>
#include <stack>
#include <algorithm>
#include "titan_cpplib/ds/fenwick_tree.cpp"
#include "titan_cpplib/ds/segment_tree.cpp"
#include "titan_cpplib/others/bit.cpp"
using namespace std;

namespace titan23 {

/// @brief Euler tour で、木の頂点と辺の重みについて部分木の和、パスの和、LCA を扱う
template<typename T>
class EulerTour {
private:
    int n;
    int bit, msk;

    vector<T> vertexcost;
    vector<T> vcost1;  // 頂点の部分木用。in に +w、out に 0
    vector<T> vcost2;  // 頂点のパス用。in に +w、out に -w
    vector<T> ecost1;  // 辺の部分木用
    vector<T> ecost2;  // 辺のパス用
    vector<int> nodein, nodeout, depth, par;  // in/out 巡回の位置。巡回の長さは 2n
    vector<int> visit;     // LCA 用に訪れた頂点を並べた巡回。長さ 2n-1
    vector<int> firstocc;  // visit での最初の出現位置

    titan23::FenwickTree<T> vcost_subtree;
    titan23::FenwickTree<T> vcost_path;
    titan23::FenwickTree<T> ecost_subtree;
    titan23::FenwickTree<T> ecost_path;

    static long long op(long long s, long long t) { return min(s, t); }
    static long long e() { return (long long)1e18; };

    titan23::SegmentTree<long long, op, e> seg;

public:

    /// @brief 重みつき隣接リスト G、根 root、頂点の重み vertexcost から構築する
    EulerTour(const vector<vector<pair<int, T>>> &G, int root, const vector<T> vertexcost) {
        n = G.size();

        this->vertexcost = vertexcost;

        vcost1.assign(2*n, 0);
        vcost2.assign(2*n, 0);
        ecost1.assign(2*n, 0);
        ecost2.assign(2*n, 0);
        nodein.assign(n, 0);
        nodeout.assign(n, 0);
        depth.assign(n, -1);
        par.assign(n, -1);
        visit.assign(2*n-1, 0);
        firstocc.assign(n, -1);

        // 1 回の DFS で、重み用の in/out 巡回 t と LCA 用の巡回 vt を作る
        int t = -1;   // in/out 巡回の添字。長さ 2n
        int vt = -1;  // visit の添字。長さ 2n-1
        depth[root] = 0;
        stack<pair<int, T>> st;
        st.push({~root, 0});
        st.push({root, 0});
        while (!st.empty()) {
            auto [v, ec] = st.top(); st.pop();
            if (v >= 0) {  // 入場
                t++;
                nodein[v] = t;
                vcost1[t] = vertexcost[v];
                vcost2[t] = vertexcost[v];
                ecost1[t] = ec;
                ecost2[t] = ec;
                vt++;
                visit[vt] = v;
                firstocc[v] = vt;
                for (const auto [x, c] : G[v]) {
                    if (depth[x] != -1) continue;
                    depth[x] = depth[v] + 1;
                    par[x] = v;
                    st.push({~x, c});
                    st.push({x, c});
                }
            } else {  // 退場
                v = ~v;
                t++;
                nodeout[v] = t + 1;
                vcost1[t] = 0;
                vcost2[t] = -vertexcost[v];
                ecost1[t] = 0;
                ecost2[t] = -ec;
                if (par[v] != -1) {  // 親へ戻る = 親を記録
                    vt++;
                    visit[vt] = par[v];
                }
            }
        }

        // ----------------------

        vcost_subtree = titan23::FenwickTree<T>(vcost1);
        vcost_path = titan23::FenwickTree<T>(vcost2);
        ecost_subtree = titan23::FenwickTree<T>(ecost1);
        ecost_path = titan23::FenwickTree<T>(ecost2);

        bit = bit_length(visit.size());
        msk = (1 << bit) - 1;
        vector<long long> a(visit.size());
        for (int i = 0; i < (int)visit.size(); ++i) {
            a[i] = ((long long)depth[visit[i]] << bit) + i;
        }
        seg = titan23::SegmentTree<long long, op, e>(a);
    }

    /// @brief u と v の LCA を返す / O(logN)
    int lca(int u, int v) {
        if (u == v) return u;
        int l = firstocc[u], r = firstocc[v];
        if (l > r) swap(l, r);
        int ind = seg.prod(l, r + 1) & msk;
        return visit[ind];
    }

    /// @brief a の全頂点の LCA を返す / a は空でないこと / O(|a| + logN)
    int lca_mul(const vector<int> &a) const {
        int l = 2*n, r = -1;
        for (const int e : a) {
            l = min(l, firstocc[e]);
            r = max(r, firstocc[e]);
        }
        int ind = seg.prod(l, r + 1) & msk;
        return visit[ind];
    }

    /// @brief v の部分木の頂点の重みの和を返す / O(logN)
    T subtree_vcost(int v) {
        return vcost_subtree.sum(nodein[v], nodeout[v]);
    }

    /// @brief v の部分木に含まれる辺の重みの和を返す / O(logN)
    T subtree_ecost(int v) {
        return ecost_subtree.sum(nodein[v] + 1, nodeout[v]);
    }

    /// @brief 根から v までの頂点の重みの和を返す / v を含む / O(logN)
    T path_vcost(int v) {
        return vcost_path.pref(nodein[v] + 1);
    }

    /// @brief 根から v までの辺の重みの和を返す / O(logN)
    T path_ecost(int v) {
        return ecost_path.pref(nodein[v] + 1);
    }

    /// @brief u-v パス上の頂点の重みの和を返す / O(logN)
    T path_vcost(int u, int v) {
        int a = lca(u, v);
        return path_vcost(u) + path_vcost(v) - 2 * path_vcost(a) + vertexcost[a];
    }

    /// @brief u-v パス上の辺の重みの和を返す / O(logN)
    T path_ecost(int u, int v) {
        return path_ecost(u) + path_ecost(v) - 2 * path_ecost(lca(u, v));
    }

    /// @brief 頂点 v の重みに w を加える / O(logN)
    void add_vertex(int v, T w) {
        vcost_subtree.add(nodein[v], w);
        vcost_path.add(nodein[v], w);
        vcost_path.add(nodeout[v] - 1, -w);
        vertexcost[v] += w;
    }

    /// @brief 頂点 v の重みを w にする / O(logN)
    void set_vertex(int v, T w) {
        add_vertex(v, w - vertexcost[v]);
    }

    /// @brief 辺 u-v の重みに w を加える / O(logN)
    void add_edge(int u, int v, T w) {
        if (depth[u] < depth[v]) swap(u, v);  // u を深い側の子にする
        ecost_subtree.add(nodein[u], w);
        ecost_path.add(nodein[u], w);
        ecost_path.add(nodeout[u] - 1, -w);
    }

    /// @brief 辺 u-v の重みを w にする / O(logN)
    void set_edge(int u, int v, T w) {
        add_edge(u, v, w - path_ecost(u, v));
    }
};
} // namespace titan23
