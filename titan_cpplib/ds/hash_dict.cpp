#pragma once
#include <vector>
#include <random>
#include <cassert>
#include <algorithm>
#include <emmintrin.h>

using namespace std;

namespace titan23 {

template<typename V>
class HashDict {
private:
    using u64 = uint64_t;
    static constexpr const uint8_t EMPTY = 0x80;

    vector<uint8_t> meta;
    vector<u64> keys;
    vector<V> vals;
    int cap;
    int msk;
    u64 xor_;
    int size;

    constexpr u64 hash(u64 key) const {
        key ^= xor_;
        key = (key ^ (key >> 30)) * 0xbf58476d1ce4e5b9;
        key = (key ^ (key >> 27)) * 0x94d049bb133111eb;
        key = key ^ (key >> 31);
        return key;
    }

    void init_seed() {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<u64> dis(0, UINT64_MAX);
        xor_ = dis(gen);
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
        int idx = (h >> 7) & msk;

        __m128i match = _mm_set1_epi8(h2);
        __m128i empty_match = _mm_set1_epi8(EMPTY);

        while (true) {
            __m128i meta_data = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&meta[idx]));

            int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(meta_data, match));
            while (mask != 0) {
                int bit_pos = __builtin_ctz(mask);
                int target = (idx + bit_pos) & msk;
                if (keys[target] == key) {
                    return {target, true};
                }
                mask &= (mask - 1);
            }

            int empty_mask = _mm_movemask_epi8(_mm_cmpeq_epi8(meta_data, empty_match));
            if (empty_mask != 0) {
                int bit_pos = __builtin_ctz(empty_mask);
                int target = (idx + bit_pos) & msk;
                return {target, false};
            }

            idx = (idx + 16) & msk;
        }
    }

    V get(const u64 key) const {
        const auto [pos, exist_res] = get_pos(key);
        if (!exist_res) return V();
        else return vals[pos];
    }

    V get(const u64 key, const V missing) const {
        const auto [pos, exist_res] = get_pos(key);
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
        const auto [pos, exist_res] = get_pos(key);
        if (!exist_res) {
            V res = V{};
            inner_set({pos, false}, key, res);
            return res;
        } else {
            return vals[pos];
        }
    }

    V inner_get(const pair<int, bool> &dat, const V missing) const {
        const auto [pos, is_exist] = dat;
        if (!is_exist) return missing;
        return vals[pos];
    }

    V inner_get(const pair<int, bool> &dat) {
        const auto [pos, is_exist] = dat;
        if (!is_exist) return V();
        return vals[pos];
    }

    void inner_set(const pair<int, bool> &dat, const u64 key, const V val) {
        const auto [pos, is_exist] = dat;
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
        const auto [pos, is_exist] = get_pos(key);
        inner_set({pos, is_exist}, key, val);
    }

    void add(const u64 key, const V val) {
        const auto [pos, is_exist] = get_pos(key);
        if (!is_exist) {
            inner_set({pos, false}, key, val);
        } else {
            vals[pos] += val;
        }
    }

    bool contains_set(const u64 key, const V val) {
        const auto [pos, is_exist] = get_pos(key);
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

    void clear() {
        if (empty()) return;
        size = 0;
        fill(meta.begin(), meta.end(), EMPTY);
    }

    int len() const {
        return size;
    }

    int inner_len() const {
        return cap;
    }

    bool empty() const {
        return size == 0;
    }
};

} // namespace titan23
