/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/sqrt_segment_tree.cpp
#pragma once

#include <cmath>
#include <cassert>
#include <vector>
using namespace std;

namespace titan23 {

template <class T, T (*op)(T, T), T (*e)(), T (*inv)(T)>
class SqrtSegmentTree {
private:
    int n, size;
    vector<vector<T>> a;
    vector<T> data;
    T total;

public:
    SqrtSegmentTree() : n(0), size(0), total(e()) {}

    SqrtSegmentTree(int n) : n(n), total(e()) {
        size = sqrt(n) + 1;
        int bucket_cnt = (n+size-1)/size;
        a.resize(bucket_cnt);
        data.resize(bucket_cnt);
        for (int i = 0; i < bucket_cnt; ++i) {
            int k = min(n, (i+1)*size) - i*size;
            a[i].resize(k, e());
            data[i] = e();
        }
    }

    SqrtSegmentTree(const vector<T> &A) : n(A.size()), total(e()) {
        size = sqrt(n) + 1;
        int bucket_cnt = (n+size-1)/size;
        a.resize(bucket_cnt);
        data.resize(bucket_cnt);
        for (int i = 0; i < bucket_cnt; ++i) {
            a[i] = vector<T>(A.begin()+i*size, A.begin()+min(n, (i+1)*size));
            T s = e();
            for (int j = 0; j < a[i].size(); ++j) {
                s = op(s, a[i][j]);
            }
            data[i] = s;
            total = op(total, s);
        }
    }

    /// @brief 区間 `[l, r)` の総積を返す / O(√n)
    T prod(int l, int r) const {
        assert(0 <= l && l <= r && r <= n);
        if (l == r) return e();
        int k1 = l / size;
        int k2 = r / size;
        l -= k1 * size;
        r -= k2 * size;
        T s = e();
        if (k1 == k2) {
            for (int i = l; i < r; ++i) s = op(s, a[k1][i]);
        } else {
            for (int i = l; i < a[k1].size(); ++i) s = op(s, a[k1][i]);
            for (int i = k1+1; i < k2; ++i) s = op(s, data[i]);
            if (k2 < a.size()) {
                for (int i = 0; i < r; ++i) s = op(s, a[k2][i]);
            }
        }
        return s;
    }

    /// @brief 全体の総積を返す / O(1)
    T all_prod() const {
        return total;
    }

    /// @brief `a[i]` を返す / O(1)
    T get(int i) const {
        int k = i / size;
        return a[k][i-k*size];
    }

    /// @brief `a[i]` を `v` に変更する / O(1)(`op` が可換群であることが前提)
    void set(int i, const T &v) {
        int k = i / size;
        int j = i - k*size;
        T old_bucket = data[k];
        data[k] = op(op(data[k], inv(a[k][j])), v);
        a[k][j] = v;
        total = op(op(total, inv(old_bucket)), data[k]);
    }
};
} // namespace titan23
