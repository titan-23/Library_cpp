/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/undoable_union_find.cpp
#pragma once

#include <vector>
#include <stack>
using namespace std;

namespace titan23 {

/// @brief unite を新しい順に取り消せる Union-Find
class UndoableUnionFind {
private:
    int n, cnt;
    vector<int> par;
    stack<pair<int, int>> history;

public:
    UndoableUnionFind() {}
    /// @brief n 頂点で構築する / O(n)
    UndoableUnionFind(int n) : n(n), cnt(n), par(n, -1) {}

    /// @brief 直前の unite を 1 回分取り消す / 併合しなかった unite も 1 回と数える / O(1)
    void undo() {
        auto [y, py] = history.top(); history.pop();
        if (y == -1) return;
        auto [x, px] = history.top(); history.pop();
        ++cnt;
        par[y] = py;
        par[x] = px;
    }

    /// @brief x の根を返す / O(log n)
    int root(int x) const {
        while (par[x] >= 0) {
            x = par[x];
        }
        return x;
    }

    /// @brief x と y の属する集合を併合し、併合したかを返す / O(log n)
    bool unite(int x, int y) {
        x = root(x);
        y = root(y);
        if (x == y) {
            history.emplace(-1, -1);
            return false;
        }
        if (par[x] > par[y]) swap(x, y);
        cnt--;
        history.emplace(x, par[x]);
        history.emplace(y, par[y]);
        par[x] += par[y];
        par[y] = x;
        return true;
    }

    /// @brief x の属する集合の要素数を返す / O(log n)
    int size(const int x) const {
        return -par[root(x)];
    }

    /// @brief x と y が同じ集合に属するかを返す / O(log n)
    bool same(const int x, const int y) const {
        return root(x) == root(y);
    }

    /// @brief 集合の個数を返す / O(1)
    int group_count() const {
        return cnt;
    }
};
} // namespace titan23
