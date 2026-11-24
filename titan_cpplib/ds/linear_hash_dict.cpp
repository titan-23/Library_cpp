/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/linear_hash_dict.cpp
#pragma once

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

using namespace std;

namespace titan23 {

/// @brief 線形探査で 64 bit 整数をキーとする辞書 / 削除はできない
/// USE_HASH_FUNC=true では乱択のハッシュ関数を使用して、固定の入力による衝突攻撃を防ぐ
/// USE_HASH_FUNC=false ではキーの下位ビットが偏ると操作が O(n) になり得る
template<typename V, bool USE_HASH_FUNC=true>
class LinearHashDict {
private:
    using u64 = uint64_t;

    vector<u64> keys;  // erase がないため 0 で空きを表す
    vector<V> vals;    // キー 0 の値は vals[cap] に格納する
    int cap;
    int msk;
    u64 salt;
    int size;
    bool has_zero;

    constexpr u64 hash(u64 key) const {
        if constexpr (USE_HASH_FUNC) {
            key ^= salt;
            key ^= rotl(key, 25) ^ rotl(key, 50);
            const __uint128_t z = (__uint128_t)key * 0xa24baed4963ee407ULL;
            return (u64)z ^ (u64)(z >> 64);
        }
        return key;
    }

    void init_seed() {
        random_device rd;
        uniform_int_distribution<uint32_t> dis;
        salt = (u64(dis(rd)) << 32) | dis(rd);
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

    void rebuild() {
        vector<u64> old_keys = move(keys);
        vector<V> old_vals = move(vals);
        int old_cap = cap;
        bool old_zero = has_zero;

        cap *= 2;
        msk = cap - 1;
        keys.assign(cap, 0);
        vals.clear();
        vals.resize(cap + 1);

        if (old_zero) vals[cap] = move(old_vals[old_cap]);
        for (int i = 0; i < old_cap; ++i) {
            u64 key = old_keys[i];
            if (key == 0) continue;
            int pos = (int)(hash(key) & msk);
            while (keys[pos] != 0) pos = (pos + 1) & msk;
            keys[pos] = key;
            vals[pos] = move(old_vals[i]);
        }
    }

public:
    LinearHashDict() : cap(16), msk(15), salt(0), size(0), has_zero(false) {
        init_seed();
        init_table(0);
    }

    /// @brief n 要素まで再構築せずに保持できる辞書を作成する
    LinearHashDict(const int n) : cap(16), msk(15), salt(0), size(0), has_zero(false) {
        assert(n >= 0);
        init_seed();
        init_table(n);
    }

    /// @brief key の位置と有無を返す / 無いときは挿入する位置を返す / 結果は次の変更操作まで有効
    pair<int, bool> get_pos(const u64 &key) const {
        if (key == 0) return {cap, has_zero};
        int pos = (int)(hash(key) & msk);
        while (keys[pos] != 0) {
            if (keys[pos] == key) return {pos, true};
            pos = (pos + 1) & msk;
        }
        return {pos, false};
    }

    /// @brief key の値を返す / 無ければ V() を返す
    V get(const u64 key) const {
        if (key == 0) return has_zero ? vals[cap] : V();
        int pos = (int)(hash(key) & msk);
        while (keys[pos] != 0) {
            if (keys[pos] == key) return vals[pos];
            pos = (pos + 1) & msk;
        }
        return V();
    }

    /// @brief key の値を返す / 無ければ missing を返す
    V get(const u64 key, const V missing) const {
        if (key == 0) return has_zero ? vals[cap] : missing;
        int pos = (int)(hash(key) & msk);
        while (keys[pos] != 0) {
            if (keys[pos] == key) return vals[pos];
            pos = (pos + 1) & msk;
        }
        return missing;
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
    V operator[](const u64 key) {
        if (key == 0) {
            if (has_zero) return vals[cap];
            V val{};
            vals[cap] = val;
            has_zero = true;
            ++size;
            if (size * 2 > cap) rebuild();
            return val;
        }
        int pos = (int)(hash(key) & msk);
        while (keys[pos] != 0) {
            if (keys[pos] == key) return vals[pos];
            pos = (pos + 1) & msk;
        }
        V val{};
        keys[pos] = key;
        vals[pos] = val;
        ++size;
        if (size * 2 > cap) rebuild();
        return val;
    }

    /// @brief get_pos の結果から値を返す / 無ければ missing を返す
    V inner_get(const pair<int, bool> &dat, const V missing) const {
        if (!dat.second) return missing;
        return vals[dat.first];
    }

    /// @brief get_pos の結果から値を返す / 無ければ V() を返す
    V inner_get(const pair<int, bool> &dat) {
        if (!dat.second) return V();
        return vals[dat.first];
    }

    /// @brief get_pos の結果を使用して key の値を val に設定する / get_pos との間に変更操作を挟まないこと
    void inner_set(const pair<int, bool> &dat, const u64 key, const V val) {
        vals[dat.first] = val;
        if (dat.second) return;
        if (key == 0) has_zero = true;
        else keys[dat.first] = key;
        ++size;
        if (size * 2 > cap) rebuild();
    }

    /// @brief key の値を val に設定する
    void set(const u64 key, const V val) {
        if (key == 0) {
            vals[cap] = val;
            if (has_zero) return;
            has_zero = true;
            ++size;
            if (size * 2 > cap) rebuild();
            return;
        }
        int pos = (int)(hash(key) & msk);
        while (keys[pos] != 0) {
            if (keys[pos] == key) {
                vals[pos] = val;
                return;
            }
            pos = (pos + 1) & msk;
        }
        keys[pos] = key;
        vals[pos] = val;
        ++size;
        if (size * 2 > cap) rebuild();
    }

    /// @brief key の値に val を加算する / 無ければ val で挿入する
    void add(const u64 key, const V val) {
        if (key == 0) {
            if (has_zero) vals[cap] += val;
            else {
                vals[cap] = val;
                has_zero = true;
                ++size;
                if (size * 2 > cap) rebuild();
            }
            return;
        }
        int pos = (int)(hash(key) & msk);
        while (keys[pos] != 0) {
            if (keys[pos] == key) {
                vals[pos] += val;
                return;
            }
            pos = (pos + 1) & msk;
        }
        keys[pos] = key;
        vals[pos] = val;
        ++size;
        if (size * 2 > cap) rebuild();
    }

    /// @brief val が現在の値より小さければ更新して true を返す / key が無ければ挿入して false を返す
    bool contains_set(const u64 key, const V val) {
        if (key == 0) {
            if (!has_zero) {
                vals[cap] = val;
                has_zero = true;
                ++size;
                if (size * 2 > cap) rebuild();
                return false;
            }
            if (!(val < vals[cap])) return false;
            vals[cap] = val;
            return true;
        }
        int pos = (int)(hash(key) & msk);
        while (keys[pos] != 0) {
            if (keys[pos] == key) {
                if (!(val < vals[pos])) return false;
                vals[pos] = val;
                return true;
            }
            pos = (pos + 1) & msk;
        }
        keys[pos] = key;
        vals[pos] = val;
        ++size;
        if (size * 2 > cap) rebuild();
        return false;
    }

    /// @brief 全ての値を返す / 順序は不定 / O(容量)
    vector<V> values() const {
        vector<V> res;
        res.reserve(size);
        if (has_zero) res.emplace_back(vals[cap]);
        for (int i = 0; i < cap; ++i) {
            if (keys[i] != 0) res.emplace_back(vals[i]);
        }
        return res;
    }

    /// @brief 全てのキーと値の組を返す / 順序は不定 / O(容量)
    vector<pair<u64, V>> items() const {
        vector<pair<u64, V>> res;
        res.reserve(size);
        if (has_zero) res.emplace_back(0, vals[cap]);
        for (int i = 0; i < cap; ++i) {
            if (keys[i] != 0) res.emplace_back(keys[i], vals[i]);
        }
        return res;
    }

    /// @brief 全要素を削除する / O(容量)
    void clear() {
        if (empty()) return;
        fill(keys.begin(), keys.end(), 0);
        size = 0;
        has_zero = false;
    }

    int len() const {
        return size;
    }

    /// @brief ハッシュ表の容量を返す
    int inner_len() const {
        return cap;
    }

    bool empty() const {
        return size == 0;
    }
};

} // namespace titan23
