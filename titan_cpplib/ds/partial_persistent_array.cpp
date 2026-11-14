/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/partial_persistent_array.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>
using namespace std;

namespace titan23 {

/// @brief 部分永続配列 / 最新の時刻にだけ書き込み、過去の任意の時刻の値を読める
template <typename T>
class PartialPersistentArray {
private:
    vector<vector<T>> a;
    vector<vector<int>> ts;
    int last;

public:
    /// @brief 列 A を時刻 0 の値として構築する / O(n)
    PartialPersistentArray(const vector<T> &A) {
        int n = A.size();
        a.resize(n);
        ts.resize(n);
        for (int i = 0; i < n; ++i) {
            a[i].push_back(A[i]);
            ts[i].push_back(0);
        }
        last = 0;
    }

    /// @brief 時刻 t に a[k] を v にする / 償却 O(1)
    /// t は直前の set の時刻以上で、a[k] を最後に変更した時刻より大きいことが前提
    void set(int k, const T &v, int t) {
        assert(t >= last);
        assert(t > ts[k].back());
        a[k].push_back(v);
        ts[k].push_back(t);
        last = t;
    }

    /// @brief 時刻 t における a[k] を返す / t = -1 なら最新の値
    T get(int k, int t = -1) const {
        if (t == -1 || t >= ts[k].back()) {
            return a[k].back();
        }
        auto it = upper_bound(ts[k].begin(), ts[k].end(), t);
        int idx = (it - ts[k].begin()) - 1;
        return a[k][idx];
    }

    /// @brief 時刻 t における配列全体を返す
    vector<T> tovector(int t) const {
        int n = a.size();
        vector<T> res(n);
        for (int i = 0; i < n; ++i) {
            res[i] = get(i, t);
        }
        return res;
    }

    /// @brief 時刻 t における配列を出力する
    void show(int t) const {
        cout << "Time: " << t << " ";
        auto v = tovector(t);
        for (const auto &x : v) cout << x << " ";
        cout << "\n";
    }

    /// @brief 配列の長さを返す / O(1)
    int size() const { return a.size(); }

    /// @brief 時刻 0 から最後に set した時刻まで、各時刻の配列を出力する
    void show_all() const {
        for (int i = 0; i <= last; ++i) {
            show(i);
        }
    }
};
} // namespace titan23
