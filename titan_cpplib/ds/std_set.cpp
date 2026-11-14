/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/std_set.cpp
#pragma once
#include <set>
#include <vector>
#include <optional>
#include <iostream>
#include <cassert>

using namespace std;

namespace titan23 {

/// @brief std::set で実装した集合 / 該当する要素がない問い合わせには missing を返す
template<typename T>
class StdSet {
private:
    set<T> s;
    T missing;

public:
    /// @brief 空の集合を作る / missing は -1
    StdSet() : missing(-1) {}
    /// @brief 該当なしの返り値を missing として空の集合を作る
    StdSet(T missing) : missing(missing) {}
    /// @brief 列 a の要素から構築する / O(n log n)
    StdSet(const vector<T>& a, T missing = T()) : s(a.begin(), a.end()), missing(missing) {}

    /// @brief key を追加し、追加できたかを返す / O(log n)
    bool insert(const T& key) {
        return s.insert(key).second;
    }

    /// @brief key を削除し、削除できたかを返す / O(log n)
    bool erase(const T& key) {
        return s.erase(key) > 0;
    }

    /// @brief erase と同じ / O(log n)
    bool discard(const T& key) {
        return erase(key);
    }

    /// @brief key を削除する / key がなければ例外を投げる / O(log n)
    void remove(const T& key) {
        bool erased = discard(key);
        if (!erased) throw runtime_error("key not found");
    }

    /// @brief key の個数を 0 か 1 で返す / O(log n)
    int count(const T& key) const {
        return contains(key);
    }

    /// @brief key を含むかを返す / O(log n)
    bool contains(const T& key) const {
        return s.find(key) != s.end();
    }

    /// @brief 要素数を返す / O(1)
    int size() const {
        return s.size();
    }

    /// @brief size と同じ / O(1)
    int len() const {
        return s.size();
    }

    /// @brief 最小の要素を返す / 空でないことが前提 / O(1)
    T get_min() const {
        assert(!s.empty());
        return *s.begin();
    }

    /// @brief 最大の要素を返す / 空でないことが前提 / O(1)
    T get_max() const {
        assert(!s.empty());
        auto it = s.end();
        it--;
        return *it;
    }

    /// @brief 要素を昇順に並べた列を返す / O(n)
    vector<T> tovector() const {
        return vector<T>(s.begin(), s.end());
    }

    /// @brief key 以下で最大の要素を返す / なければ missing / O(log n)
    T le(const T& key) const {
        auto it = s.upper_bound(key);
        if (it == s.begin()) return missing;
        return *--it;
    }

    /// @brief key 未満で最大の要素を返す / なければ missing / O(log n)
    T lt(const T& key) const {
        auto it = s.lower_bound(key);
        if (it == s.begin()) return missing;
        return *--it;
    }

    /// @brief key 以上で最小の要素を返す / なければ missing / O(log n)
    T ge(const T& key) const {
        auto it = s.lower_bound(key);
        if (it == s.end()) return missing;
        return *it;
    }

    /// @brief key より大きい最小の要素を返す / なければ missing / O(log n)
    T gt(const T& key) const {
        auto it = s.upper_bound(key);
        if (it == s.end()) return missing;
        return *it;
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
