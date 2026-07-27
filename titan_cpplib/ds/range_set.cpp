#pragma once

#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <limits>
#include <cassert>
using namespace std;

// RangeSet
namespace titan23 {

/**
 * @brief 整数集合を互いに素な半開区間の集合として管理するデータ構造
 *
 * 値は `neg_inf < x < pos_inf` を満たすものだけを扱う
 * 探索が失敗したときは `neg_inf` / `pos_inf` を返す
 */
template<typename T>
class RangeSet {
private:
    map<T, T> data;
    long long sum_len;

    /// `x` を含む区間を指すイテレータを返す 含まないとき `data.end()`
    auto find_it(const T &x) const {
        auto it = data.upper_bound(x);
        if (it == data.begin()) return data.end();
        --it;
        if (x < it->second) return it;
        return data.end();
    }

    /// 昇順に並んだ区間列から構築する 重なりと隣接は併合する / `O(n)`
    void build(const vector<pair<T, T>> &ranges) {
        data.clear();
        sum_len = 0;
        for (const auto &[l, r] : ranges) {
            assert(neg_inf < l && l <= r && r <= pos_inf);
            if (l == r) continue;
            if (!data.empty()) {
                auto it = data.end();
                --it;
                if (l <= it->second) {
                    if (it->second < r) {
                        sum_len += r - it->second;
                        it->second = r;
                    }
                    continue;
                }
            }
            data.emplace_hint(data.end(), l, r);
            sum_len += r - l;
        }
    }

public:
    T neg_inf, pos_inf;

    /// 空の RangeSet を構築する / `O(1)`
    RangeSet() : sum_len(0), neg_inf(numeric_limits<T>::min()), pos_inf(numeric_limits<T>::max()) {}

    /**
     * @brief 空の RangeSet を構築する / `O(1)`
     *
     * @param neg_inf 扱う値より小さい番兵
     * @param pos_inf 扱う値より大きい番兵
     */
    RangeSet(T neg_inf, T pos_inf) : sum_len(0), neg_inf(neg_inf), pos_inf(pos_inf) {
        assert(neg_inf < pos_inf);
    }

    /// 区間列から構築する / `O(nlogn)`
    RangeSet(const vector<pair<T, T>> &ranges,
             T neg_inf = numeric_limits<T>::min(),
             T pos_inf = numeric_limits<T>::max()) : sum_len(0), neg_inf(neg_inf), pos_inf(pos_inf) {
        assert(neg_inf < pos_inf);
        vector<pair<T, T>> a = ranges;
        sort(a.begin(), a.end());
        build(a);
    }

    /// 点の列から構築する / `O(nlogn)`
    RangeSet(const vector<T> &a,
             T neg_inf = numeric_limits<T>::min(),
             T pos_inf = numeric_limits<T>::max()) : sum_len(0), neg_inf(neg_inf), pos_inf(pos_inf) {
        assert(neg_inf < pos_inf);
        vector<pair<T, T>> b;
        b.reserve(a.size());
        for (const T &x : a) {
            b.emplace_back(x, x + 1);
        }
        sort(b.begin(), b.end());
        build(b);
    }

    /// `[l, r)` を追加し、増えた要素数を返す / 償却 `O(logn)`
    long long add(T l, T r) {
        assert(neg_inf < l && l <= r && r <= pos_inf);
        if (l == r) return 0;
        long long before = sum_len;
        auto it = data.upper_bound(l);
        if (it != data.begin()) {
            --it;
            if (it->second >= l) {
                l = it->first;
                if (r < it->second) r = it->second;
                sum_len -= it->second - it->first;
                it = data.erase(it);
            } else {
                ++it;
            }
        }
        while (it != data.end() && it->first <= r) {
            if (r < it->second) r = it->second;
            sum_len -= it->second - it->first;
            it = data.erase(it);
        }
        data.emplace_hint(it, l, r);
        sum_len += r - l;
        return sum_len - before;
    }

