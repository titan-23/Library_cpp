/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/fenwick_tree2D.cpp
#pragma once

#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {

/// @brief 一点加算と矩形和を扱う 2 次元 Fenwick 木 / 0-indexed
template<typename T>
struct FenwickTree2D {
    long long _h, _w;
    vector<T> a;

    FenwickTree2D() {}
    /// @brief H = h 行、W = w 列で構築する / 初期値 0 / O(HW)
    FenwickTree2D(int h, int w) : _h(h+1), _w(w+1), a(_h*_w, 0) {
        assert((long long)_h * _w < 1e9);
    }

    /// @brief a[h][w] に x を加算する / O(log H log W)
    void add(int h, int w, T x) {
        assert(0 <= h && h < _h);
        assert(0 <= w && w < _w);
        ++h; ++w;
        while (h < _h) {
            int j = w;
            while (j < _w) {
                a[h*_w+j] += x;
                j += (j & (-j));
            }
            h += (h & (-h));
        }
    }

    /// @brief a[h][w] を x に変更する / O(log H log W)
    void set(int h, int w, T x) {
        assert(0 <= h && h < _h);
        assert(0 <= w && w < _w);
        add(h, w, x - get(h, w));
    }

    /// @brief 矩形 [0, h) x [0, w) の和を返す / O(log H log W)
    T sum(int h, int w) const {
        assert(0 <= h && h < _h);
        assert(0 <= w && w < _w);
        T res = 0;
        while (h > 0) {
            int j = w;
            while (j > 0) {
                res += a[h*_w + j];
                j -= (j & (-j));
            }
            h -= (h & (-h));
        }
        return res;
    }

    /// @brief 矩形 [h1, h2) x [w1, w2) の和を返す / O(log H log W)
    T sum(int h1, int w1, int h2, int w2) const {
        assert(0 <= h1 && h1 <= h2 && h2 <= _h);
        assert(0 <= w1 && w1 <= w2 && w2 <= _w);
        return sum(h2, w2) - sum(h2, w1) - sum(h1, w2) + sum(h1, w1);
    }

    /// @brief a[h][w] を返す / O(log H log W)
    T get(int h, int w) const {
        assert(0 <= h && h < _h);
        assert(0 <= w && w < _w);
        return sum(h, w, h+1, w+1);
    }

    /// @brief 全要素を標準出力に出力する
    void print() const {
        cout << "[" << endl;
        for (int i = 0; i < _h-1; ++i) {
            cout << "  ";
            for (int j = 0; j < _w-1; ++j) {
                cout << get(i, j) << " ";
            }
            cout << endl;
        }
        cout << "]" << endl;
    }
};
}  // namespace titan23
