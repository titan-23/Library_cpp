/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/simple_tabulation_hash_dict.cpp
#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

using namespace std;

namespace titan23 {

/// @brief 鍵が u64 の辞書 / simple tabulation hashing を使う / erase を持たない
/// 計算量は、入力列が内部乱数と独立なときの期待値
template<typename V>
class SimpleTabulationHashDict {
private:
    using u64 = uint64_t;

    // keys[p] == 0 は空きを表す。鍵 0 の値は vals[cap] に置く
    vector<u64> keys;
    vector<V> vals;
    array<array<uint32_t, 256>, 8> tab;
    int cap;
    int msk;
    int hash_bits;
    int size;
    bool has_zero;

    uint32_t hash(u64 key) const {
        return tab[0][key & 255] ^ tab[1][key >> 8 & 255] ^ tab[2][key >> 16 & 255] ^
               tab[3][key >> 24 & 255] ^ tab[4][key >> 32 & 255] ^ tab[5][key >> 40 & 255] ^
               tab[6][key >> 48 & 255] ^ tab[7][key >> 56];
    }

    static uint32_t bit_mask(int bits) {
        return (uint32_t(1) << bits) - 1;
    }

    // tab の各値に、容量に必要な下位 bits 個の乱数 bit を入れる
    void init_hash(int bits) {
        random_device rd;
        uniform_int_distribution<uint32_t> dis;
        uint64_t pool = 0;
        int left = 0;
        uint32_t mask = bit_mask(bits);
        for (auto &row : tab) {
            for (uint32_t &v : row) {
                if (left < bits) {
                    pool |= uint64_t(dis(rd)) << left;
                    left += 32;
                }
                v = uint32_t(pool) & mask;
                pool >>= bits;
                left -= bits;
            }
        }
        hash_bits = bits;
    }

    // 拡張時に、tab の各値へ独立な乱数 bit を 1 つ上位に加える
    void add_hash_bit() {
        random_device rd;
        uniform_int_distribution<uint32_t> dis;
        uint32_t bits = 0;
        int left = 0;
        for (auto &row : tab) {
            for (uint32_t &v : row) {
                if (left == 0) {
                    bits = dis(rd);
                    left = 32;
                }
                v |= (bits & 1) << hash_bits;
                bits >>= 1;
                --left;
            }
        }
        ++hash_bits;
    }

    void init_table(int n) {
        cap = 16;
        while (cap < n * 2) cap *= 2;
        msk = cap - 1;
        keys.assign(cap, 0);
        vals.resize(cap + 1);
        size = 0;
        has_zero = false;
    }

    // 容量を 2 倍にして入れ直す
    void rebuild() {
        vector<u64> old_keys = move(keys);
        vector<V> old_vals = move(vals);
        int old_cap = cap;
        bool old_zero = has_zero;

        cap *= 2;
        msk = cap - 1;
        add_hash_bit();
        keys.assign(cap, 0);
        vals.clear();
        vals.resize(cap + 1);

        if (old_zero) vals[cap] = move(old_vals[old_cap]);
        for (int i = 0; i < old_cap; ++i) {
            u64 key = old_keys[i];
            if (key == 0) continue;
            int p = (int)(hash(key) & msk);
            while (keys[p] != 0) p = (p + 1) & msk;
            keys[p] = key;
            vals[p] = move(old_vals[i]);
        }
    }

public:
    /// @brief 空の辞書を構築する
    SimpleTabulationHashDict() : cap(16), msk(15), hash_bits(4), size(0), has_zero(false) {
        init_table(0);
        init_hash(4);
    }

    /// @brief n 要素分の容量で構築する / O(n)
    SimpleTabulationHashDict(const int n)
        : cap(16), msk(15), hash_bits(4), size(0), has_zero(false) {
        assert(n >= 0);
        init_table(n);
        init_hash(__builtin_ctz((unsigned)cap));
    }

    /// @brief key の位置と、key があるかを返す / 結果は次の変更操作まで有効 / 期待償却 O(1)
    pair<int, bool> get_pos(const u64 &key) const {
        if (key == 0) return {cap, has_zero};
        int p = (int)(hash(key) & msk);
        while (keys[p] != 0) {
            if (keys[p] == key) return {p, true};
            p = (p + 1) & msk;
        }
        return {p, false};
    }

    /// @brief key の値を返す / なければ V() / 期待償却 O(1)
    V get(const u64 key) const {
        if (key == 0) return has_zero ? vals[cap] : V();
        int p = (int)(hash(key) & msk);
        while (keys[p] != 0) {
            if (keys[p] == key) return vals[p];
            p = (p + 1) & msk;
        }
        return V();
    }

    /// @brief key の値を返す / なければ missing / 期待償却 O(1)
    V get(const u64 key, const V missing) const {
        if (key == 0) return has_zero ? vals[cap] : missing;
        int p = (int)(hash(key) & msk);
        while (keys[p] != 0) {
            if (keys[p] == key) return vals[p];
            p = (p + 1) & msk;
        }
        return missing;
    }

    /// @brief key があるか判定する / 期待償却 O(1)
    bool contains(const u64 key) const {
        return get_pos(key).second;
    }

