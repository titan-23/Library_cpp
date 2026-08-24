/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/persistent_weighted_union_find.cpp
#pragma once

#include <vector>
#include <cassert>
#include "titan_cpplib/ds/persistent_array.cpp"
using namespace std;

namespace titan23 {

/// @brief 永続重み付き UnionFind / O(log^2 n) 時間 / O(logn) 空間
template<typename T>
class PersistentWeightedUnionFind {
private:
    int n;
    using IType = titan23::PersistentArray<int>;
    using WType = titan23::PersistentArray<T>;
    IType par;
    WType weight;

    PersistentWeightedUnionFind(int n, IType par, WType weight) : n(n), par(par), weight(weight) {}

public:
    PersistentWeightedUnionFind() : n(0) {}

    /// @brief 要素数 n で初期化する / O(n)
    PersistentWeightedUnionFind(int n) : n(n) {
        vector<int> p(n, -1);
        vector<T> w(n, 0);
        par = IType(p);
        weight = WType(w);
    }

    /// @brief コピーして返す / O(1)
    PersistentWeightedUnionFind copy() const {
        return PersistentWeightedUnionFind(n, par.copy(), weight.copy());
    }

    /// @brief x の属する集合の代表元を返す / O(log^2 n)
    int root(int x) const {
        while (1) {
            int p = par.get(x);
            if (p < 0) return x;
            x = p;
        }
    }

    /// @brief 根を基準とした x のポテンシャル v[x] - v[root] を返す / O(log^2 n)
    T potential(int x) const {
        T res = 0;
        while (1) {
            int p = par.get(x);
            if (p < 0) return res;
            res += weight.get(x);
            x = p;
        }
    }

    /// @brief `v[y] - v[x] = w` とし、{矛盾なしか, 新しい uf} を返す / O(log^2 n) 時間 / O(logn) 空間
    pair<bool, PersistentWeightedUnionFind> unite(int x, int y, T w) const {
        T px = potential(x), py = potential(y);
        int rx = root(x), ry = root(y);
        if (rx == ry) {
            bool ok = (py - px == w);
            return {ok, copy()};
        }
        w += px - py;
        int sx = par.get(rx), sy = par.get(ry);
        if (sx > sy) {
            swap(rx, ry);
            swap(sx, sy);
            w = -w;
        }
        IType np = par.set(rx, sx + sy);
        np = np.set(ry, rx);
        WType nw = weight.set(ry, w);
        return {true, PersistentWeightedUnionFind(n, np, nw)};
    }

    /// @brief `x` の属する集合の要素数を返す / O(log^2 n)
    int size(int x) const {
        return -par.get(root(x));
    }

    /// @brief `x` と `y` が同じ集合に属するかを返す / O(log^2 n)
    bool same(int x, int y) const {
        return root(x) == root(y);
    }

    /// @brief `v[y] - v[x]` を返す。同じ集合に属することが前提 / O(log^2 n)
    T diff(int x, int y) const {
        assert(same(x, y));
        return potential(y) - potential(x);
    }
};
} // namespace titan23
