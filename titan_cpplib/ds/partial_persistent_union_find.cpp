/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/partial_persistent_union_find.cpp
#pragma once

#include <iostream>
#include <vector>
#include "titan_cpplib/ds/partial_persistent_array.cpp"
using namespace std;

namespace titan23 {

/// @brief 部分永続 Union-Find / 最新の時刻にだけ併合し、過去の任意の時刻の状態を調べられる
class PartialPersistentUnionFind {
private:
    int n, last_time;
    titan23::PartialPersistentArray<int> par;

public:
    PartialPersistentUnionFind() {}
    /// @brief n 頂点を全て別の集合として、時刻 0 で構築する
    PartialPersistentUnionFind(int n) {
        vector<int> p(n, -1);
        par = titan23::PartialPersistentArray<int>(p);
        last_time = 0;
    }

    /// @brief 時刻 t における x の根を返す / t = -1 なら最新
    int root(int x, int t = -1) {
        assert(t == -1 || t <= last_time);
        while (1) {
            int p = par.get(x, t);
            if (p < 0) return x;
            x = p;
        }
    }

    /// @brief 時刻 t に x と y の集合を併合し、併合したかを返す / t は 1 以上かつ直前の unite の時刻以上で、最後に併合した時刻より大きいことが前提
    bool unite(int x, int y, int t) {
        assert(t == -1 || t >= last_time);
        last_time = t;
        x = root(x, t); y = root(y, t);
        if (x == y) return false;
        if (par.get(x, t) > par.get(y, t)) swap(x, y);
        par.set(x, par.get(x, t) + par.get(y, t), t);
        par.set(y, x, t);
        return true;
    }

    /// @brief 時刻 t における x の集合の要素数を返す / t = -1 なら最新
    int size(int x, int t = -1) {
        assert(t == -1 || t <= last_time);
        return -par.get(root(x, t), t);
    }

    /// @brief 時刻 t に x と y が同じ集合にあるかを返す / t = -1 なら最新
    bool same(int x, int y, int t = -1) {
        assert(t == -1 || t <= last_time);
        return root(x, t) == root(y, t);
    }
};
} // namespace titan23
