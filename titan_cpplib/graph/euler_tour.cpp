#pragma once

#include <vector>
#include <stack>
#include <algorithm>
#include "titan_cpplib/ds/fenwick_tree.cpp"
#include "titan_cpplib/ds/segment_tree.cpp"
using namespace std;

namespace titan23 {

template<typename T>
class EulerTour {
private:
    int n;
    int bit, msk;

    vector<T> vertexcost;
    vector<T> vcost1;  // for vertex subtree (+w at in, 0 at out)
    vector<T> vcost2;  // for vertex path    (+w at in, -w at out)
    vector<T> ecost1;  // for edge subtree
    vector<T> ecost2;  // for edge path
    vector<int> nodein, nodeout, depth, par;  // in/out tour, size 2n
    vector<int> visit;     // touch tour for LCA, size 2n-1
    vector<int> firstocc;  // first occurrence in visit

    titan23::FenwickTree<T> vcost_subtree;
    titan23::FenwickTree<T> vcost_path;
    titan23::FenwickTree<T> ecost_subtree;
    titan23::FenwickTree<T> ecost_path;

    static long long op(long long s, long long t) { return min(s, t); }
    static long long e() { return (long long)1e18; };

    titan23::SegmentTree<long long, op, e> seg;

    static int bit_length(const int x) {
        return x == 0 ? 0 : 32 - __builtin_clz(x);
    }

public:

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

        // 1 回の DFS で in/out 巡回(コスト用, t)と touch 巡回(LCA 用, vt)を構築する
        int t = -1;   // in/out index (size 2n)
        int vt = -1;  // visit index  (size 2n-1)
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

    int lca(int u, int v) {
        if (u == v) return u;
        int l = firstocc[u], r = firstocc[v];
        if (l > r) swap(l, r);
        int ind = seg.prod(l, r + 1) & msk;
        return visit[ind];
    }

    int lca_mul(const vector<int> &a) const {
        int l = 2*n, r = -1;
        for (const int e : a) {
            l = min(l, firstocc[e]);
            r = max(r, firstocc[e]);
        }
        int ind = seg.prod(l, r + 1) & msk;
        return visit[ind];
    }

    T subtree_vcost(int v) {
        return vcost_subtree.sum(nodein[v], nodeout[v]);
    }

    T subtree_ecost(int v) {
        return ecost_subtree.sum(nodein[v] + 1, nodeout[v]);
    }

    // 頂点 v を含む
    T path_vcost(int v) {
        return vcost_path.pref(nodein[v] + 1);
    }

    // 根から頂点 v までの辺
    T path_ecost(int v) {
        return ecost_path.pref(nodein[v] + 1);
    }

    T path_vcost(int u, int v) {
        int a = lca(u, v);
        return path_vcost(u) + path_vcost(v) - 2 * path_vcost(a) + vertexcost[a];
    }

    T path_ecost(int u, int v) {
        return path_ecost(u) + path_ecost(v) - 2 * path_ecost(lca(u, v));
    }

    // Add w to vertex v / O(logN)
    void add_vertex(int v, T w) {
        vcost_subtree.add(nodein[v], w);
        vcost_path.add(nodein[v], w);
        vcost_path.add(nodeout[v] - 1, -w);
        vertexcost[v] += w;
    }

    // Set w to vertex v / O(logN)
    void set_vertex(int v, T w) {
        add_vertex(v, w - vertexcost[v]);
    }

    // Add w to edge([u - v]) / O(logN)
    void add_edge(int u, int v, T w) {
        if (depth[u] < depth[v]) swap(u, v);  // u を深い側(子)にする
        ecost_subtree.add(nodein[u], w);
        ecost_path.add(nodein[u], w);
        ecost_path.add(nodeout[u] - 1, -w);
    }

    // Set w to edge([u - v]). / O(logN)
    void set_edge(int u, int v, T w) {
        add_edge(u, v, w - path_ecost(u, v));
    }
};
} // namespace titan23
