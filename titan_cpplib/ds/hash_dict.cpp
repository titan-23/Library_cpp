/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/hash_dict.cpp
#pragma once
#include <vector>
#include "titan_cpplib/others/bit.cpp"
#include <random>
#include <cassert>
#include <algorithm>
#include <ostream>
#include <emmintrin.h>

using namespace std;

namespace titan23 {

/// @brief 64 bit 整数を鍵とする辞書 / 削除はできない
/// USE_HASH_FUNC=false では鍵をそのままハッシュ値に使う
template<typename V, bool USE_HASH_FUNC=true>
class HashDict {
private:
    using u64 = uint64_t;
    static constexpr const uint8_t EMPTY = 0x80;  // meta の空きの印

    vector<uint8_t> meta;
    vector<u64> keys;
    vector<V> vals;
    int cap;
    int msk;
    u64 salt;
    int size;

    constexpr u64 hash(u64 key) const {
        if constexpr (USE_HASH_FUNC) {
            // wyhash の mum と同じく、128 bit の積の上位と下位を xor する
            __uint128_t p = (__uint128_t)(key ^ salt) * 0x9e3779b97f4a7c15ULL;
            key = (u64)p ^ (u64)(p >> 64);
        }
        return key;
    }

    void init_seed() {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<u64> dis(0, UINT64_MAX);
        salt = dis(gen);
    }

    void rebuild() {
        vector<uint8_t> old_meta = move(meta);
        vector<u64> old_keys = move(keys);
        vector<V> old_vals = move(vals);
        int old_cap = cap;

        cap *= 2;
        msk = cap - 1;
        meta.assign(cap + 16, EMPTY);
        keys.resize(cap);
        vals.resize(cap);
        size = 0;

        for (int i = 0; i < old_cap; ++i) {
            if (old_meta[i] != EMPTY) {
                set(old_keys[i], old_vals[i]);
            }
        }
    }

public:
    HashDict() {
        cap = 16;
        meta.assign(cap + 16, EMPTY);
        keys.resize(cap);
        vals.resize(cap);
        msk = cap - 1;
        init_seed();
        size = 0;
    }

    /// @brief n 要素まで再構築せずに持てる辞書を作る
    HashDict(const int n) {
        cap = 16;
        while (cap < n * 2) {
            cap *= 2;
        }
        meta.assign(cap + 16, EMPTY);
        keys.resize(cap);
        vals.resize(cap);
        msk = cap - 1;
        init_seed();
        size = 0;
    }

    /// @brief key の位置と有無を返す / 無いときは挿入する位置を返す
    pair<int, bool> get_pos(const u64 &key) const {
        u64 h = hash(key);
        uint8_t h2 = h & 0x7F;
        int idx = (int)((h >> 7) & msk);

        __m128i match = _mm_set1_epi8(h2);
        __m128i empty_match = _mm_set1_epi8(EMPTY);

        while (true) {  // meta を 16 個ずつ SSE2 で比べる
            __m128i meta_data = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&meta[idx]));

            int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(meta_data, match));
            while (mask != 0) {
                int bit_pos = countr_zero(mask);
                int target = (idx + bit_pos) & msk;
                if (keys[target] == key) {
                    return {target, true};
                }
                mask &= (mask - 1);
            }

            int empty_mask = _mm_movemask_epi8(_mm_cmpeq_epi8(meta_data, empty_match));
            if (empty_mask != 0) {
                int bit_pos = countr_zero(empty_mask);
                int target = (idx + bit_pos) & msk;
                return {target, false};
            }

