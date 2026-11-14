/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/wordsize_tree_set.cpp
#pragma once
#include <vector>
#include <cassert>
#include "titan_cpplib/others/print.cpp"
#include "titan_cpplib/others/bit.cpp"
using namespace std;

namespace titan23 {

/// @brief 64 分木で [0, u] の整数を管理する集合
class WordsizeTreeSet {
private:
    using u64 = unsigned long long;
    int u, _len;
    vector<vector<u64>> data;

    void _build(int u, vector<u64> &A) {
        data[0] = A;
        int dep = 1;
        while (u) {
            data[dep] = vector<u64>((u>>6)+1);
            auto &a = data[dep];
            const auto &b = data[dep-1];
            for (int i = 0; i <= u; ++i) {
                if (b[i]) {
                    a[i>>6] |= 1ull << (i&63);
                }
            }
            dep++;
            u >>= 6;
        }
    }

    int get_dep(int u) const {
        int dep = 1;
        while (u) {
            u >>= 6;
            dep++;
        }
        return dep;
    }

public:
    WordsizeTreeSet() : u(0), _len(0) {}
    /// @brief [0, u] の整数を扱う空の集合を構築する
    WordsizeTreeSet(int u) : u(u+1), _len(0) {
        while (u) {
            u >>= 6;
            data.emplace_back(u+1);
        }
    }

    /// @brief s[i] == '1' である i を要素として構築する / 値の範囲は [0, u]
    WordsizeTreeSet(int u, const string &s) : u(u+1), _len(0) {
        u >>= 6;
        data.resize(get_dep(u));
        vector<u64> A(u+1);
        int n = s.size();
        for (int i = 0; i < n; ++i) {
            if (s[i] == '1') {
                ++_len;
                A[i>>6] |= 1ull << (i&63);
            }
        }
        _build(u, A);
    }

    /// @brief a の要素から構築する / 値の範囲は [0, u]
    WordsizeTreeSet(int u, const vector<int> &a) : u(u+1), _len(0) {
        u >>= 6;
        data.resize(get_dep(u));
        vector<u64> A(u+1);
        for (int e : a) {
            if (((A[e>>6] >> (e&63)) & 1) == 0) {
                ++_len;
                A[e>>6] |= 1ull << (e&63);
            }
        }
        _build(u, A);
    }

    /// @brief v を追加する / 既にあれば false を返す / O(log_64 u)
    bool add(int v) {
        assert(0 <= v && v < u);
        if ((data[0][v>>6] >> (v&63)) & 1) return false;
        _len++;
        for (vector<u64> &a : data) {
            a[v>>6] |= 1ull << (v&63);
            v >>= 6;
        }
        return true;
    }

    /// @brief v があれば削除して true を返す / O(log_64 u)
    bool discard(int v) {
        if (((data[0][v >> 6] >> (v & 63) & 1)) == 0) return false;
        _len--;
        for (vector<u64> &a : data) {
            a[v>>6] &= ~(1ull << (v & 63));
            v >>= 6;
            if (a[v]) break;
        }
        return true;
    }

    /// @brief v を削除する / v は集合に含まれる必要がある / O(log_64 u)
    void remove(int v) {
        if (!discard(v)) {
            assert(false);
        }
    }

    /// @brief v 以上で最小の要素を返す / なければ -1 / O(log_64 u)
    int ge(int v) const {
        int d = 0;
        while (1) {
            if (d >= (int)data.size() || ((v>>6) >= data[d].size())) { return -1; }
            u64 m = data[d][v>>6] & ((~(u64)0) << (v & 63));
            if (m == 0) {
                d++;
                v = (v >> 6) + 1;
            } else {
                v = (v >> 6 << 6) + bit_length(m & -m);
                v--;
                if (d == 0) break;
                v <<= 6;
                d--;
            }
        }
        return v;
    }

    /// @brief v より大きい最小の要素を返す / なければ -1 / O(log_64 u)
    int gt(int v) const {
        return ge(v+1);
    }

    /// @brief v 以下で最大の要素を返す / なければ -1 / O(log_64 u)
    int le(int v) const {
        int d = 0;
        while (1) {
            if (v < 0 || d >= (int)data.size()) return -1;
            u64 m = data[d][v >> 6] & ~((~1ull) << (v & 63));
            if (m == 0) {
                d++;
                v = (v >> 6) - 1;
            } else {
                v = (v >> 6 << 6) + bit_length(m) - 1;
                if (d == 0) break;
                v <<= 6;
                v += 63;
                --d;
            }
        }
        return v;
    }

    /// @brief v より小さい最大の要素を返す / なければ -1 / O(log_64 u)
    int lt(int v) const {
        return v == 0 ? -1 : le(v-1);
    }

    /// @brief v が含まれるか判定する / O(1)
    bool contains(int v) const {
        return (data[0][v>>6] >> (v&63) & 1) == 1;
    }

    /// @brief 集合を {0, 1, ..., n-1} にする
    void fill(int n) {
        _len = n;
        for (int d = 0; d < (int)data.size(); ++d) {
            int words = (int)data[d].size();
            int word = n >> 6;
            int rem = n & 63;
            for (int i = 0; i < word && i < words; ++i) data[d][i] = ~0ull;
            if (word < words) {
                data[d][word] = (rem == 0) ? 0ull : ((1ull << rem) - 1);
                for (int i = word + 1; i < words; ++i) data[d][i] = 0;
            }
            n = (n + 63) >> 6;
        }
    }

    /// @brief 要素数を返す / O(1)
    int len() const { return _len; }

    /// @brief 全要素を昇順に並べて返す
    vector<int> tovector() const {
        int v = 0;
        vector<int> a(_len);
        int idx = 0;
        if (contains(v)) {
            a[idx] = v;
            idx++;
        }
        while (1) {
            int nxt = gt(v);
            if (nxt == -1) break;
            v = nxt;
            a[idx] = v;
            idx++;
        }
        return a;
    }
};

} // namespace titan23
