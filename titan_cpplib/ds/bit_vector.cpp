/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/bit_vector.cpp
#pragma once
#include <algorithm>
#include <iostream>
#include <vector>
#include <cassert>
#include <cstdint>
#include "titan_cpplib/others/bit.cpp"
using namespace std;

namespace titan23 {

/// @brief rank と select を扱う静的なビット列 / set の後に build を呼び出してからクエリを実行する
class BitVector {
private:
    using u64 = unsigned long long;

    // 64 bit ごとにビット列とその直前までの 1 の個数を組にして保持する
    struct Block {
        u64 bits;
        u64 acc;
    };

    // select の二分探索の範囲を絞るため、1 と 0 のそれぞれについて _SELECT_SAMPLE 個ごとの出現位置を含むブロックを記録する
    static constexpr int _SELECT_SAMPLE = 256;

    int n, bsize;
    vector<Block> data;  // 末尾に番兵のブロックを 1 つ置く
    vector<int> hint0, hint1;  // hint1[j] は j * _SELECT_SAMPLE 番目の 1 を含むブロック

    int _ones_before_block(const int i) const { return data[i].acc; }

    int _zeros_before_block(const int i) const { return 64 * i - (int)data[i].acc; }

    // count_before(m) <= j * _SELECT_SAMPLE を満たす最大のブロック m (< bsize) を各 j について求める
    template<class F>
    void _build_hint(vector<int> &hint, const int total, F count_before) {
        hint.assign(total / _SELECT_SAMPLE + 1, 0);
        for (int j = 0, m = 0; j < (int)hint.size(); ++j) {
            while (m + 1 < bsize && count_before(m + 1) <= j * _SELECT_SAMPLE) ++m;
            hint[j] = m;
        }
    }

    // count_before(m) <= k を満たす最大のブロック m (< bsize) を返す
    template<class F>
    int _select_block(const vector<int> &hint, const int k, F count_before) const {
        const int j = k / _SELECT_SAMPLE;
        int l = hint[j];
        int r = j + 1 < (int)hint.size() ? hint[j + 1] + 1 : bsize;
        while (r - l > 1) {
            const int m = (l + r) >> 1;
            if (count_before(m) > k) {
                r = m;
            } else {
                l = m;
            }
        }
        return l;
    }

public:
    /// @brief 長さ 0 のビット列を構築する
    BitVector() : n(0), bsize(0), data(1, {0, 0}) {}
    /// @brief 全ビットが 0 の長さ n のビット列を構築する
    BitVector(const int n) : n(n), bsize((n+63)>>6), data(bsize+1, {0, 0}) {}

    /// @brief i 番目のビットが bit_of(i) である長さ n のビット列を構築し、build も実行する / O(n)
    template<class F>
    BitVector(const int n, F bit_of) : BitVector(n) {
        for (int i = 0; i < bsize; ++i) {
            const int begin = i << 6;
            const int width = min(64, n - begin);
            u64 bits = 0;
            for (int j = 0; j < width; ++j) bits |= u64(bool(bit_of(begin + j))) << j;
            data[i].bits = bits;
        }
        build();
    }

    /// @brief k 番目のビットを 1 にする / クエリの前に build が必要 / O(1)
    void set(const int k) {
        data[k>>6].bits |= (1ull) << (k & 63);
    }

    /// @brief rank と select で使用する累積値と索引を構築する / O(n)
    void build() {
        for (int i = 0; i < bsize; ++i) {
            data[i+1].acc = data[i].acc + popcount(data[i].bits);
        }
        const int ones = data[bsize].acc;
        _build_hint(hint1, ones, [&](const int m) { return _ones_before_block(m); });
        _build_hint(hint0, n - ones, [&](const int m) { return _zeros_before_block(m); });
    }

    /// @brief k 番目のビットを返す / O(1)
    bool access(const int k) const {
        return (data[k>>6].bits >> (k&63)) & 1;
    }

    /// @brief 区間 [0, r) に含まれる 0 の個数を返す / O(1)
    int rank0(const int r) const {
        return r - rank1(r);
    }

    /// @brief 区間 [0, r) に含まれる 1 の個数を返す / O(1)
    int rank1(const int r) const {
        const Block &b = data[r>>6];
        return b.acc + popcount(b.bits & ((1ull << (r & 63)) - 1));
    }

    /// @brief 区間 [0, r) に含まれる v の個数を返す / O(1)
    int rank(const int r, const bool v) const {
        return v ? rank1(r) : rank0(r);
    }

    /// @brief k 番目の 0 の位置を返す / k は 0 始まりとし、存在しない場合は -1 を返す / O(log n)
    int select0(int k) const {
        if (k < 0 || rank0(n) <= k) return -1;
        // 直前までの 0 の個数が k 以下である最後のブロックを探す
        const int l = _select_block(hint0, k, [&](const int m) { return _zeros_before_block(m); });
        return 64 * l + select64(~data[l].bits, k - _zeros_before_block(l));
    }

    /// @brief k 番目の 1 の位置を返す / k は 0 始まりとし、存在しない場合は -1 を返す / O(log n)
    int select1(int k) const {
        if (k < 0 || rank1(n) <= k) return -1;
        const int l = _select_block(hint1, k, [&](const int m) { return _ones_before_block(m); });
        return 64 * l + select64(data[l].bits, k - _ones_before_block(l));
    }

    /// @brief k 番目の v の位置を返す / k は 0 始まりとし、存在しない場合は -1 を返す / O(log n)
    int select(const int k, const int v) const {
        return v ? select1(k) : select0(k);
    }

    /// @brief ビット列の長さを返す / O(1)
    int len() const {
        return n;
    }

    void print() const {
        cout << "[";
        for (int i = 0; i < len()-1; ++i) {
            cout << access(i) << ", ";
        }
        if (len() > 0) {
            cout << access(len()-1);
        }
        cout << "]\n";
    }
};
}  // namespace titan23
