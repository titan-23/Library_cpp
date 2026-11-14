/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/string/edit_distance.cpp
#pragma once

#include <algorithm>
#include <climits>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>
using namespace std;

namespace titan23 {

/// @brief 編集操作の種類
enum class EditOperationType { Insert, Delete, Replace };

/// @brief s を t に変える 1 回の編集操作 / 添字は編集前の s, t の位置
/// Insert は s[source_index] の直前に t[target_index] を挿入する / Delete は s[source_index] を削除する / Replace は s[source_index] を t[target_index] に置換する
struct EditOperation {
    EditOperationType type;
    int source_index;
    int target_index;
};

namespace edit_distance_detail {

// 文字列は string_view にし、ほかの列は参照のまま返す。C 文字列は終端 NUL の手前までになる
template <class Seq>
decltype(auto) as_sequence(const Seq &s) {
    if constexpr (is_convertible_v<const Seq &, string_view>) {
        return string_view(s);
    } else {
        return (s);
    }
}

template <class Seq>
int length(const Seq &s) {
    if (s.size() >= INT_MAX) {
        throw length_error("edit_distance: length must be less than INT_MAX");
    }
    return (int)s.size();
}

// t を短い列にして、1 行分の DP を持つ
template <class S, class T>
int distance(const S &s, const T &t) {
    const int n = edit_distance_detail::length(s), m = edit_distance_detail::length(t);
    if (m == 0) return n;
    vector<int> dp(m + 1);
    for (int j = 0; j <= m; ++j) dp[j] = j;
    for (int i = 1; i <= n; ++i) {
        int diag = dp[0];
        dp[0] = i;
        for (int j = 1; j <= m; ++j) {
            const int up = dp[j];
            dp[j] = min({up + 1, dp[j - 1] + 1, diag + !(s[i - 1] == t[j - 1])});
            diag = up;
        }
    }
    return dp[m];
}

// s を短い列にして、幅 2 * lim + 1 以下の帯を走査する
template <class S, class T>
int bounded(const S &s, const T &t, int lim) {
    const int n = edit_distance_detail::length(s), m = edit_distance_detail::length(t);
    if (m - n > lim) return -1;
    if (n == 0) return m;
    // 距離は必ず lim 以下だが、その正確な値は計算する必要がある
    if (lim >= m) return edit_distance_detail::distance(t, s);
    if (lim == 0) {
        for (int i = 0; i < n; ++i) {
            if (!(s[i] == t[i])) return -1;
        }
        return 0;
    }

    const int inf = lim + 1;
    const int w = (int)min(m + 1LL, 2LL * lim + 1);
    vector<int> prev(w, inf), now(w, inf);
    int l0 = 0, r0 = lim;
    for (int j = 0; j <= r0; ++j) prev[j] = j;

    for (int i = 1; i <= n; ++i) {
        // 距離 k 以下の編集経路は |i-j| <= k の帯から出ない
        const int l = i > lim ? i - lim : 0;
        const int r = i + min(lim, m - i);
        int mn = inf;
        for (int j = l; j <= r; ++j) {
            int v = inf;
            if (j == 0) {
                v = i;
            } else {
                // (i-1, j-1) は必ず直前の行の帯に含まれる
                v = min(inf, prev[j - 1 - l0] + !(s[i - 1] == t[j - 1]));
                if (j <= r0) v = min(v, prev[j - l0] + 1);
                if (j > l) v = min(v, now[j - 1 - l] + 1);
            }
            now[j - l] = v;
            mn = min(mn, v);
        }
        if (mn > lim) return -1;
        prev.swap(now);
        l0 = l;
        r0 = r;
    }
    const int res = prev[m - l0];
    return res <= lim ? res : -1;
}

} // namespace edit_distance_detail

/// @brief 挿入・削除・置換を各コスト 1 とした s と t の編集距離を返す / O(nm+n+m), n = |s|, m = |t|
/// s, t は文字列か、size() と添字アクセスを持つ列で、要素を == で比べる / 長さが INT_MAX 以上なら length_error を送出する
template <class S, class T>
int edit_distance(const S &s, const T &t) {
    const auto &a = edit_distance_detail::as_sequence(s);
    const auto &b = edit_distance_detail::as_sequence(t);
    const int n = edit_distance_detail::length(a), m = edit_distance_detail::length(b);
    if (n < m) return edit_distance_detail::distance(b, a);
    return edit_distance_detail::distance(a, b);
}

/// @brief 編集距離が k 以下ならその値を返し、超えるか k < 0 なら -1 を返す / O((n+1) * min(m+1, 2k+1))
/// n, m は短い方と長い方の列の長さ / C 文字列では長さの取得に O(n+m) が加わる / 入力の条件は edit_distance と同じ
template <class S, class T>
int edit_distance_bounded(const S &s, const T &t, int k) {
    if (k < 0) return -1;
    const auto &a = edit_distance_detail::as_sequence(s);
    const auto &b = edit_distance_detail::as_sequence(t);
    const int n = edit_distance_detail::length(a), m = edit_distance_detail::length(b);
    const int lim = k;
    if (n > m) return edit_distance_detail::bounded(b, a, lim);
    return edit_distance_detail::bounded(a, b, lim);
}

/// @brief 編集距離が k 以下かを返す / k < 0 なら false / 入力の条件と計算量は edit_distance_bounded と同じ
template <class S, class T>
bool edit_distance_within(const S &s, const T &t, int k) {
    if (k < 0) return false;
    const auto &a = edit_distance_detail::as_sequence(s);
    const auto &b = edit_distance_detail::as_sequence(t);
    const int n = edit_distance_detail::length(a), m = edit_distance_detail::length(b);
    if (k >= max(n, m)) return true;
    return titan23::edit_distance_bounded(a, b, k) >= 0;
}

/// @brief s を t に変える最小個数の編集操作を先頭から順に返す / O((n+1)(m+1)) 時間・空間
/// n = |s|, m = |t| / 返す操作の個数は編集距離に等しい / 入力の条件は edit_distance と同じ
template <class S, class T>
vector<EditOperation> edit_distance_restore(const S &s, const T &t) {
    const auto &a = edit_distance_detail::as_sequence(s);
    const auto &b = edit_distance_detail::as_sequence(t);
    const int n = edit_distance_detail::length(a), m = edit_distance_detail::length(b);
    vector<EditOperation> res;
    if (n == 0) {
        res.reserve(m);
        for (int j = 0; j < m; ++j) {
            res.push_back({EditOperationType::Insert, 0, j});
        }
        return res;
    }
    if (m == 0) {
        res.reserve(n);
        for (int i = 0; i < n; ++i) {
            res.push_back({EditOperationType::Delete, i, 0});
        }
        return res;
    }
    vector<vector<int>> dp(n + 1, vector<int>(m + 1));
    for (int i = 0; i <= n; ++i) dp[i][0] = i;
    for (int j = 0; j <= m; ++j) dp[0][j] = j;
    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= m; ++j) {
            dp[i][j] = min({dp[i - 1][j] + 1, dp[i][j - 1] + 1,
                            dp[i - 1][j - 1] + !(a[i - 1] == b[j - 1])});
        }
    }
    res.reserve(dp[n][m]);
    int i = n, j = m;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && a[i - 1] == b[j - 1]
            && dp[i][j] == dp[i - 1][j - 1]) {
            --i;
            --j;
        } else if (i > 0 && j > 0 && dp[i][j] == dp[i - 1][j - 1] + 1) {
            res.push_back({EditOperationType::Replace, i - 1, j - 1});
            --i;
            --j;
        } else if (i > 0 && dp[i][j] == dp[i - 1][j] + 1) {
            res.push_back({EditOperationType::Delete, i - 1, j});
            --i;
        } else {
            res.push_back({EditOperationType::Insert, i, j - 1});
            --j;
        }
    }
    reverse(res.begin(), res.end());
    return res;
}

