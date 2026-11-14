/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/static_set.cpp
#pragma once

#include <iostream>
#include <cassert>
#include <vector>
#include <algorithm>
using namespace std;

// StaticSet
namespace titan23 {

/// @brief 静的な全順序集合 / 探索に失敗すると missing を返す
template<typename T>
class StaticSet {
private:
    vector<T> data;
    T missing;
    int n;

public:
    StaticSet() : missing(-1), n(0) {}

    /// @brief 空の集合を構築する / missing は要素に使わない値 / O(1)
    StaticSet(T missing) : missing(missing), n(0) {}

    /// @brief 列 a から構築する / missing は要素に使わない値 / O(nlogn)
    StaticSet(const vector<T> &a, T missing) : data(a), missing(missing) {
        sort(data.begin(), data.end());
        data.erase(unique(data.begin(), data.end()), data.end());
        n = (int)data.size();
    }

    /// @brief 要素数を返す / O(1)
    int len() const {
        return n;
    }

    /// @brief 要素数を返す / O(1)
    int size() const {
        return n;
    }

    /// @brief 空かどうか判定する / O(1)
    bool empty() const {
        return n == 0;
    }

    /// @brief 昇順で k 番目の要素を返す / k は 0 始まり / O(1)
    T get(const int k) const {
        assert(0 <= k && k < len());
        return data[k];
    }

    /// @brief key 以上で最小の要素を返す / O(logn)
    T ge(const T &key) const {
        auto it = lower_bound(data.begin(), data.end(), key);
        if (it == data.end()) return missing;
        return *it;
    }

    /// @brief key より大きい最小の要素を返す / O(logn)
    T gt(const T &key) const {
        auto it = upper_bound(data.begin(), data.end(), key);
        if (it == data.end()) return missing;
        return *it;
    }

    /// @brief key 以下で最大の要素を返す / O(logn)
    T le(const T &key) const {
        auto it = upper_bound(data.begin(), data.end(), key);
        if (it == data.begin()) return missing;
        return *(--it);
    }

    /// @brief key 未満で最大の要素を返す / O(logn)
    T lt(const T &key) const {
        auto it = lower_bound(data.begin(), data.end(), key);
        if (it == data.begin()) return missing;
        return *(--it);
    }

    /// @brief upper 未満の要素数を返す / O(logn)
    int index(const T &upper) const {
        auto it = lower_bound(data.begin(), data.end(), upper);
        return distance(data.begin(), it);
    }

    /// @brief upper 以下の要素数を返す / O(logn)
    int index_right(const T &upper) const {
        auto it = upper_bound(data.begin(), data.end(), upper);
        return distance(data.begin(), it);
    }

    /// @brief key に等しい要素の個数を返す / O(logn)
    int count(const T &key) const {
        return contains(key) ? 1 : 0;
    }

    /// @brief [lower, upper) の要素数を返す / O(logn)
    int count_range(const T &lower, const T &upper) const {
        assert(lower <= upper);
        return index(upper) - index(lower);
    }

    /// @brief key があるか判定する / O(logn)
    bool contains(const T &key) const {
        int idx = index(key);
        if (idx == (int)data.size()) {
            return false;
        }
        return data[idx] == key;
    }

    /// @brief key に最も近い要素を返す / 差が等しいときは小さい方 / O(logn)
    T neighbor(const T &key) const {
        T ge_key = ge(key);
        T le_key = le(key);
        if (ge_key == missing) return le_key;
        if (le_key == missing) return ge_key;
        return (ge_key-key < key-le_key) ? ge_key : le_key;
    }

    friend ostream& operator<<(ostream& os, const titan23::StaticSet<T>& s) {
        int n = s.len();
        os << "{";
        for (int i = 0; i < n; ++i) {
            os << s.data[i];
            if (i != n-1) os << ", ";
        }
        os << "}";
        return os;
    }
};
}
