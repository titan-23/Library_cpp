/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/pbds_set.cpp
#pragma once

#include <cassert>
#include <functional>
#include <iostream>
#include <vector>
#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/tree_policy.hpp>
using namespace std;
using namespace __gnu_pbds;

namespace titan23 {

/// @brief __gnu_pbds の順序付き木で実装した集合
template<typename T>
class PBDSSet {
private:
    using TreeType = tree<T, null_type, less<T>, rb_tree_tag, tree_order_statistics_node_update>;
    TreeType tr;
    T missing;

public:
    PBDSSet() : missing() {}
    /// @brief 空で構築する / 該当する要素がないとき le などは missing を返す
    PBDSSet(T missing) : missing(missing) {}
    /// @brief 列 a の要素で構築する
    PBDSSet(const vector<T> &a, T missing) : tr(a.begin(), a.end()), missing(missing) {}

    /// @brief key がなければ追加し、追加できたかを返す / O(log N)
    bool add(const T &key) {
        return tr.insert(key).second;
    }

    /// @brief key があれば削除し、削除できたかを返す / O(log N)
    bool discard(const T &key) {
        return tr.erase(key);
    }

    /// @brief key を削除する / key があることが前提 / O(log N)
    void remove(const T &key) {
        int erased = tr.erase(key);
        assert(erased > 0);
    }

    /// @brief key 以下で最大の要素を返す / O(log N)
    T le(const T &key) const {
        int idx = index_right(key);
        if (idx == 0) return missing;
        return (*this)[idx - 1];
    }

    /// @brief key 未満で最大の要素を返す / O(log N)
    T lt(const T &key) const {
        int idx = index(key);
        if (idx == 0) return missing;
        return (*this)[idx - 1];
    }

    /// @brief key 以上で最小の要素を返す / O(log N)
    T ge(const T &key) const {
        int idx = index(key);
        if (idx == len()) return missing;
        return (*this)[idx];
    }

    /// @brief key より大きい最小の要素を返す / O(log N)
    T gt(const T &key) const {
        int idx = index_right(key);
        if (idx == len()) return missing;
        return (*this)[idx];
    }

    /// @brief key 未満の要素数を返す / O(log N)
    int index(const T &key) const {
        return tr.order_of_key(key);
    }

    /// @brief key 以下の要素数を返す / O(log N)
    int index_right(const T &key) const {
        auto it = tr.find(key);
        if (it != tr.end()) {
            return tr.order_of_key(key) + 1;
        }
        return tr.order_of_key(key);
    }

    /// @brief key の個数を返す / O(log N)
    int count(const T &key) const {
        return tr.find(key) != tr.end();
    }

    /// @brief [low, high) にある要素数を返す / O(log N)
    int count_range(const T low, const T high) const {
        return index(high) - index(low);
    }

    /// @brief 昇順 k 番目の要素を削除して返す / 負の k は末尾から数える / O(log N)
    T pop(int k = -1) {
        if (k < 0) k += len();
        assert(k >= 0 && k < len());
        auto it = tr.find_by_order(k);
        T key = *it;
        tr.erase(it);
        return key;
    }

    /// @brief 昇順に並べた vector を返す / O(n)
    vector<T> tovector() const {
        vector<T> res;
        res.reserve(len());
        for (auto it = tr.begin(); it != tr.end(); ++it) {
            res.push_back(*it);
        }
        return res;
    }

    /// @brief key があるかを返す / O(log N)
    bool contains(const T &key) const {
        return tr.find(key) != tr.end();
    }

    /// @brief 昇順 k 番目の要素を返す / O(log N)
    T operator[](int k) const {
        assert(0 <= k && k < len());
        return *tr.find_by_order(k);
    }

    /// @brief 要素数を返す / O(1)
    int len() const { return tr.size(); }

    /// @brief 要素数を返す / O(1)
    int size() const { return tr.size(); }

    friend ostream& operator<<(ostream& os, const titan23::PBDSSet<T>& s) {
        vector<T> a = s.tovector();
        int n = a.size();
        os << "{";
        for (int i = 0; i < n - 1; ++i) {
            os << a[i] << ", ";
        }
        if (n > 0) os << a.back();
        os << "}";
        return os;
    }
};

} // namespace titan23
