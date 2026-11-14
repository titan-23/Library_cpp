/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/static_multiset.cpp
#pragma once

#include <iostream>
#include <cassert>
#include <vector>
#include <algorithm>
using namespace std;

// StaticMultiset
namespace titan23 {

/// @brief 静的な多重集合 / 探索に失敗すると missing を返す
template<typename T>
class StaticMultiset {
private:
    vector<T> data;
    T missing;
    int n;

public:
    StaticMultiset() : missing(-1), n(0) {}
    /// @brief 空の多重集合を構築する / missing は要素に使わない値 / O(1)
    StaticMultiset(T missing) : missing(missing), n(0) {}
    /// @brief 列 a から構築する / missing は要素に使わない値 / O(nlogn)
    StaticMultiset(const vector<T> &a, T missing) : data(a), missing(missing) {
        sort(data.begin(), data.end());
        n = (int)data.size();
    }

    /// @brief 要素数を返す / O(1)
    int len() const {
        return n;
    }

    /// @brief 空かどうか判定する / O(1)
    bool empty() const {
        return n == 0;
    }

    /// @brief 昇順で i 番目の要素を返す / i は 0 始まり / O(1)
    T get(const int i) const {
        assert(0 <= i && i < len());
        return data[i];
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
        return index_right(key) - index(key);
    }

    /// @brief [lower, upper) の要素数を返す / O(logn)
    int count_range(const T &lower, const T &upper) const {
        assert(lower <= upper);
        return index(upper) - index(lower);
    }

    /// @brief key があるか判定する / O(logn)
    bool contains(const T &key) const {
        auto it = lower_bound(data.begin(), data.end(), key);
        return it != data.end() && *it == key;
    }

    friend ostream& operator<<(ostream& os, const titan23::StaticMultiset<T> &s) {
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
