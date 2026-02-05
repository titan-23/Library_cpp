#include <vector>
#include <stack>
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
    vector<T> vcost1;  // for vertex subtree
    vector<T> vcost2;  // for vertex path
    vector<T> ecost1;  // for edge subtree
    vector<T> ecost2;  // for edge path
    vector<int> nodein, nodeout, depth;
    vector<int> path;

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

        vcost1.resize(2*n, 0);  // for vertex subtree
        vcost2.resize(2*n, 0);  // for vertex path
        ecost1.resize(2*n, 0);  // for edge subtree
        ecost2.resize(2*n, 0);  // for edge path
        nodein.resize(n, 0);
        nodeout.resize(n, 0);
        depth.resize(n, -1);
        path.resize(2*n-1, 0);

        int curtime = -1;
        depth[root] = 0;
        stack<pair<int, T>> st;
        // st.push({~root, 0});
        st.push({root, 0});
        while (!st.empty()) {
            curtime++;
            auto [v, ec] = st.top(); st.pop();
            if (v >= 0) {
                cerr << "pre-" << " : " << v << endl;
                nodein[v] = curtime;
                path[curtime] = v;
                ecost1[curtime] = ec;
                ecost2[curtime] = ec;
                vcost1[curtime] = vertexcost[v];
                vcost2[curtime] = vertexcost[v];
                if ((int)G[v].size() == 1) {
                    nodeout[v] = curtime + 1;
                    st.push({~v, c});
                }
                for (const auto [x, c] : G[v]) {
                    if (depth[x] != -1) {
                        continue;
                    }
                    depth[x] = depth[v] + 1;
                    st.push({~v, c});
                    st.push({x, c});
                }
            } else {
                v = ~v;
                cerr << "back-" << " : " << v << endl;
                path[curtime] = v;
                ecost1[curtime] = 0;
                ecost2[curtime] = -ec;
                vcost1[curtime] = 0;
                vcost2[curtime] = -vertexcost[v];
                nodeout[v] = curtime;
            }
        }

        // ----------------------
        cerr << path << endl;

        vcost_subtree = titan23::FenwickTree<T>(vcost1);
        vcost_path = titan23::FenwickTree<T>(vcost2);
        ecost_subtree = titan23::FenwickTree<T>(ecost1);
        ecost_path = titan23::FenwickTree<T>(ecost2);

        vcost_path.print();

        bit = bit_length(path.size());
        msk = (1 << bit) - 1;
        vector<long long> a(path.size());
        for (int i = 0; i < (int)path.size(); ++i) {
            a[i] = ((long long)depth[path[i]] << bit) + i;
        }
        seg = titan23::SegmentTree<long long, op, e>(a);
    }

    int lca(int u, int v) {
        if (u == v) return u;
        int l = min(nodein[u], nodein[v]);
        int r = max(nodeout[u], nodeout[v]);
        int ind = seg.prod(l, r) & msk;
        return path[ind];
    }

    int lca_mul(const vector<int> &a) const {
        int l = n+1, r = n-1;
        for (const int e : a) {
            l = min(l, nodein[e]);
            r = max(r, nodeout[e]);
        }
        int ind = seg.prod(l, r) & msk;
        return path[ind];
    }

    T subtree_vcost(int v) {
        int l = nodein[v];
        int r = nodeout[v];
        return vcost_subtree.prod(l, r);
    }

    T subtree_ecost(int v) {
        int l = nodein[v];
        int r = nodeout[v];
        return ecost_subtree.prod(l + 1, r);
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
        cerr << "lca=" << a << endl;
        cerr << u << " : " << path_vcost(u) << endl;
        cerr << v << " : " << path_vcost(v) << endl;
        cerr << a << " : " << path_vcost(a) << endl;
        return path_vcost(u) + path_vcost(v) - 2 * path_vcost(a) + vertexcost[a];
    }

    T path_ecost(int u, int v) {
        return path_ecost(u) + path_ecost(v) - 2 * path_ecost(lca(u, v));
    }

    // Add w to vertex x / O(logN)
    void add_vertex(int v, T w) {
        int l = nodein[v];
        int r = nodeout[v];
        vcost_subtree.add(l, w);
        vcost_path.add(l, w);
        vcost_path.add(r, -w);
        vertexcost[v] += w;
    }

    // Set w to vertex v / O(logN)
    void set_vertex(int v, T w) {
        add_vertex(v, w - vertexcost[v]);
    }

    // Add w to edge([u - v]) / O(logN)
    void add_edge(int u ,int v, T w) {
        if (depth[u] < depth[v]) swap(u, v);
        int l = nodein[u];
        int r = nodeout[u];
        ecost_subtree.add(l, w);
        ecost_subtree.add(r + 1, -w);
        ecost_path.add(l, w);
        ecost_path.add(r + 1, -w);
    }

    // Set w to edge([u - v]). / O(logN)
    void set_edge(int u, int v, T w) {
        add_edge(u, v, w - path_ecost(u, v));
    }
};
} // namespace titan23
