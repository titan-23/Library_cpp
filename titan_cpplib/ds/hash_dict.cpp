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

template<typename V, bool USE_HASH_FUNC=true>
class HashDict {
private:
    using u64 = uint64_t;
    static constexpr const uint8_t EMPTY = 0x80;

    vector<uint8_t> meta;
    vector<u64> keys;
    vector<V> vals;
    int cap;
    int msk;
    u64 salt;
    int size;

    constexpr u64 hash(u64 key) const {
        if constexpr (USE_HASH_FUNC) {
            // 128 bit の積の上位と下位を xor する (wyhash の mum)。1 回の乗算で全ビットが混ざり、
            // 上位ビットだけが変わる鍵や連番でも偏らない
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

    pair<int, bool> get_pos(const u64 &key) const {
        u64 h = hash(key);
        uint8_t h2 = h & 0x7F;
        int idx = (int)((h >> 7) & msk);

        __m128i match = _mm_set1_epi8(h2);
        __m128i empty_match = _mm_set1_epi8(EMPTY);

        while (true) {
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

    // 構造化束縛に const を付けない: `const auto [pos, exist] = get_pos(key)` だと GCC が見つかった場合と
    // 見つからなかった場合の分岐を合流させ、命令数が 2 割ほど増える (GCC 15, -O2 で確認)
    V get(const u64 key) const {
        auto [pos, exist_res] = get_pos(key);
        if (!exist_res) return V();
        else return vals[pos];
    }

    V get(const u64 key, const V missing) const {
        auto [pos, exist_res] = get_pos(key);
        if (!exist_res) return missing;
        else return vals[pos];
    }

    bool contains(const u64 key) const {
        return get_pos(key).second;
    }

    pair<int, bool> pos(const u64 key) const {
        return get_pos(key);
    }

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

    V inner_get(const pair<int, bool> &dat, const V missing) const {
        auto [pos, is_exist] = dat;
        if (!is_exist) return missing;
        return vals[pos];
    }

    V inner_get(const pair<int, bool> &dat) {
        auto [pos, is_exist] = dat;
        if (!is_exist) return V();
        return vals[pos];
    }

    void inner_set(const pair<int, bool> &dat, const u64 key, const V val) {
        auto [pos, is_exist] = dat;
        vals[pos] = val;
        if (!is_exist) {
            uint8_t h2 = hash(key) & 0x7F;
            meta[pos] = h2;
            if (pos < 16) meta[cap + pos] = h2;
            keys[pos] = key;
            ++size;
            if (size * 2 > cap) {
                rebuild();
            }
        }
    }

    void set(const u64 key, const V val) {
        auto [pos, is_exist] = get_pos(key);
        inner_set({pos, is_exist}, key, val);
    }

    void add(const u64 key, const V val) {
        auto [pos, is_exist] = get_pos(key);
        if (!is_exist) {
            inner_set({pos, false}, key, val);
        } else {
            vals[pos] += val;
        }
    }

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

    void clear() {
        if (empty()) return;
        size = 0;
        fill(meta.begin(), meta.end(), EMPTY);
    }

    int len() const { return size; }
    int inner_len() const { return cap; }
    int inner_size() const { return cap; }
    bool empty() const { return size == 0; }
};
} // namespace titan23
