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

// get_pos と pos の結果は次の変更操作まで有効
// erase を持たないため key == 0 を空きスロットとして使う
// USE_HASH_FUNC=true は固定入力への衝突攻撃を避ける実用的な乱択 mixer を使う
// USE_HASH_FUNC=false では偏った下位ビットにより操作が O(n) になり得る
template<typename V, bool USE_HASH_FUNC=true>
class LinearHashDict {
private:
    using u64 = uint64_t;

    vector<u64> keys;
    vector<V> vals;
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

    LinearHashDict(const int n) : cap(16), msk(15), salt(0), size(0), has_zero(false) {
        assert(n >= 0);
        init_seed();
        init_table(n);
    }

    pair<int, bool> get_pos(const u64 &key) const {
        if (key == 0) return {cap, has_zero};
        int pos = (int)(hash(key) & msk);
        while (keys[pos] != 0) {
            if (keys[pos] == key) return {pos, true};
            pos = (pos + 1) & msk;
        }
        return {pos, false};
    }

    V get(const u64 key) const {
        if (key == 0) return has_zero ? vals[cap] : V();
        int pos = (int)(hash(key) & msk);
        while (keys[pos] != 0) {
            if (keys[pos] == key) return vals[pos];
            pos = (pos + 1) & msk;
        }
        return V();
    }

    V get(const u64 key, const V missing) const {
        if (key == 0) return has_zero ? vals[cap] : missing;
        int pos = (int)(hash(key) & msk);
        while (keys[pos] != 0) {
            if (keys[pos] == key) return vals[pos];
            pos = (pos + 1) & msk;
        }
        return missing;
    }

    bool contains(const u64 key) const {
        return get_pos(key).second;
    }

    pair<int, bool> pos(const u64 key) const {
        return get_pos(key);
    }

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

    V inner_get(const pair<int, bool> &dat, const V missing) const {
        if (!dat.second) return missing;
        return vals[dat.first];
    }

    V inner_get(const pair<int, bool> &dat) {
        if (!dat.second) return V();
        return vals[dat.first];
    }

    void inner_set(const pair<int, bool> &dat, const u64 key, const V val) {
        vals[dat.first] = val;
        if (dat.second) return;
        if (key == 0) has_zero = true;
        else keys[dat.first] = key;
        ++size;
        if (size * 2 > cap) rebuild();
    }

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

    vector<V> values() const {
        vector<V> res;
        res.reserve(size);
        if (has_zero) res.emplace_back(vals[cap]);
        for (int i = 0; i < cap; ++i) {
            if (keys[i] != 0) res.emplace_back(vals[i]);
        }
        return res;
    }

    vector<pair<u64, V>> items() const {
        vector<pair<u64, V>> res;
        res.reserve(size);
        if (has_zero) res.emplace_back(0, vals[cap]);
        for (int i = 0; i < cap; ++i) {
            if (keys[i] != 0) res.emplace_back(keys[i], vals[i]);
        }
        return res;
    }

    void clear() {
        if (empty()) return;
        fill(keys.begin(), keys.end(), 0);
        size = 0;
        has_zero = false;
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
