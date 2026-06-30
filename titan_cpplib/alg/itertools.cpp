#pragma once

#include <vector>
#include <string>
#include <utility>
#include <cassert>
#include <algorithm>
#include <numeric>
using namespace std;

namespace titan23 {

/// @brief ランレングス圧縮 / O(n)
template<typename T> vector<pair<T, int>> rle(const vector<T> &A) {
    vector<pair<T, int>> ret;
    if (A.empty()) return ret;
    T a = A[0];
    int cnt = 1;
    for (int i = 1; i < (int)A.size(); ++i) {
        if (A[i] == a) {
            ++cnt;
        } else {
            ret.emplace_back(a, cnt);
            a = A[i];
            cnt = 1;
        }
    }
    ret.emplace_back(a, cnt);
    return ret;
}

/// @brief ランレングス圧縮 / O(n)
vector<pair<char, int>> rle(const string &S) {
    vector<pair<char, int>> ret;
    if (S.empty()) return ret;
    char a = S[0];
    int cnt = 1;
    for (int i = 1; i < (int)S.size(); ++i) {
        if (S[i] == a) {
            ++cnt;
        } else {
            ret.emplace_back(a, cnt);
            a = S[i];
            cnt = 1;
        }
    }
    ret.emplace_back(a, cnt);
    return ret;
}

/// @brief 二項係数 nCr / O(min(r, n-r))
long long nCr(long long n, long long r) {
    assert(0 <= n && 0 <= r);
    if (r > n) return 0;
    if (r*2 > n) r = n - r;
    long long res = 1;
    for (long long i = 1; i <= r; ++i) {
        res = res * (n - r + i) / i;
    }
    return res;
}

/// @brief n 個から r 個を選ぶ添字組合せの全列挙 / O(r * nCr)
vector<vector<int>> combinations(int n, int r) {
    if (r < 0 || r > n) return {};
    long long m = nCr(n, r);
    vector<vector<int>> res;
    if (m < 1e7) res.reserve(m);
    vector<int> now(r);
    for (int i = 0; i < r; ++i) {
        now[i] = i;
    }
    while (1) {
        res.emplace_back(now);
        int i = r - 1;
        while (i >= 0 && now[i] == n - r + i) {
            --i;
        }
        if (i < 0) break;
        now[i]++;
        for (int j = i + 1; j < r; ++j) {
            now[j] = now[j - 1] + 1;
        }
    }
    return res;
}

/// @brief n ビットで popcount k のビット集合を昇順列挙 f(long long mask) / O(nCk)
template<class F>
void combinations_bit(int n, int k, F f) {
    assert(0 <= n && n < 63);
    assert(0 <= k);
    if (k > n) return;
    long long c = (1LL << k) - 1;
    while (c < (1LL << n)) {
        f(c);
        if (c == 0) break;
        long long x = c & -c, y = c + x;
        c = (((c & ~y) / x) >> 1) | y;
    }
}

/// @brief b の部分集合を降順列挙 f(long long sub) / O(2^popcount(b))
template<class F>
void submasks(long long b, F f) {
    assert(0 <= b);
    long long a = b;
    while (true) {
        f(a);
        if (a == 0) break;
        a = (a - 1) & b;
    }
}

/// @brief 総和が S になる正整数の非減少な組合せを全列挙する / O(S * p(S))
// p(S) は分割数 S=10:42 S=20:627 S=30:5604 S=40:37338 S=50:204226 S=60:966467 S=70:4087968
vector<vector<int>> partitions(int S) {
    assert(S >= 0);
    vector<vector<int>> ans;
    vector<int> now;
    auto dfs = [&] (auto &&dfs, int rem) -> void {
        if (rem == 0) {
            ans.emplace_back(now);
            return;
        }
        int start = now.empty() ? 1 : now.back();
        for (int i = start; i <= rem; ++i) {
            now.emplace_back(i);
            dfs(dfs, rem - i);
            now.pop_back();
        }
    };
    dfs(dfs, S);
    return ans;
}

/// @brief 2N 個を N 組のペアに分ける全列挙 / O(N * (2N-1)!!)
vector<vector<pair<int, int>>> grouping_pair(int n) {
    assert(n >= 0);
    vector<vector<pair<int, int>>> result;
    vector<pair<int, int>> tmp;
    vector<bool> is_used(2*n, false);
    auto dfs = [&] (auto &&dfs) -> void {
        if (tmp.size() == n) {
            result.push_back(tmp);
            return;
        }
        int left = -1;
        for (int i = 0; i < 2 * n; ++i) {
            if (!is_used[i]) {
                left = i;
                break;
            }
        }
        assert(left != -1);
        is_used[left] = true;
        for (int right = 0; right < 2 * n; ++right) {
            if (!is_used[right]) {
                tmp.emplace_back(left, right);
                is_used[right] = true;
                dfs(dfs);
                tmp.pop_back();
                is_used[right] = false;
            }
        }
        is_used[left] = false;
    };
    dfs(dfs);
    return result;
}

/// @brief 0..m-1 を repeat 個の直積 / O(m^repeat)
template<class F>
void product(int m, int repeat, F f) {
    assert(repeat >= 0);
    if (repeat > 0 && m <= 0) return;
    vector<int> v(repeat, 0);
    while (true) {
        f(v);
        int i = repeat - 1;
        while (i >= 0) {
            if (++v[i] < m) break;
            v[i] = 0;
            --i;
        }
        if (i < 0) break;
    }
}

/// @brief 0..m-1 を repeat 個の直積を全列挙して返す / O(m^repeat)
vector<vector<int>> product(int m, int repeat) {
    vector<vector<int>> res;
    product(m, repeat, [&] (const vector<int> &v) { res.emplace_back(v); });
    return res;
}

/// @brief 値集合 a を repeat 個の直積 / O(|a|^repeat)
template<class F>
void product(const vector<int> &a, int repeat, F f) {
    int m = a.size();
    assert(repeat >= 0);
    if (repeat > 0 && m == 0) return;
    vector<int> idx(repeat, 0), v(repeat, repeat > 0 ? a[0] : 0);
    while (true) {
        f(v);
        int i = repeat - 1;
        while (i >= 0) {
            if (++idx[i] < m) {
                v[i] = a[idx[i]];
                break;
            }
            idx[i] = 0;
            v[i] = a[0];
            --i;
        }
        if (i < 0) break;
    }
}

/// @brief 値集合 a を repeat 個の直積を全列挙して返す / O(|a|^repeat)
vector<vector<int>> product(const vector<int> &a, int repeat) {
    vector<vector<int>> res;
    product(a, repeat, [&] (const vector<int> &v) { res.emplace_back(v); });
    return res;
}

/// @brief 0..n-1 の順列を辞書順列挙 / O(n * n!)
template<class F>
void permutations(int n, F f) {
    assert(n >= 0);
    vector<int> v(n);
    iota(v.begin(), v.end(), 0);
    do {
        f(v);
    } while (next_permutation(v.begin(), v.end()));
}

/// @brief 0..n-1 の順列を辞書順に全列挙して返す / O(n * n!)
vector<vector<int>> permutations(int n) {
    vector<vector<int>> res;
    permutations(n, [&] (const vector<int> &v) { res.emplace_back(v); });
    return res;
}

/// @brief 要素列 P の順列を辞書順列挙 相異なるもののみ / O(n * n!)
template<class T, class F>
void permutations(vector<T> P, F f) {
    sort(P.begin(), P.end());
    do {
        f(P);
    } while (next_permutation(P.begin(), P.end()));
}

/// @brief 要素列 P の順列を辞書順に全列挙して返す 相異なるもののみ / O(n * n!)
template<class T>
vector<vector<T>> permutations(vector<T> P) {
    vector<vector<T>> res;
    permutations(P, [&] (const vector<T> &v) { res.emplace_back(v); });
    return res;
}

/// @brief n 個から r 個を取る順列を辞書順列挙 / O(n!/(n-r)!)
template<class F>
void permutations(int n, int r, F f) {
    assert(r >= 0);
    if (r > n) return;
    vector<int> v(r);
    vector<bool> used(n, false);
    auto dfs = [&] (auto &&dfs, int depth) -> void {
        if (depth == r) {
            f(v);
            return;
        }
        for (int i = 0; i < n; ++i) {
            if (used[i]) continue;
            used[i] = true;
            v[depth] = i;
            dfs(dfs, depth + 1);
            used[i] = false;
        }
    };
    dfs(dfs, 0);
}

/// @brief n 個から r 個を取る順列を辞書順に全列挙して返す / O(n!/(n-r)!)
vector<vector<int>> permutations(int n, int r) {
    vector<vector<int>> res;
    permutations(n, r, [&] (const vector<int> &v) { res.emplace_back(v); });
    return res;
}

/// @brief n 種類から重複を許して k 個を選ぶ組合せを非減少で列挙 / O(C(n+k-1, k))
template<class F>
void combinations_with_replacement(int n, int k, F f) {
    assert(k >= 0);
    if (k > 0 && n <= 0) return;
    vector<int> v(k, 0);
    auto dfs = [&] (auto &&dfs, int depth, int start) -> void {
        if (depth == k) {
            f(v);
            return;
        }
        for (int i = start; i < n; ++i) {
            v[depth] = i;
            dfs(dfs, depth + 1, i);
        }
    };
    dfs(dfs, 0, 0);
}

/// @brief n 種類から重複を許して k 個を選ぶ組合せを非減少で全列挙して返す / O(C(n+k-1, k))
vector<vector<int>> combinations_with_replacement(int n, int k) {
    vector<vector<int>> res;
    combinations_with_replacement(n, k, [&] (const vector<int> &v) { res.emplace_back(v); });
    return res;
}

} // namespace titan23