    /// `x` を追加する 実際に増えたなら `true` / 償却 `O(logn)`
    bool add(T x) {
        return add(x, x + 1) > 0;
    }

    /// `[l, r)` を削除し、減った要素数を返す / 償却 `O(logn)`
    long long remove(T l, T r) {
        assert(neg_inf < l && l <= r && r <= pos_inf);
        if (l == r) return 0;
        long long before = sum_len;
        auto it = data.upper_bound(l);
        if (it != data.begin()) {
            --it;
            if (it->second > l) {
                T nl = it->first, nr = it->second;
                sum_len -= nr - nl;
                it = data.erase(it);
                if (nl < l) {
                    it = data.emplace_hint(it, nl, l);
                    sum_len += l - nl;
                    ++it;
                }
                if (nr > r) {
                    data.emplace_hint(it, r, nr);
                    sum_len += nr - r;
                    return before - sum_len;
                }
            } else {
                ++it;
            }
        }
        while (it != data.end() && it->first < r) {
            T nr = it->second;
            sum_len -= nr - it->first;
            it = data.erase(it);
            if (nr > r) {
                data.emplace_hint(it, r, nr);
                sum_len += nr - r;
                break;
            }
        }
        return before - sum_len;
    }

    /// `x` を削除する 実際に減ったなら `true` / 償却 `O(logn)`
    bool discard(T x) {
        return remove(x, x + 1) > 0;
    }

    /// 含まれている `x` を削除する / 償却 `O(logn)`
    void remove(T x) {
        assert(contains(x));
        discard(x);
    }

    /// 空にする / `O(n)`
    void clear() {
        data.clear();
        sum_len = 0;
    }

    /// 扱える範囲全体を追加する / `O(n)`
    void fill() {
        data.clear();
        data.emplace(neg_inf + 1, pos_inf);
        sum_len = pos_inf - (neg_inf + 1);
    }

    /// `x` の含む含まないを反転する 反転後に含むなら `true` / 償却 `O(logn)`
    bool toggle(T x) {
        assert(neg_inf < x && x < pos_inf);
        if (contains(x)) {
            remove(x, x + 1);
            return false;
        }
        add(x, x + 1);
        return true;
    }

    /// `[l, r)` の含む含まないを反転し、要素数の増分を返す / `O((k+1)logn)`
    long long flip(T l, T r) {
        assert(neg_inf < l && l <= r && r <= pos_inf);
        if (l == r) return 0;
        long long before = sum_len;
        vector<pair<T, T>> gaps;
        for_each_gap(l, r, [&] (T a, T b) { gaps.emplace_back(a, b); });
        remove(l, r);
        for (const auto &[a, b] : gaps) {
            add(a, b);
        }
        return sum_len - before;
    }

    /// `x` を含むか判定する / `O(logn)`
    bool contains(const T &x) const {
        return find_it(x) != data.end();
    }

    /// `[l, r)` をすべて含むか判定する / `O(logn)`
    bool contains(const T &l, const T &r) const {
        assert(l <= r);
        if (l == r) return true;
        auto it = find_it(l);
        return it != data.end() && r <= it->second;
    }

    /// `x` と `y` が同じ区間に属するか判定する / `O(logn)`
    bool same(const T &x, const T &y) const {
        assert(neg_inf < x && x < pos_inf);
        assert(neg_inf < y && y < pos_inf);
        if (y < x) return same(y, x);
        return contains(x, y + 1);
    }

    /// `[l, r)` と交差する区間があるか判定する / `O(logn)`
    bool intersects(const T &l, const T &r) const {
        assert(l <= r);
        if (l == r) return false;
        auto it = data.upper_bound(l);
        if (it != data.begin()) {
            --it;
            if (it->second > l) return true;
            ++it;
        }
        return it != data.end() && it->first < r;
    }

