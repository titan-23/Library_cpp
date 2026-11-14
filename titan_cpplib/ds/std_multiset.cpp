/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/std_multiset.cpp
#pragma once
#include <map>
#include <vector>
#include <cassert>
#include <iostream>
#include <algorithm>

using namespace std;

namespace titan23 {

/// @brief std::map で要素ごとの個数を持つ多重集合 / 該当する要素がない問い合わせには missing を返す
template<typename T>
class StdMultiset {
private:
    using ll = long long;
    map<T, ll> s;
    T missing;
    ll len;

public:
    /// @brief 空の多重集合を作る / missing は T()
    StdMultiset() : missing(T()), len(0) {}
    /// @brief 該当なしの返り値を missing として空の多重集合を作る
    StdMultiset(T missing) : missing(missing), len(0) {}
    /// @brief 列 a の要素から構築する / O(n log n)
    StdMultiset(const vector<T> &a, T missing = T()) : missing(missing), len(0) {
        for (const T &x : a) {
            s[x]++;
        }
        len = a.size();
    }

    /// @brief key を cnt 個追加する / O(log n)
    void insert(const T key, ll cnt = 1) {
        s[key] += cnt;
        len += cnt;
    }

    /// @brief key を cnt 個削除する / 個数が cnt 未満ならすべて削除する / O(log n)
    void erase(const T key, ll cnt = 1) {
        auto it = s.find(key);
        if (it == s.end()) return;
        cnt = max(0LL, it->second - cnt);
        len -= it->second - cnt;
        it->second = cnt;
        if (it->second == 0) s.erase(it);
    }

    /// @brief erase と同じ / O(log n)
    void discard(const T key, ll cnt = 1) {
        erase(key, cnt);
    }

    /// @brief key を cnt 個削除する / 個数が cnt 未満なら例外を投げる / O(log n)
    void remove(const T key, ll cnt = 1) {
        auto it = s.find(key);
        if (it == s.end() || it->second < cnt) throw runtime_error("key not found in titan23::StdMultiset");
        len -= cnt;
        it->second -= cnt;
        if (it->second == 0) s.erase(it);
    }

    /// @brief key を含むかを返す / O(log n)
    bool contains(const T key) const {
        return s.find(key) != s.end();
    }

    /// @brief key の個数を返す / O(log n)
    ll count(const T key) const {
        auto it = s.find(key);
        return it == s.end() ? 0 : it->second;
    }

    /// @brief 空かを返す / O(1)
    bool empty() const {
        return size() == 0;
    }

    /// @brief 重複を含めた要素数を返す / O(1)
    ll size() const {
        return len;
    }

    /// @brief 異なる要素の個数を返す / O(1)
    ll size_unique() const {
        return s.size();
    }

    /// @brief 最小の要素を返す / 空なら missing / O(1)
    T get_min() const {
        if (s.empty()) return missing;
        return s.begin()->first;
    }

    /// @brief 最大の要素を返す / 空なら missing / O(1)
    T get_max() const {
        if (s.empty()) return missing;
        auto it = s.end(); it--;
        return it->first;
    }

    /// @brief 異なる要素を昇順に並べた列を返す / O(n)
    vector<T> tovector_unique() const {
        vector<T> a;
        a.reserve(size_unique());
        for (auto [k, v] : s) {
            a.emplace_back(k);
        }
        return a;
    }

    /// @brief 重複を含めた全要素を昇順に並べた列を返す / O(n)
    vector<T> tovector() const {
        vector<T> a;
        a.reserve(size());
        for (auto [k, v] : s) {
            for (int i = 0; i < v; ++i) {
                a.emplace_back(k);
            }
        }
        return a;
    }

    /// @brief key 以下で最大の要素を返す / なければ missing / O(log n)
    T le(const T& key) const {
        auto it = s.upper_bound(key);
        if (it == s.begin()) return missing;
        --it;
        return it->first;
    }

    /// @brief key 未満で最大の要素を返す / なければ missing / O(log n)
    T lt(const T& key) const {
        auto it = s.lower_bound(key);
        if (it == s.begin()) return missing;
        --it;
        return it->first;
    }

    /// @brief key 以上で最小の要素を返す / なければ missing / O(log n)
    T ge(const T& key) const {
        auto it = s.lower_bound(key);
        if (it == s.end()) return missing;
        return it->first;
    }

    /// @brief key より大きい最小の要素を返す / なければ missing / O(log n)
    T gt(const T& key) const {
        auto it = s.upper_bound(key);
        if (it == s.end()) return missing;
        return it->first;
    }

    /// @brief key に最も近い要素を返す / 距離が等しければ小さい方 / なければ missing / O(log n)
    T neighbour(const T& key) const {
        T l = le(key);
        T g = ge(key);
        if (g == missing && l == missing) return missing;
        if (g == missing) return l;
        if (l == missing) return g;
        return key-l <= g-key ? l : g;
    }

    /// @brief すべての要素を削除する / O(n)
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
