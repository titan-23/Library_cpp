/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/offline_RUQ.cpp
#pragma once

#include <iostream>
#include <numeric>
#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {

/// @brief 区間代入を全て記録してから、まとめて処理する
template<typename T>
class OfflineRUQ {
private:
    int n;
    vector<T> a, nxt;
    vector<tuple<int, int, T>> Q;

    int find(int x) {
        while (nxt[x] != x) {
            nxt[x] = nxt[nxt[x]];
            x = nxt[x];
        }
        return x;
    }

public:
    OfflineRUQ() : n(0) {}
    /// @brief 長さ n、全要素 init で構築する
    OfflineRUQ(int n, T init) : n(n), a(n, init), nxt(n+1) {}
    /// @brief 長さ n の列 a で構築する
    OfflineRUQ(int n, const vector<T> &a) : n(n), a(a), nxt(n+1) {}

    /// @brief クエリ q 個分の領域を確保する
    void reserve(int q) {
        Q.reserve(q);
    }

    /// @brief 区間 [l, r) を v にする操作を記録する / O(1)
    void apply(int l, int r, T v) {
        assert(0 <= l && l <= r && r <= n);
        Q.emplace_back(l, r, v);
    }

    /// @brief 記録した操作をまとめて適用し、結果の列を返す / O(n+qα(n))
    /// 適用後は記録を消し、結果の列を次の初期値にする
    vector<T> tovector() {
        iota(nxt.begin(), nxt.end(), 0);
        for (int i = (int)Q.size()-1; i >= 0; --i) {
            auto [l, r, v] = Q[i];
            l = find(l);
            while (l < r) {
                a[l] = v;
                nxt[l] = l + 1;
                l = find(l);
            }
        }
        Q.clear();
        return a;
    }
};
} // namespace titan23