    /// `other` と共通要素があるか判定する / `O(n+m)`
    bool intersects(const RangeSet &other) const {
        auto it = data.begin();
        auto jt = other.data.begin();
        while (it != data.end() && jt != other.data.end()) {
            if (max(it->first, jt->first) < min(it->second, jt->second)) return true;
            if (it->second < jt->second) ++it;
            else ++jt;
        }
        return false;
    }

    /// 自身が `other` に含まれるか判定する / `O(nlogm)`
    bool is_subset_of(const RangeSet &other) const {
        for (const auto &[l, r] : data) {
            if (!other.contains(l, r)) return false;
        }
        return true;
    }

    /// `other` を含むか判定する / `O(mlogn)`
    bool is_superset_of(const RangeSet &other) const {
        return other.is_subset_of(*this);
    }

    /// `x` を含む区間を返す 含まないとき `{neg_inf, neg_inf}` / `O(logn)`
    pair<T, T> get_range(const T &x) const {
        auto it = find_it(x);
        if (it == data.end()) return {neg_inf, neg_inf};
        return {it->first, it->second};
    }

    /// `x` を含む区間、なければ右で最初の区間を返す 無いとき `{pos_inf, pos_inf}` / `O(logn)`
    pair<T, T> next_range(const T &x) const {
        auto it = find_it(x);
        if (it != data.end()) return {it->first, it->second};
        it = data.upper_bound(x);
        if (it == data.end()) return {pos_inf, pos_inf};
        return {it->first, it->second};
    }

    /// `x` を含む区間、なければ左で最後の区間を返す 無いとき `{neg_inf, neg_inf}` / `O(logn)`
    pair<T, T> prev_range(const T &x) const {
        auto it = data.upper_bound(x);
        if (it == data.begin()) return {neg_inf, neg_inf};
        --it;
        return {it->first, it->second};
    }

    /// `x` を含む極大な空き区間を返す 含まれているとき `{neg_inf, neg_inf}` / `O(logn)`
    pair<T, T> get_gap(const T &x) const {
        assert(neg_inf < x && x < pos_inf);
        T a = neg_inf + 1, b = pos_inf;
        auto it = data.upper_bound(x);
        if (it != data.end()) b = it->first;
        if (it != data.begin()) {
            --it;
            if (it->second > x) return {neg_inf, neg_inf};
            a = it->second;
        }
        return {a, b};
    }

    /// `x` を含む空き区間、なければ右で最初の空き区間を返す 無いとき `{pos_inf, pos_inf}` / `O(logn)`
    pair<T, T> next_gap(const T &x) const {
        assert(neg_inf < x && x < pos_inf);
        T a = neg_inf + 1, b = pos_inf;
        auto it = data.upper_bound(x);
        if (it != data.end()) b = it->first;
        if (it != data.begin()) {
            --it;
            a = it->second;
        }
        if (a == b) return {pos_inf, pos_inf};
        return {a, b};
    }

    /// `x` を含む空き区間、なければ左で最後の空き区間を返す 無いとき `{neg_inf, neg_inf}` / `O(logn)`
    pair<T, T> prev_gap(const T &x) const {
        assert(neg_inf < x && x < pos_inf);
        T a = neg_inf + 1, b = pos_inf;
        auto it = data.upper_bound(x);
        if (it != data.end()) b = it->first;
        if (it != data.begin()) {
            --it;
            if (it->second > x) {
                b = it->first;
                if (it != data.begin()) {
                    --it;
                    a = it->second;
                }
            } else {
                a = it->second;
            }
        }
        if (a == b) return {neg_inf, neg_inf};
        return {a, b};
    }

    /// 空き区間の個数を返す / `O(1)`
    int gap_count() const {
        if (data.empty()) return neg_inf + 1 < pos_inf ? 1 : 0;
        int cnt = data.size() + 1;
        if (data.begin()->first == neg_inf + 1) --cnt;
        if (data.rbegin()->second == pos_inf) --cnt;
        return cnt;
    }

