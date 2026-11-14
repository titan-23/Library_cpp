/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/graph/tree/lca_auxiliary_tree.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <stack>
#include <vector>
#include "titan_cpplib/graph/tree/lca.cpp"
using namespace std;

namespace titan23 {

/// @brief 指定した頂点集合の補助木を LCA を用いて作る / 元の木の根は 0
struct LCAAuxiliaryTree {
    int n; vector<int> in, out; LCA tree;
    /// @brief 木 G から前計算する / O(n)
    LCAAuxiliaryTree(vector<vector<int>> G) : n(G.size()), in(n, -1), out(n, -1), tree(G, 0) {
        int t = 0;
        auto dfs = [&] (auto &&dfs, int v,int p) -> void {
            in[v] = t++;
            for (int x : G[v]) if (x != p) dfs(dfs, x, v);
            out[v] = t-1;
        };
        dfs(dfs, 0, -1);
    }
    /// @brief vs とその LCA からなる補助木を作り、根を返す / O(|vs| log |vs|)
    /// 辺は親から子の向きで RES に追加する / RES の大きさは n
    int build(vector<int> vs, vector<vector<int>> &RES) {
        assert((int)RES.size() == n);
        auto cmp = [&] (int x, int y) { return in[x] < in[y]; };
        sort(vs.begin(), vs.end(), cmp);
        int k = vs.size(), root = -1;
        for (int i = 0; i < k-1; ++i) vs.push_back(tree.lca(vs[i], vs[i+1]));
        sort(vs.begin(), vs.end(), cmp);
        vs.erase(unique(vs.begin(), vs.end()), vs.end());
        stack<int> s;
        for(int v : vs) {
            while (!s.empty() && out[s.top()] < in[v]) s.pop();
            if (!s.empty()) RES[s.top()].push_back(v);
            else root = v;
            s.push(v);
        }
        return root;
    }
};

} // namespace titan23
