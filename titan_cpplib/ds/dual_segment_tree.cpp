/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/dual_segment_tree.cpp
#pragma once

#include <iostream>
#include <vector>
#include <cassert>
#include "titan_cpplib/others/bit.cpp"
using namespace std;

namespace titan23 {

/// @brief 区間への作用と1点取得を行う双対セグ木
template <class T,
        class F,
        T (*mapping)(F, T),
        F (*composition)(F, F),
        F (*id)()>
struct DualSegmentTree {

    int _n, _size, _log;
    vector<T> _data;
    vector<F> _lazy;

    DualSegmentTree() {}

    /// @brief 長さ n で構築する / 初期値は T{} で init は使わない / O(n)
    DualSegmentTree(const int n, const T init) {
        this->_n = n;
        this->_log = bit_length(n);
        this->_size = 1 << _log;
        this->_data.resize(_size);
        this->_lazy.resize(_size, id());
    }

    /// @brief 列 a から構築する / O(n)
    DualSegmentTree(const vector<T> a) {
        int n = (int)a.size();
        this->_n = n;
        this->_log = bit_length(n);
        this->_size = 1 << _log;
        this->_data.resize(_size);
        this->_lazy.resize(_size, id());
        for (int i = 0; i < _n; ++i) {
            _data[i] = a[i];
        }
    }

    void _all_apply(int k, F f) {
        if (k < _size) {
            _lazy[k] = composition(f, _lazy[k]);
            return;
        }
        k -= _size;
        _data[k] = mapping(f, _data[k]);
    }

    void _propagate(const int k) {
        if (_lazy[k] == id()) return;
        _all_apply(k<<1, _lazy[k]);
        _all_apply(k<<1|1, _lazy[k]);
        _lazy[k] = id();
    }

    /// @brief a[k] に f を作用させる / O(log n)
    void apply_point(int k, F f) {
        k += _size;
        for (int i = _log; i > 0; --i) {
            _propagate(k >> i);
        }
        _data[k-_size] = mapping(f, _data[k-_size]);
    }

    /// @brief 区間 [l, r) に f を作用させる / O(log n)
    void apply(int l, int r, const F f) {
        assert(0 <= l && l <= r && r <= _n);
        if (l == r || f == id()) return;
        l += _size;
        r += _size;
        for (int i = _log; i > 0; --i) {
            if (((l >> i << i) != l)) {
                _propagate(l>>i);
            }
            if (((r >> i << i) != r)) {
                _propagate((r-1)>>i);
            }
        }
        if ((l-_size) & 1) {
            _data[l-_size] = mapping(f, _data[l-_size]);
            ++l;
        }
        if ((r-_size) & 1) {
            _data[(r-_size)^1] = mapping(f, _data[(r-_size)^1]);
            r ^= 1;
        }
        l >>= 1;
        r >>= 1;
        while (l < r) {
            if (l & 1) {
                _lazy[l] = composition(f, _lazy[l]);
                ++l;
            }
            if (r & 1) {
                --r;
                _lazy[r] = composition(f, _lazy[r]);
            }
            l >>= 1;
            r >>= 1;
        }
    }

    /// @brief 全要素に f を作用させる / O(1)
    void all_apply(F f) {
        _lazy[1] = composition(f, _lazy[1]);
    }

    /// @brief 全ての遅延作用を葉まで伝播する / O(n)
    void all_propagate() {
        for (int i = 0;  i < _size; ++i) {
            _propagate(i);
        }
    }

    /// @brief 全要素を列で返す / O(n)
    vector<T> tovector() {
        all_propagate();
        vector<T> res = _data;
        return res;
    }

    /// @brief a[k] を返す / 負の k は末尾から数える / O(log n)
    T get(int k) {
        if (k < 0) k += _n;
        k += _size;
        for (int i = _log; i > 0; --i) {
            _propagate(k>>i);
        }
        return _data[k-_size];
    }

    /// @brief a[k] を v に変更する / 負の k は末尾から数える / O(log n)
    void set(int k, T v) {
        if (k < 0) k += _n;
        k += _size;
        for (int i = _log; i > 0; --i) {
            _propagate(k>>i);
        }
        _data[k-_size] = v;
    }

    /// @brief 列を出力する / O(n log n)
    void print() {
        cout << '[';
        for (int i = 0; i < _n-1; ++i) {
            cout << get(i) << ", ";
        }
        cout << get(_n-1);
        cout << ']' << endl;
    }
};
}  // namespace titan23