/// @brief 編集距離が k 以下なら最小個数の編集操作を返し、超えるか k < 0 なら nullopt を返す / O((n+1) * min(m+1, 2k+1)) 時間・空間
/// n = |s|, m = |t| / C 文字列では長さの取得に O(n+m) が加わる / 入力の条件と返す操作の形は edit_distance_restore と同じ
template <class S, class T>
optional<vector<EditOperation>> edit_distance_restore_bounded(const S &s, const T &t, int k) {
    if (k < 0) return nullopt;
    const auto &a = edit_distance_detail::as_sequence(s);
    const auto &b = edit_distance_detail::as_sequence(t);
    const int n = edit_distance_detail::length(a), m = edit_distance_detail::length(b);
    const int lim = k;
    if ((n > m ? n - m : m - n) > lim) return nullopt;
    if (lim >= max(n, m)) return titan23::edit_distance_restore(a, b);
    if (lim == 0) {
        for (int i = 0; i < n; ++i) {
            if (!(a[i] == b[i])) return nullopt;
        }
        return vector<EditOperation>();
    }

    const int inf = lim + 1;
    // 各行の |i-j| <= k の部分だけ保存する。添字の向きを保つため列は交換しない
    vector<vector<int>> dp(n + 1);
    const auto get = [&](int i, int j) {
        const int l = i > lim ? i - lim : 0;
        if (j < l || j - l >= (int)dp[i].size()) return inf;
        return dp[i][j - l];
    };
    for (int i = 0; i <= n; ++i) {
        const int l = i > lim ? i - lim : 0;
        const int r = i >= m ? m : i + min(lim, m - i);
        dp[i].resize(r - l + 1, inf);
        int mn = inf;
        for (int j = l; j <= r; ++j) {
            int v;
            if (i == 0) {
                v = j;
            } else if (j == 0) {
                v = i;
            } else {
                v = min({inf, get(i - 1, j - 1) + !(a[i - 1] == b[j - 1]),
                         get(i - 1, j) + 1, get(i, j - 1) + 1});
            }
            dp[i][j - l] = v;
            mn = min(mn, v);
        }
        if (mn > lim) return nullopt;
    }
    if (get(n, m) > lim) return nullopt;

    vector<EditOperation> res;
    res.reserve(get(n, m));
    int i = n, j = m;
    while (i > 0 || j > 0) {
        const int v = get(i, j);
        if (i > 0 && j > 0 && a[i - 1] == b[j - 1] && v == get(i - 1, j - 1)) {
            --i;
            --j;
        } else if (i > 0 && j > 0 && v == get(i - 1, j - 1) + 1) {
            res.push_back({EditOperationType::Replace, i - 1, j - 1});
            --i;
            --j;
        } else if (i > 0 && v == get(i - 1, j) + 1) {
            res.push_back({EditOperationType::Delete, i - 1, j});
            --i;
        } else {
            res.push_back({EditOperationType::Insert, i, j - 1});
            --j;
        }
    }
    reverse(res.begin(), res.end());
    return res;
}

} // namespace titan23
