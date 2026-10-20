/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/tree/kruskal_reconstruction_tree.cpp
#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <limits>
#include <numeric>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>
#include "titan_cpplib/ds/union_find.cpp"
using namespace std;

namespace titan23 {

/**
 * @brief 重みの昇順に連結成分を合併する Kruskal 再構築木（非連結なら森）
 * 元の頂点は葉 [0, N)、合併頂点は [N, node_count())。親の番号は子より大きい。
 * 構築 O(M log M + M alpha(N) + N log N)、保持領域 O(N log N)。辺のコピーに O(M) 領域。
 * T はデフォルト構築・コピー・大小比較 (<) が可能な型。数値の番兵は使用しない。
 */
template<typename T = long long>
class KruskalReconstructionTree {
private:
    int n = 0;
    vector<int> par, dep, root_id, forest_roots, leaves;
    vector<array<int, 2>> child;
    vector<T> weight;
    vector<pair<int, int>> ranges;
    vector<vector<int>> up;

    void check_vertex(int v) const {
        assert(0 <= v && v < n);
        (void)v;
    }

    void check_node(int v) const {
        assert(0 <= v && v < node_count());
        (void)v;
    }

    void preprocess() {
        const int total = node_count();
        if (total == 0) return;
        dep.assign(total, 0);
        root_id.resize(total);
        ranges.resize(total);
        // 親の番号が必ず大きいので、再帰せずに根と深さを計算できる。
        for (int v = total - 1; v >= 0; --v) {
            if (par[v] == -1) {
                root_id[v] = v;
            } else {
                root_id[v] = root_id[par[v]];
                dep[v] = dep[par[v]] + 1;
            }
        }
        for (int v = 0; v < total; ++v) {
            if (par[v] == -1) forest_roots.push_back(v);
        }

        int levels = 1;
        while ((1LL << levels) <= total) ++levels;
        up.assign(levels, vector<int>(total, -1));
        up[0] = par;
        for (int k = 1; k < levels; ++k) {
            for (int v = 0; v < total; ++v) {
                const int p = up[k - 1][v];
                if (p != -1) up[k][v] = up[k - 1][p];
            }
        }

        // 葉だけを DFS 順に並べ、各部分木を半開区間にする。
        leaves.reserve(n);
        vector<int> st(forest_roots.rbegin(), forest_roots.rend());
        while (!st.empty()) {
            const int v = st.back();
            st.pop_back();
            if (v < n) {
                const int pos = (int)leaves.size();
                ranges[v] = {pos, pos + 1};
                leaves.push_back(v);
            } else {
                st.push_back(child[v][1]);
                st.push_back(child[v][0]);
            }
        }
        for (int v = n; v < total; ++v) {
            ranges[v] = {ranges[child[v][0]].first, ranges[child[v][1]].second};
        }
    }

public:
    /// @brief 頂点数 0 の空の森を作る
    KruskalReconstructionTree() = default;

    /// @brief 頂点数 n、無向辺 (u, v, weight) の列から構築。自己ループ・多重辺も可。
    KruskalReconstructionTree(int n, vector<tuple<int, int, T>> edges) : n(n) {
        assert(0 <= n && n <= numeric_limits<int>::max() / 2);
        const size_t capacity = n == 0 ? 0 : size_t(n) * 2 - 1;
        par.reserve(capacity);
        child.reserve(capacity);
        weight.reserve(capacity);
        par.assign(n, -1);
        child.assign(n, array<int, 2>{-1, -1});
        weight.resize(n);  // 葉の値は使わない。

        sort(edges.begin(), edges.end(), [](const auto &a, const auto &b) {
            return get<2>(a) < get<2>(b);
        });
        titan23::UnionFind uf(n);
        vector<int> top(n);
        iota(top.begin(), top.end(), 0);
        for (const auto &[u, v, w] : edges) {
            check_vertex(u);
            check_vertex(v);
            const int a = uf.root(u), b = uf.root(v);
            if (a == b) continue;
            const int left = top[a], right = top[b], node = node_count();
            par.push_back(-1);
            child.push_back({left, right});
            weight.push_back(w);
            par[left] = par[right] = node;
            uf.unite(a, b);
            top[uf.root(a)] = node;
        }
        preprocess();
    }

