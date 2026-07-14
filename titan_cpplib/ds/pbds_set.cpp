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

template<typename T>
class PBDSSet {
private:
    using TreeType = tree<T, null_type, less<T>, rb_tree_tag, tree_order_statistics_node_update>;
    TreeType tr;
    T missing;

public:
    PBDSSet() : missing() {}
    PBDSSet(T missing) : missing(missing) {}
    PBDSSet(const vector<T> &a, T missing) : tr(a.begin(), a.end()), missing(missing) {}

    /// `key` がなければ追加し、追加できたかを返す / `O(logn)`
    bool add(const T &key) {
        return tr.insert(key).second;
    }

    /// `key` があれば削除し、削除できたかを返す / `O(logn)`
    bool discard(const T &key) {
        return tr.erase(key);
    }

    /// `key` を削除する 存在することが前提 / `O(logn)`
    void remove(const T &key) {
        int erased = tr.erase(key);
        assert(erased > 0);
    }

    /// `key` 以下で最大 / `O(logn)`
    T le(const T &key) const {
        int idx = index_right(key);
        if (idx == 0) return missing;
        return (*this)[idx - 1];
    }

    /// `key` 未満で最大 / `O(logn)`
    T lt(const T &key) const {
        int idx = index(key);
        if (idx == 0) return missing;
        return (*this)[idx - 1];
    }

    /// `key` 以上で最小 / `O(logn)`
    T ge(const T &key) const {
        int idx = index(key);
        if (idx == len()) return missing;
        return (*this)[idx];
    }

    /// `key` より大きくて最小 / `O(logn)`
    T gt(const T &key) const {
        int idx = index_right(key);
        if (idx == len()) return missing;
        return (*this)[idx];
    }

    /// `key` 未満の要素数を返す / `O(logn)`
    int index(const T &key) const {
        return tr.order_of_key(key);
    }

    /// `key` 以下の要素数を返す / `O(logn)`
    int index_right(const T &key) const {
        auto it = tr.find(key);
        if (it != tr.end()) {
            return tr.order_of_key(key) + 1;
        }
        return tr.order_of_key(key);
    }

    /// `key` の要素数を返す / `O(logn)`
    int count(const T &key) const {
        return tr.find(key) != tr.end();
    }

    /// `[low, high)` の要素数を返す / `O(logn)`
    int count_range(const T low, const T high) const {
        return index(high) - index(low);
    }

    /// 昇順 `k` 番目の要素を削除して返す / `O(logn)`
    T pop(int k = -1) {
        if (k < 0) k += len();
        assert(k >= 0 && k < len());
        auto it = tr.find_by_order(k);
        T key = *it;
        tr.erase(it);
        return key;
    }

    /// 昇順の `vector` にして返す / `O(n)`
    vector<T> tovector() const {
        vector<T> res;
        res.reserve(len());
        for (auto it = tr.begin(); it != tr.end(); ++it) {
            res.push_back(*it);
        }
        return res;
    }

    /// `key` の存在判定 / `O(logn)`
    bool contains(const T &key) const {
        return tr.find(key) != tr.end();
    }

    /// 昇順 `k` 番目の要素を返す / `O(logn)`
    T operator[](int k) const {
        assert(0 <= k && k < len());
        return *tr.find_by_order(k);
    }

    /// 要素数を返す / `O(1)`
    int len() const { return tr.size(); }

    /// 要素数を返す / `O(1)`
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
