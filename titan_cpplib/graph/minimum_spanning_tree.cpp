#include <vector>
#include <algorithm>
#include "titan_cpplib/data_structures/union_find.cpp"
using namespace std;

namespace titan23 {
    vector<pair<int, int>> minimum_spanning_tree(int n, vector<pair<int, int>> &E) {
        titan23::UnionFind uf(n);
        vector<pair<int, int>> ans;
        for (auto [u, v] : E) {
            if (uf.same(u, v)) continue;
            uf.unite(u, v);
            ans.emplace_back(u, v);
        }
        return ans;
    }
}  // namespace titan23
