/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/ds/hopscotch_hash_dict.cpp
#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

using namespace std;

namespace titan23 {

// get_pos と pos の結果は次の変更操作まで有効
// USE_HASH_FUNC=true は固定入力への衝突攻撃を避ける実用的な乱択 mixer を使う
// USE_HASH_FUNC=false で同じ home に k 個集中した場合の検索は O(k)
template<typename V, bool USE_HASH_FUNC=true>
class HopscotchHashDict {
private:
    using u64 = uint64_t;
    static constexpr int H = 32;
    static constexpr int ADD_RANGE = 256;

    struct Overflow {
        u64 key;
        V val;
        int next;
    };

    vector<uint32_t> hop;
    vector<uint8_t> used;
    vector<u64> keys;
    vector<V> vals;
    vector<int> over_head;
    vector<Overflow> over; // 同一 home への極端な集中を正確に扱う退避領域
    int cap;
    int msk;
    u64 salt;
    int size;

    constexpr u64 hash(u64 key) const {
        if constexpr (USE_HASH_FUNC) {
            key ^= salt;
            key = (key ^ (key >> 30)) * 0xbf58476d1ce4e5b9;
            key = (key ^ (key >> 27)) * 0x94d049bb133111eb;
            key ^= key >> 31;
        }
        return key;
    }

    void init_seed() {
        random_device rd;
        uniform_int_distribution<uint32_t> dis;
        salt = (u64(dis(rd)) << 32) | dis(rd);
    }

    void init_table(int n) {
        cap = n;
        msk = cap - 1;
        hop.assign(cap, 0);
        used.assign(cap, 0);
        keys.resize(cap);
        vals.resize(cap);
        over_head.clear();
        over.clear();
    }

    int find_slot(u64 key, int home) const {
        uint32_t bits = hop[home];
        while (bits) {
            int d = __builtin_ctz(bits);
            int pos = (home + d) & msk;
            if (keys[pos] == key) return pos;
            bits &= bits - 1;
        }
        if (over.empty()) return -1;
        for (int i = over_head[home]; i != -1; i = over[i].next) {
            if (over[i].key == key) return cap + i;
        }
        return -1;
    }

    bool insert_main(u64 key, const V &val) {
        int home = (int)(hash(key) & msk);
        int free = home;
        int dist = 0;
        int limit = min(cap, ADD_RANGE);
        while (dist < limit && used[free]) {
            free = (free + 1) & msk;
            ++dist;
        }
        if (dist == limit) return false;

        while (dist >= H) {
            bool moved = false;
            for (int back = H - 1; back > 0; --back) {
                int base = (free - back) & msk;
                uint32_t bits = hop[base] & ((uint32_t(1) << back) - 1);
                if (!bits) continue;
                int d = __builtin_ctz(bits);
                int src = (base + d) & msk;
                keys[free] = keys[src];
                vals[free] = vals[src];
                used[free] = 1;
                used[src] = 0;
                hop[base] ^= uint32_t(1) << d;
                hop[base] |= uint32_t(1) << back;
                free = src;
                dist -= back - d;
                moved = true;
                break;
            }
            if (!moved) return false;
        }

        keys[free] = key;
        vals[free] = val;
        used[free] = 1;
        hop[home] |= uint32_t(1) << dist;
        return true;
    }

    void insert_over(u64 key, const V &val) {
        u64 h = hash(key);
        int home = (int)(h & msk);
        if (over.empty()) over_head.assign(cap, -1);
        over.push_back({key, val, over_head[home]});
        over_head[home] = (int)over.size() - 1;
    }

    void rebuild(int n, bool reseed=false) {
        vector<pair<u64, V>> old = items();
        if (reseed) init_seed();
        init_table(n);
        size = 0;
        for (const auto &[key, val] : old) {
            if (!insert_main(key, val)) insert_over(key, val);
            ++size;
        }
    }

