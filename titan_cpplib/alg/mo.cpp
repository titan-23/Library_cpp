/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/alg/mo.cpp
#pragma once

#include <vector>
#include <cassert>
#include <utility>
#include <numeric>
#include <algorithm>
using namespace std;

// Mo
namespace titan23 {

/**
 * @brief Mo's algorithm
 *
 * @ref https://take44444.github.io/Algorithm-Book/range/mo/main.html
 */
class Mo {
private:
    int n, q;
    long long max_n;
    vector<int> _l, _r;

    long long hilbertorder(int x, int y) const {
        long long rx, ry, d = 0;
        for (long long s = max_n>>1ll; s; s >>= 1ll) {
            rx = (x & s) > 0, ry = (y & s) > 0;
            d += s * s * ((rx * 3) ^ ry);
            if (ry) continue;
            if (rx) {
                x = max_n - 1 - x;
                y = max_n - 1 - y;
            }
            swap(x, y);
        }
        return d;
    }

public:
    Mo() : n(0) {}

    /// 長さ `n` の列に対する Mo's algorithm インスタンスを生成 / `O(1)`
    Mo(const int n) : n(n), q(0), max_n(1 << 25) {
        while (max_n <= n) {
            max_n <<= 1;
        }
        assert(n <= max_n);
    }

    /// クエリ `[l, r)` を追加する / `O(1)`
    void add_query(const int l, const int r) {
        assert(0 <= l && l <= r && r <= n);
        _l.emplace_back(l);
        _r.emplace_back(r);
        ++q;
    }

    /// 追加されたクエリを一括処理する / `O(n√q)`
    /// F1 ~ F3: lambda関数
    template<typename F1, typename F2, typename F3>
    void run_light(F1 &&add, F2 &&del, F3 &&out) {
        vector<int> qi(q);
        iota(qi.begin(), qi.end(), 0);
        vector<long long> eval(q);
        for (int i = 0; i < q; ++i) {
            eval[i] = hilbertorder(_l[i], _r[i]);
        }
        sort(qi.begin(), qi.end(), [&] (const int &i, const int &j) {
            return eval[i] < eval[j];
        });
        int nl = 0, nr = 0;
        for (const int i : qi) {
            const int li = _l[i], ri = _r[i];
            while (nl > li) add(--nl);
            while (nr < ri) add(nr++);
            while (nl < li) del(nl++);
            while (nr > ri) del(--nr);
            out(i);
        }
    }

    /// 追加されたクエリを一括処理する / `O(n√q)`
    /// F1 ~ F5: lambda関数
    template<typename F1, typename F2, typename F3, typename F4, typename F5>
    void run(F1 &&add_left, F2 &&add_right, F3 &&del_left, F4 &&del_right, F5 &&out) {
        vector<int> qi(q);
        iota(qi.begin(), qi.end(), 0);
        vector<long long> eval(q);
        for (int i = 0; i < q; ++i) {
            eval[i] = hilbertorder(_l[i], _r[i]);
        }
        sort(qi.begin(), qi.end(), [&] (const int &i, const int &j) {
            return eval[i] < eval[j];
        });
        int nl = 0, nr = 0;
        for (const int i : qi) {
            const int li = _l[i], ri = _r[i];
            while (nl > li) add_left(--nl);
            while (nr < ri) add_right(nr++);
            while (nl < li) del_left(nl++);
            while (nr > ri) del_right(--nr);
            out(i);
        }
    }

    /// 追加されたクエリを一括処理する / `O(n√q)`
    /// 現在見ている区間 `[nl, nr)` を各クエリ `[l, r)` に合わせて伸縮させる
    ///
    /// - `add_left(i, r)`  : `i` を左端に追加し、区間は `[i, r)` になる
    /// - `add_right(l, i)` : `i` を右端に追加し、区間は `[l, i+1)` になる
    /// - `del_left(i, r)`  : `i` を左端から削除し、区間は `[i+1, r)` になる
    /// - `del_right(l, i)` : `i` を右端から削除し、区間は `[l, i)` になる
    /// F1 ~ F5: lambda関数
    template<typename F1, typename F2, typename F3, typename F4, typename F5>
    void run_range(F1 &&add_left, F2 &&add_right, F3 &&del_left, F4 &&del_right, F5 &&out) {
        vector<int> qi(q);
        iota(qi.begin(), qi.end(), 0);
        vector<long long> eval(q);
        for (int i = 0; i < q; ++i) {
            eval[i] = hilbertorder(_l[i], _r[i]);
        }
        sort(qi.begin(), qi.end(), [&] (const int &i, const int &j) {
            return eval[i] < eval[j];
        });
        int nl = 0, nr = 0;
        for (const int i : qi) {
            const int li = _l[i], ri = _r[i];
            while (nl > li) add_left(--nl, nr);   // 左を1広げ nl を追加
            while (nr < ri) add_right(nl, nr++);  // 右を1広げ nr を追加
            while (nl < li) del_left(nl++, nr);   // 左を1縮め nl を削除
            while (nr > ri) del_right(nl, --nr);  // 右を1縮め nr を削除
            out(i);
        }
    }
};
}  // namespace titan23