    /// `x` 以上で含まれない最小の値を返す / `O(logn)`
    T mex(const T &x) const {
        assert(neg_inf < x && x < pos_inf);
        auto it = find_it(x);
        if (it == data.end()) return x;
        return it->second;
    }

    /// `x` 以下で含まれない最大の値を返す / `O(logn)`
    T rmex(const T &x) const {
        assert(neg_inf < x && x < pos_inf);
        auto it = find_it(x);
        if (it == data.end()) return x;
        return it->first - 1;
    }

    /// `x` 以上で最小の要素を返す / `O(logn)`
    T ge(const T &x) const {
        auto it = find_it(x);
        if (it != data.end()) return x;
        it = data.upper_bound(x);
        if (it == data.end()) return pos_inf;
        return it->first;
    }

    /// `x` より大きくて最小の要素を返す / `O(logn)`
    T gt(const T &x) const {
        assert(x < pos_inf);
        return ge(x + 1);
    }

    /// `x` 以下で最大の要素を返す / `O(logn)`
    T le(const T &x) const {
        auto it = data.upper_bound(x);
        if (it == data.begin()) return neg_inf;
        --it;
        if (it->second > x) return x;
        return it->second - 1;
    }

    /// `x` 未満で最大の要素を返す / `O(logn)`
    T lt(const T &x) const {
        assert(neg_inf < x);
        return le(x - 1);
    }

    /// 最小の要素を返す / `O(1)`
    T get_min() const {
        if (data.empty()) return pos_inf;
        return data.begin()->first;
    }

    /// 最大の要素を返す / `O(1)`
    T get_max() const {
        if (data.empty()) return neg_inf;
        return data.rbegin()->second - 1;
    }

    /// 含まれる要素数を返す / `O(1)`
    long long len() const {
        return sum_len;
    }

    /// 含まれる要素数を返す / `O(1)`
    long long size() const {
        return sum_len;
    }

    /// 保持する区間の個数を返す / `O(1)`
    int range_count() const {
        return data.size();
    }

    /// 空かどうか判定する / `O(1)`
    bool empty() const {
        return data.empty();
    }

    /// `[l, r)` に含まれる要素数を返す / `O((k+1)logn)`
    long long count_range(const T &l, const T &r) const {
        long long cnt = 0;
        for_each_range(l, r, [&] (T a, T b) { cnt += b - a; });
        return cnt;
    }

    /// `[l, r)` と交差する区間を、交差部分に切り詰めて `f(a, b)` に渡す / `O((k+1)logn)`
    template<class F> // void f(T a, T b)
    void for_each_range(const T &l, const T &r, F &&f) const {
        assert(l <= r);
        if (l == r) return;
        auto it = data.upper_bound(l);
        if (it != data.begin()) {
            --it;
            if (it->second > l) f(l, min(r, it->second));
            ++it;
        }
        while (it != data.end() && it->first < r) {
            f(it->first, min(r, it->second));
            ++it;
        }
    }

    /// `[l, r)` に含まれる空き区間を `f(a, b)` に渡す / `O((k+1)logn)`
    template<class F> // void f(T a, T b)
    void for_each_gap(const T &l, const T &r, F &&f) const {
        assert(l <= r);
        if (l == r) return;
        T pos = l;
        for_each_range(l, r, [&] (T a, T b) {
            if (pos < a) f(pos, a);
            pos = b;
        });
        if (pos < r) f(pos, r);
    }

