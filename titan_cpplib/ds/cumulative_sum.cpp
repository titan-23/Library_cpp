/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/cumulative_sum.cpp
#pragma once

#include <iostream>
#include <vector>
#include "titan_cpplib/others/print.cpp"
using namespace std;

namespace titan23 {

/// @brief 1次元累積和
template<typename T>
class CumulativeSum {
private:
    int n;
    vector<T> acc;

public:
    CumulativeSum() {}
    /// @brief 列 a と単位元 e から構築する / O(n)
    CumulativeSum(const vector<T> &a, T e) : n((int)a.size()), acc(n+1, e) {
        for (int i = 0; i < n; ++i) {
            acc[i+1] = acc[i] + a[i];
        }
    }

    /// @brief 区間 [0, r) の和を返す / O(1)
    T pref(const int r) const {
        assert(0 <= r && r <= n);
        return acc[r];
    }

    /// @brief 列全体の和を返す / O(1)
    T all_sum() const {
        return acc.back();
    }

    /// @brief 区間 [l, r) の和を返す / O(1)
    T sum(const int l, const int r) const {
        assert(0 <= l && l <= r && r <= n);
        return acc[r] - acc[l];
    }

    /// @brief 区間 [l, r) の和を返す / O(1)
    T prod(const int l, const int r) const {
        assert(0 <= l && l <= r && r <= n);
        return acc[r] - acc[l];
    }

    /// @brief 列全体の和を返す / O(1)
    T all_prod() const {
        return acc.back();
    }

    /// @brief 列の長さを返す / O(1)
    int len() const {
        return n;
    }

    /// @brief 累積和の配列を出力する / O(n)
    void print() const {
        cout << '[';
        for (int i = 0; i < n; ++i) {
            cout << acc[i] << ", ";
        }
        if (n > 0) cout << acc.back();
        cout << ']' << endl;
    }

    friend ostream& operator<<(ostream& os, CumulativeSum<T> &s) {
        os << s.acc;
        return os;
    }

};
}  // namespace titan23
