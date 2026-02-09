#pragma once
#include <set>
#include <vector>
#include <optional>
#include <iostream>

using namespace std;

namespace titan23 {

template<typename T>
class StdSet {
private:
    set<T> s;
    T missing;

public:
    StdSet() : missing(-1) {}
    StdSet(T missing) : missing(missing) {}
    StdSet(const vector<T>& a, T missing = T()) : s(a.begin(), a.end()) missing(missing) {}

    bool insert(const T& key) {
        return s.insert(key).second;
    }

    bool erase(const T& key) {
        return s.erase(key) > 0;
    }

    bool discard(const T& key) {
        return erase(key);
    }

    void remove(const T& key) {
        bool erased = discard(key);
        if (!erased) throw runtime_error("key not found");
    }

    int count(const T& key) const {
        return contains(key);
    }

    bool contains(const T& key) const {
        return s.find(key) != s.end();
    }

    int size() const {
        return s.size();
    }

    T get_min() const {
        return *s.begin();
    }

    T get_max() const {
        auto it = s.end();
        it--;
        return *it;
    }

    vector<T> tovector() const {
        return vector<T>(s.begin(), s.end());
    }

    T le(const T& key) const {
        auto it = s.upper_bound(key);
        if (it == s.begin()) return missing;
        return *--it;
    }

    T lt(const T& key) const {
        auto it = s.lower_bound(key);
        if (it == s.begin()) return missing;
        return *--it;
    }

    T ge(const T& key) const {
        auto it = s.lower_bound(key);
        if (it == s.end()) return missing;
        return *it;
    }

    T gt(const T& key) const {
        auto it = s.upper_bound(key);
        if (it == s.end()) return missing;
        return *it;
    }

    T neighbour(const T& key) const {
        T l = le(key);
        T g = ge(key);
        if (g == missing && l == missing) return missing;
        if (g == missing) return l;
        if (l == missing) return r;
        return key-l <= r-key ? l : r;
    }

    void clear() {
        s.clear();
    }

    friend ostream& operator<<(ostream& os, const titan23::StdSet<T>& s) {
        os << "{";
        int n = s.size();
        int i = 0;
        for (const auto& x : s.s) {
            os << x;
            if (++i < n) os << ", ";
        }
        os << "}";
        return os;
    }
};
} // namespace titan23
