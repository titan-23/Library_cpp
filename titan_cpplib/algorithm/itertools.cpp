#pragma once

#include <vector>
#include <string>
#include <utility>
#include <cassert>
#include <algorithm>
#include <numeric>
using namespace std;

namespace titan23 {

// ----- RLE -----

// RLE
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

// RLE
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

// ----- combinations -----

int nCr(int n, int r) {
    if (r > n) return 0;
    if (r * 2 > n) {
        r = n - r;
    }
    int res = 1;
    for (int i = 1; i <= r; ++i) {
        res = res * (n - r + i) / i;
    }
    return res;
}

// n 個から r 個を選ぶ添字組合せを昇順で全列挙する
vector<vector<int>> combinations(int n, int r) {
    if (r < 0 || r > n) return {};
    int m = nCr(n, r);
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

// n ビットのうち popcount が k のビット集合を昇順で列挙する f(int mask)
template<class F>
void combinations_bit(int n, int k, F f) {
    if (k > n) return;
    int c = (1 << k) - 1;
    while (c < (1 << n)) {
        f(c);
        if (c == 0) break;
        int x = c & -c, y = c + x;
        c = (((c & ~y) / x) >> 1) | y;
    }
}

// b の部分集合を降順で列挙する b 自身と 0 を含む f(int sub)
template<class F>
void submasks(int b, F f) {
    int a = b;
    while (true) {
        f(a);
        if (a == 0) break;
        a = (a - 1) & b;
    }
}

// ----- partitions -----

// 総和が S になる正整数の組合せを全列挙する
// S=10:42 S=20:627 S=30:5604 S=40:37338 S=50:204226 S=60:966467 S=70:4087968
vector<vector<int>> partitions(int S) {
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

// ----- grouping_pair -----

// 2N 個を N 組のペアに分ける方法を全列挙する
// 場合の数は (2N-1) * (2N-3) * ... * 1
vector<vector<pair<int, int>>> grouping_pair(int n) {
    vector<vector<pair<int, int>>> result;
    vector<pair<int, int>> tmp;
    vector<bool> is_used(2 * n, false);
    auto dfs = [&] (auto &&dfs) -> void {
        if ((int)tmp.size() == n) {
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

// ----- product -----

// 値 0..m-1 を repeat 個ぶん直積する f(const vector<int> &v) v.size() == repeat
template<class F>
void product(int m, int repeat, F f) {
    if (repeat < 0) return;
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

// 値集合 a を repeat 個ぶん直積する f(const vector<int> &v) v の各要素は a の値
template<class F>
void product(const vector<int> &a, int repeat, F f) {
    int m = (int)a.size();
    if (repeat < 0) return;
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

// ----- permutations -----

// 添字 0..n-1 の順列を辞書順で列挙する f(const vector<int> &v)
template<class F>
void permutations(int n, F f) {
    vector<int> v(n);
    iota(v.begin(), v.end(), 0);
    do {
        f(v);
    } while (next_permutation(v.begin(), v.end()));
}

// 要素列 P の順列を列挙する 位置を区別し n! 通り出す f(const vector<T> &v)
template<class T, class F>
void permutations(const vector<T> &P, F f) {
    int n = (int)P.size();
    vector<int> idx(n);
    iota(idx.begin(), idx.end(), 0);
    vector<T> v(n);
    do {
        for (int i = 0; i < n; ++i) v[i] = P[idx[i]];
        f(v);
    } while (next_permutation(idx.begin(), idx.end()));
}

// n 個から r 個を取る順列を辞書順で列挙する f(const vector<int> &v) v.size() == r
template<class F>
void permutations(int n, int r, F f) {
    if (r < 0 || r > n) return;
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

// ----- combinations_with_replacement -----

// n 種類から重複を許して k 個を選ぶ添字を非減少で列挙する f(const vector<int> &v) v.size() == k
template<class F>
void combinations_with_replacement(int n, int k, F f) {
    if (k < 0) return;
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

} // namespace titan23
