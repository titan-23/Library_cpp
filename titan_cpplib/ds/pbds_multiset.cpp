/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/pbds_multiset.cpp
#pragma once

#include <cassert>
#include <functional>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>
#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/tree_policy.hpp>
using namespace std;
using namespace __gnu_pbds;

namespace titan23 {

/// @brief __gnu_pbds の順序付き木で実装した多重集合
template<typename T>
class PBDSMultiset {
private:
    using TreeType = tree<pair<T, int>, null_type, less<pair<T, int>>, rb_tree_tag, tree_order_statistics_node_update>;
    TreeType tr;
    T missing;
    int id_counter;

public:
    PBDSMultiset() : missing(), id_counter(0) {}
    /// @brief 空で構築する / 該当する要素がないとき le などは missing を返す
    PBDSMultiset(T missing) : missing(missing), id_counter(0) {}
    /// @brief 列 a の要素で構築する / O(n log n)
    PBDSMultiset(const vector<T> &a, T missing) : missing(missing), id_counter(0) {
        for (const auto& x : a) {
            add(x, 1);
        }
    }

    /// @brief key を val 個追加する / O(val log n)
    void add(const T &key, int val = 1) {
        assert(val >= 0);
        for (int i = 0; i < val; ++i) {
            tr.insert({key, id_counter++});
        }
    }

    /// @brief key を最大 val 個削除し、1 個以上削除できたかを返す / O(val log n)
    bool discard(const T &key, int val = 1) {
        assert(val >= 0);
        bool removed = false;
        for (int i = 0; i < val; ++i) {
            auto it = tr.lower_bound({key, -1});
            if (it == tr.end() || it->first != key) {
                break;
            }
            tr.erase(it);
            removed = true;
        }
        return removed;
    }

    /// @brief key を val 個削除する / key が val 個以上あることが前提 / O(val log n)
    void remove(const T &key, int val = 1) {
        for (int i = 0; i < val; ++i) {
            auto it = tr.lower_bound({key, -1});
            assert(it != tr.end() && it->first == key);
            tr.erase(it);
        }
    }

    /// @brief key 以下で最大の要素を返す / O(log n)
    T le(const T &key) const {
        int idx = index_right(key);
        if (idx == 0) return missing;
        return (*this)[idx - 1];
    }

    /// @brief key 未満で最大の要素を返す / O(log n)
    T lt(const T &key) const {
        int idx = index(key);
        if (idx == 0) return missing;
        return (*this)[idx - 1];
    }

    /// @brief key 以上で最小の要素を返す / O(log n)
    T ge(const T &key) const {
        int idx = index(key);
        if (idx == len()) return missing;
        return (*this)[idx];
    }

    /// @brief key より大きい最小の要素を返す / O(log n)
    T gt(const T &key) const {
        int idx = index_right(key);
        if (idx == len()) return missing;
        return (*this)[idx];
    }

    /// @brief key 未満の要素数を返す / O(log n)
    int index(const T &key) const {
        return tr.order_of_key({key, -1});
    }

    /// @brief key 以下の要素数を返す / O(log n)
    int index_right(const T &key) const {
        return tr.order_of_key({key, numeric_limits<int>::max()});
    }

    /// @brief key の個数を返す / O(log n)
    int count(const T &key) const {
        return index_right(key) - index(key);
    }

    /// @brief [low, high) にある要素数を返す / O(log n)
    int count_range(const T low, const T high) const {
        return index(high) - index(low);
    }

    /// @brief 昇順 k 番目の要素を削除して返す / 負の k は末尾から数える / O(log n)
    T pop(int k = -1) {
        if (k < 0) k += len();
        assert(k >= 0 && k < len());
        auto it = tr.find_by_order(k);
        T key = it->first;
        tr.erase(it);
        return key;
    }

    /// @brief 昇順に並べた vector を返す / O(n)
    vector<T> tovector() const {
        vector<T> res;
        res.reserve(len());
        for (auto it = tr.begin(); it != tr.end(); ++it) {
            res.push_back(it->first);
        }
        return res;
    }

    /// @brief key があるかを返す / O(log n)
    bool contains(const T &key) const {
        auto it = tr.lower_bound({key, -1});
        return it != tr.end() && it->first == key;
    }

    /// @brief 昇順 k 番目の要素を返す / O(log n)
    T operator[](int k) const {
        assert(0 <= k && k < len());
        return tr.find_by_order(k)->first;
    }

    /// @brief 要素数を返す / O(1)
    int len() const { return tr.size(); }

    /// @brief 要素数を返す / O(1)
    int size() const { return tr.size(); }

    friend ostream& operator<<(ostream& os, const titan23::PBDSMultiset<T> &s) {
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