    /// 和集合を返す / `O(n+m)`
    RangeSet operator|(const RangeSet &other) const {
        assert(neg_inf == other.neg_inf && pos_inf == other.pos_inf);
        vector<pair<T, T>> res;
        res.reserve(data.size() + other.data.size());
        auto it = data.begin();
        auto jt = other.data.begin();
        while (it != data.end() && jt != other.data.end()) {
            if (it->first <= jt->first) {
                res.emplace_back(it->first, it->second);
                ++it;
            } else {
                res.emplace_back(jt->first, jt->second);
                ++jt;
            }
        }
        for (; it != data.end(); ++it) {
            res.emplace_back(it->first, it->second);
        }
        for (; jt != other.data.end(); ++jt) {
            res.emplace_back(jt->first, jt->second);
        }
        RangeSet ret(neg_inf, pos_inf);
        ret.build(res);
        return ret;
    }

    /// 積集合を返す / `O(n+m)`
    RangeSet operator&(const RangeSet &other) const {
        assert(neg_inf == other.neg_inf && pos_inf == other.pos_inf);
        vector<pair<T, T>> res;
        auto it = data.begin();
        auto jt = other.data.begin();
        while (it != data.end() && jt != other.data.end()) {
            T l = max(it->first, jt->first);
            T r = min(it->second, jt->second);
            if (l < r) res.emplace_back(l, r);
            if (it->second < jt->second) ++it;
            else ++jt;
        }
        RangeSet ret(neg_inf, pos_inf);
        ret.build(res);
        return ret;
    }

    /// 差集合を返す / `O(n+m)`
    RangeSet operator-(const RangeSet &other) const {
        assert(neg_inf == other.neg_inf && pos_inf == other.pos_inf);
        vector<pair<T, T>> res;
        auto jt = other.data.begin();
        for (const auto &[l, r] : data) {
            T pos = l;
            while (jt != other.data.end() && jt->first < r) {
                if (jt->second <= pos) {
                    ++jt;
                    continue;
                }
                if (pos < jt->first) res.emplace_back(pos, jt->first);
                pos = jt->second;
                if (r <= pos) break;
                ++jt;
            }
            if (pos < r) res.emplace_back(pos, r);
        }
        RangeSet ret(neg_inf, pos_inf);
        ret.build(res);
        return ret;
    }

    /// 対称差を返す / `O(n+m)`
    RangeSet operator^(const RangeSet &other) const {
        assert(neg_inf == other.neg_inf && pos_inf == other.pos_inf);
        return (*this - other) | (other - *this);
    }

    /// 扱える範囲の中での補集合を返す / `O(n)`
    RangeSet complement() const {
        vector<pair<T, T>> res;
        res.reserve(data.size() + 1);
        T pos = neg_inf + 1;
        for (const auto &[l, r] : data) {
            if (pos < l) res.emplace_back(pos, l);
            pos = r;
        }
        if (pos < pos_inf) res.emplace_back(pos, pos_inf);
        RangeSet ret(neg_inf, pos_inf);
        ret.build(res);
        return ret;
    }

    RangeSet& operator|=(const RangeSet &other) {
        *this = *this | other;
        return *this;
    }

    RangeSet& operator&=(const RangeSet &other) {
        *this = *this & other;
        return *this;
    }

    RangeSet& operator-=(const RangeSet &other) {
        *this = *this - other;
        return *this;
    }

    RangeSet& operator^=(const RangeSet &other) {
        *this = *this ^ other;
        return *this;
    }

    bool operator==(const RangeSet &other) const {
        return data == other.data;
    }

    bool operator!=(const RangeSet &other) const {
        return data != other.data;
    }

    /// 区間列を昇順で返す / `O(n)`
    vector<pair<T, T>> tovector() const {
        vector<pair<T, T>> res;
        res.reserve(data.size());
        for (const auto &[l, r] : data) {
            res.emplace_back(l, r);
        }
        return res;
    }

    friend ostream& operator<<(ostream& os, const titan23::RangeSet<T> &s) {
        int n = s.range_count();
        int i = 0;
        os << "[";
        for (const auto &[l, r] : s.data) {
            os << "[" << l << ", " << r << ")";
            if (++i < n) os << ", ";
        }
        os << "]";
        return os;
    }
};
} // namespace titan23
