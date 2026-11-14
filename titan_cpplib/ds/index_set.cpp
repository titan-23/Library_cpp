/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/index_set.cpp
#pragma once

#include <iostream>
#include <algorithm>
#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {

/// @brief 0 以上 u 未満の整数の集合 / 追加と削除が O(1)
/// 参考: https://topcoder-tomerun.hatenablog.jp/entry/2021/06/12/134643
class IndexSet {
public:
    vector<int> que, pos;  // que は要素の列、pos[v] は que での v の位置で、無ければ -1

    IndexSet() {}
    /// @brief 0 以上 u 未満の整数を入れる空の集合を作る / O(u)
    IndexSet(int u) : pos(u, -1) {}

    /// @brief v を追加する / O(1)
    void add(int v) {
        if (pos[v] != -1) return;
        pos[v] = que.size();
        que.push_back(v);
    }

    /// @brief v があれば削除して true を返す / 無ければ false / O(1)
    bool discard(int v) {
        int p = pos[v];
        if (p == -1) return false;
        int b = que.back();
        que[p] = b;
        que.pop_back();
        pos[b] = p;
        pos[v] = -1;
        return true;
    }

    /// @brief v を削除する / v があることが前提 / O(1)
    void remove(int v) {
        int p = pos[v];
        assert(p != -1);
        int b = que.back();
        que[p] = b;
        que.pop_back();
        pos[b] = p;
        pos[v] = -1;
    }

    /// @brief v があるかを返す / O(1)
    bool contains(int v) const {
        return pos[v] != -1;
    }

    /// @brief 全要素を削除する / O(要素数)
    void clear() {
        for (int v : que) {
            pos[v] = -1;
        }
        que.clear();
    }

    int len() const {
        return que.size();
    }

    bool empty() const {
        return que.size() == 0;
    }

    /// @brief que の v 番目の要素を返す / 順序は削除で入れ替わる / O(1)
    int get(int v) const {
        assert(0 <= v && v < len());
        return que[v];
    }

    friend ostream& operator<<(ostream& os, const titan23::IndexSet &ist) {
        vector<int> a = ist.que;
        sort(a.begin(), a.end());
        int n = a.size();
        os << "{";
        for (int i = 0; i < n; ++i) {
            os << a[i];
            if (i != n-1) os << ", ";
        }
        os << "}";
        return os;
    }
};
} // namespace titan23
