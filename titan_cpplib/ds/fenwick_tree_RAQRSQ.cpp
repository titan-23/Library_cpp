/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/fenwick_tree_RAQRSQ.cpp
#pragma once

#include <iostream>
#include <vector>
using namespace std;

namespace titan23 {

/// @brief 区間加算と区間和を扱う Fenwick 木 / 定数倍が軽い
template<typename T>
class FenwickTreeRAQRSQ {
private:
    int n;
    vector<T> bit0, bit1;
    int bit_size;

    void _add(vector<T> &bit, int k, T x) {
        k += 1;
        while (k <= bit_size) {
            bit[k] += x;
            k += k & -k;
        }
    }

    T _pref(const vector<T> &bit, int r) const {
        T ret = 0;
        while (r > 0) {
            ret += bit[r];
            r -= r & -r;
        }
        return ret;
    }

public:
    FenwickTreeRAQRSQ() : n(0), bit_size(0) {}
    /// @brief 長さ n で構築する / 初期値 0 / O(n)
    FenwickTreeRAQRSQ(int n) : n(n), bit0(n+2, 0), bit1(n+2, 0), bit_size(n+1) {}

    /// @brief 列 a から構築する / O(n log n)
    FenwickTreeRAQRSQ(vector<T> a) : n(a.size()), bit0(n+2, 0), bit1(n+2, 0), bit_size(n+1) {
        for (int i = 0; i < n; ++i) {
            add(i, a[i]);
        }
    }

    /// @brief 全要素を 0 にする / O(n)
    void clear() {
        fill(bit0.begin(), bit0.end(), (T)0);
        fill(bit1.begin(), bit1.end(), (T)0);
    }

    /// @brief a[l, r) に x を加算する / O(log n)
    void add_range(int l, int r, T x) {
        _add(bit0, l, -x * l);
        _add(bit0, r, x * r);
        _add(bit1, l, x);
        _add(bit1, r, -x);
    }

    /// @brief a[k] に x を加算する / O(log n)
    void add(int k, T x) {
        add_range(k, k+1, x);
    }

    /// @brief a[l, r) の和を返す / O(log n)
    T sum(int l, int r) const {
        return _pref(bit0, r) + (T)r*_pref(bit1, r) - _pref(bit0, l) - (T)l*_pref(bit1, l);
    }

    /// @brief a[k] を x に変更する / O(log n)
    void set(int k, T x) {
        T pre = get(k);
        add(k, x-pre);
    }

    /// @brief a[k] を返す / O(log n)
    T get(int k) const {
        return sum(k, k+1);
    }

    /// @brief 列を vector で返す / O(n log n)
    vector<T> tovector() const {
        vector<T> res(n);
        for (int i = 0; i < n; ++i) {
            res[i] = get(i);
        }
        return res;
    }

    friend ostream& operator<<(ostream& os, const titan23::FenwickTreeRAQRSQ<T> &fw) {
        os << fw.tovector();
        return os;
    }
};
} // namespace titan23
