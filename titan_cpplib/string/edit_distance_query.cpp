/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/string/edit_distance_query.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>
#include "titan_cpplib/string/edit_distance.cpp"
using namespace std;

namespace titan23 {
namespace edit_distance_query_detail {

class LCE {
    int n = 0;
    vector<int> rank;
    vector<int> lg;
    vector<vector<int>> st;

public:
    LCE() = default;

    explicit LCE(const vector<int> &seq) { build(seq); }

    void build(const vector<int> &seq) {
        assert(!seq.empty());
        assert(seq.size() <= INT_MAX);
        n = (int)seq.size();
        for (int symbol : seq) {
            assert(0 <= symbol && symbol <= n);
            (void)symbol;
        }

        vector<int> sa(n), tmp(n);
        vector<int> nrank(n);
        vector<int> cnt(n + 1LL, 0);
        rank.resize(n);

        // 各要素をソートし、使われている値を連続した順位へ圧縮する
        for (int symbol : seq) ++cnt[symbol];
        int sum = 0;
        for (int &x : cnt) {
            const int freq = x;
            x = sum;
            sum += freq;
        }
        for (int i = 0; i < n; ++i) sa[cnt[seq[i]]++] = i;
        int cls = 1;
        rank[sa[0]] = 0;
        for (int i = 1; i < n; ++i) {
            if (seq[sa[i - 1]] != seq[sa[i]]) ++cls;
            rank[sa[i]] = cls - 1;
        }

        for (int len = 1; cls < n;) {
            // 末尾の順位を 0 とし、実際の順位は 1 だけずらす
            const auto key = [&](int idx, bool second) {
                if (!second) return rank[idx] + 1;
                return idx < n - len ? rank[idx + len] + 1 : 0;
            };
            const auto counting_sort = [&](const vector<int> &src, vector<int> &dst, bool second) {
                fill(cnt.begin(), cnt.end(), 0);
                for (int idx : src) ++cnt[key(idx, second)];
                int pos = 0;
                for (int k = 0; k <= cls; ++k) {
                    const int freq = cnt[k];
                    cnt[k] = pos;
                    pos += freq;
                }
                for (int idx : src) dst[cnt[key(idx, second)]++] = idx;
            };
            counting_sort(sa, tmp, true);
            counting_sort(tmp, sa, false);

            int ncls = 1;
            nrank[sa[0]] = 0;
            for (int i = 1; i < n; ++i) {
                const int a = sa[i - 1], b = sa[i];
                if (rank[a] != rank[b] || key(a, true) != key(b, true)) {
                    ++ncls;
                }
                nrank[b] = ncls - 1;
            }
            rank.swap(nrank);
            cls = ncls;
            // 2 * len 要素まで比較済み
            // 次の倍増によるオーバーフローを避ける
            if (len >= n - len) break;
            len *= 2;
        }

        // lcp[r] は sa[r - 1] と sa[r] の LCP
        // lcp[0] は使わない
        vector<int> lcp(n, 0);
        int h = 0;
        for (int i = 0; i < n; ++i) {
            const int r = rank[i];
            if (r == 0) {
                h = 0;
                continue;
            }
            const int j = sa[r - 1];
            while (h < n - i && h < n - j
                   && seq[i + h] == seq[j + h]) {
                ++h;
            }
            lcp[r] = h;
            if (h > 0) --h;
        }

        lg.assign(n + 1LL, 0);
        for (int i = 1; i < n; ++i) lg[i + 1] = lg[(i + 1) / 2] + 1;
        st.clear();
        st.resize(lg[n] + 1);
        st[0].swap(lcp);
        for (int k = 1; k <= lg[n]; ++k) {
            const int half = 1 << (k - 1);
            const int len = n - 2 * half + 1;
            st[k].resize(len);
            for (int i = 0; i < len; ++i) {
                st[k][i] = min(st[k - 1][i], st[k - 1][i + half]);
            }
        }
    }

    int lce(int i, int j) const {
        assert(0 <= i && i < n && 0 <= j && j < n);
        if (i == j) return n - i;
        int l = rank[i], r = rank[j];
        if (l > r) swap(l, r);
        const int k = lg[r - l];
        const int len = 1 << k;
        return min(st[k][l + 1], st[k][r - len + 1]);
    }
};

} // namespace edit_distance_query_detail

/// @brief 固定した 2 列の任意の半開区間同士の編集距離クエリ
/// @details 挿入・削除・置換は各コスト 1
/// 入力列の更新には再構築が必要
/// N = |s|+|t|+1 として O(N log N) 時間・空間で前処理する
/// 距離 d のクエリは O((d+1)^2) 時間、上限 k つきなら O((k+1)^2) 時間
/// 決定的な LCE を使うため、ハッシュの衝突はない
class EditDistanceQuery {
private:
    int len_s = 0, len_t = 0;
    edit_distance_query_detail::LCE lc;

    void check_ranges([[maybe_unused]] int l1, [[maybe_unused]] int r1,
                      [[maybe_unused]] int l2, [[maybe_unused]] int r2) const {
        assert(0 <= l1 && l1 <= r1 && r1 <= len_s);
        assert(0 <= l2 && l2 <= r2 && r2 <= len_t);
    }

