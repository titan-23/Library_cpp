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

    bool add(const T &key) {
        return tr.insert(key).second;
    }

    bool discard(const T &key) {
        return tr.erase(key);
    }

    void remove(const T &key) {
        int erased = tr.erase(key);
        assert(erased > 0);
    }

    T le(const T &key) const {
        int idx = index_right(key);
        if (idx == 0) return missing;
        return (*this)[idx - 1];
    }

    T lt(const T &key) const {
        int idx = index(key);
        if (idx == 0) return missing;
        return (*this)[idx - 1];
    }

    T ge(const T &key) const {
        int idx = index(key);
        if (idx == len()) return missing;
        return (*this)[idx];
    }

    T gt(const T &key) const {
        int idx = index_right(key);
        if (idx == len()) return missing;
        return (*this)[idx];
    }

    int index(const T &key) const {
        return tr.order_of_key(key);
    }

    int index_right(const T &key) const {
        auto it = tr.find(key);
        if (it != tr.end()) {
            return tr.order_of_key(key) + 1;
        }
        return tr.order_of_key(key);
    }

    int count(const T &key) const {
        return tr.find(key) != tr.end();
    }

    int count_range(const T low, const T high) const {
        return index(high) - index(low);
    }

    T pop(int k = -1) {
        if (k < 0) k += len();
        assert(k >= 0 && k < len());
        auto it = tr.find_by_order(k);
        T key = *it;
        tr.erase(it);
        return key;
    }

    vector<T> tovector() const {
        vector<T> res;
        res.reserve(len());
        for (auto it = tr.begin(); it != tr.end(); ++it) {
            res.push_back(*it);
        }
        return res;
    }

    bool contains(const T &key) const {
        return tr.find(key) != tr.end();
    }

    T operator[](int k) const {
        assert(0 <= k && k < len());
        return *tr.find_by_order(k);
    }

    int len() const { return tr.size(); }
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
