/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/sqrt_segment_tree_sum.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>
using namespace std;

namespace titan23 {

/// @brief 加算専用の平方分割 (`T{}` を零とする)
template<class T>
class SqrtSegmentTreeSum {
private:
    int n, bucket_size;
    vector<vector<T>> a;
    vector<T> data;
    T total;

public:
    SqrtSegmentTreeSum() : SqrtSegmentTreeSum(0) {}

    /// @brief 長さ n の零列で構築する / O(n)
    SqrtSegmentTreeSum(int n) : n(n), bucket_size(1), total(T{}) {
        assert(n >= 0);
        bucket_size = sqrt(n) + 1;
        const int bucket_cnt = n / bucket_size + (n % bucket_size != 0);
        a.resize(bucket_cnt);
        data.assign(bucket_cnt, T{});
        for (int k = 0; k < bucket_cnt; ++k) {
            a[k].assign(min(bucket_size, n - k * bucket_size), T{});
        }
    }

    /// @brief 列 values から構築する / O(n)
    SqrtSegmentTreeSum(const vector<T> &values)
            : SqrtSegmentTreeSum(values.size()) {
        int i = 0;
        for (int k = 0; k < (int)a.size(); ++k) {
            for (T &v : a[k]) {
                v = values[i++];
                data[k] += v;
            }
            total += data[k];
        }
    }

    /// @brief 列の長さを返す / O(1)
    int size() const { return n; }

    /// @brief `a[i]` を返す / O(1)
    T get(int i) const {
        assert(0 <= i && i < n);
        const int k = i / bucket_size;
        return a[k][i - k * bucket_size];
    }

    /// @brief `a[i]` を `v` に変更する / O(1)
    void set(int i, const T &v) {
        assert(0 <= i && i < n);
        const int k = i / bucket_size, j = i - k * bucket_size;
        data[k] = data[k] - a[k][j] + v;
        total = total - a[k][j] + v;
        a[k][j] = v;
    }

    /// @brief `a[i]` に `v` を加算する / O(1)
    void add(int i, const T &v) {
        assert(0 <= i && i < n);
        const int k = i / bucket_size;
        a[k][i - k * bucket_size] += v;
        data[k] += v;
        total += v;
    }

    /// @brief 区間 `[l, r)` の和を返す / O(√n)
    T prod(int l, int r) const {
        assert(0 <= l && l <= r && r <= n);
        if (l == r) return T{};
        const int k1 = l / bucket_size, k2 = r / bucket_size;
        l -= k1 * bucket_size;
        r -= k2 * bucket_size;
        T sum{};
        if (k1 == k2) {
            for (int i = l; i < r; ++i) sum += a[k1][i];
        } else {
            for (int i = l; i < (int)a[k1].size(); ++i) sum += a[k1][i];
            for (int k = k1 + 1; k < k2; ++k) sum += data[k];
            for (int i = 0; i < r; ++i) sum += a[k2][i];
        }
        return sum;
    }

    /// @brief 列全体の和を返す / O(1)
    T all_prod() const { return total; }
};
}