            idx = (idx + 16) & msk;
        }
    }

    /// @brief key の値を返す / 無ければ V()
    V get(const u64 key) const {
        auto [pos, exist_res] = get_pos(key);  // const を付けると GCC の生成コードが遅くなる
        if (!exist_res) return V();
        else return vals[pos];
    }

    /// @brief key の値を返す / 無ければ missing
    V get(const u64 key, const V missing) const {
        auto [pos, exist_res] = get_pos(key);
        if (!exist_res) return missing;
        else return vals[pos];
    }

    /// @brief key があるかを返す
    bool contains(const u64 key) const {
        return get_pos(key).second;
    }

    /// @brief get_pos と同じ
    pair<int, bool> pos(const u64 key) const {
        return get_pos(key);
    }

    /// @brief key の値を返す / 無ければ V() を挿入して返す
    V operator[] (const u64 key) {
        auto [pos, exist_res] = get_pos(key);
        if (!exist_res) {
            V res = V{};
            inner_set({pos, false}, key, res);
            return res;
        } else {
            return vals[pos];
        }
    }

    /// @brief get_pos の結果から値を返す / 無ければ missing
    V inner_get(const pair<int, bool> &dat, const V missing) const {
        auto [pos, is_exist] = dat;
        if (!is_exist) return missing;
        return vals[pos];
    }

    /// @brief get_pos の結果から値を返す / 無ければ V()
    V inner_get(const pair<int, bool> &dat) {
        auto [pos, is_exist] = dat;
        if (!is_exist) return V();
        return vals[pos];
    }

    /// @brief get_pos の結果を使って key の値を val にする / get_pos との間に変更操作を挟まないこと
    void inner_set(const pair<int, bool> &dat, const u64 key, const V val) {
        auto [pos, is_exist] = dat;
        vals[pos] = val;
        if (!is_exist) {
            uint8_t h2 = hash(key) & 0x7F;
            meta[pos] = h2;
            if (pos < 16) meta[cap + pos] = h2;  // 末尾をまたいで 16 個読めるよう、先頭 16 個を末尾に写す
            keys[pos] = key;
            ++size;
            if (size * 2 > cap) {
                rebuild();
            }
        }
    }

    /// @brief key の値を val にする
    void set(const u64 key, const V val) {
        auto [pos, is_exist] = get_pos(key);
        inner_set({pos, is_exist}, key, val);
    }

    /// @brief key の値に val を加算する / 無ければ val で挿入する
    void add(const u64 key, const V val) {
        auto [pos, is_exist] = get_pos(key);
        if (!is_exist) {
            inner_set({pos, false}, key, val);
        } else {
            vals[pos] += val;
        }
    }

    /// @brief val が今の値より小さければ更新して true を返す / 無ければ挿入して false を返す
    bool contains_set(const u64 key, const V val) {
        auto [pos, is_exist] = get_pos(key);
        if (is_exist) {
            if (val < vals[pos]) {
                vals[pos] = val;
            } else {
                return false;
            }
            return true;
        } else {
            inner_set({pos, false}, key, val);
            return false;
        }
    }

    /// @brief 全ての値を返す / 順序は不定 / O(容量)
    vector<V> values() const {
        vector<V> res;
        res.reserve(size);
        for (int i = 0; i < cap; ++i) {
            if (meta[i] != EMPTY) {
                res.emplace_back(vals[i]);
            }
        }
        return res;
    }

    /// @brief 全ての鍵と値の組を返す / 順序は不定 / O(容量)
    vector<pair<u64, V>> items() const {
        vector<pair<u64, V>> res;
        res.reserve(size);
        for (int i = 0; i < cap; ++i) {
            if (meta[i] != EMPTY) {
                res.emplace_back(keys[i], vals[i]);
            }
        }
        return res;
    }

    /// @brief 鍵の昇順に出力する
    friend ostream& operator<<(ostream& os, const HashDict &hd) {
        vector<pair<u64, V>> a = hd.items();
        sort(a.begin(), a.end(), [](const pair<u64, V> &x, const pair<u64, V> &y) { return x.first < y.first; });
        os << "{";
        for (int i = 0; i < (int)a.size(); ++i) {
            if (i > 0) os << ", ";
            os << a[i].first << ": " << a[i].second;
        }
        os << "}";
        return os;
    }

    /// @brief 全要素を削除する / O(容量)
    void clear() {
        if (empty()) return;
        size = 0;
        fill(meta.begin(), meta.end(), EMPTY);
    }

    int len() const { return size; }
    /// @brief inner_len と inner_size は表の容量を返す
    int inner_len() const { return cap; }
    int inner_size() const { return cap; }
    bool empty() const { return size == 0; }
};
} // namespace titan23