    /// @brief 元のグラフの頂点数 / O(1)
    int vertex_count() const { return n; }

    /// @brief 合併頂点を含む頂点数。連結成分数を C として 2N-C / O(1)
    int node_count() const { return (int)par.size(); }

    /// @brief 元のグラフの連結成分数 / O(1)
    int component_count() const { return (int)forest_roots.size(); }

    /// @brief 森の根の列 / O(1)
    const vector<int> &roots() const { return forest_roots; }

    /// @brief 再構築木上の親。根なら -1 / O(1)
    int parent(int node) const {
        check_node(node);
        return par[node];
    }

    /// @brief 再構築木上の 2 つの子。葉なら {-1, -1} / O(1)
    array<int, 2> children(int node) const {
        check_node(node);
        return child[node];
    }

    /// @brief 合併頂点の重み。葉は指定不可 / O(1)
    const T &merge_weight(int node) const {
        check_node(node);
        assert(n <= node);
        return weight[node];
    }

    /// @brief 葉（元の頂点）の DFS 順 / O(1)
    const vector<int> &leaf_order() const { return leaves; }

    /// @brief 元の頂点 v の leaf_order() 上の位置 / O(1)
    int leaf_index(int v) const {
        check_vertex(v);
        return ranges[v].first;
    }

    /// @brief node の子孫の葉に対応する leaf_order() 上の半開区間 [l, r) / O(1)
    pair<int, int> subtree_range(int node) const {
        check_node(node);
        return ranges[node];
    }

    /// @brief node の子孫の葉の数 / O(1)
    int leaf_count(int node) const {
        const auto [l, r] = subtree_range(node);
        return r - l;
    }

    /// @brief 再構築木上の LCA。合併頂点も指定可。異なる木なら -1 / O(log N)
    int lca(int u, int v) const {
        check_node(u);
        check_node(v);
        if (root_id[u] != root_id[v]) return -1;
        if (dep[u] < dep[v]) swap(u, v);
        int diff = dep[u] - dep[v];
        for (int k = 0; diff; ++k, diff >>= 1) {
            if (diff & 1) u = up[k][u];
        }
        if (u == v) return u;
        for (int k = (int)up.size() - 1; k >= 0; --k) {
            if (up[k][u] != up[k][v]) {
                u = up[k][u];
                v = up[k][v];
            }
        }
        return par[u];
    }

    /// @brief 全ての辺を使ったとき元の頂点 u, v が連結か / O(1)
    bool same(int u, int v) const {
        check_vertex(u);
        check_vertex(v);
        return root_id[u] == root_id[v];
    }

    /// @brief 重み <= x の辺だけで元の頂点 u, v が連結か。同一頂点なら常に true / O(log N)
    bool same(int u, int v, const T &x) const {
        check_vertex(u);
        check_vertex(v);
        const int node = lca(u, v);
        return node != -1 && (u == v || !(x < weight[node]));
    }

    /// @brief u, v が初めて連結になる重み。同一頂点または非連結なら nullopt / O(log N)
    optional<T> merge_threshold(int u, int v) const {
        check_vertex(u);
        check_vertex(v);
        const int node = lca(u, v);
        if (node == -1 || u == v) return nullopt;
        return weight[node];
    }

    /// @brief 重み <= x の辺で v が属する成分を表す再構築木の頂点 / O(log N)
    int component_node(int v, const T &x) const {
        check_vertex(v);
        for (int k = (int)up.size() - 1; k >= 0; --k) {
            const int p = up[k][v];
            if (p != -1 && !(x < weight[p])) v = p;
        }
        return v;
    }

    /// @brief 重み <= x の辺で v から到達できる元の頂点数（v 自身を含む） / O(log N)
    int component_size(int v, const T &x) const {
        return leaf_count(component_node(v, x));
    }
};

}  // namespace titan23
