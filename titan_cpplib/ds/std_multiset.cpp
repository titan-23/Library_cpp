#pragma once
#include <map>
#include <vector>
#include <cassert>
#include <iostream>
#include <algorithm>

using namespace std;

namespace titan23 {

template<typename T>
class StdMultiset {
private:
    using ll = long long;
    map<T, ll> s;
    T missing;
    ll len;

public:
    StdMultiset() : missing(T()), len(0) {}
    StdMultiset(T missing) : missing(missing), len(0) {}
    StdMultiset(const vector<T> &a, T missing = T()) : missing(missing), len(0) {
        for (const T &x : a) {
            s[x]++;
        }
        len = a.size();
    }

    void insert(const T key, ll cnt = 1) {
        s[key] += cnt;
        len += cnt;
    }

    void erase(const T key, ll cnt = 1) {
        auto it = s.find(key);
        if (it == s.end()) return;
        cnt = max(0, it->second - cnt);
        len -= it->second - cnt;
        it->second = cnt;
        if (it->second == 0) s.erase(it);
    }

    void discard(const T key, ll cnt = 1) {
        erase(key, cnt);
    }

    void remove(const T key, ll cnt = 1) {
        auto it = s.find(key);
        if (it == s.end() || it->second < cnt) throw runtime_error("key not found in titan23::StdMultiset");
        len -= cnt;
        it->second -= cnt;
        if (it->second == 0) s.erase(it);
    }

    bool contains(const T key) const {
        return s.find(key) != s.end();
    }

    ll count(const T key) const {
        auto it = s.find(key);
        return it == s.end() ? 0 : it->second;
    }

    bool empty() const {
        return size() == 0;
    }

    ll size() const {
        return len;
    }

    ll size_unique() const {
        return s.size();
    }

    T get_min() const {
        if (s.empty()) return missing;
        return s.begin()->first;
    }

    T get_max() const {
        if (s.empty()) return missing;
        auto it = s.end(); it--;
        return it->first;
    }

    vector<T> to_vector_unique() const {
        vector<T> a;
        a.reserve(size_unique());
        for (auto [k, v] : s) {
            a.emplace_back(k);
        }
        return a;
    }

    vector<T> to_vector() const {
        vector<T> a;
        a.reserve(size());
        for (auto [k, v] : s) {
            for (int i = 0; i < v; ++i) {
                a.emplace_back(k);
            }
        }
        return a;
    }

    T le(const T& key) const {
        auto it = s.upper_bound(key);
        if (it == s.begin()) return missing;
        --it;
        return it->first;
    }

    T lt(const T& key) const {
        auto it = s.lower_bound(key);
        if (it == s.begin()) return missing;
        --it;
        return it->first;
    }

    T ge(const T& key) const {
        auto it = s.lower_bound(key);
        if (it == s.end()) return missing;
        return it->first;
    }

    T gt(const T& key) const {
        auto it = s.upper_bound(key);
        if (it == s.end()) return missing;
        return it->first;
    }

    T neighbour(const T& key) const {
        T l = le(key);
        T g = ge(key);
        if (g == missing && l == missing) return missing;
        if (g == missing) return l;
        if (l == missing) return g;
        return key-l <= g-key ? l : g;
    }

    void clear() {
        s.clear();
        len = 0;
    }

    friend ostream& operator<<(ostream& os, const titan23::StdMultiset<T> &ms) {
        os << "{";
        bool f = false;
        for (auto [k, v] : ms.s) {
            for (int i = 0; i < v; ++i) {
                if (f) os << ", ";
                f = true;
                os << k;
            }
        }
        os << "}";
        return os;
    }
};

} // namespace titan23
