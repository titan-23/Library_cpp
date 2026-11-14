/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/range_distinct_query_point_update.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <iterator>
#include <map>
#include <set>
#include <vector>
#include "titan_cpplib/ds/dynamic_wavelet_tree.cpp"
using namespace std;

namespace titan23 {

/// @brief 一点更新に対応し、区間内の異なる値の個数を数える / 0-indexed
/// 値の同一性は operator< で判定する
template<typename T>
class PointUpdateRangeDistinctQuery {
private:
    vector<T> a;
    map<T, std::set<int>> pos;
    // wm[i] は a[i] と同じ値が直前に現れた位置 +1 で、無ければ 0
    DynamicWaveletTree<int> wm;

    static vector<int> build_prev(const vector<T> &a, map<T, std::set<int>> &pos) {
        vector<int> prev(a.size());
        for (int i = 0; i < static_cast<int>(a.size()); ++i) {
            auto &s = pos[a[i]];
            prev[i] = s.empty() ? 0 : *s.rbegin() + 1;
            s.insert(s.end(), i);
        }
        return prev;
    }

public:
    /// @brief 空の列を構築する / O(1)
    PointUpdateRangeDistinctQuery() : wm(1) {}

    /// @brief 列 a から構築する / O(n log n)
    explicit PointUpdateRangeDistinctQuery(const vector<T> &a)
        : a(a), pos(), wm(max(1, static_cast<int>(a.size())), build_prev(a, pos)) {}

    /// @brief 区間 [l, r) の異なる値の個数を返す / O(log^2 n)
    int count(const int l, const int r) const {
        assert(0 <= l && l <= r && r <= len());
        // 区間内で wm[i] <= l となる個数が答え
        return wm.range_freq(l, r, l + 1);
    }

    /// @brief count(l, r) と同じ / O(log^2 n)
    int range_distinct(const int l, const int r) const { return count(l, r); }

    /// @brief a[k] を返す / O(1)
    const T &access(const int k) const {
        assert(0 <= k && k < len());
        return a[k];
    }

    /// @brief a[k] を v に変更する / O(log^2 n)
    void set(const int k, const T &v) {
        assert(0 <= k && k < len());
        const auto less = pos.key_comp();
        if (!less(a[k], v) && !less(v, a[k])) {
            a[k] = v;
            return;
        }

        auto old = pos.find(a[k]);
        auto it = old->second.find(k);
        auto nxt = next(it);
        int prev = it == old->second.begin() ? 0 : *std::prev(it) + 1;
        const int old_prev = prev;
        if (nxt != old->second.end()) wm.set(*nxt, prev);
        old->second.erase(it);
        if (old->second.empty()) pos.erase(old);

        auto &s = pos[v];
        nxt = s.lower_bound(k);
        prev = nxt == s.begin() ? 0 : *std::prev(nxt) + 1;
        if (prev != old_prev) wm.set(k, prev);
        if (nxt != s.end()) wm.set(*nxt, k + 1);
        s.insert(nxt, k);
        a[k] = v;
    }

    /// @brief 要素数を返す / O(1)
    int len() const { return a.size(); }
    /// @brief 空か判定する / O(1)
    bool empty() const { return a.empty(); }

    /// @brief 列を配列にして返す / O(n)
    vector<T> tovector() const { return a; }
};

} // namespace titan23
