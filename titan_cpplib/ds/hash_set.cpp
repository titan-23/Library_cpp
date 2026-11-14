/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/hash_set.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <random>
#include <vector>
#include "titan_cpplib/others/simd_int.cpp"

using namespace std;

namespace titan23 {

/// @brief 64 bit 整数の集合 / 要素の削除はできない
class HashSet {
private:
    using u64 = unsigned long long;
    static constexpr int WINDOW = 4;  // contains で一度に比べる鍵の数
    static constexpr int PAD = 64;    // 表の末尾の余白

    vector<u64> keys;  // 長さ cap + PAD。[limit, cap + PAD) は常に 0
    u64 msk;  // cap - 1
    int cap, limit;
    int size;
    u64 salt;
    bool has_zero;

    // オブジェクトごとに異なる salt を返す。random_device はスレッドごとに 1 回だけ使う
    static u64 new_salt() {
        static thread_local u64 state = [] {
            random_device rd;
            return (u64(rd()) << 32) ^ rd();
        }();
        u64 z = (state += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    // 128 bit の積の上位と下位を xor する。積を変数に置くと GCC 12 で遅くなるので、上位と下位を別の式で求める
    u64 home(const u64 key) const {
        constexpr u64 K = 0x9e3779b97f4a7c15ULL;
        const u64 x = key ^ salt;
        const u64 hi = (u64)(((__uint128_t)x * K) >> 64);
        return ((x * K) ^ hi) & msk;
    }

    void alloc(const int c) {
        cap = c;
        msk = c - 1;
        limit = c + PAD - WINDOW;
        keys.assign(c + PAD, 0);
    }

    // 再構築では呼び出し元の挿入を小さく保つため、インライン展開させない
    __attribute__((noinline)) void rebuild() {
        const vector<u64> old = move(keys);
        int c = cap;
        while (true) {
            assert(c <= (1 << 29));
            c *= 2;
            alloc(c);
            bool ok = true;
            for (const u64 key : old) {
                if (key == 0) continue;
                u64 p = home(key);
                while (keys[p] != 0) ++p;
                if (p >= (u64)limit) {  // 余白を使い切った。容量を増やしてやり直す
                    ok = false;
                    break;
                }
                keys[p] = key;
            }
            if (ok) return;
        }
    }

public:
    HashSet() : salt(new_salt()), has_zero(false) {
        alloc(16);
        size = 0;
    }

    /// @brief n 要素まで再構築せずに持てる集合を作る
    HashSet(const int n) : salt(new_salt()), has_zero(false) {
        assert(0 <= n && n <= (1 << 28));
        int c = 16;
        while (c < 2 * n) c *= 2;
        alloc(c);
        size = 0;
    }

    /// @brief key があるかを返す / 期待 O(1)
    // SSE2 だけの環境の -O2 ではインライン展開されないので強制する
    __attribute__((always_inline)) bool contains(const u64 key) const {
        if (key == 0) return has_zero;
        const u64 *p = keys.data() + home(key);
        if constexpr (simd_equal_bits_vectorized_v<u64>) {
            // 窓に鍵があれば、窓に空きがあっても見つかったとしてよい
            while (true) {
                if (simd_equal_bits<WINDOW>(p, key)) return true;
                if (simd_equal_bits<WINDOW>(p, u64(0))) return false;
                p += WINDOW;
            }
        } else {
            for (; *p != 0; ++p) {
                if (*p == key) return true;
            }
            return false;
        }
    }

    /// @brief key を追加する / 期待 O(1)
    void insert(const u64 key) {
        contains_insert(key);
    }

    /// @brief key があれば true を返す / 無ければ挿入して false を返す / 期待 O(1)
    bool contains_insert(const u64 key) {
        if (key == 0) {
            if (has_zero) return true;
            has_zero = true;
            ++size;
            return false;
        }
        u64 p = home(key);
        while (true) {
            const u64 k = keys[p];
            if (k == key) return true;
            if (k == 0) break;
            ++p;
        }
        // [limit, cap + PAD) は空なので p <= limit。p == limit なら余白を使い切ったので作り直す
        keys[p] = key;
        ++size;
        if (size * 2 > cap || p >= (u64)limit) rebuild();
        return false;
    }

    /// @brief 全要素を削除する / O(容量)
    void clear() {
        if (empty()) return;
        size = 0;
        has_zero = false;
        fill(keys.begin(), keys.begin() + limit, 0);
    }

    bool empty() const {
        return size == 0;
    }

    int len() const {
        return size;
    }
};
} // namespace titan23
