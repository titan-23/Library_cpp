/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/weight_union_find.cpp
#pragma once

#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {

/// @brief 重み付き Union-Find / 0-indexed
template<typename T>
class WeightedUnionFind {
private:
    int n, group_members;
    vector<int> par;
    vector<T> weight;

public:
    WeightedUnionFind() {}
    /// @brief n 要素で構築する / O(n)
    WeightedUnionFind(int n) : n(n), group_members(n), par(n, -1), weight(n, 0) {}

    /// @brief x を含む集合の根を返す
    int root(int x) {
        vector<int> path = {x};
        while (par[x] >= 0) {
            x = par[x];
            path.emplace_back(x);
        }
        int a = path.back();
        path.pop_back();
        while (!path.empty()) {
            x = path.back();
            path.pop_back();
            weight[x] += weight[par[x]];
            par[x] = a;
        }
        return a;
    }

    /// @brief weight(y) - weight(x) = w となるように x と y を併合する / 既存の関係と矛盾すれば false を返す
    bool unite(int x, int y, T w) {
        int rx = root(x);
        int ry = root(y);
        if (rx == ry) {
            return diff(x, y) == w ? true : false;
        }
        w += weight[x] - weight[y];
        group_members--;
        if (par[rx] > par[ry]) {
            swap(rx, ry);
            w = -w;
        }
        par[rx] += par[ry];
        par[ry] = rx;
        weight[ry] = w;
        return true;
    }

    /// @brief x を含む集合の要素数を返す
    int size(int x) {
        return -par[root(x)];
    }

    /// @brief x と y が同じ集合に属するか判定する
    bool same(int x, int y) {
        return root(x) == root(y);
    }

    /// @brief 集合の個数を返す / O(1)
    int group_count() const {
        return group_members;
    }

    /// @brief weight(y) - weight(x) を返す / x と y は同じ集合に属する必要がある
    T diff(int x, int y) {
        assert(same(x, y));
        return weight[y] - weight[x];
    }
};
}