    int bounded_impl(int l1, int r1, int l2, int r2, int k) const {
        if (k < 0) return -1;
        const int n = r1 - l1, m = r2 - l2;
        const int gap = n > m ? n - m : m - n;
        if (gap > k) return -1;
        if (n == 0 || m == 0) return n + m;
        const int lim = min(k, max(n, m));
        const int offset = len_s + 1 + l2;
        // LCE が指定された区間の外まで続いても、区間端で打ち切る
        const auto extend = [&](int x, int d) {
            const int y = x - d;
            if (x == n || y == m) return x;
            return x + min({lc.lce(l1 + x, offset + y), n - x, m - y});
        };

        const int init = extend(0, 0);
        if (n == m && init == n) return 0;
        if (lim == 0) return -1;
        // prev[d] は、編集回数を固定したときに対角線 d = x-y 上で
        // 一致部分を進み終えた最も遠い x
        // 添字は prev_l だけずらす
        vector<int> prev(1, init), now;
        int prev_l = 0, prev_r = 0;
        const int end_d = n - m;
        for (int cost = 1; cost <= lim; ++cost) {
            // 残りの編集回数で終点の対角線へ戻れない状態も除く
            const int64_t rem = lim - cost;
            const int lo = (int)max({
                -int64_t(cost), -int64_t(m),
                int64_t(end_d) - rem});
            const int hi = (int)min({
                int64_t(cost), int64_t(n),
                int64_t(end_d) + rem});
            if (lo > hi) return -1;
            now.assign(hi - lo + 1, -1);
            const auto get = [&](int d) {
                return prev_l <= d && d <= prev_r
                           ? prev[d - prev_l] : -1;
            };
            for (int d = lo; d <= hi; ++d) {
                int x = -1;
                const int rep = get(d);
                const int del = get(d - 1);
                const int ins = get(d + 1);
                if (rep >= 0) x = rep + 1;
                if (del >= 0) x = max(x, del + 1);
                if (ins >= 0) x = max(x, ins);
                // 端を越える遷移は同じ対角線上の端へ切り詰める
                // 「高々 cost 回」で到達可能な最遠点を保つ
                x = min({x, n, m + d});
                if (x < max(0, d)) continue;
                x = extend(x, d);
                now[d - lo] = x;
                if (d == end_d && x == n) return cost;
            }
            prev.swap(now);
            prev_l = lo;
            prev_r = hi;
        }
        return -1;
    }

public:
    /// @brief 2 列を前処理する
    /// 構築後は元の列を変更・破棄してよい
    /// @details 文字列、または const な size()/添字アクセスを持つ列を受け取る
    /// 要素の < は等値比較と整合する strict weak ordering であること
    /// |s|+|t|+1 <= INT_MAX が必要
    /// 超える場合は length_error を送出する
    /// 要素の比較・添字アクセスが O(1) なら O(N log N) 時間・空間
    template <class S, class T>
    EditDistanceQuery(const S &s, const T &t) {
        const auto &a = edit_distance_detail::as_sequence(s);
        const auto &b = edit_distance_detail::as_sequence(t);
        const int n = edit_distance_detail::length(a), m = edit_distance_detail::length(b);
        if (n > INT_MAX - 1 - m) {
            throw length_error("EditDistanceQuery: total length exceeds INT_MAX - 1");
        }
        len_s = n;
        len_t = m;
        const int total = len_s + len_t;
        vector<int> ord(total);
        iota(ord.begin(), ord.end(), 0);
        const auto cmp = [&](int i, int j) {
            if (i < len_s) {
                if (j < len_s) return a[i] < a[j];
                return a[i] < b[j - len_s];
            }
            if (j < len_s) return b[i - len_s] < a[j];
            return b[i - len_s] < b[j - len_s];
        };
        sort(ord.begin(), ord.end(), cmp);
        // 0 を区切り専用にし、入力の値は 1 以上へ座標圧縮する
        vector<int> seq(total + 1, 0);
        int rank = 0;
        for (int p = 0; p < total; ++p) {
            if (p == 0 || cmp(ord[p - 1], ord[p])) ++rank;
            const int i = ord[p];
            seq[i + (i >= len_s)] = rank;
        }
        lc.build(seq);
    }

    int source_size() const { return len_s; }
    int target_size() const { return len_t; }

    /// @brief s[l1:r1) と t[l2:r2) の正確な編集距離を返す
    /// @details 距離 d に対し O((d+1)^2) 時間、O(d+1) 作業空間
    int distance(int l1, int r1, int l2, int r2) const {
        check_ranges(l1, r1, l2, r2);
        return bounded_impl(l1, r1, l2, r2, max(r1 - l1, r2 - l2));
    }

    /// @brief 区間の編集距離が k 以下なら正確な距離、超えるなら -1 を返す
    /// @details k < 0 なら -1
    /// O((k+1)^2) 時間、O(k+1) 作業空間 (k >= 0)
    int distance_bounded(int l1, int r1, int l2, int r2, int k) const {
        check_ranges(l1, r1, l2, r2);
        return bounded_impl(l1, r1, l2, r2, k);
    }

    /// @brief 区間の編集距離が k 以下かを判定する
    /// k < 0 なら false
    /// @details O((k+1)^2) 時間、O(k+1) 作業空間 (k >= 0)
    bool within(int l1, int r1, int l2, int r2, int k) const {
        check_ranges(l1, r1, l2, r2);
        if (k < 0) return false;
        if (k >= max(r1 - l1, r2 - l2)) return true;
        return bounded_impl(l1, r1, l2, r2, k) >= 0;
    }
};

} // namespace titan23
