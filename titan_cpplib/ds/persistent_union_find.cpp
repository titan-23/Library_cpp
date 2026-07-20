#pragma once

#include <vector>
#include "titan_cpplib/ds/persistent_array.cpp"
using namespace std;

namespace titan23 {

/// @brief 永続 UnionFind / O(log^2 n) 時間 / O(logn) 空間
class PersistentUnionFind {
private:
    int n;
    using PType = titan23::PersistentArray<int>;
    PType par;

    PersistentUnionFind(PType p) : n(p.len()), par(p) {}

public:
    PersistentUnionFind() : n(0) {}

    /// @brief 要素数 n で初期化する / O(n)
    PersistentUnionFind(int n) : n(n) {
        vector<int> p(n, -1);
        par = PType(p);
    }

    /// @brief コピーして返す / O(1)
    PersistentUnionFind copy() const {
        return PersistentUnionFind(par.copy());
    }

    /// @brief x の属する集合の代表元を返す / O(log^2 n)
    int root(int x) const {
        while (1) {
            int p = par.get(x);
            if (p < 0) return x;
            x = p;
        }
        // vector<int> a;
        // while (1) {
        //     int p = par.get(x);
        //     if (p < 0) break;
        //     a.emplace_back(x);
        //     x = p;
        // }
        // par = par.multiset_uf(a, x);
        // return x;
    }

    /// @brief `x` と `y` を併合した新しい uf を返す / O(log^2 n) 時間 / O(logn) 空間
    PersistentUnionFind unite(int x, int y) const {
        x = root(x);
        y = root(y);
        PType p = par.copy();
        if (x == y) {
            return PersistentUnionFind(p);
        }
        int px = p.get(x);
        int py = p.get(y);
        if (px > py) swap(x, y);
        p = p.set(x, px + py);
        p = p.set(y, x);
        return PersistentUnionFind(p);
    }

    /// @brief `x` の属する集合の要素数を返す / O(log^2 n)
    int size(int x) const { return -par.get(root(x)); }

    /// @brief `x` と `y` が同じ集合に属するかを返す / O(log^2 n)
    bool same(int x, int y) const { return root(x) == root(y); }
};
} // namespace titan23
