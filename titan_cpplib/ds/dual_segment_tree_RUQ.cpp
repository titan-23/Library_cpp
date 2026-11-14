/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/dual_segment_tree_RUQ.cpp
#pragma once

#include <iostream>
#include <vector>
#include <cassert>
#include "titan_cpplib/others/bit.cpp"
using namespace std;

namespace titan23 {

/// @brief 区間代入と1点取得を行う双対セグ木
template <class T>
class DualSegmentTreeRUQ {
private:
    int n, time, log, size;
    vector<pair<int, T>> data;

public:
    DualSegmentTreeRUQ() : time(0) {}
    /// @brief 長さ n で構築する / 初期値 init / O(n)
    DualSegmentTreeRUQ(const int n, const T init) : n(n), time(0), log(bit_length(n)), size(1<<log) {
        this->data.resize(2*size, {-1, init});
        for (int i = 0; i < n; ++i) {
            data[i+size].first = time;
        }
    }

    /// @brief 列 a から構築する / O(n)
    DualSegmentTreeRUQ(const vector<T> a) : n(a.size()), time(0), log(bit_length(n)), size(1<<log) {
        this->data.resize(2*size, {-1, T{}});
        for (int i = 0; i < n; ++i) {
            data[i+size] = {time, a[i]};
        }
    }

    /// @brief a[k] を f に更新する / O(1)
    void apply_point(int k, T f) {
        time++;
        data[k+size].first = time;
        data[k+size].second = f;
    }

    /// @brief 区間 [l, r) を f に更新する / O(log n)
    void apply(int l, int r, const T f) {
        assert(0 <= l && l <= r && r <= n);
        l += size;
        r += size;
        time++;
        while (l < r) {
            if (l & 1) {
                data[l].first = time;
                data[l].second = f;
                l++;
            }
            if (r & 1) {
                r--;
                data[r].first = time;
                data[r].second = f;
            }
            l >>= 1;
            r >>= 1;
        }
    }

    /// @brief 全要素を f に更新する / O(1)
    void all_apply(T f) {
        time++;
        data[1].first = time;
        data[1].second = f;
    }

    /// @brief 全要素を列で返す / O(n)
    vector<T> tovector() {
        for (int k = 1; k < size; ++k) {
            int t = data[k].first;
            if (data[k<<1].first < t) {
                data[k<<1] = data[k];
            }
            if (data[k<<1|1].first < t) {
                data[k<<1|1] = data[k];
            }
        }
        vector<T> res(n);
        for (int i = 0; i < n; ++i) {
            res[i] = data[i+size].second;
        }
        return res;
    }

    /// @brief a[k] を返す / O(log n)
    T get(int k) const {
        k += size;
        auto [t, ans] = data[k];
        for (int i = 1; i <= log; ++i) {
            if (t < data[k>>i].first) {
                t = data[k>>i].first;
                ans = data[k>>i].second;
            }
        }
        return ans;
    }
};
}  // namespace titan23
