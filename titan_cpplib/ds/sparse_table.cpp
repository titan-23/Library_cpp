/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/sparse_table.cpp
#pragma once

#include <iostream>
#include <vector>
#include <cassert>
#include "titan_cpplib/others/bit.cpp"
using namespace std;

// SparseTable
namespace titan23 {

/// @brief 静的な列の区間積を O(1) で返す / op は冪等であること
template <class T, T (*op)(T, T), T (*e)()>
struct SparseTable {
private:
    int n;
    vector<T> data;
    vector<int> offset;

public:
    SparseTable() {}
    /// @brief 列 a から構築する / O(n log n)
    SparseTable(const vector<T> &a) : n((int)a.size()) {
        if (n == 0) return;
        int log = bit_length(n) - 1;
        offset.resize(log+1);
        int sm = 0;
        for (int i = 0; i <= log; ++i) {
            offset[i] = sm;
            sm += n - (1<<i) + 1;
        }
        data.resize(sm);
        copy(a.begin(), a.end(), data.begin());
        for (int i = 0; i < log; ++i) {
            int l = 1 << i;
            int s = n - l + 1;
            const int x = s - l;
            const auto pre1 = &data[offset[i]];
            const auto pre2 = &data[offset[i]+l];
            auto nxt = &data[offset[i+1]];
            for (int j = 0; j < x; ++j) {
                nxt[j] = op(pre1[j], pre2[j]);
            }
        }
    }

    /// @brief 区間 [l, r) の総積を返す / O(1)
    T prod(const int l, const int r) const {
        assert(0 <= l && l <= r && r <= n);
        if (l == r) return e();
        int u = bit_length(r-l) - 1;
        return op(data[offset[u]+l], data[offset[u]+r-(1<<u)]);
    }

    /// @brief 列全体の総積を返す / O(1)
    T all_prod() const {
        return prod(0, n);
    }

    /// @brief a[k] を返す / O(1)
    T get(const int k) const {
        assert(0 <= k && k < n);
        return data[k];
    }

    /// @brief 列の長さを返す / O(1)
    int size() const {
        return n;
    }

    /// @brief 列を標準出力に出す / O(n)
    void print() const {
        cout << '[';
        for (int i = 0; i < n-1; ++i) {
            cout << data[i] << ", ";
        }
        if (n > 0) {
            cout << data[n-1];
        }
        cout << ']' << endl;
    }
};
}  // namespace titan23