    /// @brief get_pos と同じ / 結果は次の変更操作まで有効 / 期待償却 O(1)
    pair<int, bool> pos(const u64 key) const {
        return get_pos(key);
    }

    /// @brief key の値のコピーを返す / なければ V{} を挿入する / 期待償却 O(1)
    V operator[](const u64 key) {
        if (key == 0) {
            if (has_zero) return vals[cap];
            V val{};
            vals[cap] = V(val);
            has_zero = true;
            ++size;
            if (size * 2 > cap) rebuild();
            return val;
        }
        int p = (int)(hash(key) & msk);
        while (keys[p] != 0) {
            if (keys[p] == key) return vals[p];
            p = (p + 1) & msk;
        }
        V val{};
        keys[p] = key;
        vals[p] = V(val);
        ++size;
        if (size * 2 > cap) rebuild();
        return val;
    }

    /// @brief get_pos の結果 dat が指す値を返す / なければ missing / O(1)
    V inner_get(const pair<int, bool> &dat, const V missing) const {
        if (!dat.second) return missing;
        return vals[dat.first];
    }

    /// @brief get_pos の結果 dat が指す値を返す / なければ V() / O(1)
    V inner_get(const pair<int, bool> &dat) const {
        if (!dat.second) return V();
        return vals[dat.first];
    }

    /// @brief get_pos(key) の結果 dat を使い、key の値を val にする / 期待償却 O(1)
    void inner_set(const pair<int, bool> &dat, const u64 key, V val) {
        vals[dat.first] = move(val);
        if (dat.second) return;
        if (key == 0) has_zero = true;
        else keys[dat.first] = key;
        ++size;
        if (size * 2 > cap) rebuild();
    }

    /// @brief key の値を val にする / 期待償却 O(1)
    void set(const u64 key, V val) {
        if (key == 0) {
            vals[cap] = move(val);
            if (has_zero) return;
            has_zero = true;
            ++size;
            if (size * 2 > cap) rebuild();
            return;
        }
        int p = (int)(hash(key) & msk);
        while (keys[p] != 0) {
            if (keys[p] == key) {
                vals[p] = move(val);
                return;
            }
            p = (p + 1) & msk;
        }
        keys[p] = key;
        vals[p] = move(val);
        ++size;
        if (size * 2 > cap) rebuild();
    }

    /// @brief key の値に val を加える / なければ val を挿入する / 期待償却 O(1)
    void add(const u64 key, const V val) {
        if (key == 0) {
            if (has_zero) vals[cap] += val;
            else {
                vals[cap] = V(val);
                has_zero = true;
                ++size;
                if (size * 2 > cap) rebuild();
            }
            return;
        }
        int p = (int)(hash(key) & msk);
        while (keys[p] != 0) {
            if (keys[p] == key) {
                vals[p] += val;
                return;
            }
            p = (p + 1) & msk;
        }
        keys[p] = key;
        vals[p] = V(val);
        ++size;
        if (size * 2 > cap) rebuild();
    }

    /// @brief key があり val が今の値より小さいとき、更新して true を返す / key がなければ val を挿入して false / 期待償却 O(1)
    bool contains_set(const u64 key, const V val) {
        if (key == 0) {
            if (!has_zero) {
                vals[cap] = V(val);
                has_zero = true;
                ++size;
                if (size * 2 > cap) rebuild();
                return false;
            }
            if (!(val < vals[cap])) return false;
            vals[cap] = val;
            return true;
        }
        int p = (int)(hash(key) & msk);
        while (keys[p] != 0) {
            if (keys[p] == key) {
                if (!(val < vals[p])) return false;
                vals[p] = val;
                return true;
            }
            p = (p + 1) & msk;
        }
        keys[p] = key;
        vals[p] = V(val);
        ++size;
        if (size * 2 > cap) rebuild();
        return false;
    }

    /// @brief 値の一覧を返す / O(cap)
    vector<V> values() const {
        vector<V> res;
        res.reserve(size);
        if (has_zero) res.emplace_back(vals[cap]);
        for (int i = 0; i < cap; ++i) {
            if (keys[i] != 0) res.emplace_back(vals[i]);
        }
        return res;
    }

    /// @brief (鍵, 値) の組の一覧を返す / O(cap)
    vector<pair<u64, V>> items() const {
        vector<pair<u64, V>> res;
        res.reserve(size);
        if (has_zero) res.emplace_back(0, vals[cap]);
        for (int i = 0; i < cap; ++i) {
            if (keys[i] != 0) res.emplace_back(keys[i], vals[i]);
        }
        return res;
    }

    /// @brief 全要素を削除する / O(cap)
    void clear() {
        if (empty()) return;
        fill(keys.begin(), keys.end(), 0);
        size = 0;
        has_zero = false;
    }

    /// @brief 要素数を返す / O(1)
    int len() const {
        return size;
    }

    /// @brief 表の容量 cap を返す / O(1)
    int inner_len() const {
        return cap;
    }

    bool empty() const {
        return size == 0;
    }
};

} // namespace titan23
