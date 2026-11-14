/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/dynamic_fenwick_tree2D_RAQ.cpp
#pragma once

#include "titan_cpplib/ds/dynamic_fenwick_tree2D.cpp"
#include <cassert>
using namespace std;

namespace titan23 {

/// @brief 矩形加算と1点取得を扱う動的2次元木 / DynamicFenwickTree2D に差分を持たせる
template<typename T, typename W>
struct DynamicFenwicktree2D_RAQ {
    DynamicFenwickTree2D<T, W> bit;
    T _h, _w;

    DynamicFenwicktree2D_RAQ() : _h(0), _w(0) {}

    /// @brief 縦 h、横 w の空間を管理する木を構築する / O(1)
    DynamicFenwicktree2D_RAQ(T h, T w) : bit(h, w), _h(h), _w(w) {}

    /// @brief 領域 [h1, h2) × [w1, w2) に x を加算する / O(logH logW)
    void add(T h1, T w1, T h2, T w2, W x) {
        assert(0 <= h1 && h1 <= h2 && h2 <= _h);
        assert(0 <= w1 && w1 <= w2 && w2 <= _w);
        // 内部の add は範囲外の (h, w) を弾くため、境界を確認する
        if (h1 < _h && w1 < _w) bit.add(h1, w1, x);
        if (h1 < _h && w2 < _w) bit.add(h1, w2, -x);
        if (h2 < _h && w1 < _w) bit.add(h2, w1, -x);
        if (h2 < _h && w2 < _w) bit.add(h2, w2, x);
    }

    /// @brief 座標 (h, w) の値を返す / O(logH logW)
    W get(T h, T w) const {
        assert(0 <= h && h < _h);
        assert(0 <= w && w < _w);
        // 差分配列の [0, h+1) × [0, w+1) の和が (h, w) の値になる
        return bit.sum(h + 1, w + 1);
    }

    /// @brief 木が持つノードを解放し、根を初期化する / O(K)、K はノード数
    void reset() {
        bit.reset();
    }
};

}  // namespace titan23
