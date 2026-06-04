#include "titan_cpplib/ds/dynamic_fenwick_tree2D.cpp"
#include <cassert>
using namespace std;

namespace titan23 {

template<typename T, typename W>
struct DynamicFenwicktree2D_RAQ {
    DynamicFenwickTree2D<T, W> bit;
    T _h, _w;

    // デフォルトコンストラクタ / O(1)
    DynamicFenwicktree2D_RAQ() : _h(0), _w(0) {}

    // 縦 h、横 w の空間を管理する木を構築する / O(1)
    DynamicFenwicktree2D_RAQ(T h, T w) : bit(h, w), _h(h), _w(w) {}

    // 領域 [h1, h2) × [w1, w2) に x を加算する / O(logH logW)
    void add(T h1, T w1, T h2, T w2, W x) {
        assert(0 <= h1 && h1 <= h2 && h2 <= _h);
        assert(0 <= w1 && w1 <= w2 && w2 <= _w);
        // 内部のaddは(h, w)が範囲外(_h, _w)の場合に弾かれるため、境界チェックを行う
        if (h1 < _h && w1 < _w) bit.add(h1, w1, x);
        if (h1 < _h && w2 < _w) bit.add(h1, w2, -x);
        if (h2 < _h && w1 < _w) bit.add(h2, w1, -x);
        if (h2 < _h && w2 < _w) bit.add(h2, w2, x);
    }

    // 座標 (h, w) の値を取得する / O(logH logW)
    W get(T h, T w) const {
        assert(0 <= h && h < _h);
        assert(0 <= w && w < _w);
        // 差分配列に対する (h, w) の値は、[0, h] × [0, w] の2次元累積和となる
        // 半開区間 [0, h+1) × [0, w+1) として取得する
        return bit.sum(h + 1, w + 1);
    }

    // 木が保持するノードを開放し、根を初期化する / O(K) Kはノード数
    void reset() {
        bit.reset();
    }
};

}  // namespace titan23
