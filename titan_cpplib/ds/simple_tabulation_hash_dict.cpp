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

// 入力列が内部乱数と独立なら負荷率 1/2 で検索と更新は期待償却 O(1)
// 保証は tab の各値が独立な一様乱数であるモデルに基づく
// 容量に必要な下位 bit だけを生成し 拡張時は独立な新しい bit plane を加える
// get_pos と pos の結果は次の変更操作まで有効
// erase を持たないため key == 0 を空きスロットとして使う
// clear と values と items は O(cap)
template<typename V>
class SimpleTabulationHashDict {
private:
    using u64 = uint64_t;

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
    SimpleTabulationHashDict() : cap(16), msk(15), hash_bits(4), size(0), has_zero(false) {
        init_table(0);
        init_hash(4);
    }

    SimpleTabulationHashDict(const int n)
        : cap(16), msk(15), hash_bits(4), size(0), has_zero(false) {
        assert(n >= 0);
        init_table(n);
        init_hash(__builtin_ctz((unsigned)cap));
    }

    pair<int, bool> get_pos(const u64 &key) const {
        if (key == 0) return {cap, has_zero};
        int p = (int)(hash(key) & msk);
        while (keys[p] != 0) {
            if (keys[p] == key) return {p, true};
            p = (p + 1) & msk;
        }
        return {p, false};
    }

    V get(const u64 key) const {
        if (key == 0) return has_zero ? vals[cap] : V();
        int p = (int)(hash(key) & msk);
        while (keys[p] != 0) {
            if (keys[p] == key) return vals[p];
            p = (p + 1) & msk;
        }
        return V();
    }

    V get(const u64 key, const V missing) const {
        if (key == 0) return has_zero ? vals[cap] : missing;
        int p = (int)(hash(key) & msk);
        while (keys[p] != 0) {
            if (keys[p] == key) return vals[p];
            p = (p + 1) & msk;
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

    V inner_get(const pair<int, bool> &dat, const V missing) const {
        if (!dat.second) return missing;
        return vals[dat.first];
    }

    V inner_get(const pair<int, bool> &dat) const {
        if (!dat.second) return V();
        return vals[dat.first];
    }

    void inner_set(const pair<int, bool> &dat, const u64 key, V val) {
        vals[dat.first] = move(val);
        if (dat.second) return;
        if (key == 0) has_zero = true;
        else keys[dat.first] = key;
        ++size;
        if (size * 2 > cap) rebuild();
    }

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
