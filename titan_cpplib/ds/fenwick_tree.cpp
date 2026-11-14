/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/fenwick_tree.cpp
#pragma once
#include <iostream>
#include <vector>
#include <cassert>
#include <algorithm>
using namespace std;

namespace titan23 {

/// @brief 一点加算と区間和を扱う Fenwick 木 / 0-indexed
template<typename T>
struct FenwickTree {
    int _n, _s;
    vector<T> _tree;

    // n 以上の最小の 2 冪を返す
    static int _calc_s(const int n) {
        int s = 1;
        while (s < n) s <<= 1;
        return s;
    }

    FenwickTree() {}

    /// @brief 長さ n で構築する / 初期値 0 / O(n)
    FenwickTree(const int n) {
        _n = n;
        _s = _calc_s(_n);
        _tree.resize(n+1, 0);
    }

    /// @brief 列 a から構築する / O(n)
    FenwickTree(const vector<T> &a)
                : _n(a.size()),
                _s(_calc_s(_n)),
                _tree(_n+1, 0) {
        for (int i = 1; i <= _n; ++i) _tree[i] = a[i-1];
        for (int i = 1; i < _n; ++i) {
            if (i + (i & (-i)) <= _n) {
                _tree[i + (i & (-i))] += _tree[i];
            }
        }
    }

    /// @brief 区間 [0, r) の和を返す / O(log n)
    T pref(int r) const {
        T res = 0;
        while (r > 0) {
            res += _tree[r];
            r -= r & (-r);
        }
        return res;
    }

    /// @brief 区間 [l, n) の和を返す / O(log n)
    T suff(int l) const {
        return pref(_n) - pref(l);
    }

    /// @brief 区間 [l, r) の和を返す / O(log n)
    T sum(const int l, const int r) const {
        return pref(r) - pref(l);
    }

    /// @brief a[k] に x を加算する / O(log n)
    void add(int k, const T x) {
        ++k;
        while (k <= _n) {
            _tree[k] += x;
            k += k & (-k);
        }
    }

    /// @brief a[k] を返す / O(log n)
    T get(const int k) const {
        return pref(k+1) - pref(k);
    }

    /// @brief a[k] を x に変更する / O(log n)
    void set(const int k, const T x) {
        T pre = get(k);
        add(k, x-pre);
    }

    /// @brief a[0] + ... + a[i] >= w となる最小の i を返す / 無ければ n、w == 0 なら -1 / 要素は非負が前提 / O(log n)
    int bisect_left(T w) const {
        int i = 0, s = _s;
        while (s) {
            if (i+s <= _n && _tree[i+s] < w) {
                w -= _tree[i+s];
                i += s;
            }
            s >>= 1;
        }
        return (w ? i : -1);
    }

    /// @brief a[0] + ... + a[i] > w となる最小の i を返す / 無ければ n / 要素は非負が前提 / O(log n)
    int bisect_right(T w) const {
        int i = 0, s = _s;
        while (s) {
            if (i+s <= _n && _tree[i+s] <= w) {
                w -= _tree[i+s];
                i += s;
            }
            s >>= 1;
        }
        return i;
    }

    /// @brief 列を vector で返す / O(n log n)
    vector<T> tovector() const {
        vector<T> sub(_n+1), res(_n);
        for (int i = 0; i <= _n; ++i) sub[i] = pref(i);
        for (int i = 0; i < _n; ++i) res[i] = sub[i+1] - sub[i];
        return res;
    }

    /// @brief 全要素を 0 にする / O(n)
    void clear() {
        fill(_tree.begin(), _tree.end(), 0);
    }

    /// @brief 列を標準出力に出力する
    void print() const {
        vector<T> fw = tovector();
        cout << "[";
        for (int i = 0; i < _n-1; ++i) cout << fw[i] << ", ";
        if (_n > 0) cout << fw[_n-1];
        cout << "]\n";
    }
};
}  // namespace titan23