    void insert_new(u64 key, const V &val) {
        if (size + 1 > cap - cap / 8) rebuild(cap * 2);
        if (insert_main(key, val)) {
            ++size;
            return;
        }
        if constexpr (USE_HASH_FUNC) {
            rebuild(cap, true);
            if (insert_main(key, val)) {
                ++size;
                return;
            }
            rebuild(cap * 2, true);
            if (insert_main(key, val)) {
                ++size;
                return;
            }
        }
        insert_over(key, val);
        ++size;
    }

    V &value_at(int pos) {
        if (pos < cap) return vals[pos];
        return over[pos - cap].val;
    }

    const V &value_at(int pos) const {
        if (pos < cap) return vals[pos];
        return over[pos - cap].val;
    }

public:
    HopscotchHashDict() : cap(32), msk(31), salt(0), size(0) {
        init_seed();
        init_table(cap);
    }

    HopscotchHashDict(const int n) : cap(32), msk(31), salt(0), size(0) {
        assert(n >= 0);
        while (n > cap - cap / 8) cap *= 2;
        init_seed();
        init_table(cap);
    }

    pair<int, bool> get_pos(const u64 &key) const {
        u64 h = hash(key);
        int home = (int)(h & msk);
        int pos = find_slot(key, home);
        if (pos == -1) return {home, false};
        return {pos, true};
    }

    V get(const u64 key) const {
        const auto [pos, exist] = get_pos(key);
        if (!exist) return V();
        return value_at(pos);
    }

    V get(const u64 key, const V missing) const {
        const auto [pos, exist] = get_pos(key);
        if (!exist) return missing;
        return value_at(pos);
    }

    bool contains(const u64 key) const {
        return get_pos(key).second;
    }

    pair<int, bool> pos(const u64 key) const {
        return get_pos(key);
    }

    V operator[](const u64 key) {
        const auto [pos, exist] = get_pos(key);
        if (exist) return value_at(pos);
        V val{};
        inner_set({pos, false}, key, val);
        return val;
    }

    V inner_get(const pair<int, bool> &dat, const V missing) const {
        const auto [pos, exist] = dat;
        if (!exist) return missing;
        return value_at(pos);
    }

    V inner_get(const pair<int, bool> &dat) {
        const auto [pos, exist] = dat;
        if (!exist) return V();
        return value_at(pos);
    }

    void inner_set(const pair<int, bool> &dat, const u64 key, const V val) {
        const auto [pos, exist] = dat;
        if (exist) {
            value_at(pos) = val;
        } else {
            insert_new(key, val);
        }
    }

    void set(const u64 key, const V val) {
        const auto dat = get_pos(key);
        inner_set(dat, key, val);
    }

    void add(const u64 key, const V val) {
        const auto [pos, exist] = get_pos(key);
        if (exist) {
            value_at(pos) += val;
        } else {
            insert_new(key, val);
        }
    }

    bool contains_set(const u64 key, const V val) {
        const auto [pos, exist] = get_pos(key);
        if (exist) {
            if (val < value_at(pos)) {
                value_at(pos) = val;
                return true;
            }
            return false;
        }
        insert_new(key, val);
        return false;
    }

    vector<V> values() const {
        vector<V> res;
        res.reserve(size);
        for (int i = 0; i < cap; ++i) {
            if (used[i]) res.emplace_back(vals[i]);
        }
        for (const auto &item : over) res.emplace_back(item.val);
        return res;
    }

    vector<pair<u64, V>> items() const {
        vector<pair<u64, V>> res;
        res.reserve(size);
        for (int i = 0; i < cap; ++i) {
            if (used[i]) res.emplace_back(keys[i], vals[i]);
        }
        for (const auto &item : over) res.emplace_back(item.key, item.val);
        return res;
    }

    void clear() {
        if (empty()) return;
        fill(hop.begin(), hop.end(), 0);
        fill(used.begin(), used.end(), 0);
        over_head.clear();
        over.clear();
        size = 0;
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
