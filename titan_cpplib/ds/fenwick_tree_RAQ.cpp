/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/fenwick_tree_RAQ.cpp
#pragma once

#include <iostream>
#include <vector>
#include "titan_cpplib/ds/fenwick_tree.cpp"
using namespace std;

namespace titan23 {

/// 定数倍の軽い range add point get
template<typename T>
class FenwickTreeRAQ {
private:
    int n;
    titan23::FenwickTree<T> fw;

public:
    FenwickTreeRAQ() : n(0), fw(0) {}
    FenwickTreeRAQ(int n) : n(n), fw(n) {}
    FenwickTreeRAQ(vector<T> a) : n(a.size()), fw(n) {
        for (int i = 0; i < n; ++i) add(i, a[i]);
    }

    /// all 0
    void clear() {
        fw.clear();
    }

    /// add x to a[l, r)
    void add_range(int l, int r, T x) {
        fw.add(l, x);
        if (r < n) fw.add(r, -x);
    }

    /// a[k] += x;
    void add(int k, T x) {
        add_range(k, k+1, x);
    }

    /// a[k]
    T get(int k) const {
        return fw.pref(k+1);
    }

    /// a[k] <- x
    void set(int k, T x) {
        T pre = get(k);
        add(k, x-pre);
    }

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
