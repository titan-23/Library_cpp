#pragma once

#include <vector>
#include <algorithm>
#include "titan_cpplib/ds/union_find.cpp"
using namespace std;

namespace titan23 {

/// 最小全域木を求める / (idx, u, v) / O()
template<typename T>
pair<T, vector<tuple<int, int, int>>> minimum_spanning_tree(int n, const vector<tuple<int, int, T>> &E) {
    vector<tuple<T, int>> F(E.size());
    for (int i = 0; i < (int)E.size(); ++i) {
        F[i] = {get<2>(E[i]), i};
    }
    sort(F.begin(), F.end());
    titan23::UnionFind uf(n);
    vector<tuple<int, int, int>> res;
    T s = 0;
    for (const auto &[w, idx] : F) {
        auto [u, v, ew] = E[idx];
        if (uf.same(u, v)) continue;
        uf.unite(u, v);
        res.emplace_back(idx, u, v);
        s += w;
    }
    return {s, res};
}
}  // namespace titan23
