/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/dual_segment_tree_RUQ2.cpp
#pragma once

#include <iostream>
#include <vector>
#include <cassert>
#include "titan_cpplib/others/bit.cpp"
using namespace std;

namespace titan23 {

/// @brief 区間代入と1点取得を行う双対セグ木 / 更新値を更新順の配列に持つため空間 O(n + q)、q は更新回数
/// q が n 以下のとき速いかもしれない
template <class T>
class DualSegmentTreeRUQ {
private:
    int n, time, log, size;
    vector<T> stamp;
    vector<int> data;

public:
    DualSegmentTreeRUQ() : time(0) {}
    /// @brief 長さ n で構築する / 初期値 init / O(n)
    DualSegmentTreeRUQ(const int n, const T init) : n(n), time(0), log(bit_length(n)), size(1<<log) {
        data.resize(2*size, 0);
        stamp.emplace_back(init);
    }

    /// @brief 列 a から構築する / O(n)
    DualSegmentTreeRUQ(const vector<T> a) : n(a.size()), time(0), log(bit_length(n)), size(1<<log) {
        data.resize(2*size);
        for (int i = 0; i < n; ++i) {
            data[i+size] = time;
            time++;
        }
        stamp = a;
    }

    /// @brief 更新 q 回分の領域を予約する
    void reserve(int q) {
        stamp.reserve(q+1);
    }

    /// @brief a[k] を f に更新する / O(1)
    void apply_point(int k, T f) {
        time++;
        data[k+size] = time;
        stamp.emplace_back(f);
    }

    /// @brief 区間 [l, r) を f に更新する / O(log n)
    void apply(int l, int r, const T f) {
        assert(0 <= l && l <= r && r <= n);
        l += size;
        r += size;
        time++;
        stamp.emplace_back(f);
        while (l < r) {
            if (l & 1) {
                data[l] = time;
                l++;
            }
            if (r & 1) {
                r--;
                data[r] = time;
            }
            l >>= 1;
            r >>= 1;
        }
    }

    /// @brief 全要素を f に更新する / O(1)
    void all_apply(T f) {
        time++;
        data[1] = time;
        stamp.emplace_back(f);
    }

    /// @brief 全要素を列で返す / O(n)
    vector<T> tovector() {
        for (int k = 1; k < size; ++k) {
            int t = data[k];
            if (data[k<<1] < t) data[k<<1] = t;
            if (data[k<<1|1] < t) data[k<<1|1] = t;
        }
        vector<T> res(n);
        for (int i = 0; i < n; ++i) {
            res[i] = stamp[data[i+size]];
        }
        return res;
    }

    /// @brief a[k] を返す / O(log n)
    T get(int k) const {
        k += size;
        int t = data[k];
        for (int i = 1; i <= log; ++i) {
            if (t < data[k>>i]) {
                t = data[k>>i];
            }
        }
        return stamp[t];
    }
};
}  // namespace titan23
