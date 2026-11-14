/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/fenwick_tree_RAQ.cpp
#pragma once

#include <iostream>
#include <vector>
#include "titan_cpplib/ds/fenwick_tree.cpp"
using namespace std;

namespace titan23 {

/// @brief 区間加算と一点取得を扱う Fenwick 木 / 定数倍が軽い
template<typename T>
class FenwickTreeRAQ {
private:
    int n;
    titan23::FenwickTree<T> fw;

public:
    FenwickTreeRAQ() : n(0), fw(0) {}
    /// @brief 長さ n で構築する / 初期値 0 / O(n)
    FenwickTreeRAQ(int n) : n(n), fw(n) {}
    /// @brief 列 a から構築する / O(n log n)
    FenwickTreeRAQ(vector<T> a) : n(a.size()), fw(n) {
        for (int i = 0; i < n; ++i) add(i, a[i]);
    }

    /// @brief 全要素を 0 にする / O(n)
    void clear() {
        fw.clear();
    }

    /// @brief a[l, r) に x を加算する / O(log n)
    void add_range(int l, int r, T x) {
        fw.add(l, x);
        if (r < n) fw.add(r, -x);
    }

    /// @brief a[k] に x を加算する / O(log n)
    void add(int k, T x) {
        add_range(k, k+1, x);
    }

    /// @brief a[k] を返す / O(log n)
    T get(int k) const {
        return fw.pref(k+1);
    }

    /// @brief a[k] を x に変更する / O(log n)
    void set(int k, T x) {
        T pre = get(k);
        add(k, x-pre);
    }

    /// @brief 列を vector で返す / O(n log n)
    vector<T> tovector() const {
        vector<T> res(n);
        for (int i = 0; i < n; ++i) {
            res[i] = get(i);
        }
        return res;
    }

    friend ostream& operator<<(ostream& os, const titan23::FenwickTreeRAQ<T> &fw) {
        os << fw.tovector();
        return os;
    }
};
} // namespace titan23
